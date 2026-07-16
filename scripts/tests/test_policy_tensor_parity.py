from __future__ import annotations

import json
import math
from pathlib import Path

from Training.physanim.policy_tensor_reference import (
    BodySample,
    FuturePoseSample,
    build_mimic_target_poses,
    build_self_observation,
    build_terrain_observation,
)
from scripts.validate_policy_tensor_parity import validate_policy_tensor_parity


def _yaw(degrees: float) -> tuple[float, float, float, float]:
    half = math.radians(degrees) * 0.5
    return (0.0, 0.0, math.sin(half), math.cos(half))


def _bodies(
    *,
    root=(0.0, 0.0, 1.0),
    rotation=(0.0, 0.0, 0.0, 1.0),
) -> list[BodySample]:
    return [
        BodySample(
            position=(root[0] + index * 0.01, root[1], root[2]),
            rotation=rotation,
            linear_velocity=(index * 0.001, 0.0, 0.0),
            angular_velocity=(0.0, 0.0, index * 0.002),
        )
        for index in range(24)
    ]


def _body_json(body: BodySample) -> dict:
    return {
        "position_xyz": list(body.position),
        "rotation_xyzw": list(body.rotation),
        "linear_velocity_xyz": list(body.linear_velocity),
        "angular_velocity_xyz": list(body.angular_velocity),
    }


def _future_json(sample: FuturePoseSample) -> dict:
    return {
        "future_time_seconds": sample.future_time,
        "body_transforms": [
            {
                "translation_xyz": list(body.position),
                "rotation_xyzw": list(body.rotation),
                "scale_xyz": [1.0, 1.0, 1.0],
            }
            for body in sample.bodies
        ],
    }


def _write_fixture(tmp_path: Path) -> tuple[Path, Path]:
    current = _bodies(root=(1.0, 2.0, 1.2), rotation=_yaw(15.0))
    mimic_reference = _bodies(root=(0.0, 0.0, 1.2), rotation=_yaw(15.0))
    futures = [
        FuturePoseSample(
            bodies=_bodies(
                root=(0.08 * (step + 1), 0.02 * (step + 1), 1.2),
                rotation=_yaw(15.0 + 2.0 * (step + 1)),
            ),
            future_time=(step + 1) / 30.0,
        )
        for step in range(15)
    ]
    terrain_heights = [0.2 + index * 0.0001 for index in range(256)]
    provenance = {
        "schema_version": "physanim-policy-input-provenance/v1",
        "captured": True,
        "valid": True,
        "capture_scope": "unit-test",
        "runtime_state": "LocomotionActiveShell",
        "policy_control_tick": 7,
        "self_observation_ground_height": 0.2,
        "canonical_body_samples": [_body_json(body) for body in current],
        "mimic_reference_body_samples": [_body_json(body) for body in mimic_reference],
        "canonical_future_pose_samples": [_future_json(sample) for sample in futures],
        "terrain_ground_heights": terrain_heights,
    }
    snapshot = {
        "schema_version": "physanim-policy-input-snapshot/v1",
        "captured": True,
        "self_observation_width": 358,
        "mimic_target_poses_width": 6495,
        "terrain_width": 256,
        "action_width": 69,
        "self_observation": build_self_observation(current, 0.2),
        "mimic_target_poses": build_mimic_target_poses(mimic_reference, futures),
        "terrain": build_terrain_observation(current[0].position[2], terrain_heights),
        "actions": [0.0] * 69,
    }
    provenance_path = tmp_path / "policy-input-provenance.json"
    snapshot_path = tmp_path / "policy-input-snapshot.json"
    provenance_path.write_text(json.dumps(provenance), encoding="utf-8")
    snapshot_path.write_text(json.dumps(snapshot), encoding="utf-8")
    return provenance_path, snapshot_path


def test_policy_tensor_parity_rebuilds_all_runtime_inputs(tmp_path: Path) -> None:
    provenance_path, snapshot_path = _write_fixture(tmp_path)

    report = validate_policy_tensor_parity(provenance_path, snapshot_path)

    assert report["verdict"] == "PASS"
    assert report["tensors"]["self_observation"]["mismatch_count"] == 0
    assert report["tensors"]["mimic_target_poses"]["mismatch_count"] == 0
    assert report["tensors"]["terrain"]["mismatch_count"] == 0


def test_policy_tensor_parity_uses_mimic_reference_frame_not_live_world_frame(
    tmp_path: Path,
) -> None:
    provenance_path, snapshot_path = _write_fixture(tmp_path)
    provenance = json.loads(provenance_path.read_text(encoding="utf-8"))
    assert provenance["canonical_body_samples"][0]["position_xyz"] != provenance[
        "mimic_reference_body_samples"
    ][0]["position_xyz"]

    report = validate_policy_tensor_parity(provenance_path, snapshot_path)

    assert report["tensors"]["mimic_target_poses"]["passed"] is True


def test_policy_tensor_parity_reports_exact_mismatched_index(tmp_path: Path) -> None:
    provenance_path, snapshot_path = _write_fixture(tmp_path)
    snapshot = json.loads(snapshot_path.read_text(encoding="utf-8"))
    snapshot["mimic_target_poses"][1234] += 0.25
    snapshot_path.write_text(json.dumps(snapshot), encoding="utf-8")

    report = validate_policy_tensor_parity(provenance_path, snapshot_path)

    comparison = report["tensors"]["mimic_target_poses"]
    assert report["verdict"] == "FAIL"
    assert comparison["mismatch_count"] == 1
    assert comparison["worst_index"] == 1234
    assert math.isclose(comparison["max_abs_error"], 0.25)
