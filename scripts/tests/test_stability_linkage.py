from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve().parents[1] / "collect_evidence.py"

class StabilityLinkageTests(unittest.TestCase):
    def _remap_path(self, path: Path) -> Path:
        path_str = path.as_posix()
        if "PhysAnimUE5/Saved/PhysAnim/ProofArtifacts" in path_str:
            path_str = path_str.replace("PhysAnimUE5/Saved/PhysAnim/ProofArtifacts", "test-results/proof-artifacts")
        elif "PhysAnimUE5/Saved/PhysAnim/EvidenceSummaries" in path_str:
            path_str = path_str.replace("PhysAnimUE5/Saved/PhysAnim/EvidenceSummaries", "test-results/evidence-summaries")
        return Path(path_str)

    def _write_json(self, path: Path, payload: dict) -> None:
        mapped_path = self._remap_path(path)
        mapped_path.parent.mkdir(parents=True, exist_ok=True)
        if "terminal" in path.name:
            if "thigh_net_work" not in payload:
                payload["thigh_net_work"] = 1.0
            if "policy_inference_success_count" not in payload:
                payload["policy_inference_success_count"] = 5
            if "policy_inference_attempt_count" not in payload:
                payload["policy_inference_attempt_count"] = 5
            if "terminal_reason_name" not in payload:
                payload["terminal_reason_name"] = "None"
        mapped_path.write_text(json.dumps(payload), encoding="utf-8")

    def _run_cli(self, repo_root: Path) -> subprocess.CompletedProcess[str]:
        command = [sys.executable, str(SCRIPT_PATH), "--repo-root", str(repo_root)]
        return subprocess.run(command, capture_output=True, text=True, check=False)

    def test_success_with_perfect_metrics(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "success_terminal.json",
                {
                    "attempt_uuid": "success",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 3.0,
                    "max_root_tilt_deg": 10.0,
                    "peak_angular_speed_deg_per_sec": 100.0,
                    "support_churn_hz": 5.0,
                    "proxy_outside_hull_duration_ms": 0.0,
                    "terminal_reason": 0,
                    "terminal_reason_name": "None"
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "success_evidence_summary.json",
                {
                    "attempt_uuid": "success",
                    "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                    "quality_flags": {
                        "missing_evidence": False,
                        "terminal_failure": False,
                        "artifact_log_contradiction": False
                    }
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- PRODUCT SUCCESS", result.stdout)

    def test_failure_due_to_short_hold(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "short_terminal.json",
                {
                    "attempt_uuid": "short",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 2.9,
                    "terminal_reason": 0
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "short_evidence_summary.json",
                {
                    "attempt_uuid": "short",
                    "strict_verdict": "BLOCKED",
                    "quality_flags": {"missing_evidence": False, "terminal_failure": False}
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)
            self.assertIn("terminal artifact does not satisfy the success support check", result.stdout)

    def test_failure_due_to_high_tilt(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "tilt_terminal.json",
                {
                    "attempt_uuid": "tilt",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 3.0,
                    "max_root_tilt_deg": 20.1,
                    "terminal_reason": 0
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "tilt_evidence_summary.json",
                {
                    "attempt_uuid": "tilt",
                    "strict_verdict": "BLOCKED",
                    "quality_flags": {"missing_evidence": False, "terminal_failure": False}
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

    def test_failure_due_to_high_angular_speed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "speed_terminal.json",
                {
                    "attempt_uuid": "speed",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 3.0,
                    "peak_angular_speed_deg_per_sec": 720.1,
                    "terminal_reason": 0
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "speed_evidence_summary.json",
                {
                    "attempt_uuid": "speed",
                    "strict_verdict": "BLOCKED",
                    "quality_flags": {"missing_evidence": False, "terminal_failure": False}
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

    def test_failure_due_to_support_churn(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "churn_terminal.json",
                {
                    "attempt_uuid": "churn",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 3.0,
                    "support_churn_hz": 12.1,
                    "terminal_reason": 0
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "churn_evidence_summary.json",
                {
                    "attempt_uuid": "churn",
                    "strict_verdict": "BLOCKED",
                    "quality_flags": {"missing_evidence": False, "terminal_failure": False}
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

    def test_failure_due_to_proxy_drift(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "drift_terminal.json",
                {
                    "attempt_uuid": "drift",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 3.0,
                    "proxy_outside_hull_duration_ms": 100.1,
                    "terminal_reason": 0
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "drift_evidence_summary.json",
                {
                    "attempt_uuid": "drift",
                    "strict_verdict": "BLOCKED",
                    "quality_flags": {"missing_evidence": False, "terminal_failure": False}
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

    def test_failure_due_to_terminal_reason(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "fail_terminal.json",
                {
                    "attempt_uuid": "fail",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 3.0,
                    "terminal_reason": 9 # activation_instability_threshold_breach
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "fail_evidence_summary.json",
                {
                    "attempt_uuid": "fail",
                    "strict_verdict": "BLOCKED",
                    "quality_flags": {"missing_evidence": False, "terminal_failure": False}
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

    def test_failure_due_to_authority_conflict(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"
            
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "conflict_terminal.json",
                {
                    "attempt_uuid": "conflict",
                    "physical_continuity_validator_passed": True,
                    "runtime_simulating_body_count": 22,
                    "hold_duration_sec": 3.0,
                    "authority_conflict_count": 1,
                    "terminal_reason": 0
                }
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "conflict_evidence_summary.json",
                {
                    "attempt_uuid": "conflict",
                    "strict_verdict": "BLOCKED",
                    "quality_flags": {"missing_evidence": False, "terminal_failure": False}
                }
            )
            
            result = self._run_cli(repo_root)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

if __name__ == "__main__":
    unittest.main()
