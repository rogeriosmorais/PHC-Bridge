from __future__ import annotations

import hashlib
import json
import math
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

from scripts.product_gate import CONTRACT_PATH, evaluate_product_gate, parse_args


NOW = datetime(2026, 7, 10, 12, 0, 0, tzinfo=timezone.utc)


def canonical_evidence() -> dict[str, object]:
    return {
        "schema_version": "physanim-runtime-facts/v1",
        "attempt_uuid": "11111111-2222-3333-4444-555555555555",
        "attempt_nonce": "gate-run-0123456789abcdef",
        "captured_at_utc": NOW.isoformat().replace("+00:00", "Z"),
        "source_commit": "0123456789abcdef0123456789abcdef01234567",
        "source_tree_dirty": False,
        "final_runtime_outcome": "BalanceActive_Standing",
        "terminal_reason": 0,
        "terminal_reason_name": "None",
        "balance_active_standing_continuous_sec": 3.25,
        "balance_active_standing_exit_count": 0,
        "physics_asset_violation_count": 0,
        "skeleton_contract_violation_count": 0,
        "mass_drift_total_pct": 0.0,
        "capsule_collision_enabled": 0,
        "capsule_lock_delta_cm": 0.0,
        "cmc_is_active": False,
        "cmc_tick_enabled": False,
        "cmc_updated_component_is_null": True,
        "max_root_tilt_deg": 12.0,
        "peak_angular_speed_deg_per_sec": 400.0,
        "runtime_max_body_angular_speed_deg_per_second": 400.0,
        "rms_mismatch_deg": 8.0,
        "max_body_mismatch_deg": 12.0,
        "target_discontinuity_deg": 8.0,
        "mismatch_duration_ms": 50.0,
        "controller_gain_scale": 1.0,
        "controller_damping_ratio": 1.0,
        "support_mode_name": "TwoFootStable",
        "support_gap_timer_ms": 0.0,
        "proxy_outside_hull_duration_ms": 0.0,
        "active_support_side_count": 2,
        "support_hull_area_cm2": 80.0,
        "support_churn_hz": 1.0,
        "calf_world_contact_l": False,
        "calf_world_contact_r": False,
        "policy_model_loaded": True,
        "policy_runtime_name": "NNERuntimeORTDml",
        "policy_model_name": "phc_policy",
        "policy_input_buffers_finite": True,
        "policy_inference_attempt_count": 100,
        "policy_inference_success_count": 100,
        "policy_inference_failure_count": 0,
        "policy_action_sample_count": 100,
        "policy_action_raw_mean_abs_max": 0.10,
        "policy_action_conditioned_mean_abs_max": 0.05,
        "physics_control_component_available": True,
        "controlled_body_count": 10,
        "control_target_sample_count": 100,
        "control_target_normal_writes": 100,
        "control_target_total_writes": 100,
        "runtime_body_sample_count": 100,
        "runtime_min_simulating_body_count": 10,
        "critical_body_valid_all_frames_mask": 0x3F,
        "critical_body_simulating_all_frames_mask": 0x3F,
        "support_body_valid_all_frames_mask": 0x0F,
        "support_body_simulating_all_frames_mask": 0x0F,
        "root_mode": "ChaosSimulated",
        "pose_search_query_count": 100,
        "pose_search_valid_result_count": 100,
        "pose_search_selected_animation_name": "MM_Idle",
        "renderer_facing_motion_used_null_rhi": False,
        "topology_change_count": 0,
        "authority_conflict_count": 0,
        "shell_helper_used_count": 0,
        "movement_reclaim_count": 0,
        "continuity_bookkeeping_mismatch": False,
        "pelvis_sleep_duration_ms": 0.0,
        "mesh_wide_assist_detected": False,
        "non_critical_body_assist_detected": False,
        "setup_override_count": 0,
    }


class ProductGateTests(unittest.TestCase):
    def _evaluate(self, evidence: dict[str, object]):
        with tempfile.TemporaryDirectory() as tmp_dir:
            evidence_path = Path(tmp_dir) / "runtime-facts.json"
            evidence_bytes = json.dumps(
                evidence, sort_keys=True, separators=(",", ":"), allow_nan=True
            ).encode("utf-8")
            evidence_path.write_bytes(evidence_bytes)
            result = evaluate_product_gate(
                evidence_path=evidence_path,
                now=NOW,
                git_revision="0123456789abcdef0123456789abcdef01234567",
            )
            return result, evidence_bytes

    def test_complete_factual_evidence_emits_non_authoritative_diagnostic_report(self) -> None:
        result, evidence_bytes = self._evaluate(canonical_evidence())

        self.assertTrue(result.passed, result.failures)
        self.assertNotIn("verdict", result.report)
        self.assertEqual(result.report["authority"], "LOCAL_DIAGNOSTIC_ONLY")
        self.assertEqual(result.report["diagnostic_outcome"], "ALL_CRITERIA_OBSERVED")
        self.assertEqual(result.report["schema_version"], "physanim-local-gate-diagnostic/v1")
        self.assertEqual(result.report["gate_id"], "standing-v0")
        self.assertEqual(result.report["gate_version"], 1)
        self.assertEqual(
            result.report["evidence_sha256"], hashlib.sha256(evidence_bytes).hexdigest()
        )
        self.assertEqual(
            result.report["contract_sha256"],
            hashlib.sha256(CONTRACT_PATH.read_bytes()).hexdigest(),
        )
        self.assertEqual(
            result.report["git_revision"],
            "0123456789abcdef0123456789abcdef01234567",
        )
        self.assertRegex(str(result.report["verifier_version"]), r"^1\.")

    def test_missing_required_fact_fails_closed(self) -> None:
        evidence = canonical_evidence()
        del evidence["control_target_normal_writes"]

        result, _ = self._evaluate(evidence)

        self.assertFalse(result.passed)
        self.assertIn("missing required fact: control_target_normal_writes", result.failures)

    def test_non_finite_fact_fails_closed(self) -> None:
        evidence = canonical_evidence()
        evidence["max_root_tilt_deg"] = math.nan

        result, _ = self._evaluate(evidence)

        self.assertFalse(result.passed)
        self.assertIn("max_root_tilt_deg must be finite", result.failures)

    def test_negative_count_or_magnitude_fails_closed(self) -> None:
        for field in (
            "balance_active_standing_exit_count",
            "policy_inference_failure_count",
            "setup_override_count",
            "max_root_tilt_deg",
            "capsule_lock_delta_cm",
        ):
            with self.subTest(field=field):
                evidence = canonical_evidence()
                evidence[field] = -1

                result, _ = self._evaluate(evidence)

                self.assertFalse(result.passed)
                self.assertIn(f"{field} must be non-negative", result.failures)

    def test_stale_evidence_fails_closed(self) -> None:
        evidence = canonical_evidence()
        evidence["captured_at_utc"] = (NOW - timedelta(minutes=31)).isoformat()

        result, _ = self._evaluate(evidence)

        self.assertFalse(result.passed)
        self.assertTrue(any("evidence is stale" in item for item in result.failures))

    def test_embedded_success_flags_have_no_authority(self) -> None:
        evidence = canonical_evidence()
        evidence.update(
            {
                "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                "product_success": True,
                "physical_continuity_validator_passed": True,
                "controller_stability_passed": True,
                "artifact_pass": True,
            }
        )
        evidence["control_target_normal_writes"] = 0

        result, _ = self._evaluate(evidence)

        self.assertFalse(result.passed)
        self.assertIn("control_target_normal_writes must be >= 1", result.failures)
        self.assertFalse(any("strict_verdict" in item for item in result.checks))

    def test_embedded_threshold_override_has_no_authority(self) -> None:
        evidence = canonical_evidence()
        evidence["max_root_tilt_deg"] = 21.0
        evidence["thresholds"] = {"max_root_tilt_deg": 180.0}
        evidence["max_root_tilt_deg_limit"] = 180.0

        result, _ = self._evaluate(evidence)

        self.assertFalse(result.passed)
        self.assertIn("max_root_tilt_deg must be <= 20.0", result.failures)

    def test_each_product_boundary_fails_when_inactive_or_assisted(self) -> None:
        cases = {
            "kinematic root": ("root_mode", "Stage1_KinematicRoot"),
            "inactive policy": ("policy_action_conditioned_mean_abs_max", 0.0),
            "no control writes": ("control_target_normal_writes", 0),
            "insufficient simulated bodies": ("runtime_min_simulating_body_count", 9),
            "incomplete critical mask": ("critical_body_simulating_all_frames_mask", 0x3E),
            "short active standing window": ("balance_active_standing_continuous_sec", 2.99),
            "null renderer": ("renderer_facing_motion_used_null_rhi", True),
            "shell assistance": ("shell_helper_used_count", 1),
            "test setup override": ("setup_override_count", 1),
        }
        for label, (field, value) in cases.items():
            with self.subTest(label=label):
                evidence = canonical_evidence()
                evidence[field] = value
                result, _ = self._evaluate(evidence)
                self.assertFalse(result.passed, label)

    def test_source_commit_must_match_judged_revision(self) -> None:
        evidence = canonical_evidence()
        evidence["source_commit"] = "ffffffffffffffffffffffffffffffffffffffff"

        result, _ = self._evaluate(evidence)

        self.assertFalse(result.passed)
        self.assertIn("source_commit must match git revision", result.failures)

    def test_cli_exposes_no_contract_or_threshold_override(self) -> None:
        with self.assertRaises(SystemExit):
            parse_args(
                [
                    "--evidence",
                    "facts.json",
                    "--report",
                    "diagnostic.json",
                    "--contract",
                    "mutable.json",
                ]
            )


if __name__ == "__main__":
    unittest.main()
