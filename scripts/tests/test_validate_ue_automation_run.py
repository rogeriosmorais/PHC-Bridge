from __future__ import annotations

import json
from pathlib import Path

from scripts.validate_ue_automation_run import validate_run


def _write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def _success_report(test_name: str) -> dict:
    return {
        "succeeded": 1,
        "succeededWithWarnings": 0,
        "failed": 0,
        "notRun": 0,
        "inProcess": 0,
        "tests": [
            {
                "fullTestPath": test_name,
                "state": "Success",
                "warnings": 0,
                "errors": 0,
            }
        ],
    }


def _complete_run(tmp_path: Path) -> tuple[Path, Path, Path]:
    test_name = "PhysAnim.Product.ScriptedLocomotion.Normal"
    protocol = tmp_path / "product-gates" / "scripted-locomotion.v2.json"
    _write_json(protocol, {"status": "LOCKED", "protocol_id": "scripted-causal-locomotion", "version": 2})
    run = tmp_path / "run"
    report = run / "automation-report"
    _write_json(report / "index.json", _success_report(test_name))
    for filename in (
        "physics.jsonl",
        "policy.jsonl",
        "scenario-summary.json",
        "policy-input-snapshot.json",
        "render.png",
    ):
        path = run / filename
        if filename.endswith(".json"):
            _write_json(path, {})
        else:
            path.write_bytes(b"nonempty")
    manifest = {
        "source_commit": "a" * 40,
        "source_tree_dirty": False,
        "protocol_path": str(protocol),
        "variant": "Normal",
        "repetition": 1,
        "physics_samples": "physics.jsonl",
        "policy_samples": "policy.jsonl",
        "scenario_summary": "scenario-summary.json",
        "policy_input_snapshot": "policy-input-snapshot.json",
        "render_capture": "render.png",
    }
    _write_json(run / "manifest.json", manifest)
    return run, report, protocol


def test_complete_run_is_valid(tmp_path: Path) -> None:
    run, report, protocol = _complete_run(tmp_path)

    result = validate_run(
        run_root=run,
        report_root=report,
        expected_test="PhysAnim.Product.ScriptedLocomotion.Normal",
        expected_source_commit="a" * 40,
        expected_protocol=protocol,
        expected_variant="Normal",
        expected_repetition=1,
    )

    assert result["verdict"] == "PASS"
    assert result["checks"]["automation_success"] is True
    assert result["checks"]["required_artifacts_present"] is True


def test_success_with_warnings_is_valid_when_exact_test_passed(tmp_path: Path) -> None:
    run, report, protocol = _complete_run(tmp_path)
    payload = _success_report("PhysAnim.Product.ScriptedLocomotion.Normal")
    payload["succeeded"] = 0
    payload["succeededWithWarnings"] = 1
    payload["tests"][0]["warnings"] = 1
    _write_json(report / "index.json", payload)

    result = validate_run(
        run_root=run,
        report_root=report,
        expected_test="PhysAnim.Product.ScriptedLocomotion.Normal",
        expected_source_commit="a" * 40,
        expected_protocol=protocol,
        expected_variant="Normal",
        expected_repetition=1,
    )

    assert result["verdict"] == "PASS"
    assert result["checks"]["automation_success"] is True


def test_failed_or_wrong_automation_test_is_invalid(tmp_path: Path) -> None:
    run, report, protocol = _complete_run(tmp_path)
    payload = _success_report("PhysAnim.Product.Other")
    payload["failed"] = 1
    payload["succeeded"] = 0
    payload["tests"][0]["state"] = "Fail"
    _write_json(report / "index.json", payload)

    result = validate_run(
        run_root=run,
        report_root=report,
        expected_test="PhysAnim.Product.ScriptedLocomotion.Normal",
        expected_source_commit="a" * 40,
        expected_protocol=protocol,
        expected_variant="Normal",
        expected_repetition=1,
    )

    assert result["verdict"] == "INVALID"
    assert result["checks"]["automation_success"] is False
    assert any(issue["code"] == "expected_test_missing" for issue in result["issues"])


def test_dirty_or_mismatched_manifest_is_invalid(tmp_path: Path) -> None:
    run, report, protocol = _complete_run(tmp_path)
    manifest_path = run / "manifest.json"
    manifest = json.loads(manifest_path.read_text())
    manifest["source_tree_dirty"] = True
    manifest["source_commit"] = "b" * 40
    manifest["variant"] = "ZeroActions"
    _write_json(manifest_path, manifest)

    result = validate_run(
        run_root=run,
        report_root=report,
        expected_test="PhysAnim.Product.ScriptedLocomotion.Normal",
        expected_source_commit="a" * 40,
        expected_protocol=protocol,
        expected_variant="Normal",
        expected_repetition=1,
    )

    codes = {issue["code"] for issue in result["issues"]}
    assert result["verdict"] == "INVALID"
    assert {"dirty_source", "source_commit_mismatch", "variant_mismatch"} <= codes


def test_missing_manifest_artifact_is_invalid(tmp_path: Path) -> None:
    run, report, protocol = _complete_run(tmp_path)
    (run / "physics.jsonl").unlink()

    result = validate_run(
        run_root=run,
        report_root=report,
        expected_test="PhysAnim.Product.ScriptedLocomotion.Normal",
        expected_source_commit="a" * 40,
        expected_protocol=protocol,
        expected_variant="Normal",
        expected_repetition=1,
    )

    assert result["verdict"] == "INVALID"
    assert result["checks"]["required_artifacts_present"] is False
    assert any(issue["code"] == "missing_artifact" for issue in result["issues"])
