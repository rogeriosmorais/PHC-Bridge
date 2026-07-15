from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Sequence

try:
    from scripts.evaluate_manny_local_frame_roundtrip import EvaluationError, evaluate_trace
except ModuleNotFoundError:
    from evaluate_manny_local_frame_roundtrip import EvaluationError, evaluate_trace


TRACE_SCHEMA_V2 = "physanim-manny-local-frame-roundtrip/v2"
EVALUATION_SCHEMA = "physanim-action-axis-frame-experiment-evaluation/v1"
WORLD_AXIS_MODE = "cached_parent_world"
COMPONENT_AXIS_MODE = "cached_parent_mesh_component"
ANGULAR_TOLERANCE_DEGREES = 1.0e-3
SCALAR_PUBLICATION_TOLERANCE = 1.0e-5
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
PRE_INTERVENTION_QUATERNION_FIELDS = (
    "cached_action_axis_world_rotation_xyzw",
    "component_corrected_action_axis_rotation_xyzw",
    "action_bind_component_world_rotation_xyzw",
    "action_bind_parent_relative_rotation_xyzw",
    "policy_neutral_parent_relative_rotation_xyzw",
    "observation_parent_bind_component_rotation_xyzw",
    "observation_body_bind_component_rotation_xyzw",
    "observation_bind_parent_relative_rotation_xyzw",
    "actual_decoded_rotation_ue_xyzw",
)


class AxisFrameExperimentError(ValueError):
    pass


def _read_json(path: Path, label: str) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise AxisFrameExperimentError(f"{label} is not valid UTF-8 JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise AxisFrameExperimentError(f"{label} must be a JSON object")
    return value


def _finite(value: object, label: str) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise AxisFrameExperimentError(f"{label} must be numeric")
    result = float(value)
    if not math.isfinite(result):
        raise AxisFrameExperimentError(f"{label} must be finite")
    return result


def _quat(value: object, label: str) -> tuple[float, float, float, float]:
    if not isinstance(value, list) or len(value) != 4:
        raise AxisFrameExperimentError(f"{label} must be an xyzw quaternion")
    result = tuple(_finite(component, f"{label}[{index}]") for index, component in enumerate(value))
    norm = math.sqrt(sum(component * component for component in result))
    if abs(norm - 1.0) > 1.0e-5:
        raise AxisFrameExperimentError(f"{label} must be normalized")
    return result  # type: ignore[return-value]


def _normalize(value: Sequence[float]) -> tuple[float, float, float, float]:
    norm = math.sqrt(sum(component * component for component in value))
    if not math.isfinite(norm) or norm <= 1.0e-12:
        raise AxisFrameExperimentError("cannot normalize a degenerate quaternion")
    return tuple(component / norm for component in value)  # type: ignore[return-value]


def _inverse(value: Sequence[float]) -> tuple[float, float, float, float]:
    x, y, z, w = value
    return (-x, -y, -z, w)


def _multiply(
    left: Sequence[float], right: Sequence[float]
) -> tuple[float, float, float, float]:
    lx, ly, lz, lw = left
    rx, ry, rz, rw = right
    return (
        lw * rx + lx * rw + ly * rz - lz * ry,
        lw * ry - lx * rz + ly * rw + lz * rx,
        lw * rz + lx * ry - ly * rx + lz * rw,
        lw * rw - lx * rx - ly * ry - lz * rz,
    )


def _angular_error_degrees(left: Sequence[float], right: Sequence[float]) -> float:
    normalized_left = _normalize(left)
    normalized_right = _normalize(right)
    dot = abs(sum(a * b for a, b in zip(normalized_left, normalized_right)))
    return math.degrees(2.0 * math.acos(max(-1.0, min(1.0, dot))))


def _compose_target(
    axis: Sequence[float], canonical: Sequence[float], neutral: Sequence[float]
) -> tuple[float, float, float, float]:
    return _normalize(
        _multiply(
            _multiply(_multiply(_inverse(axis), canonical), axis),
            neutral,
        )
    )


def _effective_axis_in_component(
    entry: dict, mode: str
) -> tuple[float, float, float, float]:
    effective = _quat(entry["effective_action_axis_rotation_xyzw"], "effective axis")
    if mode == COMPONENT_AXIS_MODE:
        return effective
    component_world = _quat(
        entry["action_bind_component_world_rotation_xyzw"], "component world"
    )
    return _normalize(_multiply(_inverse(component_world), effective))


def _validate_arm(path: Path, expected_mode: str, label: str) -> tuple[dict, dict]:
    try:
        single_result = evaluate_trace(path)
    except EvaluationError as exc:
        raise AxisFrameExperimentError(f"{label} trace is invalid: {exc}") from exc
    trace = _read_json(path, f"{label} trace")
    if trace.get("schema_version") != TRACE_SCHEMA_V2:
        raise AxisFrameExperimentError(f"{label} trace must use the v2 action-axis schema")
    if trace.get("configured_action_axis_mode") != expected_mode:
        raise AxisFrameExperimentError(f"{label} configured action-axis mode is wrong")
    if trace.get("effective_action_axis_mode") != expected_mode:
        raise AxisFrameExperimentError(f"{label} effective action-axis mode is wrong")
    if single_result["metrics"]["total_control_count"] != 21:
        raise AxisFrameExperimentError(f"{label} trace does not contain 21 complete controls")
    if single_result["metrics"]["decisive_one_to_one_control_count"] != 19:
        raise AxisFrameExperimentError(f"{label} trace does not contain 19 decisive controls")
    if single_result["metrics"]["decisive_roundtrip_case_count"] != 152:
        raise AxisFrameExperimentError(f"{label} trace does not contain 152 decisive cases")
    return trace, single_result


def evaluate_experiment(
    baseline_path: Path | str, experimental_path: Path | str
) -> dict:
    baseline_path = Path(baseline_path).resolve()
    experimental_path = Path(experimental_path).resolve()
    baseline, baseline_single = _validate_arm(
        baseline_path, WORLD_AXIS_MODE, "baseline"
    )
    experimental, experimental_single = _validate_arm(
        experimental_path, COMPONENT_AXIS_MODE, "experimental"
    )

    baseline_controls = baseline["controls"]
    experimental_controls = experimental["controls"]
    if len(baseline_controls) != len(experimental_controls):
        raise AxisFrameExperimentError("arms have different control counts")

    identity_input_deltas: list[float] = []
    identity_target_deltas: list[float] = []
    identity_error_deltas: list[float] = []
    actual_residuals: list[float] = []
    probe_residuals: list[float] = []
    effective_axis_relationships: list[float] = []
    actual_target_recompute_errors: list[float] = []
    controls: list[dict] = []

    for control_index, (baseline_entry, experimental_entry) in enumerate(
        zip(baseline_controls, experimental_controls)
    ):
        bone = baseline_entry.get("manny_bone_name")
        if bone != experimental_entry.get("manny_bone_name"):
            raise AxisFrameExperimentError(f"control {control_index} ownership differs between arms")
        decisive = baseline_entry.get("decisive_one_to_one") is True
        if decisive != (experimental_entry.get("decisive_one_to_one") is True):
            raise AxisFrameExperimentError(f"control {control_index} decisive ownership differs")

        for field in PRE_INTERVENTION_QUATERNION_FIELDS:
            delta = _angular_error_degrees(
                _quat(baseline_entry[field], f"baseline {bone} {field}"),
                _quat(experimental_entry[field], f"experimental {bone} {field}"),
            )
            if delta > ANGULAR_TOLERANCE_DEGREES:
                raise AxisFrameExperimentError(
                    f"pre-intervention field {field} differs for {bone} by {delta:g} degrees"
                )

        baseline_cases = baseline_entry.get("roundtrip_cases")
        experimental_cases = experimental_entry.get("roundtrip_cases")
        if not isinstance(baseline_cases, list) or not isinstance(experimental_cases, list):
            raise AxisFrameExperimentError(f"{bone} roundtrip cases are malformed")
        if len(baseline_cases) != 8 or len(experimental_cases) != 8:
            raise AxisFrameExperimentError(f"{bone} must contain eight cases in both arms")

        baseline_identity = baseline_cases[0]
        experimental_identity = experimental_cases[0]
        identity_input_delta = _angular_error_degrees(
            _quat(baseline_identity["input_canonical_rotation_ue_xyzw"], f"baseline {bone} identity input"),
            _quat(experimental_identity["input_canonical_rotation_ue_xyzw"], f"experimental {bone} identity input"),
        )
        identity_target_delta = _angular_error_degrees(
            _quat(baseline_identity["manny_pre_range_target_parent_relative_xyzw"], f"baseline {bone} identity target"),
            _quat(experimental_identity["manny_pre_range_target_parent_relative_xyzw"], f"experimental {bone} identity target"),
        )
        identity_error_delta = abs(
            _finite(baseline_identity["angular_error_degrees"], f"baseline {bone} identity error")
            - _finite(experimental_identity["angular_error_degrees"], f"experimental {bone} identity error")
        )

        effective_axis = _quat(
            experimental_entry["effective_action_axis_rotation_xyzw"],
            f"experimental {bone} effective axis",
        )
        parent_bind = _quat(
            experimental_entry["observation_parent_bind_component_rotation_xyzw"],
            f"experimental {bone} observation parent bind",
        )
        effective_relationship = _angular_error_degrees(
            _effective_axis_in_component(experimental_entry, COMPONENT_AXIS_MODE),
            parent_bind,
        )
        published_relationship = _finite(
            experimental_entry[
                "effective_action_axis_vs_observation_parent_bind_component_angular_delta_degrees"
            ],
            f"experimental {bone} effective-axis relationship",
        )
        if abs(effective_relationship - published_relationship) > SCALAR_PUBLICATION_TOLERANCE:
            raise AxisFrameExperimentError(f"{bone} effective-axis metric is inconsistent")

        neutral = _quat(
            experimental_entry["policy_neutral_parent_relative_rotation_xyzw"],
            f"experimental {bone} neutral",
        )
        actual_case = experimental_cases[1]
        actual_input = _quat(
            actual_case["input_canonical_rotation_ue_xyzw"],
            f"experimental {bone} actual input",
        )
        recomputed_actual_target = _compose_target(effective_axis, actual_input, neutral)
        actual_target_error = _angular_error_degrees(
            recomputed_actual_target,
            _quat(
                experimental_entry[
                    "actual_manny_pre_range_target_parent_relative_xyzw"
                ],
                f"experimental {bone} actual runtime target",
            ),
        )

        experimental_identity_error = _finite(
            experimental_identity["angular_error_degrees"],
            f"experimental {bone} identity error",
        )
        actual_residual = abs(
            _finite(actual_case["angular_error_degrees"], f"experimental {bone} actual error")
            - experimental_identity_error
        )
        joint_probe_residuals = []
        for case_index, (expected_label, case) in enumerate(
            zip(CASE_LABELS[2:], experimental_cases[2:]), start=2
        ):
            if case.get("label") != expected_label:
                raise AxisFrameExperimentError(
                    f"experimental {bone} case {case_index} label/order is wrong"
                )
            residual = abs(
                _finite(case["angular_error_degrees"], f"experimental {bone} {expected_label} error")
                - experimental_identity_error
            )
            joint_probe_residuals.append(residual)
            if decisive:
                probe_residuals.append(residual)

        if decisive:
            identity_input_deltas.append(identity_input_delta)
            identity_target_deltas.append(identity_target_delta)
            identity_error_deltas.append(identity_error_delta)
            actual_residuals.append(actual_residual)
            effective_axis_relationships.append(effective_relationship)
            actual_target_recompute_errors.append(actual_target_error)
        controls.append(
            {
                "control_index": control_index,
                "manny_bone_name": bone,
                "decisive_one_to_one": decisive,
                "identity_input_cross_arm_delta_degrees": identity_input_delta,
                "identity_target_cross_arm_delta_degrees": identity_target_delta,
                "identity_error_cross_arm_delta_degrees": identity_error_delta,
                "actual_error_minus_identity_error_absolute_residual_degrees": actual_residual,
                "maximum_probe_error_minus_identity_error_absolute_residual_degrees": max(joint_probe_residuals),
                "effective_axis_vs_observation_parent_bind_component_degrees": effective_relationship,
                "actual_runtime_target_recompute_error_degrees": actual_target_error,
            }
        )

    maximum_identity_input_delta = max(identity_input_deltas)
    maximum_identity_target_delta = max(identity_target_deltas)
    maximum_identity_error_delta = max(identity_error_deltas)
    maximum_actual_residual = max(actual_residuals)
    maximum_probe_residual = max(probe_residuals)
    supported = (
        maximum_identity_input_delta <= ANGULAR_TOLERANCE_DEGREES
        and maximum_identity_target_delta <= ANGULAR_TOLERANCE_DEGREES
        and maximum_identity_error_delta <= ANGULAR_TOLERANCE_DEGREES
        and maximum_actual_residual <= ANGULAR_TOLERANCE_DEGREES
        and maximum_probe_residual <= ANGULAR_TOLERANCE_DEGREES
    )

    return {
        "schema_version": EVALUATION_SCHEMA,
        "status": "VALID",
        "valid": True,
        "hypothesis_verdict": "SUPPORTED" if supported else "FALSIFIED",
        "authority": "DEVELOPMENT_EVIDENCE_ONLY",
        "product_success": False,
        "baseline_trace": str(baseline_path),
        "experimental_trace": str(experimental_path),
        "arms": {
            "baseline": WORLD_AXIS_MODE,
            "experimental": COMPONENT_AXIS_MODE,
        },
        "tolerance_degrees": ANGULAR_TOLERANCE_DEGREES,
        "metrics": {
            "complete_control_count_per_arm": 21,
            "decisive_control_count_per_arm": 19,
            "decisive_case_count_per_arm": 152,
            "maximum_identity_input_cross_arm_delta_degrees": maximum_identity_input_delta,
            "maximum_identity_target_cross_arm_delta_degrees": maximum_identity_target_delta,
            "maximum_identity_error_cross_arm_delta_degrees": maximum_identity_error_delta,
            "maximum_actual_error_minus_identity_error_absolute_residual_degrees": maximum_actual_residual,
            "maximum_probe_error_minus_identity_error_absolute_residual_degrees": maximum_probe_residual,
            "maximum_effective_axis_vs_observation_parent_bind_component_degrees": max(effective_axis_relationships),
            "maximum_actual_runtime_target_recompute_error_degrees": max(actual_target_recompute_errors),
            "baseline_single_trace_verdict": baseline_single["contract_verdict"],
            "experimental_single_trace_verdict": experimental_single["contract_verdict"],
        },
        "controls": controls,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Evaluate the two-arm E16 action-axis frame experiment"
    )
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--experimental", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        result = evaluate_experiment(args.baseline, args.experimental)
        exit_code = 0
    except AxisFrameExperimentError as exc:
        result = {
            "schema_version": EVALUATION_SCHEMA,
            "status": "INVALID",
            "valid": False,
            "hypothesis_verdict": "NOT_EVALUATED",
            "authority": "DEVELOPMENT_EVIDENCE_ONLY",
            "product_success": False,
            "error": str(exc),
        }
        exit_code = 2
    payload = json.dumps(result, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
    else:
        sys.stdout.write(payload)
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
