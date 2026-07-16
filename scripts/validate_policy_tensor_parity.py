from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path
from typing import Any, Sequence

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from Training.physanim.policy_tensor_reference import (
    BodySample,
    FuturePoseSample,
    build_mimic_target_poses,
    build_self_observation,
    build_terrain_observation,
)


class PolicyTensorParityError(ValueError):
    pass


def _read_object(path: Path, label: str) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise PolicyTensorParityError(f"{label} does not exist: {path}") from exc
    except json.JSONDecodeError as exc:
        raise PolicyTensorParityError(f"{label} is invalid JSON: {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise PolicyTensorParityError(f"{label} must be a JSON object: {path}")
    return value


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def _numeric_sequence(value: object, *, length: int, label: str) -> tuple[float, ...]:
    if not isinstance(value, Sequence) or isinstance(value, (str, bytes)) or len(value) != length:
        raise PolicyTensorParityError(f"{label} must contain exactly {length} numbers")
    result: list[float] = []
    for index, item in enumerate(value):
        if isinstance(item, bool) or not isinstance(item, (int, float)):
            raise PolicyTensorParityError(f"{label}[{index}] must be numeric")
        number = float(item)
        if not math.isfinite(number):
            raise PolicyTensorParityError(f"{label}[{index}] must be finite")
        result.append(number)
    return tuple(result)


def _body_sample(value: object, label: str) -> BodySample:
    if not isinstance(value, dict):
        raise PolicyTensorParityError(f"{label} must be an object")
    return BodySample(
        position=_numeric_sequence(value.get("position_xyz"), length=3, label=f"{label}.position_xyz"),
        rotation=_numeric_sequence(value.get("rotation_xyzw"), length=4, label=f"{label}.rotation_xyzw"),
        linear_velocity=_numeric_sequence(
            value.get("linear_velocity_xyz"), length=3, label=f"{label}.linear_velocity_xyz"
        ),
        angular_velocity=_numeric_sequence(
            value.get("angular_velocity_xyz"), length=3, label=f"{label}.angular_velocity_xyz"
        ),
    )


def _body_samples(value: object, label: str) -> list[BodySample]:
    if not isinstance(value, list):
        raise PolicyTensorParityError(f"{label} must be an array")
    return [_body_sample(item, f"{label}[{index}]") for index, item in enumerate(value)]


def _future_samples(value: object, label: str) -> list[FuturePoseSample]:
    if not isinstance(value, list):
        raise PolicyTensorParityError(f"{label} must be an array")
    samples: list[FuturePoseSample] = []
    for sample_index, sample in enumerate(value):
        if not isinstance(sample, dict):
            raise PolicyTensorParityError(f"{label}[{sample_index}] must be an object")
        transforms = sample.get("body_transforms")
        if not isinstance(transforms, list):
            raise PolicyTensorParityError(f"{label}[{sample_index}].body_transforms must be an array")
        bodies: list[BodySample] = []
        for body_index, transform in enumerate(transforms):
            if not isinstance(transform, dict):
                raise PolicyTensorParityError(
                    f"{label}[{sample_index}].body_transforms[{body_index}] must be an object"
                )
            bodies.append(
                BodySample(
                    position=_numeric_sequence(
                        transform.get("translation_xyz"),
                        length=3,
                        label=f"{label}[{sample_index}].body_transforms[{body_index}].translation_xyz",
                    ),
                    rotation=_numeric_sequence(
                        transform.get("rotation_xyzw"),
                        length=4,
                        label=f"{label}[{sample_index}].body_transforms[{body_index}].rotation_xyzw",
                    ),
                    linear_velocity=(0.0, 0.0, 0.0),
                    angular_velocity=(0.0, 0.0, 0.0),
                )
            )
        future_time = sample.get("future_time_seconds")
        if isinstance(future_time, bool) or not isinstance(future_time, (int, float)):
            raise PolicyTensorParityError(f"{label}[{sample_index}].future_time_seconds must be numeric")
        future_time_value = float(future_time)
        if not math.isfinite(future_time_value):
            raise PolicyTensorParityError(f"{label}[{sample_index}].future_time_seconds must be finite")
        samples.append(FuturePoseSample(bodies=bodies, future_time=future_time_value))
    return samples


def _captured_buffer(snapshot: dict[str, Any], field: str, expected_width: int) -> list[float]:
    width = snapshot.get(f"{field}_width")
    if width != expected_width:
        raise PolicyTensorParityError(
            f"snapshot {field}_width must be {expected_width}, found {width!r}"
        )
    values = snapshot.get(field)
    return list(_numeric_sequence(values, length=expected_width, label=f"snapshot.{field}"))


def _compare(expected: Sequence[float], actual: Sequence[float], tolerance: float) -> dict[str, Any]:
    if len(expected) != len(actual):
        raise PolicyTensorParityError(
            f"buffer width mismatch: rebuilt={len(expected)}, captured={len(actual)}"
        )
    differences = [abs(float(left) - float(right)) for left, right in zip(expected, actual)]
    worst_index = max(range(len(differences)), key=differences.__getitem__) if differences else -1
    mismatch_count = sum(difference > tolerance for difference in differences)
    return {
        "width": len(expected),
        "tolerance": tolerance,
        "max_abs_error": differences[worst_index] if worst_index >= 0 else 0.0,
        "mean_abs_error": sum(differences) / len(differences) if differences else 0.0,
        "mismatch_count": mismatch_count,
        "worst_index": worst_index,
        "rebuilt_at_worst_index": expected[worst_index] if worst_index >= 0 else None,
        "captured_at_worst_index": actual[worst_index] if worst_index >= 0 else None,
        "passed": mismatch_count == 0,
    }


def validate_policy_tensor_parity(
    provenance_path: Path | str,
    snapshot_path: Path | str,
    *,
    tolerance: float = 1.0e-5,
) -> dict[str, Any]:
    if not math.isfinite(tolerance) or tolerance <= 0.0:
        raise PolicyTensorParityError("tolerance must be a finite positive number")

    provenance_path = Path(provenance_path).resolve()
    snapshot_path = Path(snapshot_path).resolve()
    provenance = _read_object(provenance_path, "policy-input provenance")
    snapshot = _read_object(snapshot_path, "policy-input snapshot")

    if provenance.get("schema_version") != "physanim-policy-input-provenance/v1":
        raise PolicyTensorParityError("unsupported policy-input provenance schema")
    if provenance.get("captured") is not True or provenance.get("valid") is not True:
        raise PolicyTensorParityError("policy-input provenance must be captured and valid")
    if snapshot.get("schema_version") != "physanim-policy-input-snapshot/v1":
        raise PolicyTensorParityError("unsupported policy-input snapshot schema")
    if snapshot.get("captured") is not True:
        raise PolicyTensorParityError("policy-input snapshot must be captured")

    current_bodies = _body_samples(provenance.get("canonical_body_samples"), "canonical_body_samples")
    mimic_reference_bodies = _body_samples(
        provenance.get("mimic_reference_body_samples"), "mimic_reference_body_samples"
    )
    future_samples = _future_samples(
        provenance.get("canonical_future_pose_samples"), "canonical_future_pose_samples"
    )
    ground_height = provenance.get("self_observation_ground_height")
    if isinstance(ground_height, bool) or not isinstance(ground_height, (int, float)):
        raise PolicyTensorParityError("self_observation_ground_height must be numeric")
    ground_height_value = float(ground_height)
    if not math.isfinite(ground_height_value):
        raise PolicyTensorParityError("self_observation_ground_height must be finite")

    terrain_ground_heights = _numeric_sequence(
        provenance.get("terrain_ground_heights"), length=256, label="terrain_ground_heights"
    )

    rebuilt_self = build_self_observation(current_bodies, ground_height_value)
    rebuilt_mimic = build_mimic_target_poses(mimic_reference_bodies, future_samples)
    rebuilt_terrain = build_terrain_observation(current_bodies[0].position[2], terrain_ground_heights)

    comparisons = {
        "self_observation": _compare(
            rebuilt_self, _captured_buffer(snapshot, "self_observation", 358), tolerance
        ),
        "mimic_target_poses": _compare(
            rebuilt_mimic, _captured_buffer(snapshot, "mimic_target_poses", 6495), tolerance
        ),
        "terrain": _compare(rebuilt_terrain, _captured_buffer(snapshot, "terrain", 256), tolerance),
    }
    passed = all(comparison["passed"] for comparison in comparisons.values())
    return {
        "schema_version": "physanim-policy-tensor-parity/v1",
        "verdict": "PASS" if passed else "FAIL",
        "provenance_path": str(provenance_path),
        "provenance_sha256": _sha256(provenance_path),
        "snapshot_path": str(snapshot_path),
        "snapshot_sha256": _sha256(snapshot_path),
        "capture_scope": provenance.get("capture_scope"),
        "runtime_state": provenance.get("runtime_state"),
        "policy_control_tick": provenance.get("policy_control_tick"),
        "tensors": comparisons,
        "actions_rebuilt": False,
        "note": (
            "The parity verdict covers self_observation, mimic_target_poses, and terrain. "
            "The ONNX action output is not reconstructed by this input-packing oracle."
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Rebuild UE policy inputs from provenance and compare them with the captured buffers."
    )
    parser.add_argument("provenance", type=Path)
    parser.add_argument("snapshot", type=Path)
    parser.add_argument("--tolerance", type=float, default=1.0e-5)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        report = validate_policy_tensor_parity(
            args.provenance, args.snapshot, tolerance=args.tolerance
        )
    except PolicyTensorParityError as exc:
        report = {
            "schema_version": "physanim-policy-tensor-parity/v1",
            "verdict": "INVALID",
            "error": str(exc),
        }
    rendered = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    print(rendered, end="")
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
