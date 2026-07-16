from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from typing import Optional


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "collect_evidence.py"
CONTRACT_PATH = SCRIPT_PATH.parents[1] / "product-gates" / "standing-v0.v2.json"
SEGMENTS = (
    "PoseSearch",
    "PhcPolicy",
    "PhysicsControl",
    "Chaos",
    "RendererFacingMotion",
)


class CollectEvidenceCLITests(unittest.TestCase):
    def _write_json(self, path: Path, payload: dict, mtime: float = 20.0) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload), encoding="utf-8")
        os.utime(path, (mtime, mtime))

    def _write_text(self, path: Path, text: str, mtime: float = 20.0) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        os.utime(path, (mtime, mtime))

    def _run_cli(
        self,
        repo_root: Path,
        attempt_uuid: Optional[str] = None,
    ) -> subprocess.CompletedProcess[str]:
        command = [sys.executable, str(SCRIPT_PATH), "--repo-root", str(repo_root)]
        if attempt_uuid is not None:
            command.extend(["--attempt-uuid", attempt_uuid])
        return subprocess.run(command, capture_output=True, text=True, check=False)

    def _complete_runtime_facts(self, attempt_uuid: str) -> dict:
        contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        facts: dict[str, object] = {}
        for criterion in contract["criteria"]:
            fact = criterion["fact"]
            operator = criterion["operator"]
            if operator == "equals":
                facts[fact] = criterion["value"]
            elif operator == "nonempty":
                facts[fact] = f"observed-{fact}"
            elif operator == "timestamp":
                facts[fact] = "2026-07-10T12:00:00Z"
            elif operator == "commit":
                facts[fact] = "0123456789abcdef0123456789abcdef01234567"
            elif operator == "one_of":
                facts[fact] = criterion["value"][0]
            elif operator in {"minimum", "maximum"}:
                facts[fact] = criterion["value"]
            elif operator == "range":
                facts[fact] = criterion["maximum"]
            elif operator == "positive":
                facts[fact] = 1.0
            else:
                self.fail(f"unsupported contract operator in fixture: {operator}")

        facts.update(
            {
                "attempt_uuid": attempt_uuid,
                "emitter_attempt_uuid": attempt_uuid,
                "terminal_frame_artifact_captured": True,
                "standing_window_sample_count": 180,
                "standing_window_max_delta_sec": 1.0 / 60.0,
                "policy_inference_attempt_count": 180,
                "policy_inference_success_count": 180,
                "policy_action_sample_count": 180,
                "control_target_sample_count": 180,
                "control_target_normal_writes": 180,
                "control_target_total_writes": 180,
                "runtime_body_sample_count": 180,
                "renderer_facing_motion_sample_count": 180,
                "renderer_facing_motion_active_sample_count": 180,
                "runtime_simulating_body_count": 10,
                "hold_duration_sec": 3.0,
                "thigh_net_work": 1.0,
                "setup_override_count": None,
            }
        )
        return facts

    def _complete_summary(self, attempt_uuid: str) -> dict:
        return {
            "attempt_uuid": attempt_uuid,
            "diagnostic_classification": "DIAGNOSTIC",
            "missing_evidence": False,
            "quality_flags": {
                "missing_evidence": False,
                "terminal_failure": False,
                "artifact_log_contradiction": False,
            },
            "segments": [
                {"segment_name": name, "state": "Active"}
                for name in SEGMENTS
            ],
        }

    def _capture_line(
        self,
        facts: dict,
        capture: str = "COMPLETED",
    ) -> str:
        return (
            "PhysAnimProof: AttemptCapture "
            f"uuid={facts['attempt_uuid']} capture={capture} "
            "active_standing_duration="
            f"{float(facts['balance_active_standing_continuous_sec']):.3f} "
            f"terminal_reason={facts['terminal_reason_name']}\n"
        )

    def _write_complete_attempt(
        self,
        repo_root: Path,
        attempt_uuid: str,
        *,
        facts: Optional[dict] = None,
        summary: Optional[dict] = None,
        capture: str = "COMPLETED",
        mtime: float = 20.0,
    ) -> tuple[dict, dict]:
        facts = facts or self._complete_runtime_facts(attempt_uuid)
        summary = summary or self._complete_summary(attempt_uuid)
        self._write_json(
            repo_root / "test-results" / "proof-artifacts" / f"{attempt_uuid}_terminal.json",
            facts,
            mtime,
        )
        self._write_json(
            repo_root
            / "test-results"
            / "evidence-summaries"
            / f"{attempt_uuid}_evidence_summary.json",
            summary,
            mtime,
        )
        self._write_text(
            repo_root / "test-results" / "logs" / f"{attempt_uuid}.log",
            self._capture_line(facts, capture),
            mtime,
        )
        return facts, summary

    def test_complete_production_facts_remain_non_authoritative_diagnostic(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("complete")
            summary = self._complete_summary("complete")
            summary["diagnostic_classification"] = "DIAGNOSTIC_ALL_SIGNALS_OBSERVED"
            self._write_complete_attempt(repo_root, "complete", facts=facts, summary=summary)

            result = self._run_cli(repo_root, "complete")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Authority\n- LOCAL_DIAGNOSTIC_ONLY", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- DIAGNOSTIC", result.stdout)
            self.assertNotIn(
                "Local Diagnostic Classification\n- DIAGNOSTIC_ALL_SIGNALS_OBSERVED",
                result.stdout,
            )

    def test_default_selection_does_not_merge_different_attempts(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            self._write_json(
                repo_root / "test-results" / "proof-artifacts" / "attempt-a_terminal.json",
                self._complete_runtime_facts("attempt-a"),
                20.0,
            )
            self._write_json(
                repo_root
                / "test-results"
                / "evidence-summaries"
                / "attempt-b_evidence_summary.json",
                self._complete_summary("attempt-b"),
                30.0,
            )

            result = self._run_cli(repo_root)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("attempt-a_terminal.json", result.stdout)
            self.assertNotIn("attempt-b_evidence_summary.json", result.stdout)
            self.assertIn("evidence summary for attempt_uuid=attempt-a", result.stdout)
            self.assertIn("INSUFFICIENT_EVIDENCE", result.stdout)

    def test_explicit_attempt_does_not_fall_back_to_another_attempt(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            self._write_complete_attempt(repo_root, "available")

            result = self._run_cli(repo_root, "requested")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("terminal artifact for attempt_uuid=requested", result.stdout)
            self.assertNotIn("available_terminal.json", result.stdout)
            self.assertIn("INSUFFICIENT_EVIDENCE", result.stdout)

    def test_incomplete_artifact_cannot_be_promoted_by_runtime_summary(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            self._write_json(
                repo_root / "test-results" / "proof-artifacts" / "incomplete_terminal.json",
                {"attempt_uuid": "incomplete", "hold_duration_sec": 3.0},
            )
            self._write_json(
                repo_root
                / "test-results"
                / "evidence-summaries"
                / "incomplete_evidence_summary.json",
                self._complete_summary("incomplete"),
            )

            result = self._run_cli(repo_root, "incomplete")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("missing mandatory terminal field: schema_version", result.stdout)
            self.assertIn("INSUFFICIENT_EVIDENCE", result.stdout)

    def test_unobservable_setup_override_fact_is_not_faked_as_zero(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("null-setup")
            facts["setup_override_count"] = None
            self._write_complete_attempt(repo_root, "null-setup", facts=facts)

            result = self._run_cli(repo_root, "null-setup")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("setup_override_count=None", result.stdout)
            self.assertNotIn("missing mandatory terminal field: setup_override_count", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- DIAGNOSTIC", result.stdout)

    def test_out_of_contract_observation_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("tilt")
            facts["max_root_tilt_deg"] = 20.1
            self._write_complete_attempt(repo_root, "tilt", facts=facts)

            result = self._run_cli(repo_root, "tilt")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("max_root_tilt_deg range 0.0..20.0; observed 20.1", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- BLOCKED", result.stdout)

    def test_negative_magnitude_cannot_satisfy_a_maximum(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("negative")
            facts["max_root_tilt_deg"] = -1.0
            self._write_complete_attempt(repo_root, "negative", facts=facts)

            result = self._run_cli(repo_root, "negative")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("max_root_tilt_deg range 0.0..20.0; observed -1.0", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- BLOCKED", result.stdout)

    def test_single_sample_duration_claim_is_blocked_by_v2_cadence(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("one-sample")
            facts["balance_active_standing_continuous_sec"] = 3.0
            facts["standing_window_sample_count"] = 1
            facts["standing_window_max_delta_sec"] = 3.0
            self._write_complete_attempt(repo_root, "one-sample", facts=facts)

            result = self._run_cli(repo_root, "one-sample")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("standing_window_sample_count minimum 90", result.stdout)
            self.assertIn("standing_window_max_delta_sec range 0.0..0.05", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- BLOCKED", result.stdout)

    def test_contract_invariant_violation_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("invariant")
            facts["policy_inference_attempt_count"] = 1
            facts["policy_inference_success_count"] = 2
            self._write_complete_attempt(repo_root, "invariant", facts=facts)

            result = self._run_cli(repo_root, "invariant")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "policy_inference_success_count <= policy_inference_attempt_count",
                result.stdout,
            )
            self.assertIn("Local Diagnostic Classification\n- BLOCKED", result.stdout)

    def test_runtime_failure_with_consistent_summary_is_blocked(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("runtime-failure")
            facts["terminal_reason"] = 9
            facts["terminal_reason_name"] = "ActivationInstabilityThresholdBreach"
            facts["final_runtime_outcome"] = "FailStopped"
            summary = self._complete_summary("runtime-failure")
            summary["quality_flags"]["terminal_failure"] = True
            self._write_complete_attempt(
                repo_root,
                "runtime-failure",
                facts=facts,
                summary=summary,
                capture="TERMINATED",
            )

            result = self._run_cli(repo_root, "runtime-failure")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Contradictions\n- none", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- BLOCKED", result.stdout)

    def test_summary_terminal_failure_disagreement_is_contradictory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            summary = self._complete_summary("summary-conflict")
            summary["quality_flags"]["terminal_failure"] = True
            self._write_complete_attempt(repo_root, "summary-conflict", summary=summary)

            result = self._run_cli(repo_root, "summary-conflict")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "summary quality_flags.terminal_failure disagrees with terminal facts",
                result.stdout,
            )
            self.assertIn("Local Diagnostic Classification\n- CONTRADICTORY", result.stdout)

    def test_attempt_capture_disagreement_is_contradictory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            self._write_complete_attempt(repo_root, "capture-conflict", capture="TERMINATED")

            result = self._run_cli(repo_root, "capture-conflict")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "log reports capture=TERMINATED but terminal facts require capture=COMPLETED",
                result.stdout,
            )
            self.assertIn("Local Diagnostic Classification\n- CONTRADICTORY", result.stdout)

    def test_generic_log_cannot_replace_attempt_capture(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("generic-log")
            self._write_complete_attempt(repo_root, "generic-log", facts=facts)
            self._write_text(
                repo_root / "test-results" / "logs" / "generic-log.log",
                "Result: PASSED\n",
            )

            result = self._run_cli(repo_root, "generic-log")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "production AttemptCapture log for attempt_uuid=generic-log",
                result.stdout,
            )
            self.assertIn("INSUFFICIENT_EVIDENCE", result.stdout)

    def test_attempt_specific_log_is_preferred_over_saved_editor_log(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts, _ = self._write_complete_attempt(repo_root, "preferred")
            self._write_text(
                repo_root / "PhysAnimUE5" / "Saved" / "Logs" / "PhysAnimUE5.log",
                "PhysAnimProof: AttemptCapture uuid=preferred capture=TERMINATED "
                "active_standing_duration=3.000 terminal_reason=None\n",
                30.0,
            )

            result = self._run_cli(repo_root, "preferred")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("preferred.log", result.stdout)
            self.assertNotIn("PhysAnimUE5.log", result.stdout)
            self.assertIn(self._capture_line(facts).strip(), result.stdout)

    def test_non_active_segment_is_reported_without_changing_authority(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            summary = self._complete_summary("segment")
            summary["segments"][2]["state"] = "NotReached"
            self._write_complete_attempt(repo_root, "segment", summary=summary)

            result = self._run_cli(repo_root, "segment")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Next Blocking Segment: segment_name=PhysicsControl", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- DIAGNOSTIC", result.stdout)

    def test_missing_summary_is_explicit_and_nonzero(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("no-summary")
            self._write_json(
                repo_root / "test-results" / "proof-artifacts" / "no-summary_terminal.json",
                facts,
            )
            self._write_text(
                repo_root / "test-results" / "logs" / "no-summary.log",
                self._capture_line(facts),
            )

            result = self._run_cli(repo_root, "no-summary")

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("evidence summary for attempt_uuid=no-summary", result.stdout)
            self.assertIn("INSUFFICIENT_EVIDENCE", result.stdout)

    def test_legacy_success_candidate_is_ignored(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            summary = self._complete_summary("legacy")
            summary["strict_verdict"] = "PRODUCT_SUCCESS_CANDIDATE"
            self._write_complete_attempt(repo_root, "legacy", summary=summary)

            result = self._run_cli(repo_root, "legacy")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertNotIn("PRODUCT_SUCCESS_CANDIDATE", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- DIAGNOSTIC", result.stdout)

    def test_verdict_like_log_line_is_context_not_authority(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts, _ = self._write_complete_attempt(repo_root, "log-claim")
            self._write_text(
                repo_root / "test-results" / "logs" / "log-claim.log",
                self._capture_line(facts) + "log-claim StrictVerdict=BLOCKED\n",
            )

            result = self._run_cli(repo_root, "log-claim")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("StrictVerdict=BLOCKED", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- DIAGNOSTIC", result.stdout)

    def test_metrics_are_reported_from_runtime_facts(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("metrics")
            facts["max_root_tilt_deg"] = 12.5
            facts["peak_angular_speed_deg_per_sec"] = 450.0
            facts["support_churn_hz"] = 8.0
            facts["proxy_outside_hull_duration_ms"] = 25.0
            self._write_complete_attempt(repo_root, "metrics", facts=facts)

            result = self._run_cli(repo_root, "metrics")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Max Root Tilt=12.5", result.stdout)
            self.assertIn("Peak Angular Speed=450.0", result.stdout)
            self.assertIn("Support Churn=8.0", result.stdout)
            self.assertIn("Proxy Drift=25.0", result.stdout)


if __name__ == "__main__":
    unittest.main()
