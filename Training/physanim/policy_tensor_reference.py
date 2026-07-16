from __future__ import annotations

from dataclasses import dataclass
import math
from typing import Iterable, Sequence

Vec3 = tuple[float, float, float]
Quat = tuple[float, float, float, float]  # x, y, z, w

NUM_BODIES = 24
NUM_FUTURE_STEPS = 15
SELF_OBS_WIDTH = 358
MIMIC_TARGET_WIDTH = 6495
FLOATS_PER_FUTURE_STEP = 433


@dataclass(frozen=True)
class BodySample:
    position: Vec3
    rotation: Quat
    linear_velocity: Vec3
    angular_velocity: Vec3


@dataclass(frozen=True)
class FuturePoseSample:
    bodies: Sequence[BodySample]
    future_time: float


def _add(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def _sub(a: Vec3, b: Vec3) -> Vec3:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def _scale(v: Vec3, factor: float) -> Vec3:
    return (v[0] * factor, v[1] * factor, v[2] * factor)


def _dot(a: Vec3, b: Vec3) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a: Vec3, b: Vec3) -> Vec3:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def _normalize_quat(q: Quat) -> Quat:
    length = math.sqrt(sum(component * component for component in q))
    if length <= 1.0e-15:
        return (0.0, 0.0, 0.0, 1.0)
    return tuple(component / length for component in q)  # type: ignore[return-value]


def _quat_inverse(q: Quat) -> Quat:
    q = _normalize_quat(q)
    return (-q[0], -q[1], -q[2], q[3])


def _quat_multiply(a: Quat, b: Quat) -> Quat:
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return _normalize_quat(
        (
            aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz,
        )
    )


def _quat_rotate(q: Quat, v: Vec3) -> Vec3:
    q = _normalize_quat(q)
    qv = (q[0], q[1], q[2])
    uv = _cross(qv, v)
    uuv = _cross(qv, uv)
    return _add(v, _add(_scale(uv, 2.0 * q[3]), _scale(uuv, 2.0)))


def _yaw_quat(radians: float) -> Quat:
    half = radians * 0.5
    return (0.0, 0.0, math.sin(half), math.cos(half))


def calculate_heading_inverse(root_rotation: Quat) -> Quat:
    forward = _quat_rotate(root_rotation, (1.0, 0.0, 0.0))
    heading = math.atan2(forward[1], forward[0])
    return _yaw_quat(-heading)


def quaternion_tan_norm(rotation: Quat) -> list[float]:
    tangent = _quat_rotate(rotation, (1.0, 0.0, 0.0))
    normal = _quat_rotate(rotation, (0.0, 0.0, 1.0))
    return [*tangent, *normal]


def _require_body_count(bodies: Sequence[BodySample], label: str) -> None:
    if len(bodies) != NUM_BODIES:
        raise ValueError(f"{label} must contain {NUM_BODIES} bodies, found {len(bodies)}")


def build_self_observation(
    bodies: Sequence[BodySample], ground_height: float
) -> list[float]:
    _require_body_count(bodies, "Current body samples")
    root = bodies[0]
    heading_inverse = calculate_heading_inverse(root.rotation)
    values: list[float] = [root.position[2] - ground_height]

    for body in bodies[1:]:
        values.extend(_quat_rotate(heading_inverse, _sub(body.position, root.position)))
    for body in bodies:
        values.extend(quaternion_tan_norm(_quat_multiply(heading_inverse, body.rotation)))
    for body in bodies:
        values.extend(_quat_rotate(heading_inverse, body.linear_velocity))
    for body in bodies:
        values.extend(_quat_rotate(heading_inverse, body.angular_velocity))

    if len(values) != SELF_OBS_WIDTH:
        raise AssertionError(f"Built {len(values)} self_obs floats, expected {SELF_OBS_WIDTH}")
    return values


def build_mimic_target_poses(
    current_bodies: Sequence[BodySample], future_samples: Sequence[FuturePoseSample]
) -> list[float]:
    _require_body_count(current_bodies, "Current body samples")
    if len(future_samples) != NUM_FUTURE_STEPS:
        raise ValueError(
            f"Future samples must contain {NUM_FUTURE_STEPS} steps, found {len(future_samples)}"
        )
    for index, sample in enumerate(future_samples):
        _require_body_count(sample.bodies, f"Future step {index}")

    values: list[float] = []
    for future_index, target in enumerate(future_samples):
        if future_index == 0:
            reference_bodies = current_bodies
        else:
            reference_bodies = future_samples[future_index - 1].bodies
        reference_root = reference_bodies[0]
        heading_inverse = calculate_heading_inverse(reference_root.rotation)

        for body_index, target_body in enumerate(target.bodies):
            reference_position = reference_bodies[body_index].position
            values.extend(
                _quat_rotate(heading_inverse, _sub(target_body.position, reference_position))
            )

        for target_body in target.bodies:
            values.extend(
                _quat_rotate(heading_inverse, _sub(target_body.position, reference_root.position))
            )

        for body_index, target_body in enumerate(target.bodies):
            relative_rotation = _quat_multiply(
                _quat_inverse(reference_bodies[body_index].rotation), target_body.rotation
            )
            values.extend(quaternion_tan_norm(relative_rotation))

        for target_body in target.bodies:
            values.extend(
                quaternion_tan_norm(_quat_multiply(heading_inverse, target_body.rotation))
            )

        values.append(float(target.future_time))

    if len(values) != MIMIC_TARGET_WIDTH:
        raise AssertionError(
            f"Built {len(values)} mimic_target_poses floats, expected {MIMIC_TARGET_WIDTH}"
        )
    return values


def build_terrain_observation(
    root_height: float, sample_ground_heights: Iterable[float]
) -> list[float]:
    samples = list(sample_ground_heights)
    if len(samples) != 256:
        raise ValueError(f"Terrain must contain 256 samples, found {len(samples)}")
    return [root_height - height for height in samples]
