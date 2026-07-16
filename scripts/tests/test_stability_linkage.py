from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT_PATH = Path(__file__).resolve().parents[1] / "collect_evidence.py"
CONTRACT_PATH = SCRIPT_PATH.parents[1] / "product-gates" / "standing-v0.v2.json"
SEGMENTS = (
    "PoseSearch",
    "PhcPolicy",
    "PhysicsControl",
    "Chaos",
    "RendererFacingMotion",
)


class StabilityLinkageTests(unittest.TestCase):
    def _complete_runtime_facts(self, attempt_uuid: str) -> dict:
        contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
        facts: dict[str, object] = {}
        for criterion in contract["criteria"]:
            operator = criterion["operator"]
            if operator == "equals":
                value = criterion["value"]
            elif operator == "nonempty":
                value = f"observed-{criterion['fact']}"
            elif operator == "timestamp":
                value = "2026-07-10T12:00:00Z"
            elif operator == "commit":
                value = "0123456789abcdef0123456789abcdef01234567"
            elif operator == "one_of":
                value = criterion["value"][0]
            elif operator in {"minimum", "maximum"}:
                value = criterion["value"]
            elif operator == "range":
                value = criterion["maximum"]
            elif operator == "positive":
                value = 1.0
            else:
                self.fail(f"unsupported contract operator in fixture: {operator}")
            facts[criterion["fact"]] = value

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
                "setup_override_count": None,
                "runtime_simulating_body_count": 10,
                "hold_duration_sec": 3.0,
                "thigh_net_work": 1.0,
            }
        )
        return facts

    def _summary(self, attempt_uuid: str, terminal_failure: bool = False) -> dict:
        return {
            "attempt_uuid": attempt_uuid,
            "diagnostic_classification": "DIAGNOSTIC",
            "missing_evidence": False,
            "quality_flags": {
                "missing_evidence": False,
                "terminal_failure": terminal_failure,
                "artifact_log_contradiction": False,
            },
            "segments": [
                {"segment_name": name, "state": "Active"}
                for name in SEGMENTS
            ],
        }

    def _run_attempt(
        self,
        repo_root: Path,
        facts: dict,
        *,
        terminal_failure: bool = False,
    ) -> subprocess.CompletedProcess[str]:
        attempt_uuid = str(facts["attempt_uuid"])
        artifact_path = (
            repo_root / "test-results" / "proof-artifacts" / f"{attempt_uuid}_terminal.json"
        )
        summary_path = (
            repo_root
            / "test-results"
            / "evidence-summaries"
            / f"{attempt_uuid}_evidence_summary.json"
        )
        log_path = repo_root / "test-results" / "logs" / f"{attempt_uuid}.log"
        artifact_path.parent.mkdir(parents=True, exist_ok=True)
        summary_path.parent.mkdir(parents=True, exist_ok=True)
        log_path.parent.mkdir(parents=True, exist_ok=True)
        artifact_path.write_text(json.dumps(facts), encoding="utf-8")
        summary_path.write_text(
            json.dumps(self._summary(attempt_uuid, terminal_failure)),
            encoding="utf-8",
        )
        capture = "TERMINATED" if terminal_failure else "COMPLETED"
        log_path.write_text(
            "PhysAnimProof: AttemptCapture "
            f"uuid={attempt_uuid} capture={capture} "
            "active_standing_duration="
            f"{float(facts['balance_active_standing_continuous_sec']):.3f} "
            f"terminal_reason={facts['terminal_reason_name']}\n",
            encoding="utf-8",
        )
        return subprocess.run(
            [
                sys.executable,
                str(SCRIPT_PATH),
                "--repo-root",
                str(repo_root),
                "--attempt-uuid",
                attempt_uuid,
            ],
            capture_output=True,
            text=True,
            check=False,
        )

    def _assert_contract_blocked(self, field: str, value: object) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts(field)
            facts[field] = value

            result = self._run_attempt(repo_root, facts)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(f"locked contract observation failed: {field}", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- BLOCKED", result.stdout)

    def test_complete_stability_observations_are_diagnostic_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("stable")

            result = self._run_attempt(repo_root, facts)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Local Diagnostic Classification\n- DIAGNOSTIC", result.stdout)
            self.assertIn("LOCAL_DIAGNOSTIC_ONLY", result.stdout)

    def test_short_standing_window_is_blocked(self) -> None:
        self._assert_contract_blocked("balance_active_standing_continuous_sec", 2.9)

    def test_excessive_root_tilt_is_blocked(self) -> None:
        self._assert_contract_blocked("max_root_tilt_deg", 20.1)

    def test_excessive_angular_speed_is_blocked(self) -> None:
        self._assert_contract_blocked("peak_angular_speed_deg_per_sec", 720.1)

    def test_excessive_support_churn_is_blocked(self) -> None:
        self._assert_contract_blocked("support_churn_hz", 12.1)

    def test_excessive_proxy_drift_is_blocked(self) -> None:
        self._assert_contract_blocked("proxy_outside_hull_duration_ms", 100.1)

    def test_authority_conflict_is_blocked(self) -> None:
        self._assert_contract_blocked("authority_conflict_count", 1)

    def test_terminal_failure_is_blocked_without_summary_contradiction(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            facts = self._complete_runtime_facts("terminal-failure")
            facts["terminal_reason"] = 9
            facts["terminal_reason_name"] = "ActivationInstabilityThresholdBreach"

            result = self._run_attempt(repo_root, facts, terminal_failure=True)

            self.assertNotEqual(result.returncode, 0)
            self.assertIn("Contradictions\n- none", result.stdout)
            self.assertIn("Local Diagnostic Classification\n- BLOCKED", result.stdout)


if __name__ == "__main__":
    unittest.main()
