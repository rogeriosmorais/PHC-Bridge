from __future__ import annotations

import json
from pathlib import Path

import pytest

from scripts.evaluate_action_axis_frame_experiment import (
    AxisFrameExperimentError,
    evaluate_experiment,
    main,
)
from scripts.evaluate_manny_local_frame_roundtrip import evaluate_trace
from scripts.tests.test_manny_local_frame_roundtrip_evaluator import (
    _angle,
    _axis_angle,
    _compose,
    _load,
    _mul,
    _recover,
    _store,
    _make_trace,
)


WORLD_MODE = "cached_parent_world"
COMPONENT_MODE = "cached_parent_mesh_component"


def _refresh_v2(entry: dict, mode: str) -> None:
    axis = entry["effective_action_axis_rotation_xyzw"]
    neutral = entry["policy_neutral_parent_relative_rotation_xyzw"]
    parent_bind = entry["observation_parent_bind_component_rotation_xyzw"]
    bind_relative = entry["observation_bind_parent_relative_rotation_xyzw"]
    actual = entry["actual_decoded_rotation_ue_xyzw"]
    case_inputs = (
        ("identity", [0.0, 0.0, 0.0, 1.0]),
        ("actual_decoded", actual),
        ("positive_x_10_deg", _axis_angle(0, 10.0)),
        ("negative_x_10_deg", _axis_angle(0, -10.0)),
        ("positive_y_10_deg", _axis_angle(1, 10.0)),
        ("negative_y_10_deg", _axis_angle(1, -10.0)),
        ("positive_z_10_deg", _axis_angle(2, 10.0)),
        ("negative_z_10_deg", _axis_angle(2, -10.0)),
    )
    cases = []
    for label, canonical in case_inputs:
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
    entry["effective_action_axis_mode"] = mode
    entry["roundtrip_cases"] = cases
    entry["actual_manny_pre_range_target_parent_relative_xyzw"] = cases[1][
        "manny_pre_range_target_parent_relative_xyzw"
    ]
    effective_in_component = (
        axis
        if mode == COMPONENT_MODE
        else entry["component_corrected_action_axis_rotation_xyzw"]
    )
    entry[
        "effective_action_axis_vs_observation_parent_bind_component_angular_delta_degrees"
    ] = _angle(effective_in_component, parent_bind)


def _make_v2_arm(path: Path, mode: str, corrected_axis_offset_degrees: float = 0.0) -> Path:
    _make_trace(path)
    trace = _load(path)
    trace["schema_version"] = "physanim-manny-local-frame-roundtrip/v2"
    trace["configured_action_axis_mode"] = mode
    trace["effective_action_axis_mode"] = mode
    component_world = _axis_angle(2, 90.0)
    correction_offset = _axis_angle(0, corrected_axis_offset_degrees)
    for entry in trace["controls"]:
        parent_bind = entry["observation_parent_bind_component_rotation_xyzw"]
        component_corrected = _mul(correction_offset, parent_bind)
        cached_world = _mul(component_world, component_corrected)
        entry["action_bind_component_world_rotation_xyzw"] = component_world
        entry["cached_action_axis_reference_rotation_xyzw"] = cached_world
        entry["cached_action_axis_world_rotation_xyzw"] = cached_world
        entry["component_corrected_action_axis_rotation_xyzw"] = component_corrected
        entry["effective_action_axis_rotation_xyzw"] = (
            cached_world if mode == WORLD_MODE else component_corrected
        )
        entry["action_axis_vs_observation_parent_bind_angular_delta_degrees"] = _angle(
            component_corrected, parent_bind
        )
        _refresh_v2(entry, mode)
    _store(path, trace)
    return path


def test_two_arm_component_axis_supports_locked_residual_contract(tmp_path: Path) -> None:
    result = evaluate_experiment(
        _make_v2_arm(tmp_path / "baseline.json", WORLD_MODE),
        _make_v2_arm(tmp_path / "experimental.json", COMPONENT_MODE),
    )

    assert result["status"] == "VALID"
    assert result["hypothesis_verdict"] == "SUPPORTED"
    assert result["product_success"] is False
    assert result["metrics"]["complete_control_count_per_arm"] == 21
    assert result["metrics"]["decisive_control_count_per_arm"] == 19
    assert result["metrics"]["decisive_case_count_per_arm"] == 152
    assert result["metrics"][
        "maximum_identity_target_cross_arm_delta_degrees"
    ] < 1.0e-9
    assert result["metrics"][
        "maximum_actual_error_minus_identity_error_absolute_residual_degrees"
    ] < 1.0e-5
    assert result["metrics"][
        "maximum_probe_error_minus_identity_error_absolute_residual_degrees"
    ] < 1.0e-5
    assert result["metrics"][
        "maximum_actual_runtime_target_recompute_error_degrees"
    ] < 1.0e-9


def test_valid_two_arm_result_can_falsify_axis_hypothesis(tmp_path: Path) -> None:
    result = evaluate_experiment(
        _make_v2_arm(tmp_path / "baseline.json", WORLD_MODE, 5.0),
        _make_v2_arm(tmp_path / "experimental.json", COMPONENT_MODE, 5.0),
    )

    assert result["status"] == "VALID"
    assert result["hypothesis_verdict"] == "FALSIFIED"
    assert result["metrics"][
        "maximum_effective_axis_vs_observation_parent_bind_component_degrees"
    ] > 4.9


def test_e15_v1_trace_remains_historically_evaluable(tmp_path: Path) -> None:
    result = evaluate_trace(_make_trace(tmp_path / "e15-v1.json"))

    assert result["status"] == "VALID"
    assert result["trace_schema_version"] == "physanim-manny-local-frame-roundtrip/v1"
    assert result["effective_action_axis_mode"] == WORLD_MODE


def test_v2_single_trace_recomputes_effective_axis_and_targets(tmp_path: Path) -> None:
    result = evaluate_trace(_make_v2_arm(tmp_path / "v2.json", COMPONENT_MODE))

    assert result["status"] == "VALID"
    assert result["trace_schema_version"] == "physanim-manny-local-frame-roundtrip/v2"
    assert result["effective_action_axis_mode"] == COMPONENT_MODE
    assert result["metrics"][
        "maximum_effective_action_axis_vs_observation_parent_bind_component_angular_delta_degrees"
    ] < 1.0e-9


def test_pre_intervention_decoded_action_mismatch_is_invalid(tmp_path: Path) -> None:
    baseline = _make_v2_arm(tmp_path / "baseline.json", WORLD_MODE)
    experimental = _make_v2_arm(tmp_path / "experimental.json", COMPONENT_MODE)
    trace = _load(experimental)
    trace["controls"][0]["actual_decoded_rotation_ue_xyzw"] = _axis_angle(0, 1.0)
    _refresh_v2(trace["controls"][0], COMPONENT_MODE)
    _store(experimental, trace)

    with pytest.raises(AxisFrameExperimentError, match="pre-intervention"):
        evaluate_experiment(baseline, experimental)


@pytest.mark.parametrize(
    ("mutation", "error"),
    (
        (
            lambda trace: trace.update(effective_action_axis_mode=WORLD_MODE),
            "invalid",
        ),
        (
            lambda trace: trace["controls"].pop(),
            "invalid",
        ),
        (
            lambda trace: trace["controls"][0].update(ownership_complete=False),
            "invalid",
        ),
        (
            lambda trace: trace["controls"][0].update(
                effective_action_axis_rotation_xyzw=[0.0, 0.0, 0.0, 2.0]
            ),
            "invalid",
        ),
        (
            lambda trace: trace["controls"][0].update(
                effective_action_axis_vs_observation_parent_bind_component_angular_delta_degrees=float(
                    "nan"
                )
            ),
            "invalid",
        ),
    ),
)
def test_malformed_mode_ownership_completeness_and_quaternions_are_rejected(
    tmp_path: Path, mutation, error: str
) -> None:
    baseline = _make_v2_arm(tmp_path / "baseline.json", WORLD_MODE)
    experimental = _make_v2_arm(tmp_path / "experimental.json", COMPONENT_MODE)
    trace = _load(experimental)
    mutation(trace)
    _store(experimental, trace)

    with pytest.raises(AxisFrameExperimentError, match=error):
        evaluate_experiment(baseline, experimental)


def test_inconsistent_actual_runtime_target_is_rejected(tmp_path: Path) -> None:
    baseline = _make_v2_arm(tmp_path / "baseline.json", WORLD_MODE)
    experimental = _make_v2_arm(tmp_path / "experimental.json", COMPONENT_MODE)
    trace = _load(experimental)
    trace["controls"][0][
        "actual_manny_pre_range_target_parent_relative_xyzw"
    ] = _axis_angle(0, 30.0)
    _store(experimental, trace)

    with pytest.raises(AxisFrameExperimentError, match="invalid"):
        evaluate_experiment(baseline, experimental)


def test_cli_writes_machine_readable_invalid_result(tmp_path: Path) -> None:
    output = tmp_path / "evaluation.json"
    exit_code = main(
        [
            "--baseline",
            str(tmp_path / "missing-baseline.json"),
            "--experimental",
            str(tmp_path / "missing-experimental.json"),
            "--output",
            str(output),
        ]
    )

    assert exit_code == 2
    result = json.loads(output.read_text(encoding="utf-8"))
    assert result["status"] == "INVALID"
    assert result["hypothesis_verdict"] == "NOT_EVALUATED"
    assert result["product_success"] is False
