from __future__ import annotations

import math

import pytest

from scripts.locomotion_causal_metrics import (
    CausalMetricError,
    analyze_trace,
    validate_bundle_membership,
)


def _row(t: float, shell_x: float, shell_y: float, root_x: float, root_y: float, error: float) -> dict:
    return {
        "time_sec": t,
        "actor_location_x_cm": shell_x,
        "actor_location_y_cm": shell_y,
        "physical_root_location_xyz_cm": [root_x, root_y, 90.0],
        "root_shell_tracking_error_cm": error,
        "cmc_active": False,
        "simroot_active": False,
        "shell_helper_used_count": 0,
    }


def test_statue_behavior_is_exposed() -> None:
    rows = [_row(0.0, 0.0, 0.0, 0.0, 0.0, 0.0), _row(1.0, 100.0, 0.0, 0.0, 0.0, 100.0)]

    result = analyze_trace(rows)

    assert result["classification"] == "STATUE"
    assert result["shell_net_progress_cm"] == 100.0
    assert result["root_route_projected_progress_cm"] == 0.0
    assert result["root_to_shell_progress_ratio"] == 0.0


def test_backward_motion_is_not_rewarded_by_path_length() -> None:
    rows = [
        _row(0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        _row(0.5, 50.0, 0.0, -30.0, 0.0, 80.0),
        _row(1.0, 100.0, 0.0, -60.0, 0.0, 160.0),
    ]

    result = analyze_trace(rows)

    assert result["classification"] == "REVERSED"
    assert result["root_path_length_cm"] == 60.0
    assert result["root_route_projected_progress_cm"] == -60.0
    assert result["tracking_error_auc_cm_sec"] == 80.0


def test_lateral_motion_is_separated_from_route_progress() -> None:
    rows = [_row(0.0, 0.0, 0.0, 0.0, 0.0, 0.0), _row(1.0, 100.0, 0.0, 10.0, 80.0, 120.0)]

    result = analyze_trace(rows)

    assert result["classification"] == "LATERAL_DIVERGENCE"
    assert result["root_route_projected_progress_cm"] == 10.0
    assert result["root_route_lateral_displacement_cm"] == 80.0
    assert math.isclose(result["root_path_length_cm"], math.hypot(10.0, 80.0))


def test_tracking_trace_reports_auc_final_and_max_independently() -> None:
    rows = [
        _row(0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
        _row(1.0, 50.0, 0.0, 45.0, 0.0, 5.0),
        _row(2.0, 100.0, 0.0, 95.0, 0.0, 5.0),
    ]

    result = analyze_trace(rows)

    assert result["classification"] == "FORWARD_TRACKING"
    assert result["tracking_error_auc_cm_sec"] == 7.5
    assert result["final_tracking_error_cm"] == 5.0
    assert result["max_tracking_error_cm"] == 5.0
    assert result["root_to_shell_progress_ratio"] == 0.95


def test_forbidden_assistance_is_reported_without_changing_kinematics() -> None:
    rows = [_row(0.0, 0.0, 0.0, 0.0, 0.0, 0.0), _row(1.0, 100.0, 0.0, 95.0, 0.0, 5.0)]
    rows[1]["cmc_active"] = True
    rows[1]["shell_helper_used_count"] = 1

    result = analyze_trace(rows)

    assert result["classification"] == "FORWARD_TRACKING"
    assert result["assistance_clean"] is False
    assert set(result["forbidden_assistance_observed"]) == {"cmc_active", "shell_helper_used_count"}


def test_non_monotonic_trace_is_invalid() -> None:
    rows = [_row(1.0, 0.0, 0.0, 0.0, 0.0, 0.0), _row(0.5, 1.0, 0.0, 1.0, 0.0, 0.0)]

    with pytest.raises(CausalMetricError, match="strictly increasing"):
        analyze_trace(rows)


def test_bundle_membership_rejects_omission_duplicate_and_unexpected_run() -> None:
    expected = {"Normal": 3, "ZeroActions": 3, "DropTrajectoryConditioning": 1, "SuppressStopTransition": 1}
    actual = [
        ("Normal", 1),
        ("Normal", 2),
        ("Normal", 2),
        ("ZeroActions", 1),
        ("ZeroActions", 2),
        ("ZeroActions", 3),
        ("DropTrajectoryConditioning", 1),
        ("Unexpected", 1),
    ]

    result = validate_bundle_membership(expected, actual)

    assert result["valid"] is False
    assert ["Normal", 3] in result["missing"]
    assert ["SuppressStopTransition", 1] in result["missing"]
    assert ["Normal", 2] in result["duplicates"]
    assert ["Unexpected", 1] in result["unexpected"]
