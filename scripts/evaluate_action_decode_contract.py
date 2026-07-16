from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from typing import Iterable, Sequence


class EvaluationError(ValueError):
    pass


TRACE_SCHEMA = "physanim-action-semantic-trace/v1"
EVALUATION_SCHEMA = "physanim-action-decode-contract-evaluation/v1"
TRACE_AUTHORITY = "DEVELOPMENT_DIAGNOSTIC_ONLY"
CAPTURE_SCOPE = "first_active_standing_target_write"

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

ACTION_SCALE = 1.0
ACTION_CLAMP = 1.0
PD_ACTION_OFFSET = 0.0
PD_ACTION_SCALE = math.pi

CONDITIONED_SCALAR_ABSOLUTE_ERROR_TOLERANCE = 1.0e-6
QUATERNION_COMPONENT_ABSOLUTE_ERROR_TOLERANCE = 2.0e-6
QUATERNION_ANGULAR_ERROR_DEGREES_TOLERANCE = 1.0e-3
QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE = 1.0e-5

SOURCE_CONTRACT = {
    "repository": "https://github.com/NVlabs/ProtoMotions",
    "tag": "v2.3",
    "commit": "4a905b998101333a2fb91f2de8e2cab4bd0db68e",
    "checkpoint_config": "data/pretrained_models/motion_tracker/smpl/config.yaml",
    "source_files": [
        "protomotions/config/robot/smpl.yaml",
        "protomotions/data/assets/mjcf/smpl_humanoid.xml",
        "protomotions/envs/base_env/env_utils/humanoid_utils.py",
        "protomotions/simulator/base_simulator/simulator.py",
    ],
    "number_of_actions": 69,
    "mimic_residual_control": False,
    "map_actions_to_pd_range": True,
    "action_scale": ACTION_SCALE,
    "clamp_actions": ACTION_CLAMP,
    "three_dof_pd_action_offset_radians": PD_ACTION_OFFSET,
    "three_dof_pd_action_scale_radians": PD_ACTION_SCALE,
    "coordinate_transform": "Isaac RH (X forward, Y left, Z up) to UE LH (X forward, Y right, Z up)",
}


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
        raise EvaluationError(
            f"{label} is missing required fields: {', '.join(missing)}"
        )


def _finite_number(value: object) -> bool:
    return (
        not isinstance(value, bool)
        and isinstance(value, (int, float))
        and math.isfinite(float(value))
    )


def _finite_vector(
    value: object,
    count: int,
    label: str,
) -> tuple[float, ...]:
    if not isinstance(value, list) or len(value) != count:
        raise EvaluationError(f"{label} must contain exactly {count} values")
    if not all(_finite_number(component) for component in value):
        raise EvaluationError(f"{label} must contain finite numeric values")
    return tuple(float(component) for component in value)


def _vector_norm(value: Sequence[float]) -> float:
    return math.sqrt(sum(component * component for component in value))


def _normalize_quaternion(value: Sequence[float]) -> tuple[float, float, float, float]:
    norm = _vector_norm(value)
    if norm == 0.0:
        raise EvaluationError("quaternion norm must be nonzero")
    return tuple(component / norm for component in value)  # type: ignore[return-value]


def _validated_quaternion(value: object, label: str) -> tuple[float, float, float, float]:
    quaternion = _finite_vector(value, 4, label)
    norm_error = abs(_vector_norm(quaternion) - 1.0)
    if norm_error > QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE:
        raise EvaluationError(
            f"{label} must be normalized within "
            f"{QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE:g}; norm error is {norm_error:g}"
        )
    return _normalize_quaternion(quaternion)


def _condition_action(raw_action: Sequence[float]) -> tuple[float, float, float]:
    return tuple(
        max(-ACTION_CLAMP, min(ACTION_CLAMP, ACTION_SCALE * component))
        for component in raw_action
    )  # type: ignore[return-value]


def _map_action_to_pd_target(
    conditioned_action: Sequence[float],
) -> tuple[float, float, float]:
    return tuple(
        PD_ACTION_OFFSET + PD_ACTION_SCALE * component
        for component in conditioned_action
    )  # type: ignore[return-value]


def _exp_map_to_quaternion(
    exp_map: Sequence[float],
) -> tuple[float, float, float, float]:
    angle = _vector_norm(exp_map)
    if angle <= sys.float_info.epsilon:
        return (0.0, 0.0, 0.0, 1.0)
    sine = math.sin(angle / 2.0)
    return (
        exp_map[0] / angle * sine,
        exp_map[1] / angle * sine,
        exp_map[2] / angle * sine,
        math.cos(angle / 2.0),
    )


def _isaac_quaternion_to_ue(
    isaac_quaternion: Sequence[float],
) -> tuple[float, float, float, float]:
    # Reflecting vectors across Y changes the rotation pseudovector by
    # det(S) * S = diag(-1, 1, -1), while preserving quaternion W.
    return _normalize_quaternion(
        (
            -isaac_quaternion[0],
            isaac_quaternion[1],
            -isaac_quaternion[2],
            isaac_quaternion[3],
        )
    )


def _sign_invariant_component_error(
    expected: Sequence[float], observed: Sequence[float]
) -> float:
    same_sign = max(abs(left - right) for left, right in zip(expected, observed))
    opposite_sign = max(abs(left + right) for left, right in zip(expected, observed))
    return min(same_sign, opposite_sign)


def _quaternion_angular_error_degrees(
    expected: Sequence[float], observed: Sequence[float]
) -> float:
    dot = abs(sum(left * right for left, right in zip(expected, observed)))
    return math.degrees(2.0 * math.acos(max(0.0, min(1.0, dot))))


def evaluate_trace(trace_path: Path | str) -> dict:
    trace_path = Path(trace_path).resolve()
    trace = _read_json(trace_path, "action semantic trace")
    _require_fields(
        trace,
        (
            "schema_version",
            "authority",
            "enabled",
            "captured",
            "capture_scope",
            "capture_error",
            "action_joints",
        ),
        "action semantic trace",
    )
    if trace["schema_version"] != TRACE_SCHEMA:
        raise EvaluationError("action semantic trace schema_version is unsupported")
    if trace["authority"] != TRACE_AUTHORITY:
        raise EvaluationError("action semantic trace authority is unsupported")
    if trace["enabled"] is not True or trace["captured"] is not True:
        raise EvaluationError("action semantic trace must be enabled and captured")
    if trace["capture_scope"] != CAPTURE_SCOPE:
        raise EvaluationError("action semantic trace capture_scope is unsupported")
    if trace["capture_error"] != "":
        raise EvaluationError("action semantic trace reports a capture_error")

    action_joints = trace["action_joints"]
    if not isinstance(action_joints, list) or len(action_joints) != len(JOINT_ORDER):
        raise EvaluationError("action semantic trace must contain exactly 23 action joints")

    joint_results = []
    scalar_errors: list[float] = []
    quaternion_component_errors: list[float] = []
    quaternion_angular_errors: list[float] = []
    expected_quaternion_norm_errors: list[float] = []
    observed_quaternion_norm_errors: list[float] = []
    scalar_mismatch_count = 0
    quaternion_mismatch_count = 0

    for expected_index, (expected_name, joint) in enumerate(
        zip(JOINT_ORDER, action_joints)
    ):
        label = f"action joint {expected_index}"
        if not isinstance(joint, dict):
            raise EvaluationError(f"{label} must be a JSON object")
        _require_fields(
            joint,
            (
                "proto_joint_index",
                "proto_joint_name",
                "raw_action",
                "conditioned_action",
                "conditioned_decoded_rotation_ue_xyzw",
            ),
            label,
        )
        if (
            isinstance(joint["proto_joint_index"], bool)
            or not isinstance(joint["proto_joint_index"], int)
            or joint["proto_joint_index"] != expected_index
            or joint["proto_joint_name"] != expected_name
        ):
            raise EvaluationError(
                f"action joint order mismatch at {expected_index}: expected {expected_name}"
            )

        raw_action = _finite_vector(joint["raw_action"], 3, f"{label} raw_action")
        observed_conditioned = _finite_vector(
            joint["conditioned_action"], 3, f"{label} conditioned_action"
        )
        observed_serialized = _finite_vector(
            joint["conditioned_decoded_rotation_ue_xyzw"],
            4,
            f"{label} conditioned_decoded_rotation_ue_xyzw",
        )
        observed_norm_error = abs(_vector_norm(observed_serialized) - 1.0)
        observed_quaternion = _validated_quaternion(
            joint["conditioned_decoded_rotation_ue_xyzw"],
            f"{label} conditioned_decoded_rotation_ue_xyzw",
        )

        expected_conditioned = _condition_action(raw_action)
        expected_pd_target = _map_action_to_pd_target(expected_conditioned)
        expected_isaac_quaternion = _exp_map_to_quaternion(expected_pd_target)
        expected_quaternion = _isaac_quaternion_to_ue(expected_isaac_quaternion)
        expected_norm_error = abs(_vector_norm(expected_quaternion) - 1.0)

        joint_scalar_errors = [
            abs(expected - observed)
            for expected, observed in zip(expected_conditioned, observed_conditioned)
        ]
        joint_scalar_mismatch_count = sum(
            error > CONDITIONED_SCALAR_ABSOLUTE_ERROR_TOLERANCE
            for error in joint_scalar_errors
        )
        component_error = _sign_invariant_component_error(
            expected_quaternion, observed_quaternion
        )
        angular_error = _quaternion_angular_error_degrees(
            expected_quaternion, observed_quaternion
        )
        quaternion_matches = (
            component_error <= QUATERNION_COMPONENT_ABSOLUTE_ERROR_TOLERANCE
            and angular_error <= QUATERNION_ANGULAR_ERROR_DEGREES_TOLERANCE
        )

        scalar_mismatch_count += joint_scalar_mismatch_count
        quaternion_mismatch_count += int(not quaternion_matches)
        scalar_errors.extend(joint_scalar_errors)
        quaternion_component_errors.append(component_error)
        quaternion_angular_errors.append(angular_error)
        expected_quaternion_norm_errors.append(expected_norm_error)
        observed_quaternion_norm_errors.append(observed_norm_error)
        joint_results.append(
            {
                "proto_joint_index": expected_index,
                "proto_joint_name": expected_name,
                "raw_action": list(raw_action),
                "expected_conditioned_action": list(expected_conditioned),
                "observed_conditioned_action": list(observed_conditioned),
                "conditioned_scalar_absolute_errors": joint_scalar_errors,
                "conditioned_scalar_mismatch_count": joint_scalar_mismatch_count,
                "expected_pd_target_radians": list(expected_pd_target),
                "expected_decoded_rotation_isaac_xyzw": list(
                    expected_isaac_quaternion
                ),
                "expected_decoded_rotation_ue_xyzw": list(expected_quaternion),
                "observed_decoded_rotation_ue_xyzw": list(observed_quaternion),
                "expected_quaternion_norm_error": expected_norm_error,
                "observed_quaternion_norm_error": observed_norm_error,
                "sign_invariant_quaternion_component_absolute_error": component_error,
                "quaternion_angular_error_degrees": angular_error,
                "quaternion_matches": quaternion_matches,
            }
        )

    contract_matches = scalar_mismatch_count == 0 and quaternion_mismatch_count == 0
    return {
        "schema_version": EVALUATION_SCHEMA,
        "status": "VALID",
        "valid": True,
        "contract_verdict": "MATCH" if contract_matches else "MISMATCH",
        "authority": "DEVELOPMENT_EVIDENCE_ONLY",
        "product_success": False,
        "trace": str(trace_path),
        "source_contract": SOURCE_CONTRACT,
        "tolerances": {
            "conditioned_scalar_absolute_error": CONDITIONED_SCALAR_ABSOLUTE_ERROR_TOLERANCE,
            "sign_invariant_quaternion_component_absolute_error": QUATERNION_COMPONENT_ABSOLUTE_ERROR_TOLERANCE,
            "quaternion_angular_error_degrees": QUATERNION_ANGULAR_ERROR_DEGREES_TOLERANCE,
            "quaternion_norm_absolute_error_for_validity": QUATERNION_NORM_ABSOLUTE_ERROR_TOLERANCE,
        },
        "joint_order": list(JOINT_ORDER),
        "metrics": {
            "joint_count": len(joint_results),
            "conditioned_scalar_count": len(scalar_errors),
            "conditioned_scalar_mismatch_count": scalar_mismatch_count,
            "maximum_conditioned_scalar_absolute_error": max(scalar_errors),
            "decoded_quaternion_count": len(quaternion_component_errors),
            "decoded_quaternion_mismatch_count": quaternion_mismatch_count,
            "maximum_sign_invariant_quaternion_component_absolute_error": max(
                quaternion_component_errors
            ),
            "maximum_decoded_quaternion_angular_error_degrees": max(
                quaternion_angular_errors
            ),
            "maximum_expected_quaternion_norm_error": max(
                expected_quaternion_norm_errors
            ),
            "maximum_observed_quaternion_norm_error": max(
                observed_quaternion_norm_errors
            ),
        },
        "joints": joint_results,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Evaluate one action-semantic trace against ProtoMotions v2.3"
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
            "source_contract": SOURCE_CONTRACT,
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
