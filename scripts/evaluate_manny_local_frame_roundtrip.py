from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Iterable, Sequence


class EvaluationError(ValueError):
    pass


TRACE_SCHEMA = "physanim-manny-local-frame-roundtrip/v1"
EVALUATION_SCHEMA = "physanim-manny-local-frame-roundtrip-evaluation/v1"
TRACE_AUTHORITY = "DEVELOPMENT_DIAGNOSTIC_ONLY"
CAPTURE_SCOPE = "first_active_standing_pre_range_target"
AXIS_PROBE_DEGREES = 10.0
ANGULAR_TOLERANCE_DEGREES = 1.0e-3
QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE = 1.0e-5
QUATERNION_COMPONENT_PUBLICATION_TOLERANCE = 2.0e-6
QUATERNION_ANGULAR_PUBLICATION_TOLERANCE_DEGREES = 1.0e-3
SCALAR_PUBLICATION_TOLERANCE = 1.0e-5

QUATERNION_MULTIPLICATION = (
    "Hamilton product; expressions are evaluated in the explicitly parenthesized order"
)
ACTION_COMPOSITION_ORDER = (
    "inverse(action_axis_reference) * canonical_input * "
    "action_axis_reference * policy_neutral"
)
OBSERVATION_RECOVERY_ORDER = (
    "observation_parent_bind * (manny_target * "
    "inverse(observation_bind_parent_relative)) * "
    "inverse(observation_parent_bind)"
)
ROUNDTRIP_BODY_SELECTION = "lowest_source_proto_joint_index"

PROTO_JOINT_NAMES = (
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

OBSERVATION_BODY_NAMES = (
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

SMPL_PARENT_INDICES = (
    -1,
    0,
    1,
    2,
    3,
    0,
    5,
    6,
    7,
    0,
    9,
    10,
    11,
    12,
    11,
    14,
    15,
    16,
    17,
    11,
    19,
    20,
    21,
    22,
)

EXPECTED_CONTROLS = (
    ("thigh_l", (0,)),
    ("calf_l", (1,)),
    ("foot_l", (2,)),
    ("ball_l", (3,)),
    ("thigh_r", (4,)),
    ("calf_r", (5,)),
    ("foot_r", (6,)),
    ("ball_r", (7,)),
    ("spine_01", (8,)),
    ("spine_02", (9,)),
    ("spine_03", (10,)),
    ("neck_01", (11,)),
    ("head", (12,)),
    ("clavicle_l", (13,)),
    ("upperarm_l", (14,)),
    ("lowerarm_l", (15,)),
    ("hand_l", (16, 17)),
    ("clavicle_r", (18,)),
    ("upperarm_r", (19,)),
    ("lowerarm_r", (20,)),
    ("hand_r", (21, 22)),
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


def _read_json(path: Path, label: str) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise EvaluationError(f"{label} does not exist: {path}") from exc
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        raise EvaluationError(f"{label} is not valid UTF-8 JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise EvaluationError(f"{label} must be a JSON object: {path}")
    return value


def _require_fields(value: dict, fields: Iterable[str], label: str) -> None:
    missing = sorted(field for field in fields if field not in value)
    if missing:
        raise EvaluationError(f"{label} is missing required fields: {', '.join(missing)}")


def _finite_number(value: object) -> bool:
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and math.isfinite(float(value))
    )


def _finite_nonnegative(value: object, label: str) -> float:
    if not _finite_number(value) or float(value) < 0.0:
        raise EvaluationError(f"{label} must be a finite nonnegative number")
    return float(value)


def _vector_norm(value: Sequence[float]) -> float:
    return math.sqrt(sum(component * component for component in value))


def _quaternion(
    value: object,
    label: str,
    normalized_quaternions: list[float],
) -> tuple[float, float, float, float]:
    if not isinstance(value, list) or len(value) != 4:
        raise EvaluationError(f"{label} must contain exactly 4 values")
    if not all(_finite_number(component) for component in value):
        raise EvaluationError(f"{label} must contain finite numeric values")
    result = tuple(float(component) for component in value)
    norm = _vector_norm(result)
    norm_error = abs(norm - 1.0)
    if norm_error > QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE:
        raise EvaluationError(
            f"{label} must be normalized within "
            f"{QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE:g}; norm error is {norm_error:g}"
        )
    normalized_quaternions.append(norm_error)
    return tuple(component / norm for component in result)  # type: ignore[return-value]


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


def _inverse(value: Sequence[float]) -> tuple[float, float, float, float]:
    return (-value[0], -value[1], -value[2], value[3])


def _normalize(value: Sequence[float]) -> tuple[float, float, float, float]:
    norm = _vector_norm(value)
    return tuple(component / norm for component in value)  # type: ignore[return-value]


def _compose_action_target(
    action_axis: Sequence[float],
    canonical_input: Sequence[float],
    policy_neutral: Sequence[float],
) -> tuple[float, float, float, float]:
    return _normalize(
        _multiply(
            _multiply(
                _multiply(_inverse(action_axis), canonical_input),
                action_axis,
            ),
            policy_neutral,
        )
    )


def _recover_observation_rotation(
    parent_bind: Sequence[float],
    bind_parent_relative: Sequence[float],
    manny_target: Sequence[float],
) -> tuple[float, float, float, float]:
    return _normalize(
        _multiply(
            _multiply(
                parent_bind,
                _multiply(manny_target, _inverse(bind_parent_relative)),
            ),
            _inverse(parent_bind),
        )
    )


def _angular_error_degrees(left: Sequence[float], right: Sequence[float]) -> float:
    dot = abs(sum(a * b for a, b in zip(left, right)))
    return math.degrees(2.0 * math.acos(max(0.0, min(1.0, dot))))


def _component_error(left: Sequence[float], right: Sequence[float]) -> float:
    same_sign = max(abs(a - b) for a, b in zip(left, right))
    opposite_sign = max(abs(a + b) for a, b in zip(left, right))
    return min(same_sign, opposite_sign)


def _require_published_quaternion(
    observed: Sequence[float], expected: Sequence[float], label: str
) -> None:
    component_error = _component_error(observed, expected)
    angular_error = _angular_error_degrees(observed, expected)
    if (
        component_error > QUATERNION_COMPONENT_PUBLICATION_TOLERANCE
        or angular_error > QUATERNION_ANGULAR_PUBLICATION_TOLERANCE_DEGREES
    ):
        raise EvaluationError(
            f"{label} does not match independently recomputed quaternion: "
            f"component error {component_error:g}, angular error {angular_error:g} degrees"
        )


def _axis_angle(axis: int, degrees: float) -> tuple[float, float, float, float]:
    half_angle = math.radians(degrees) / 2.0
    components = [0.0, 0.0, 0.0, math.cos(half_angle)]
    components[axis] = math.sin(half_angle)
    return tuple(components)  # type: ignore[return-value]


def _expected_case_inputs(
    actual: Sequence[float],
) -> tuple[tuple[str, tuple[float, float, float, float]], ...]:
    return (
        ("identity", (0.0, 0.0, 0.0, 1.0)),
        ("actual_decoded", tuple(actual)),  # type: ignore[arg-type]
        ("positive_x_10_deg", _axis_angle(0, AXIS_PROBE_DEGREES)),
        ("negative_x_10_deg", _axis_angle(0, -AXIS_PROBE_DEGREES)),
        ("positive_y_10_deg", _axis_angle(1, AXIS_PROBE_DEGREES)),
        ("negative_y_10_deg", _axis_angle(1, -AXIS_PROBE_DEGREES)),
        ("positive_z_10_deg", _axis_angle(2, AXIS_PROBE_DEGREES)),
        ("negative_z_10_deg", _axis_angle(2, -AXIS_PROBE_DEGREES)),
    )


def evaluate_trace(trace_path: Path | str) -> dict:
    trace_path = Path(trace_path).resolve()
    trace = _read_json(trace_path, "Manny local-frame round-trip trace")
    _require_fields(
        trace,
        (
            "schema_version",
            "authority",
            "enabled",
            "captured",
            "valid",
            "validation_error",
            "capture_scope",
            "capture_error",
            "axis_probe_degrees",
            "quaternion_layout",
            "quaternion_multiplication",
            "action_composition_order",
            "observation_recovery_order",
            "roundtrip_observation_body_selection",
            "controls",
        ),
        "Manny local-frame round-trip trace",
    )
    if trace["schema_version"] != TRACE_SCHEMA:
        raise EvaluationError("trace schema_version is unsupported")
    if trace["authority"] != TRACE_AUTHORITY:
        raise EvaluationError("trace authority is unsupported")
    if trace["enabled"] is not True or trace["captured"] is not True:
        raise EvaluationError("trace must be enabled and captured")
    if trace["valid"] is not True or trace["validation_error"] != "":
        raise EvaluationError("trace reports failed runtime validation")
    if trace["capture_scope"] != CAPTURE_SCOPE or trace["capture_error"] != "":
        raise EvaluationError("trace is not a successful first-active-standing capture")
    if (
        not _finite_number(trace["axis_probe_degrees"])
        or not math.isclose(
            float(trace["axis_probe_degrees"]),
            AXIS_PROBE_DEGREES,
            abs_tol=1.0e-9,
        )
    ):
        raise EvaluationError("trace axis_probe_degrees does not match the locked probe")
    metadata_contract = {
        "quaternion_layout": "xyzw",
        "quaternion_multiplication": QUATERNION_MULTIPLICATION,
        "action_composition_order": ACTION_COMPOSITION_ORDER,
        "observation_recovery_order": OBSERVATION_RECOVERY_ORDER,
        "roundtrip_observation_body_selection": ROUNDTRIP_BODY_SELECTION,
    }
    for field, expected in metadata_contract.items():
        if trace[field] != expected:
            raise EvaluationError(f"trace {field} is unsupported")

    controls = trace["controls"]
    if not isinstance(controls, list) or len(controls) != len(EXPECTED_CONTROLS):
        raise EvaluationError("trace must contain exactly 21 controls")

    normalized_quaternion_errors: list[float] = []
    decisive_control_count = 0
    decisive_case_count = 0
    identity_errors: list[tuple[str, float]] = []
    actual_errors: list[tuple[str, float]] = []
    probe_errors: list[tuple[str, str, float]] = []
    axis_relationships: list[tuple[str, float]] = []
    neutral_action_bind_deltas: list[tuple[str, float]] = []
    neutral_observation_bind_deltas: list[tuple[str, float]] = []
    control_results: list[dict] = []
    observed_control_names: set[str] = set()

    frame_fields = (
        "cached_action_axis_reference_rotation_xyzw",
        "action_bind_parent_relative_rotation_xyzw",
        "policy_neutral_parent_relative_rotation_xyzw",
        "observation_parent_bind_component_rotation_xyzw",
        "observation_body_bind_component_rotation_xyzw",
        "observation_bind_parent_relative_rotation_xyzw",
        "actual_decoded_rotation_ue_xyzw",
        "actual_manny_pre_range_target_parent_relative_xyzw",
    )

    for control_index, (expected, entry) in enumerate(zip(EXPECTED_CONTROLS, controls)):
        expected_bone, expected_source_indices = expected
        label = f"control {control_index} ({expected_bone})"
        if not isinstance(entry, dict):
            raise EvaluationError(f"{label} must be a JSON object")
        _require_fields(
            entry,
            (
                "control_index",
                "manny_bone_name",
                "control_name",
                "initial_control_child_bone_name",
                "initial_control_parent_bone_name",
                "source_proto_joint_indices",
                "source_proto_joint_names",
                "observation_body_indices",
                "observation_body_names",
                "roundtrip_observation_body_index",
                "roundtrip_observation_body_name",
                "observation_parent_body_index",
                "observation_parent_body_name",
                "decisive_one_to_one",
                "ownership_complete",
                *frame_fields,
                "action_axis_vs_observation_parent_bind_angular_delta_degrees",
                "policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees",
                "policy_neutral_vs_observation_bind_parent_relative_angular_delta_degrees",
                "roundtrip_cases",
            ),
            label,
        )
        expected_observation_indices = tuple(index + 1 for index in expected_source_indices)
        expected_source_names = tuple(PROTO_JOINT_NAMES[index] for index in expected_source_indices)
        expected_observation_names = tuple(
            OBSERVATION_BODY_NAMES[index] for index in expected_observation_indices
        )
        chosen_observation_index = expected_observation_indices[0]
        expected_parent_index = SMPL_PARENT_INDICES[chosen_observation_index]
        expected_parent_name = OBSERVATION_BODY_NAMES[expected_parent_index]
        expected_decisive = len(expected_source_indices) == 1

        ownership_matches = (
            entry["control_index"] == control_index
            and entry["manny_bone_name"] == expected_bone
            and entry["control_name"] == f"PACtrl_{expected_bone}"
            and entry["initial_control_child_bone_name"] == expected_bone
            and entry["initial_control_parent_bone_name"] == expected_parent_name
            and tuple(entry["source_proto_joint_indices"]) == expected_source_indices
            and tuple(entry["source_proto_joint_names"]) == expected_source_names
            and tuple(entry["observation_body_indices"]) == expected_observation_indices
            and tuple(entry["observation_body_names"]) == expected_observation_names
            and entry["roundtrip_observation_body_index"] == chosen_observation_index
            and entry["roundtrip_observation_body_name"]
            == OBSERVATION_BODY_NAMES[chosen_observation_index]
            and entry["observation_parent_body_index"] == expected_parent_index
            and entry["observation_parent_body_name"] == expected_parent_name
            and entry["decisive_one_to_one"] is expected_decisive
            and entry["ownership_complete"] is True
        )
        if not ownership_matches:
            raise EvaluationError(f"{label} ownership is inconsistent with Proto/SMPL mapping")
        if expected_bone in observed_control_names:
            raise EvaluationError(f"{label} duplicates a Manny control owner")
        observed_control_names.add(expected_bone)

        frames = {
            field: _quaternion(entry[field], f"{label} {field}", normalized_quaternion_errors)
            for field in frame_fields
        }
        action_axis = frames["cached_action_axis_reference_rotation_xyzw"]
        action_bind_relative = frames["action_bind_parent_relative_rotation_xyzw"]
        policy_neutral = frames["policy_neutral_parent_relative_rotation_xyzw"]
        parent_bind = frames["observation_parent_bind_component_rotation_xyzw"]
        body_bind = frames["observation_body_bind_component_rotation_xyzw"]
        observation_bind_relative = frames[
            "observation_bind_parent_relative_rotation_xyzw"
        ]
        actual_decoded = frames["actual_decoded_rotation_ue_xyzw"]
        actual_manny_target = frames[
            "actual_manny_pre_range_target_parent_relative_xyzw"
        ]

        recomputed_bind_relative = _normalize(_multiply(_inverse(parent_bind), body_bind))
        _require_published_quaternion(
            observation_bind_relative,
            recomputed_bind_relative,
            f"{label} observation bind parent-relative publication",
        )

        relationship_values = (
            (
                "action_axis_vs_observation_parent_bind_angular_delta_degrees",
                _angular_error_degrees(action_axis, parent_bind),
            ),
            (
                "policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees",
                _angular_error_degrees(policy_neutral, action_bind_relative),
            ),
            (
                "policy_neutral_vs_observation_bind_parent_relative_angular_delta_degrees",
                _angular_error_degrees(policy_neutral, observation_bind_relative),
            ),
        )
        for field, recomputed in relationship_values:
            published = _finite_nonnegative(entry[field], f"{label} {field}")
            if abs(published - recomputed) > SCALAR_PUBLICATION_TOLERANCE:
                raise EvaluationError(f"{label} {field} does not match recomputed value")
        axis_relationships.append((expected_bone, relationship_values[0][1]))
        neutral_action_bind_deltas.append((expected_bone, relationship_values[1][1]))
        neutral_observation_bind_deltas.append((expected_bone, relationship_values[2][1]))

        cases = entry["roundtrip_cases"]
        if not isinstance(cases, list) or len(cases) != len(CASE_LABELS):
            raise EvaluationError(f"{label} must contain exactly 8 cases")
        recomputed_cases = []
        for case_index, ((expected_case_label, expected_input), case) in enumerate(
            zip(_expected_case_inputs(actual_decoded), cases)
        ):
            case_label = f"{label} case {case_index} ({expected_case_label})"
            if not isinstance(case, dict):
                raise EvaluationError(f"{case_label} must be a JSON object")
            _require_fields(
                case,
                (
                    "label",
                    "input_canonical_rotation_ue_xyzw",
                    "manny_pre_range_target_parent_relative_xyzw",
                    "recovered_canonical_rotation_ue_xyzw",
                    "angular_error_degrees",
                ),
                case_label,
            )
            if case["label"] != expected_case_label:
                raise EvaluationError(f"{case_label} label/order is invalid")
            published_input = _quaternion(
                case["input_canonical_rotation_ue_xyzw"],
                f"{case_label} input",
                normalized_quaternion_errors,
            )
            published_target = _quaternion(
                case["manny_pre_range_target_parent_relative_xyzw"],
                f"{case_label} target",
                normalized_quaternion_errors,
            )
            published_recovered = _quaternion(
                case["recovered_canonical_rotation_ue_xyzw"],
                f"{case_label} recovered",
                normalized_quaternion_errors,
            )
            _require_published_quaternion(published_input, expected_input, f"{case_label} input")
            recomputed_target = _compose_action_target(
                action_axis, expected_input, policy_neutral
            )
            recomputed_recovered = _recover_observation_rotation(
                parent_bind, observation_bind_relative, recomputed_target
            )
            recomputed_error = _angular_error_degrees(expected_input, recomputed_recovered)
            _require_published_quaternion(
                published_target, recomputed_target, f"{case_label} target publication"
            )
            _require_published_quaternion(
                published_recovered,
                recomputed_recovered,
                f"{case_label} recovered publication",
            )
            published_error = _finite_nonnegative(
                case["angular_error_degrees"], f"{case_label} angular_error_degrees"
            )
            if abs(published_error - recomputed_error) > SCALAR_PUBLICATION_TOLERANCE:
                raise EvaluationError(
                    f"{case_label} published angular error does not match recomputed value"
                )
            if expected_case_label == "actual_decoded":
                _require_published_quaternion(
                    actual_manny_target,
                    recomputed_target,
                    f"{label} actual pre-range target publication",
                )
            recomputed_cases.append(
                {
                    "label": expected_case_label,
                    "angular_error_degrees": recomputed_error,
                }
            )
            if expected_decisive:
                decisive_case_count += 1
                if expected_case_label == "identity":
                    identity_errors.append((expected_bone, recomputed_error))
                elif expected_case_label == "actual_decoded":
                    actual_errors.append((expected_bone, recomputed_error))
                else:
                    probe_errors.append(
                        (expected_bone, expected_case_label, recomputed_error)
                    )

        if expected_decisive:
            decisive_control_count += 1
        control_results.append(
            {
                "control_index": control_index,
                "manny_bone_name": expected_bone,
                "decisive_one_to_one": expected_decisive,
                "action_axis_vs_observation_parent_bind_angular_delta_degrees": relationship_values[
                    0
                ][1],
                "policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees": relationship_values[
                    1
                ][1],
                "policy_neutral_vs_observation_bind_parent_relative_angular_delta_degrees": relationship_values[
                    2
                ][1],
                "roundtrip_cases": recomputed_cases,
            }
        )

    if decisive_control_count != 19:
        raise EvaluationError(
            f"trace must identify exactly 19 decisive one-to-one controls, found {decisive_control_count}"
        )

    failing_identity = sorted(
        bone for bone, error in identity_errors if error > ANGULAR_TOLERANCE_DEGREES
    )
    failing_actual = sorted(
        bone for bone, error in actual_errors if error > ANGULAR_TOLERANCE_DEGREES
    )
    failing_probes = sorted(
        f"{bone}:{case_label}"
        for bone, case_label, error in probe_errors
        if error > ANGULAR_TOLERANCE_DEGREES
    )
    contract_matches = not failing_identity and not failing_actual and not failing_probes

    return {
        "schema_version": EVALUATION_SCHEMA,
        "status": "VALID",
        "valid": True,
        "contract_verdict": "MATCH" if contract_matches else "MISMATCH",
        "authority": "DEVELOPMENT_EVIDENCE_ONLY",
        "product_success": False,
        "trace": str(trace_path),
        "tolerances": {
            "roundtrip_angular_error_degrees": ANGULAR_TOLERANCE_DEGREES,
            "axis_probe_degrees": AXIS_PROBE_DEGREES,
            "quaternion_norm_absolute_error_for_validity": QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE,
            "quaternion_component_publication": QUATERNION_COMPONENT_PUBLICATION_TOLERANCE,
            "quaternion_angular_publication_degrees": QUATERNION_ANGULAR_PUBLICATION_TOLERANCE_DEGREES,
            "scalar_publication": SCALAR_PUBLICATION_TOLERANCE,
        },
        "metrics": {
            "total_control_count": len(control_results),
            "decisive_one_to_one_control_count": decisive_control_count,
            "diagnostic_collapsed_control_count": len(control_results)
            - decisive_control_count,
            "complete_frame_control_count": len(control_results),
            "normalized_quaternion_count": len(normalized_quaternion_errors),
            "maximum_quaternion_norm_error": max(normalized_quaternion_errors),
            "decisive_roundtrip_case_count": decisive_case_count,
            "maximum_identity_roundtrip_angular_error_degrees": max(
                error for _, error in identity_errors
            ),
            "maximum_actual_roundtrip_angular_error_degrees": max(
                error for _, error in actual_errors
            ),
            "maximum_axis_probe_roundtrip_angular_error_degrees": max(
                error for _, _, error in probe_errors
            ),
            "failing_identity_joints": failing_identity,
            "failing_actual_joints": failing_actual,
            "failing_axis_probes": failing_probes,
            "maximum_action_axis_vs_observation_parent_bind_angular_delta_degrees": max(
                error for _, error in axis_relationships
            ),
            "maximum_policy_neutral_vs_action_bind_parent_relative_angular_delta_degrees": max(
                error for _, error in neutral_action_bind_deltas
            ),
            "maximum_policy_neutral_vs_observation_bind_parent_relative_angular_delta_degrees": max(
                error for _, error in neutral_observation_bind_deltas
            ),
        },
        "controls": control_results,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Evaluate a Manny action/observation local-frame round-trip trace"
    )
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        result = evaluate_trace(args.trace)
    except EvaluationError as exc:
        result = {
            "schema_version": EVALUATION_SCHEMA,
            "status": "INVALID",
            "valid": False,
            "contract_verdict": "NOT_EVALUATED",
            "authority": "DEVELOPMENT_EVIDENCE_ONLY",
            "product_success": False,
            "error": str(exc),
        }
    serialized = json.dumps(result, indent=2, sort_keys=True, allow_nan=False)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized + "\n", encoding="utf-8")
    print(serialized)
    if result["status"] == "INVALID":
        return 2
    return 0 if result["contract_verdict"] == "MATCH" else 1


if __name__ == "__main__":
    sys.exit(main())
