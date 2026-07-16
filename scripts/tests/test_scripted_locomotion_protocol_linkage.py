from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path

import pytest

from scripts.evaluate_scripted_locomotion_protocol import (
    ProtocolLinkageError,
    _resolve_step,
    validate_protocol_identity_and_observed_schedule,
    validate_protocol_linkage,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
LOCKED_V2_PATH = REPO_ROOT / "product-gates" / "scripted-locomotion.v2.json"


def _write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def _write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def _make_valid_run(tmp_path: Path) -> Path:
    protocol_path = tmp_path / "scripted-locomotion.v2.json"
    protocol_path.write_bytes(LOCKED_V2_PATH.read_bytes())
    protocol = json.loads(protocol_path.read_text(encoding="utf-8"))
    protocol_sha = hashlib.sha256(protocol_path.read_bytes()).hexdigest().upper()
    script = protocol["script"]
    fixed_delta = float(script["fixed_delta_time_sec"])

    physics_rows: list[dict] = []
    for sequence in range(600):
        time_sec = sequence * fixed_delta
        phase, intent, move = _resolve_step(protocol, time_sec)
        row = {field: 0 for field in protocol["sample_streams"]["physics"]["required_fields"]}
        row.update(
            {
                "sequence": sequence,
                "time_sec": time_sec,
                "script_phase": phase,
                "runtime_state": "BalanceActive_Standing" if not move else "LocomotionActiveShell",
                "script_intent_magnitude": intent,
                "trajectory_conditioning_published": move,
                "shell_accepted_speed_cm_per_sec": intent * float(script["nominal_speed_cm_per_sec"]),
                "human_input": False,
            }
        )
        physics_rows.append(row)

    policy_rows: list[dict] = []
    for sequence in range(300):
        time_sec = sequence / 30.0
        phase, _, move = _resolve_step(protocol, time_sec)
        row = {field: 0 for field in protocol["sample_streams"]["policy"]["required_fields"]}
        row.update(
            {
                "sequence": sequence,
                "time_sec": time_sec,
                "script_phase": phase,
                "trajectory_conditioning_published": move,
                "inference_attempted": True,
                "inference_succeeded": True,
            }
        )
        policy_rows.append(row)

    _write_jsonl(tmp_path / "physics.jsonl", physics_rows)
    _write_jsonl(tmp_path / "policy.jsonl", policy_rows)
    _write_json(
        tmp_path / "scenario-summary.json",
        {
            "schema_version": "physanim-scripted-locomotion-summary/v2",
            "protocol_id": protocol["protocol_id"],
            "protocol_version": protocol["version"],
            "protocol_sha256": protocol_sha,
            "nominal_speed_cm_per_sec": script["nominal_speed_cm_per_sec"],
        },
    )

    manifest = {
        "schema_version": "physanim-scripted-locomotion-run/v2",
        "fixture_authority": "PRODUCT_RUN",
        "run_id": "unit-run",
        "protocol_path": str(protocol_path.resolve()),
        "protocol_sha256": protocol_sha,
        "protocol_schema_version": protocol["schema_version"],
        "protocol_id": protocol["protocol_id"],
        "protocol_version": protocol["version"],
        "protocol_status": protocol["status"],
        "protocol_map": protocol["map"],
        "protocol_actor_class": protocol["actor_class"],
        "protocol_skeleton": protocol["skeleton"],
        "protocol_model_asset": protocol["model_asset"],
        "protocol_test_family": protocol["test_family"],
        "protocol_renderer_mode": protocol["renderer_mode"],
        "resolved_script": copy.deepcopy(protocol["script"]),
        "resolved_acceptance": copy.deepcopy(protocol["acceptance"]),
        "resolved_stability_cost": copy.deepcopy(protocol["stability_cost"]),
        "resolved_physics_minimum_samples": protocol["sample_streams"]["physics"]["minimum_samples"],
        "resolved_policy_minimum_samples": protocol["sample_streams"]["policy"]["minimum_samples"],
        "variant": "Normal",
        "repetition": 1,
        "physics_samples": "physics.jsonl",
        "policy_samples": "policy.jsonl",
        "scenario_summary": "scenario-summary.json",
        "scripted_locomotion_run": True,
        "human_input": False,
        "root_authority": protocol["root_authority"],
        "motion_source": protocol["motion_source"],
    }
    manifest_path = tmp_path / "manifest.json"
    _write_json(manifest_path, manifest)
    return manifest_path


def test_authoritative_protocol_linkage_accepts_exact_protocol_and_evidence(tmp_path: Path) -> None:
    result = validate_protocol_linkage(_make_valid_run(tmp_path))

    assert result["valid"] is True
    assert result["protocol_version"] == 2
    assert result["physics_samples"] == 600
    assert result["policy_samples"] == 300


def test_protocol_identity_can_pass_while_complete_evidence_is_invalid(tmp_path: Path) -> None:
    manifest_path = _make_valid_run(tmp_path)
    policy_path = tmp_path / "policy.jsonl"
    rows = policy_path.read_text(encoding="utf-8").splitlines()[:188]
    policy_path.write_text("\n".join(rows) + "\n", encoding="utf-8")

    identity = validate_protocol_identity_and_observed_schedule(manifest_path)
    assert identity["valid"] is True
    assert identity["policy_samples"] == 188
    with pytest.raises(ProtocolLinkageError, match="policy stream is shorter"):
        validate_protocol_linkage(manifest_path)


def test_authoritative_protocol_linkage_rejects_hash_only_metadata(tmp_path: Path) -> None:
    manifest_path = _make_valid_run(tmp_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["protocol_sha256"] = "0" * 64
    _write_json(manifest_path, manifest)

    with pytest.raises(ProtocolLinkageError, match="protocol SHA-256"):
        validate_protocol_linkage(manifest_path)


def test_authoritative_protocol_linkage_rejects_silent_schedule_change(tmp_path: Path) -> None:
    manifest_path = _make_valid_run(tmp_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["resolved_script"]["acceleration"]["end_sec"] = 2.0
    _write_json(manifest_path, manifest)

    with pytest.raises(ProtocolLinkageError, match="resolved script"):
        validate_protocol_linkage(manifest_path)


def test_authoritative_protocol_linkage_rejects_runtime_phase_drift(tmp_path: Path) -> None:
    manifest_path = _make_valid_run(tmp_path)
    physics_path = tmp_path / "physics.jsonl"
    rows = [json.loads(line) for line in physics_path.read_text(encoding="utf-8").splitlines()]
    rows[100]["script_phase"] = "StandingHold"
    _write_jsonl(physics_path, rows)

    with pytest.raises(ProtocolLinkageError, match="physics phase row 100"):
        validate_protocol_linkage(manifest_path)


def test_authoritative_protocol_linkage_rejects_protocol_speed_violation(tmp_path: Path) -> None:
    manifest_path = _make_valid_run(tmp_path)
    physics_path = tmp_path / "physics.jsonl"
    rows = [json.loads(line) for line in physics_path.read_text(encoding="utf-8").splitlines()]
    rows[100]["shell_accepted_speed_cm_per_sec"] = 161.0
    _write_jsonl(physics_path, rows)

    with pytest.raises(ProtocolLinkageError, match="exceeds protocol nominal speed"):
        validate_protocol_linkage(manifest_path)
