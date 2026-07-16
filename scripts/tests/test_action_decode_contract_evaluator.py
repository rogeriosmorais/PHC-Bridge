from __future__ import annotations

import json
import math
from pathlib import Path

import pytest

from scripts.evaluate_action_decode_contract import EvaluationError, evaluate_trace, main


JOINT_ORDER = (
    "L_Hip",
    "L_Knee",
    "L_Ankle",
    "L_Toe",
    "R_Hip",
    "R_Knee",
    "R_Ankle",
    "R_Toe",
    "Torso",
    "Spine",
    "Chest",
    "Neck",
    "Head",
    "L_Thorax",
    "L_Shoulder",
    "L_Elbow",
    "L_Wrist",
    "L_Hand",
    "R_Thorax",
    "R_Shoulder",
    "R_Elbow",
    "R_Wrist",
    "R_Hand",
)


def _expected_ue_quaternion(raw_action: list[float]) -> list[float]:
    target = [math.pi * max(-1.0, min(1.0, value)) for value in raw_action]
    angle = math.sqrt(sum(value * value for value in target))
    if angle == 0.0:
        return [0.0, 0.0, 0.0, 1.0]
    sine = math.sin(angle / 2.0)
    isaac = [value / angle * sine for value in target]
    return [-isaac[0], isaac[1], -isaac[2], math.cos(angle / 2.0)]


def _write_trace(
    path: Path,
    *,
    raw_actions: list[list[float]] | None = None,
) -> Path:
    raw_actions = raw_actions or [
        [0.01 * (index + 1), -0.005 * index, 0.0025 * (index % 5)]
        for index in range(len(JOINT_ORDER))
    ]
    joints = []
    for index, (name, raw_action) in enumerate(zip(JOINT_ORDER, raw_actions)):
        conditioned = [max(-1.0, min(1.0, value)) for value in raw_action]
        joints.append(
            {
                "proto_joint_index": index,
                "proto_joint_name": name,
                "manny_bone_name": f"fixture_bone_{index}",
                "shares_mapped_control": False,
                "raw_action": raw_action,
                "conditioned_action": conditioned,
                "raw_decoded_rotation_ue_xyzw": [0.0, 0.0, 0.0, 1.0],
                "conditioned_decoded_rotation_ue_xyzw": _expected_ue_quaternion(
                    raw_action
                ),
            }
        )
    trace = {
        "schema_version": "physanim-action-semantic-trace/v1",
        "authority": "DEVELOPMENT_DIAGNOSTIC_ONLY",
        "enabled": True,
        "captured": True,
        "capture_scope": "first_active_standing_target_write",
        "capture_error": "",
        "policy_step_delta_time": 1.0 / 30.0,
        "policy_influence_alpha": 1.0,
        "max_angular_step_deg": 0.0,
        "constraint_adapter_enabled": True,
        "action_joints": joints,
        "control_targets": [],
    }
    path.write_text(json.dumps(trace), encoding="utf-8")
    return path


def _load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _store(path: Path, value: dict) -> None:
    path.write_text(json.dumps(value), encoding="utf-8")


def test_exact_pinned_contract_matches_and_publishes_metrics(tmp_path: Path) -> None:
    path = _write_trace(
        tmp_path / "trace.json",
        raw_actions=[[1.5, -1.5, 0.25]]
        + [[0.02 * index, -0.01, 0.005] for index in range(1, 23)],
    )

    result = evaluate_trace(path)

    assert result["status"] == "VALID"
    assert result["contract_verdict"] == "MATCH"
    assert result["product_success"] is False
    assert result["source_contract"]["tag"] == "v2.3"
    assert result["source_contract"]["commit"] == (
        "4a905b998101333a2fb91f2de8e2cab4bd0db68e"
    )
    assert result["metrics"]["joint_count"] == 23
    assert result["metrics"]["conditioned_scalar_count"] == 69
    assert result["metrics"]["conditioned_scalar_mismatch_count"] == 0
    assert result["metrics"]["decoded_quaternion_mismatch_count"] == 0
    assert result["metrics"]["maximum_conditioned_scalar_absolute_error"] == 0.0
    assert result["metrics"]["maximum_observed_quaternion_norm_error"] < 1.0e-12
    assert result["joints"][0]["expected_conditioned_action"] == [1.0, -1.0, 0.25]
    assert result["joints"][0]["expected_pd_target_radians"] == [
        math.pi,
        -math.pi,
        math.pi / 4.0,
    ]


def test_scalar_difference_is_a_valid_contract_mismatch(tmp_path: Path) -> None:
    path = _write_trace(tmp_path / "trace.json")
    trace = _load(path)
    trace["action_joints"][4]["conditioned_action"][2] += 0.01
    _store(path, trace)

    result = evaluate_trace(path)

    assert result["status"] == "VALID"
    assert result["contract_verdict"] == "MISMATCH"
    assert result["metrics"]["conditioned_scalar_mismatch_count"] == 1
    assert result["metrics"]["decoded_quaternion_mismatch_count"] == 0


def test_decoded_quaternion_difference_is_a_valid_contract_mismatch(
    tmp_path: Path,
) -> None:
    path = _write_trace(tmp_path / "trace.json")
    trace = _load(path)
    trace["action_joints"][7]["conditioned_decoded_rotation_ue_xyzw"] = [
        0.0,
        0.0,
        0.0,
        1.0,
    ]
    _store(path, trace)

    result = evaluate_trace(path)

    assert result["contract_verdict"] == "MISMATCH"
    assert result["metrics"]["decoded_quaternion_mismatch_count"] == 1
    assert result["metrics"]["maximum_decoded_quaternion_angular_error_degrees"] > 0.0


def test_quaternion_sign_is_invariant(tmp_path: Path) -> None:
    path = _write_trace(tmp_path / "trace.json")
    trace = _load(path)
    observed = trace["action_joints"][12]["conditioned_decoded_rotation_ue_xyzw"]
    trace["action_joints"][12]["conditioned_decoded_rotation_ue_xyzw"] = [
        -value for value in observed
    ]
    _store(path, trace)

    result = evaluate_trace(path)

    assert result["contract_verdict"] == "MATCH"
    assert result["joints"][12]["sign_invariant_quaternion_component_absolute_error"] == 0.0
    assert result["joints"][12]["quaternion_angular_error_degrees"] == 0.0


def test_wrong_joint_order_is_rejected(tmp_path: Path) -> None:
    path = _write_trace(tmp_path / "trace.json")
    trace = _load(path)
    trace["action_joints"][1], trace["action_joints"][2] = (
        trace["action_joints"][2],
        trace["action_joints"][1],
    )
    _store(path, trace)

    with pytest.raises(EvaluationError, match="joint order"):
        evaluate_trace(path)


@pytest.mark.parametrize(
    ("mutation", "error"),
    [
        (lambda trace: trace.update(schema_version="wrong/v1"), "schema_version"),
        (lambda trace: trace["action_joints"].pop(), "exactly 23"),
        (
            lambda trace: trace["action_joints"][0].update(raw_action=[0.0, 0.0]),
            "raw_action must contain exactly 3",
        ),
        (
            lambda trace: trace["action_joints"][0].update(
                raw_action=[float("nan"), 0.0, 0.0]
            ),
            "raw_action must contain finite",
        ),
    ],
)
def test_malformed_or_nonfinite_trace_is_rejected(
    tmp_path: Path, mutation, error: str
) -> None:
    path = _write_trace(tmp_path / "trace.json")
    trace = _load(path)
    mutation(trace)
    _store(path, trace)

    with pytest.raises(EvaluationError, match=error):
        evaluate_trace(path)


def test_non_normalized_quaternion_is_rejected(tmp_path: Path) -> None:
    path = _write_trace(tmp_path / "trace.json")
    trace = _load(path)
    trace["action_joints"][0]["conditioned_decoded_rotation_ue_xyzw"] = [
        0.0,
        0.0,
        0.0,
        2.0,
    ]
    _store(path, trace)

    with pytest.raises(EvaluationError, match="must be normalized"):
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
    assert result["valid"] is False
    assert result["contract_verdict"] == "NOT_EVALUATED"
    assert result["product_success"] is False
