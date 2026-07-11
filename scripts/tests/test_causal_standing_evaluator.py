import json
from pathlib import Path

import pytest

from scripts.evaluate_causal_standing import (
    EvaluationError,
    evaluate_bundle,
    evaluate_manifest,
)


REPO_ROOT = Path(__file__).resolve().parents[2]
PROTOCOL_PATH = REPO_ROOT / "product-gates" / "causal-standing.v1.json"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    path.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")


def make_run(
    root: Path,
    *,
    variant: str = "Normal",
    repetition: int = 1,
    recovery_scale: float = 1.0,
    cmc_active: bool = False,
    dispatch: bool = True,
    support_loss: bool = False,
) -> Path:
    run_dir = root / f"{variant}-{repetition}"
    run_dir.mkdir(parents=True)
    physics_path = run_dir / "physics.jsonl"
    policy_path = run_dir / "policy.jsonl"
    render_path = run_dir / "standing.png"
    render_path.write_bytes(b"not-a-real-product-capture")

    physics_rows = []
    for sequence in range(121):
        time_sec = sequence / 10.0
        after_impulse = time_sec >= 2.0
        pose_error = (8.0 if not after_impulse else 12.0 * recovery_scale)
        state = "BalanceSafeDeny" if support_loss and time_sec >= 2.0 else "BalanceActive_Standing"
        physics_rows.append(
            {
                "sequence": sequence,
                "time_sec": time_sec,
                "runtime_state": state,
                "pelvis_height_cm": 90.0,
                "root_tilt_deg": 5.0,
                "max_penetration_cm": 0.0,
                "support_gap_ms": 150.0 if support_loss and time_sec >= 2.0 else 0.0,
                "critical_body_valid_mask": 63,
                "critical_body_simulating_mask": 63,
                "support_body_valid_mask": 15,
                "support_body_simulating_mask": 15,
                "root_is_simulating": True,
                "cmc_active": cmc_active,
                "cmc_tick_enabled": cmc_active,
                "cmc_updated_component_is_null": not cmc_active,
                "capsule_collision_enabled": 1 if cmc_active else 0,
                "movement_reclaim_count": 0,
                "shell_helper_used_count": 0,
                "topology_change_count": 0,
                "pose_rms_error_deg": pose_error,
            }
        )

    policy_rows = []
    for sequence in range(300):
        policy_rows.append(
            {
                "sequence": sequence,
                "time_sec": sequence / 30.0,
                "pose_search_valid": True,
                "selected_animation": "MM_Idle",
                "inference_attempted": True,
                "inference_succeeded": True,
                "raw_action_l2": 1.0,
                "conditioned_action_l2": 0.0 if variant == "ZeroActions" else 0.5,
                "target_write_attempt_count": 8,
                "target_readback_match_count": 8 if dispatch else 0,
                "target_readback_max_error_deg": 0.1 if dispatch else 180.0,
            }
        )

    write_jsonl(physics_path, physics_rows)
    write_jsonl(policy_path, policy_rows)
    manifest = {
        "schema_version": "physanim-product-run/v1",
        "fixture_authority": "EVALUATOR_UNIT_ONLY",
        "run_id": f"{variant}-{repetition}",
        "protocol_path": str(PROTOCOL_PATH),
        "variant": variant,
        "repetition": repetition,
        "source_commit": "a" * 40,
        "source_tree_dirty": False,
        "model_onnx_sha256": "b" * 64,
        "reference_pelvis_height_cm": 100.0,
        "standing_window_start_sec": 0.0,
        "perturbation_time_sec": 2.0,
        "physics_samples": physics_path.name,
        "policy_samples": policy_path.name,
        "render_capture": render_path.name,
        "render_nonblank_pixel_count": 100,
    }
    manifest_path = run_dir / "manifest.json"
    manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
    return manifest_path


def test_normal_fixture_proves_evaluator_logic_only(tmp_path: Path) -> None:
    result = evaluate_manifest(make_run(tmp_path))

    assert result["status"] == "PASS"
    assert result["fixture_authority"] == "EVALUATOR_UNIT_ONLY"
    assert result["recovery_auc"] > 0.0


def test_cmc_assistance_is_a_behavioral_failure(tmp_path: Path) -> None:
    result = evaluate_manifest(make_run(tmp_path, cmc_active=True))

    assert result["status"] == "FAIL"
    assert "cmc_inactive" in result["failed_criteria"]


def test_dropped_dispatch_is_detected_from_readback(tmp_path: Path) -> None:
    result = evaluate_manifest(
        make_run(tmp_path, variant="DropControlDispatch", dispatch=False)
    )

    assert result["status"] == "FAIL"
    assert "target_readback" in result["failed_criteria"]


def test_non_monotonic_stream_is_invalid(tmp_path: Path) -> None:
    manifest_path = make_run(tmp_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    policy_path = manifest_path.parent / manifest["policy_samples"]
    rows = [json.loads(line) for line in policy_path.read_text(encoding="utf-8").splitlines()]
    rows[10]["sequence"] = 1
    write_jsonl(policy_path, rows)

    with pytest.raises(EvaluationError, match="strictly increasing"):
        evaluate_manifest(manifest_path)


def test_bundle_requires_all_variants_and_causal_advantage(tmp_path: Path) -> None:
    manifests = []
    for repetition in range(1, 4):
        manifests.append(make_run(tmp_path, variant="Normal", repetition=repetition, recovery_scale=0.5))
        manifests.append(make_run(tmp_path, variant="ZeroActions", repetition=repetition, recovery_scale=1.0))
    manifests.append(make_run(tmp_path, variant="DropControlDispatch", dispatch=False))
    manifests.append(make_run(tmp_path, variant="ForcedSupportLoss", support_loss=True))

    result = evaluate_bundle(manifests)

    assert result["status"] == "PASS"
    assert result["normal_to_zero_recovery_auc_ratio"] <= 0.8


def test_bundle_does_not_turn_mechanics_into_product_progress(tmp_path: Path) -> None:
    manifests = [make_run(tmp_path, variant="Normal", repetition=1)]

    result = evaluate_bundle(manifests)

    assert result["status"] == "INVALID"
    assert "required_repetitions" in result["failed_criteria"]
