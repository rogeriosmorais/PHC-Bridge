from __future__ import annotations

import json
import math
from pathlib import Path

from Training.physanim.policy_tensor_reference import (
    BodySample,
    FuturePoseSample,
    build_mimic_target_poses,
    build_self_observation,
)


def _yaw(degrees: float) -> tuple[float, float, float, float]:
    half = math.radians(degrees) * 0.5
    return (0.0, 0.0, math.sin(half), math.cos(half))


def _bodies(root_position=(0.0, 0.0, 1.0), root_rotation=(0.0, 0.0, 0.0, 1.0)) -> list[BodySample]:
    bodies = []
    for index in range(24):
        bodies.append(
            BodySample(
                position=(root_position[0] + index * 0.01, root_position[1], root_position[2]),
                rotation=root_rotation,
                linear_velocity=(0.0, 0.0, 0.0),
                angular_velocity=(0.0, 0.0, 0.0),
            )
        )
    return bodies


def test_self_observation_layout_is_exact() -> None:
    values = build_self_observation(_bodies(), ground_height=0.0)

    assert len(values) == 358
    assert values[0] == 1.0
    assert values[1:4] == [0.01, 0.0, 0.0]
    assert values[70:76] == [1.0, 0.0, 0.0, 0.0, 0.0, 1.0]


def test_self_observation_positions_are_root_heading_local() -> None:
    bodies = _bodies(root_rotation=_yaw(90.0))
    bodies[1] = BodySample(
        position=(0.0, 1.0, 1.0),
        rotation=_yaw(90.0),
        linear_velocity=(0.0, 1.0, 0.0),
        angular_velocity=(0.0, 0.0, 0.0),
    )

    values = build_self_observation(bodies, ground_height=0.0)

    assert math.isclose(values[1], 1.0, abs_tol=1.0e-9)
    assert math.isclose(values[2], 0.0, abs_tol=1.0e-9)
    linear_velocity_start = 1 + (23 * 3) + (24 * 6)
    body_one_velocity = linear_velocity_start + 3
    assert math.isclose(values[body_one_velocity], 1.0, abs_tol=1.0e-9)
    assert math.isclose(values[body_one_velocity + 1], 0.0, abs_tol=1.0e-9)


def test_mimic_tensor_encodes_previous_frame_and_root_relative_positions() -> None:
    current = _bodies(root_position=(0.0, 0.0, 1.0))
    futures: list[FuturePoseSample] = []
    for step in range(15):
        x = float(step + 1) * 0.1
        futures.append(FuturePoseSample(bodies=_bodies(root_position=(x, 0.0, 1.0)), future_time=(step + 1) / 30.0))

    values = build_mimic_target_poses(current, futures)

    assert len(values) == 6495
    assert values[0:3] == [0.1, 0.0, 0.0]
    assert values[72:75] == [0.1, 0.0, 0.0]
    assert math.isclose(values[432], 1.0 / 30.0)
    assert math.isclose(values[433], 0.1, abs_tol=1.0e-9)


def test_machine_readable_contract_matches_reference_widths() -> None:
    contract_path = Path(__file__).parents[2] / "docs" / "contracts" / "phc-policy-tensor-contract.v1.json"
    contract = json.loads(contract_path.read_text(encoding="utf-8"))

    assert contract["inputs"]["self_obs"]["width"] == 358
    assert contract["inputs"]["mimic_target_poses"]["width"] == 6495
    assert contract["inputs"]["mimic_target_poses"]["floats_per_future_step"] == 433
    assert contract["inputs"]["terrain"]["width"] == 256
    assert contract["output"]["actions"]["width"] == 69
