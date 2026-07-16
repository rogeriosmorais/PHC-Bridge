from __future__ import annotations

import json
from pathlib import Path

from scripts.audit_experiment_evidence import audit_repository, compare_scripted_schedule


def _write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def test_schedule_comparison_detects_locked_protocol_drift(tmp_path: Path) -> None:
    protocol = {
        "script": {
            "standing_hold": {"start_sec": 0.0, "end_sec": 1.0},
            "acceleration": {"start_sec": 1.0, "end_sec": 2.0},
            "cruise": {"start_sec": 2.0, "end_sec": 4.0},
            "moving_turn": {"start_sec": 4.0, "end_sec": 5.0},
            "deceleration": {"start_sec": 5.0, "end_sec": 6.0},
            "settle": {"start_sec": 6.0, "end_sec": 10.0},
        }
    }
    source = """
static constexpr double StandingEndSeconds = 1.0;
static constexpr double AccelerationEndSeconds = 1.6;
static constexpr double CruiseEndSeconds = 2.1;
static constexpr double MovingTurnEndSeconds = 2.4;
static constexpr double DecelerationEndSeconds = 3.0;
"""

    result = compare_scripted_schedule(protocol, source)

    assert result["status"] == "MISMATCH"
    assert result["runtime_phase_end_sec"]["acceleration"] == 1.6
    assert result["protocol_phase_end_sec"]["acceleration"] == 2.0
    assert "acceleration" in result["mismatched_phases"]


def test_repository_audit_finds_missing_result_dirty_run_and_missing_artifact(tmp_path: Path) -> None:
    _write_json(
        tmp_path / "experiments" / "stage2" / "example.e90.preregister.json",
        {"experiment_id": "E90", "baseline_commit": "abc"},
    )
    _write_json(
        tmp_path / "test-results" / "run" / "manifest.json",
        {
            "source_tree_dirty": True,
            "protocol_path": str(tmp_path / "product-gates" / "missing.json"),
            "physics_samples": "physics.jsonl",
        },
    )

    result = audit_repository(tmp_path, check_git=False)

    codes = {issue["code"] for issue in result["issues"]}
    assert "experiment_missing_result" in codes
    assert "dirty_source_run" in codes
    assert "missing_protocol" in codes
    assert "missing_run_artifact" in codes
    assert result["summary"]["error_count"] >= 4


def test_repository_audit_accepts_complete_pair_and_artifacts(tmp_path: Path) -> None:
    _write_json(
        tmp_path / "experiments" / "stage2" / "example.e90.preregister.json",
        {"experiment_id": "E90", "baseline_commit": "abc"},
    )
    _write_json(
        tmp_path / "experiments" / "stage2" / "example.e90.json",
        {"experiment_id": "E90", "status": "SUPPORTED", "baseline_commit": "abc"},
    )
    protocol = tmp_path / "product-gates" / "scripted-locomotion.v2.json"
    _write_json(protocol, {"protocol_id": "scripted-causal-locomotion", "version": 2})
    run = tmp_path / "test-results" / "run"
    _write_json(
        run / "manifest.json",
        {
            "source_tree_dirty": False,
            "protocol_path": str(protocol),
            "physics_samples": "physics.jsonl",
            "scenario_summary": "scenario-summary.json",
        },
    )
    (run / "physics.jsonl").write_text("{}\n", encoding="utf-8")
    _write_json(run / "scenario-summary.json", {})

    result = audit_repository(tmp_path, check_git=False)

    assert result["summary"]["error_count"] == 0
    assert result["experiments"]["E90"]["has_preregistration"] is True
    assert result["experiments"]["E90"]["has_result"] is True
