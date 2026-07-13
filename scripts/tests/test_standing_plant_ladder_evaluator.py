from __future__ import annotations

import json
from pathlib import Path

import pytest

from scripts.evaluate_standing_plant import EvaluationError, evaluate_manifest


REPO_ROOT = Path(__file__).resolve().parents[2]
PROTOCOL_PATH = REPO_ROOT / "product-gates" / "standing-plant-ladder.v2.json"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def make_run(
    root: Path,
    *,
    layer: str = "ControlsOff",
    root_linear_speed: float = 10.0,
    pelvis_height: float = 90.0,
    conditioned_action_l2: float | None = None,
    omit_physics_field: str | None = None,
) -> Path:
    protocol = json.loads(PROTOCOL_PATH.read_text(encoding="utf-8"))
    window = float(protocol["layers"][layer]["capture_window_sec"])
    run_dir = root / layer
    run_dir.mkdir(parents=True)
    physics_path = run_dir / "physics.jsonl"
    policy_path = run_dir / "policy.jsonl"

    passive = layer in {"ControlsOff", "DampingOnly"}
    physics_rows = []
    for sequence in range(round(window * 20.0) + 1):
        row = {
            "sequence": sequence,
            "time_sec": sequence / 20.0,
            "runtime_state": "FailStopped" if passive else "BalanceActive_Standing",
            "pelvis_height_cm": 0.0 if passive else pelvis_height,
            "root_tilt_deg": 90.0 if passive else 5.0,
            "max_penetration_cm": 0.0,
            "support_gap_ms": 0.0,
            "root_is_simulating": True,
            "body_valid_count": 22,
            "body_simulating_count": 22,
            "control_gain_match_count": 21,
            "full_simulation_committed": True,
            "cmc_active": False,
            "cmc_tick_enabled": False,
            "cmc_updated_component_is_null": True,
            "capsule_collision_enabled": 0,
            "movement_reclaim_count": 0,
            "shell_helper_used_count": 0,
            "topology_change_count": 0,
            "root_linear_speed_cm_per_sec": root_linear_speed,
            "root_angular_speed_deg_per_sec": 10.0,
            "max_body_linear_speed_cm_per_sec": 20.0,
            "max_body_angular_speed_deg_per_sec": 20.0,
        }
        if omit_physics_field:
            row.pop(omit_physics_field)
        physics_rows.append(row)

    policy_rows = []
    if layer in {"ZeroActions", "RealOnnxPolicy"}:
        conditioned = (
            conditioned_action_l2
            if conditioned_action_l2 is not None
            else (0.0 if layer == "ZeroActions" else 0.5)
        )
        for sequence in range(round(window * 30.0)):
            policy_rows.append(
                {
                    "sequence": sequence,
                    "time_sec": sequence / 30.0,
                    "inference_attempted": True,
                    "inference_succeeded": True,
                    "raw_action_l2": 1.0,
                    "conditioned_action_l2": conditioned,
                    "target_write_attempt_count": 21,
                    "target_readback_match_count": 21,
                    "target_readback_max_error_deg": 0.1,
                }
            )

    write_jsonl(physics_path, physics_rows)
    write_jsonl(policy_path, policy_rows)
    manifest = {
        "schema_version": "physanim-development-run/v1",
        "fixture_authority": "EVALUATOR_UNIT_ONLY",
        "run_id": f"{layer}-1",
        "protocol_path": str(PROTOCOL_PATH),
        "variant": layer,
        "repetition": 1,
        "source_commit": "a" * 40,
        "source_tree_dirty": False,
        "model_onnx_sha256": "b" * 64,
        "reference_pelvis_height_cm": 100.0,
        "standing_window_start_sec": 0.0,
        "capture_window_sec": window,
        "perturbation_time_sec": -1.0,
        "physics_samples": physics_path.name,
        "policy_samples": policy_path.name,
        "render_capture": "render.png",
        "render_nonblank_pixel_count": 0,
    }
    manifest_path = run_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    return manifest_path


def test_passive_layer_allows_falling_but_enforces_explosion_limits(tmp_path: Path) -> None:
    passing = evaluate_manifest(make_run(tmp_path / "pass"))
    failing = evaluate_manifest(
        make_run(tmp_path / "fail", root_linear_speed=1200.1)
    )

    assert passing["status"] == "PASS"
    assert passing["fixture_authority"] == "EVALUATOR_UNIT_ONLY"
    assert failing["status"] == "FAIL"
    assert "root_linear_speed" in failing["failed_criteria"]


def test_fixed_neutral_must_hold_the_locked_standing_envelope(tmp_path: Path) -> None:
    result = evaluate_manifest(
        make_run(tmp_path, layer="FixedNeutralTarget", pelvis_height=69.9)
    )

    assert result["status"] == "FAIL"
    assert "pelvis_height" in result["failed_criteria"]


def test_zero_actions_must_be_zero_after_real_inference(tmp_path: Path) -> None:
    result = evaluate_manifest(
        make_run(tmp_path, layer="ZeroActions", conditioned_action_l2=0.01)
    )

    assert result["status"] == "FAIL"
    assert "zero_action_conditioning" in result["failed_criteria"]


def test_real_onnx_policy_meets_cadence_and_readback(tmp_path: Path) -> None:
    result = evaluate_manifest(make_run(tmp_path, layer="RealOnnxPolicy"))

    assert result["status"] == "PASS"
    assert result["target_readback_match_ratio"] == 1.0


def test_missing_raw_field_is_invalid_not_a_behavioral_failure(tmp_path: Path) -> None:
    manifest = make_run(tmp_path, omit_physics_field="body_valid_count")

    with pytest.raises(EvaluationError, match="missing required fields"):
        evaluate_manifest(manifest)


def test_v1_manifest_linkage_is_rejected_after_v2_lock(tmp_path: Path) -> None:
    manifest_path = make_run(tmp_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["protocol_path"] = str(
        REPO_ROOT / "product-gates" / "standing-plant-ladder.v1.json"
    )
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")

    with pytest.raises(EvaluationError, match="wrong standing-plant protocol"):
        evaluate_manifest(manifest_path)
