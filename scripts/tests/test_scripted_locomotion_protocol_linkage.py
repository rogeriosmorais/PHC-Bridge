from __future__ import annotations

import copy
import hashlib
import json
from pathlib import Path

import pytest

from scripts.evaluate_scripted_locomotion_protocol import (
    ProtocolLinkageError,
    _resolve_step,
    evaluate_protocol_causal_metrics,
    validate_protocol_identity_and_observed_schedule,
    validate_protocol_linkage,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
LOCKED_V2_PATH = REPO_ROOT / "product-gates" / "scripted-locomotion.v2.json"
LOCKED_V3_PATH = REPO_ROOT / "product-gates" / "scripted-locomotion.v3.json"


def _write_json(path: Path, value: object) -> None:
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


def _write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def _make_valid_run(
    tmp_path: Path,
    *,
    locked_protocol_path: Path = LOCKED_V2_PATH,
    root_mode: str = "healthy",
) -> Path:
    protocol_path = tmp_path / locked_protocol_path.name
    protocol_path.write_bytes(locked_protocol_path.read_bytes())
    protocol = json.loads(protocol_path.read_text(encoding="utf-8"))
    protocol_bytes = protocol_path.read_bytes().replace(b"\r\n", b"\n").replace(b"\r", b"\n")
    protocol_sha = hashlib.sha256(protocol_bytes).hexdigest().upper()
    script = protocol["script"]
    fixed_delta = float(script["fixed_delta_time_sec"])

    physics_rows: list[dict] = []
    for sequence in range(600):
        time_sec = sequence * fixed_delta
        phase, intent, move = _resolve_step(protocol, time_sec)
        row = {field: 0 for field in protocol["sample_streams"]["physics"]["required_fields"]}
        shell_x = sequence / 599.0 * 200.0
        if root_mode == "healthy":
            root_x, root_y = shell_x * 0.95, shell_x * 0.02
        elif root_mode == "statue":
            root_x, root_y = 0.0, 0.0
        elif root_mode == "lateral":
            root_x, root_y = shell_x * 0.2, shell_x * 0.8
        elif root_mode == "overshoot":
            root_x, root_y = shell_x * 2.0, 0.0
        else:
            raise AssertionError(f"unknown root_mode: {root_mode}")
        tracking_error = ((shell_x - root_x) ** 2 + root_y**2) ** 0.5
        row.update(
            {
                "sequence": sequence,
                "time_sec": time_sec,
                "script_phase": phase,
                "runtime_state": "BalanceActive_Standing" if not move else "LocomotionActiveShell",
                "actor_location_x_cm": shell_x,
                "actor_location_y_cm": 0.0,
                "actor_location_z_cm": 300.0,
                "physical_root_location_x_cm": root_x,
                "physical_root_location_y_cm": root_y,
                "physical_root_location_z_cm": 300.0,
                "root_shell_tracking_error_cm": tracking_error,
                "script_intent_magnitude": intent,
                "trajectory_conditioning_published": move,
                "shell_accepted_speed_cm_per_sec": intent * float(script["nominal_speed_cm_per_sec"]),
                "human_input": False,
                "cmc_active": False,
                "shell_helper_used_count": 0,
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


def test_v2_causal_evaluation_is_explicitly_not_applicable(tmp_path: Path) -> None:
    result = evaluate_protocol_causal_metrics(_make_valid_run(tmp_path))

    assert result["verdict"] == "NOT_APPLICABLE"


def test_v3_causal_evaluation_accepts_healthy_physical_root_tracking(tmp_path: Path) -> None:
    result = evaluate_protocol_causal_metrics(
        _make_valid_run(tmp_path, locked_protocol_path=LOCKED_V3_PATH)
    )

    assert result["verdict"] == "PASS"
    assert result["failed_criteria"] == []
    assert result["metrics"]["classification"] == "FORWARD_TRACKING"


@pytest.mark.parametrize(
    ("root_mode", "expected_criterion"),
    [
        ("statue", "classification"),
        ("lateral", "root_route_lateral_displacement_cm"),
        ("overshoot", "root_to_shell_progress_ratio_max"),
    ],
)
def test_v3_causal_evaluation_rejects_adversarial_physical_root_traces(
    tmp_path: Path, root_mode: str, expected_criterion: str
) -> None:
    result = evaluate_protocol_causal_metrics(
        _make_valid_run(
            tmp_path,
            locked_protocol_path=LOCKED_V3_PATH,
            root_mode=root_mode,
        )
    )

    assert result["verdict"] == "FAIL"
    assert expected_criterion in result["failed_criteria"]


def test_v3_preregisters_causal_and_cross_process_determinism_contracts() -> None:
    protocol = json.loads(LOCKED_V3_PATH.read_text(encoding="utf-8"))

    assert protocol["version"] == 3
    assert protocol["supersedes"] == "scripted-locomotion.v2.json"
    assert protocol["causal_metrics"]["required_classification"] == "FORWARD_TRACKING"
    campaign = protocol["determinism"]["campaigns"]["normal_cross_process_same_machine"]
    assert campaign["scope"] == "cross_process_same_machine"
    assert campaign["repetitions"] == 3
    assert campaign["byte_identical_artifacts"] == ["policy-input-snapshot.json"]


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
