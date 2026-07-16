from __future__ import annotations

import math
from collections import Counter
from typing import Iterable, Sequence


class CausalMetricError(ValueError):
    pass


def _finite(value: object, label: str) -> float:
    if isinstance(value, bool):
        raise CausalMetricError(f"{label} must be numeric")
    try:
        number = float(value)
    except (TypeError, ValueError) as exc:
        raise CausalMetricError(f"{label} must be numeric") from exc
    if not math.isfinite(number):
        raise CausalMetricError(f"{label} must be finite")
    return number


def _xy(row: dict, prefix: str, index: int) -> tuple[float, float]:
    if prefix == "shell":
        return (
            _finite(row.get("actor_location_x_cm"), f"row {index} shell x"),
            _finite(row.get("actor_location_y_cm"), f"row {index} shell y"),
        )
    root = row.get("physical_root_location_xyz_cm")
    if not isinstance(root, Sequence) or isinstance(root, (str, bytes)) or len(root) < 2:
        raise CausalMetricError(f"row {index} physical_root_location_xyz_cm must contain at least x and y")
    return (_finite(root[0], f"row {index} root x"), _finite(root[1], f"row {index} root y"))


def _distance(a: tuple[float, float], b: tuple[float, float]) -> float:
    return math.hypot(b[0] - a[0], b[1] - a[1])


def _trapezoid_auc(times: Sequence[float], values: Sequence[float]) -> float:
    return sum(
        (times[index] - times[index - 1]) * (values[index] + values[index - 1]) * 0.5
        for index in range(1, len(times))
    )


def analyze_trace(
    rows: Sequence[dict], *, movement_epsilon_cm: float = 1.0
) -> dict:
    if len(rows) < 2:
        raise CausalMetricError("trace must contain at least two rows")
    times: list[float] = []
    shell: list[tuple[float, float]] = []
    root: list[tuple[float, float]] = []
    errors: list[float] = []
    forbidden: set[str] = set()

    previous_time: float | None = None
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            raise CausalMetricError(f"row {index} must be an object")
        time = _finite(row.get("time_sec"), f"row {index} time_sec")
        if previous_time is not None and time <= previous_time:
            raise CausalMetricError("time_sec must be strictly increasing")
        previous_time = time
        times.append(time)
        shell.append(_xy(row, "shell", index))
        root.append(_xy(row, "root", index))
        errors.append(
            _finite(row.get("root_shell_tracking_error_cm"), f"row {index} tracking error")
        )
        if row.get("cmc_active") is True:
            forbidden.add("cmc_active")
        if row.get("simroot_active") is True or row.get("root_mode") == "SimRoot":
            forbidden.add("simroot_active")
        if _finite(row.get("shell_helper_used_count", 0), f"row {index} shell helper count") > 0:
            forbidden.add("shell_helper_used_count")

    shell_start, shell_end = shell[0], shell[-1]
    shell_delta = (shell_end[0] - shell_start[0], shell_end[1] - shell_start[1])
    shell_net = math.hypot(*shell_delta)
    if shell_net <= movement_epsilon_cm:
        raise CausalMetricError("shell route is too short to define a causal locomotion direction")
    route = (shell_delta[0] / shell_net, shell_delta[1] / shell_net)
    lateral = (-route[1], route[0])

    root_delta = (root[-1][0] - root[0][0], root[-1][1] - root[0][1])
    projected = root_delta[0] * route[0] + root_delta[1] * route[1]
    lateral_displacement = root_delta[0] * lateral[0] + root_delta[1] * lateral[1]
    root_path = sum(_distance(root[index - 1], root[index]) for index in range(1, len(root)))
    shell_path = sum(_distance(shell[index - 1], shell[index]) for index in range(1, len(shell)))

    if abs(projected) <= movement_epsilon_cm and abs(lateral_displacement) <= movement_epsilon_cm:
        classification = "STATUE"
    elif projected < -movement_epsilon_cm:
        classification = "REVERSED"
    elif abs(lateral_displacement) > max(abs(projected), movement_epsilon_cm):
        classification = "LATERAL_DIVERGENCE"
    else:
        classification = "FORWARD_TRACKING"

    return {
        "schema_version": "physanim-locomotion-causal-metrics/v1",
        "classification": classification,
        "shell_path_length_cm": shell_path,
        "shell_net_progress_cm": shell_net,
        "root_path_length_cm": root_path,
        "root_net_displacement_cm": math.hypot(*root_delta),
        "root_route_projected_progress_cm": projected,
        "root_route_lateral_displacement_cm": lateral_displacement,
        "root_to_shell_progress_ratio": projected / shell_net,
        "tracking_error_auc_cm_sec": _trapezoid_auc(times, errors),
        "tracking_error_time_average_cm": _trapezoid_auc(times, errors) / (times[-1] - times[0]),
        "final_tracking_error_cm": errors[-1],
        "max_tracking_error_cm": max(errors),
        "assistance_clean": not forbidden,
        "forbidden_assistance_observed": sorted(forbidden),
        "note": "These are interpretable endpoints, not product thresholds or a weighted acceptance score.",
    }


def validate_bundle_membership(
    expected_repetitions: dict[str, int], actual_runs: Iterable[tuple[str, int]]
) -> dict:
    expected: set[tuple[str, int]] = set()
    for variant, count in expected_repetitions.items():
        if isinstance(count, bool) or not isinstance(count, int) or count < 1:
            raise CausalMetricError(f"expected repetition count for {variant!r} must be positive")
        expected.update((variant, repetition) for repetition in range(1, count + 1))

    actual_list = list(actual_runs)
    normalized: list[tuple[str, int]] = []
    for index, value in enumerate(actual_list):
        if not isinstance(value, tuple) or len(value) != 2:
            raise CausalMetricError(f"actual run {index} must be a (variant, repetition) tuple")
        variant, repetition = value
        if not isinstance(variant, str) or not variant:
            raise CausalMetricError(f"actual run {index} variant must be nonempty")
        if isinstance(repetition, bool) or not isinstance(repetition, int) or repetition < 1:
            raise CausalMetricError(f"actual run {index} repetition must be positive")
        normalized.append((variant, repetition))

    counts = Counter(normalized)
    actual_set = set(normalized)
    missing = sorted(expected - actual_set)
    unexpected = sorted(actual_set - expected)
    duplicates = sorted(item for item, count in counts.items() if count > 1)
    return {
        "schema_version": "physanim-causal-bundle-membership/v1",
        "valid": not missing and not unexpected and not duplicates,
        "expected_count": len(expected),
        "actual_count": len(normalized),
        "missing": [[variant, repetition] for variant, repetition in missing],
        "unexpected": [[variant, repetition] for variant, repetition in unexpected],
        "duplicates": [[variant, repetition] for variant, repetition in duplicates],
    }
