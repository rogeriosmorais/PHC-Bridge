from __future__ import annotations

import copy
import json
from pathlib import Path

from scripts.audit_locomotion_determinism import audit_campaign_records


REPO_ROOT = Path(__file__).resolve().parents[2]
PROTOCOL = json.loads(
    (REPO_ROOT / "product-gates" / "scripted-locomotion.v3.json").read_text(encoding="utf-8")
)
CAMPAIGN = "normal_cross_process_same_machine"


def _metrics(offset: float = 0.0) -> dict:
    return {
        "root_route_projected_progress_cm": 190.0 + offset,
        "root_to_shell_progress_ratio": 0.95 + offset * 0.0001,
        "root_route_lateral_displacement_cm": 4.0 + offset * 0.1,
        "tracking_error_time_average_cm": 6.0 + offset * 0.1,
        "final_tracking_error_cm": 10.0 + offset * 0.1,
        "max_tracking_error_cm": 12.0 + offset * 0.1,
    }


def _record(repetition: int, *, offset: float = 0.0) -> dict:
    return {
        "variant": "Normal",
        "repetition": repetition,
        "source_commit": "a" * 40,
        "source_tree_dirty": False,
        "model_sha256": "b" * 64,
        "protocol_sha256": "c" * 64,
        "authority_digest_sha256": "d" * 64,
        "fingerprint_source_commit": "a" * 40,
        "fingerprint_source_tree_dirty": False,
        "fingerprint_model_sha256": "b" * 64,
        "fingerprint_protocol_sha256": "c" * 64,
        "artifact_hashes": {"policy-input-snapshot.json": "e" * 64},
        "causal_verdict": "PASS",
        "metrics": _metrics(offset),
    }


def test_determinism_campaign_passes_identical_authority_and_bounded_endpoints() -> None:
    result = audit_campaign_records(
        PROTOCOL,
        CAMPAIGN,
        [_record(1), _record(2, offset=1.0), _record(3, offset=2.0)],
    )

    assert result["verdict"] == "PASS"
    assert result["issues"] == []
    assert result["behavioral_failures"] == []
    assert result["byte_determinism"]["policy-input-snapshot.json"]["passed"] is True


def test_determinism_campaign_invalidates_missing_and_mismatched_authority() -> None:
    records = [_record(1), _record(2)]
    records[1]["fingerprint_source_commit"] = "f" * 40

    result = audit_campaign_records(PROTOCOL, CAMPAIGN, records)

    assert result["verdict"] == "INVALID"
    codes = {issue["code"] for issue in result["issues"]}
    assert "missing_repetitions" in codes
    assert "fingerprint_source_commit_mismatch" in codes


def test_determinism_campaign_fails_byte_identity_without_invalidating_authority() -> None:
    records = [_record(1), _record(2), _record(3)]
    records[2]["artifact_hashes"]["policy-input-snapshot.json"] = "f" * 64

    result = audit_campaign_records(PROTOCOL, CAMPAIGN, records)

    assert result["verdict"] == "FAIL"
    assert result["issues"] == []
    assert {failure["code"] for failure in result["behavioral_failures"]} == {
        "byte_artifact_mismatch"
    }


def test_determinism_campaign_fails_numeric_spread_and_causal_contract() -> None:
    records = [_record(1), _record(2), _record(3)]
    records[2]["metrics"] = _metrics(offset=100.0)
    records[2]["causal_verdict"] = "FAIL"

    result = audit_campaign_records(PROTOCOL, CAMPAIGN, records)

    assert result["verdict"] == "FAIL"
    codes = {failure["code"] for failure in result["behavioral_failures"]}
    assert "causal_metric_contract_failed" in codes
    assert "numeric_spread_exceeded" in codes


def test_determinism_campaign_rejects_dirty_or_cross_commit_runs() -> None:
    records = [_record(1), _record(2), _record(3)]
    records[1]["source_tree_dirty"] = True
    records[1]["fingerprint_source_tree_dirty"] = True
    records[2]["source_commit"] = "f" * 40
    records[2]["fingerprint_source_commit"] = "f" * 40

    result = audit_campaign_records(PROTOCOL, CAMPAIGN, records)

    assert result["verdict"] == "INVALID"
    codes = {issue["code"] for issue in result["issues"]}
    assert "dirty_source" in codes
    assert "source_commit_mismatch" in codes


def test_audit_does_not_mutate_protocol_or_records() -> None:
    protocol = copy.deepcopy(PROTOCOL)
    records = [_record(1), _record(2), _record(3)]
    expected_protocol = copy.deepcopy(protocol)
    expected_records = copy.deepcopy(records)

    audit_campaign_records(protocol, CAMPAIGN, records)

    assert protocol == expected_protocol
    assert records == expected_records
