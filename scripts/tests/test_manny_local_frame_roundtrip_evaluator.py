from __future__ import annotations

import json
import math
from pathlib import Path

import pytest

from scripts.evaluate_manny_local_frame_roundtrip import (
    EvaluationError,
    evaluate_trace,
    main,
)


CONTROL_SPECS = (
    ("thigh_l", "pelvis", (0,), ("L_Hip",), (1,), 0),
    ("calf_l", "thigh_l", (1,), ("L_Knee",), (2,), 1),
    ("foot_l", "calf_l", (2,), ("L_Ankle",), (3,), 2),
    ("ball_l", "foot_l", (3,), ("L_Toe",), (4,), 3),
    ("thigh_r", "pelvis", (4,), ("R_Hip",), (5,), 0),
    ("calf_r", "thigh_r", (5,), ("R_Knee",), (6,), 5),
    ("foot_r", "calf_r", (6,), ("R_Ankle",), (7,), 6),
    ("ball_r", "foot_r", (7,), ("R_Toe",), (8,), 7),
    ("spine_01", "pelvis", (8,), ("Torso",), (9,), 0),
    ("spine_02", "spine_01", (9,), ("Spine",), (10,), 9),
    ("spine_03", "spine_02", (10,), ("Chest",), (11,), 10),
    ("neck_01", "spine_03", (11,), ("Neck",), (12,), 11),
    ("head", "neck_01", (12,), ("Head",), (13,), 12),
    ("clavicle_l", "spine_03", (13,), ("L_Thorax",), (14,), 11),
    ("upperarm_l", "clavicle_l", (14,), ("L_Shoulder",), (15,), 14),
    ("lowerarm_l", "upperarm_l", (15,), ("L_Elbow",), (16,), 15),
    (
        "hand_l",
        "lowerarm_l",
        (16, 17),
        ("L_Wrist", "L_Hand"),
        (17, 18),
        16,
    ),
    ("clavicle_r", "spine_03", (18,), ("R_Thorax",), (19,), 11),
    ("upperarm_r", "clavicle_r", (19,), ("R_Shoulder",), (20,), 19),
    ("lowerarm_r", "upperarm_r", (20,), ("R_Elbow",), (21,), 20),
    (
        "hand_r",
        "lowerarm_r",
        (21, 22),
        ("R_Wrist", "R_Hand"),
        (22, 23),
        21,
    ),
)

OBSERVATION_NAMES = (
    "pelvis",
    "thigh_l",
    "calf_l",
    "foot_l",
    "ball_l",
    "thigh_r",
    "calf_r",
    "foot_r",
    "ball_r",
    "spine_01",
    "spine_02",
    "spine_03",
    "neck_01",
    "head",
    "clavicle_l",
    "upperarm_l",
    "lowerarm_l",
    "hand_l",
    "hand_l",
    "clavicle_r",
    "upperarm_r",
    "lowerarm_r",
    "hand_r",
    "hand_r",
)

CASE_LABELS = (
    "identity",
    "actual_decoded",
    "positive_x_10_deg",
    "negative_x_10_deg",
    "positive_y_10_deg",
    "negative_y_10_deg",
    "positive_z_10_deg",
    "negative_z_10_deg",
)


def _mul(left: list[float], right: list[float]) -> list[float]:
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return [
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    ]


def _inverse(value: list[float]) -> list[float]:
    return [-value[0], -value[1], -value[2], value[3]]


def _axis_angle(axis: int, degrees: float) -> list[float]:
    result = [0.0, 0.0, 0.0, math.cos(math.radians(degrees) / 2.0)]
    result[axis] = math.sin(math.radians(degrees) / 2.0)
    return result


def _angle(left: list[float], right: list[float]) -> float:
    dot = abs(sum(a * b for a, b in zip(left, right)))
    return math.degrees(2.0 * math.acos(max(0.0, min(1.0, dot))))


def _compose(axis: list[float], canonical: list[float], neutral: list[float]) -> list[float]:
    return _mul(_mul(_mul(_inverse(axis), canonical), axis), neutral)


def _recover(
    parent_bind: list[float], bind_relative: list[float], target: list[float]
) -> list[float]:
    return _mul(
        _mul(parent_bind, _mul(target, _inverse(bind_relative))),
        _inverse(parent_bind),
    )


def _case_inputs(actual: list[float]) -> list[tuple[str, list[float]]]:
    return [
        ("identity", [0.0, 0.0, 0.0, 1.0]),
        ("actual_decoded", actual),
        ("positive_x_10_deg", _axis_angle(0, 10.0)),
        ("negative_x_10_deg", _axis_angle(0, -10.0)),
        ("positive_y_10_deg", _axis_angle(1, 10.0)),
        ("negative_y_10_deg", _axis_angle(1, -10.0)),
        ("positive_z_10_deg", _axis_angle(2, 10.0)),
        ("negative_z_10_deg", _axis_angle(2, -10.0)),
    ]


def _refresh_entry(entry: dict) -> None:
    axis = entry["cached_action_axis_reference_rotation_xyzw"]
    neutral = entry["policy_neutral_parent_relative_rotation_xyzw"]
    parent_bind = entry["observation_parent_bind_component_rotation_xyzw"]
    bind_relative = entry["observation_bind_parent_relative_rotation_xyzw"]
    actual = entry["actual_decoded_rotation_ue_xyzw"]
    cases = []
    for label, canonical in _case_inputs(actual):
        target = _compose(axis, canonical, neutral)
        recovered = _recover(parent_bind, bind_relative, target)
        cases.append(
            {
                "label": label,
                "input_canonical_rotation_ue_xyzw": canonical,
                "manny_pre_range_target_parent_relative_xyzw": target,
                "recovered_canonical_rotation_ue_xyzw": recovered,
                "angular_error_degrees": _angle(canonical, recovered),
            }
        )
    entry["roundtrip_cases"] = cases
    entry["actual_manny_pre_range_target_parent_relative_xyzw"] = cases[1][
        "manny_pre_range_target_parent_relative_xyzw"
    ]
    entry["action_axis_vs_observation_parent_bind_angular_delta_degrees"] = _angle(
        axis, parent_bind
    )
    entry[
        "action_bind_vs_observation_bind_parent_relative_angular_delta_degrees"
    ] = _angle(entry["action_bind_parent_relative_rotation_xyzw"], bind_relative)
    entry["policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees"] = _angle(
        neutral, entry["action_bind_parent_relative_rotation_xyzw"]
    )
    entry[
        "policy_neutral_vs_observation_bind_parent_relative_angular_delta_degrees"
    ] = _angle(neutral, bind_relative)


def _make_trace(path: Path) -> Path:
    controls = []
    for control_index, spec in enumerate(CONTROL_SPECS):
        bone, parent_bone, source_indices, source_names, observation_indices, parent_index = spec
        parent_bind = _axis_angle(2, float((control_index % 7) - 3))
        bind_relative = _axis_angle(1, float((control_index % 5) - 2))
        body_bind = _mul(parent_bind, bind_relative)
        actual = _mul(_axis_angle(0, 3.0), _axis_angle(2, -2.0))
        entry = {
            "control_index": control_index,
            "manny_bone_name": bone,
            "control_name": f"PACtrl_{bone}",
            "initial_control_child_bone_name": bone,
            "initial_control_parent_bone_name": parent_bone,
            "source_proto_joint_indices": list(source_indices),
            "source_proto_joint_names": list(source_names),
            "observation_body_indices": list(observation_indices),
            "observation_body_names": [
                OBSERVATION_NAMES[index] for index in observation_indices
            ],
            "roundtrip_observation_body_index": observation_indices[0],
            "roundtrip_observation_body_name": OBSERVATION_NAMES[observation_indices[0]],
            "observation_parent_body_index": parent_index,
            "observation_parent_body_name": OBSERVATION_NAMES[parent_index],
            "decisive_one_to_one": len(source_indices) == 1,
            "ownership_complete": True,
            "cached_action_axis_reference_rotation_xyzw": parent_bind,
            "action_bind_component_world_rotation_xyzw": [0.0, 0.0, 0.0, 1.0],
            "action_bind_parent_relative_rotation_xyzw": bind_relative,
            "policy_neutral_parent_relative_rotation_xyzw": bind_relative,
            "observation_parent_bind_component_rotation_xyzw": parent_bind,
            "observation_body_bind_component_rotation_xyzw": body_bind,
            "observation_bind_parent_relative_rotation_xyzw": bind_relative,
            "actual_decoded_rotation_ue_xyzw": actual,
        }
        _refresh_entry(entry)
        controls.append(entry)
    trace = {
        "schema_version": "physanim-manny-local-frame-roundtrip/v1",
        "authority": "DEVELOPMENT_DIAGNOSTIC_ONLY",
        "enabled": True,
        "captured": True,
        "valid": True,
        "validation_error": "",
        "capture_scope": "first_active_standing_pre_range_target",
        "capture_error": "",
        "axis_probe_degrees": 10.0,
        "quaternion_layout": "xyzw",
        "quaternion_multiplication": (
            "Hamilton product; expressions are evaluated in the explicitly parenthesized order"
        ),
        "action_composition_order": (
            "inverse(action_axis_reference) * canonical_input * "
            "action_axis_reference * policy_neutral"
        ),
        "observation_recovery_order": (
            "observation_parent_bind * (manny_target * "
            "inverse(observation_bind_parent_relative)) * "
            "inverse(observation_parent_bind)"
        ),
        "roundtrip_observation_body_selection": "lowest_source_proto_joint_index",
        "cached_action_axis_reference_frame": "world_rotation_at_initial_control_bind_capture",
        "action_bind_component_world_rotation_frame": (
            "component_to_world_rotation_derived_from_initial_parent_and_observation_parent_bind"
        ),
        "observation_bind_rotation_frame": "skeletal_mesh_component_space_bind",
        "controls": controls,
    }
    path.write_text(json.dumps(trace), encoding="utf-8")
    return path


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _store(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value), encoding="utf-8")


def test_symmetric_frames_match_and_publish_complete_metrics(tmp_path: Path) -> None:
    result = evaluate_trace(_make_trace(tmp_path / "trace.json"))

    assert result["status"] == "VALID"
    assert result["contract_verdict"] == "MATCH"
    assert result["product_success"] is False
    assert result["metrics"]["total_control_count"] == 21
    assert result["metrics"]["decisive_one_to_one_control_count"] == 19
    assert result["metrics"]["complete_frame_control_count"] == 21
    assert result["metrics"]["decisive_roundtrip_case_count"] == 152
    assert result["metrics"]["maximum_identity_roundtrip_angular_error_degrees"] < 1.0e-9
    assert result["metrics"]["maximum_actual_roundtrip_angular_error_degrees"] < 1.0e-9
    assert result["metrics"]["maximum_axis_probe_roundtrip_angular_error_degrees"] < 1.0e-9
    assert result["metrics"][
        "maximum_action_bind_vs_observation_bind_parent_relative_angular_delta_degrees"
    ] < 1.0e-9
    assert result["metrics"]["failing_identity_joints"] == []
    assert result["metrics"]["failing_actual_joints"] == []
    assert result["metrics"]["failing_axis_probes"] == []


def test_asymmetric_neutral_is_a_valid_contract_mismatch(tmp_path: Path) -> None:
    path = _make_trace(tmp_path / "trace.json")
    trace = _load(path)
    entry = trace["controls"][0]
    entry["policy_neutral_parent_relative_rotation_xyzw"] = _mul(
        _axis_angle(0, 2.0), entry["observation_bind_parent_relative_rotation_xyzw"]
    )
    _refresh_entry(entry)
    _store(path, trace)

    result = evaluate_trace(path)

    assert result["status"] == "VALID"
    assert result["contract_verdict"] == "MISMATCH"
    assert result["metrics"]["failing_identity_joints"] == ["thigh_l"]
    assert "thigh_l" in result["metrics"]["failing_actual_joints"]
    assert result["metrics"]["maximum_identity_roundtrip_angular_error_degrees"] > 1.9


def test_action_and_observation_bind_mismatch_is_independently_reported(
    tmp_path: Path,
) -> None:
    path = _make_trace(tmp_path / "trace.json")
    trace = _load(path)
    entry = trace["controls"][0]
    entry["action_bind_parent_relative_rotation_xyzw"] = _mul(
        _axis_angle(0, 5.0), entry["observation_bind_parent_relative_rotation_xyzw"]
    )
    _refresh_entry(entry)
    _store(path, trace)

    result = evaluate_trace(path)

    assert result["contract_verdict"] == "MATCH"
    assert result["metrics"][
        "maximum_action_bind_vs_observation_bind_parent_relative_angular_delta_degrees"
    ] > 4.9


def test_quaternion_sign_is_invariant(tmp_path: Path) -> None:
    path = _make_trace(tmp_path / "trace.json")
    trace = _load(path)
    trace["controls"][5]["cached_action_axis_reference_rotation_xyzw"] = [
        -value
        for value in trace["controls"][5][
            "cached_action_axis_reference_rotation_xyzw"
        ]
    ]
    _refresh_entry(trace["controls"][5])
    _store(path, trace)

    assert evaluate_trace(path)["contract_verdict"] == "MATCH"


def test_ownership_mismatch_is_rejected(tmp_path: Path) -> None:
    path = _make_trace(tmp_path / "trace.json")
    trace = _load(path)
    trace["controls"][16]["source_proto_joint_indices"] = [16]
    _store(path, trace)

    with pytest.raises(EvaluationError, match="ownership"):
        evaluate_trace(path)


@pytest.mark.parametrize(
    ("mutation", "error"),
    [
        (lambda trace: trace.update(schema_version="wrong/v1"), "schema_version"),
        (lambda trace: trace["controls"].pop(), "exactly 21"),
        (
            lambda trace: trace["controls"][0].update(
                cached_action_axis_reference_rotation_xyzw=[0.0, 0.0, 0.0, 2.0]
            ),
            "normalized",
        ),
        (
            lambda trace: trace["controls"][0].update(
                policy_neutral_parent_relative_rotation_xyzw=[
                    float("nan"),
                    0.0,
                    0.0,
                    1.0,
                ]
            ),
            "finite",
        ),
        (lambda trace: trace["controls"][0]["roundtrip_cases"].pop(), "8 cases"),
    ],
)
def test_malformed_trace_is_rejected(tmp_path: Path, mutation, error: str) -> None:
    path = _make_trace(tmp_path / "trace.json")
    trace = _load(path)
    mutation(trace)
    _store(path, trace)

    with pytest.raises(EvaluationError, match=error):
        evaluate_trace(path)


def test_inconsistent_published_roundtrip_math_is_rejected(tmp_path: Path) -> None:
    path = _make_trace(tmp_path / "trace.json")
    trace = _load(path)
    trace["controls"][0]["roundtrip_cases"][2]["angular_error_degrees"] = 5.0
    _store(path, trace)

    with pytest.raises(EvaluationError, match="published angular error"):
        evaluate_trace(path)


def test_cli_publishes_invalid_machine_readable_result(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    path = tmp_path / "trace.json"
    path.write_text("{}", encoding="utf-8")

    exit_code = main(["--trace", str(path)])
    result = json.loads(capsys.readouterr().out)

    assert exit_code == 2
    assert result["status"] == "INVALID"
    assert result["contract_verdict"] == "NOT_EVALUATED"
    assert result["product_success"] is False
