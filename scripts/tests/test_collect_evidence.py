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


class CollectEvidenceCLITests(unittest.TestCase):
    def _write_json(self, path: Path, payload: dict, mtime: float) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload), encoding="utf-8")
        os.utime(path, (mtime, mtime))

    def _write_text(self, path: Path, text: str, mtime: float) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        os.utime(path, (mtime, mtime))

    def _run_cli(
        self,
        repo_root: Optional[Path] = None,
        attempt_uuid: Optional[str] = None,
    ) -> subprocess.CompletedProcess[str]:
        command = [sys.executable, str(SCRIPT_PATH)]
        if repo_root is not None:
            command.extend(["--repo-root", str(repo_root)])
        if attempt_uuid is not None:
            command.extend(["--attempt-uuid", attempt_uuid])
        return subprocess.run(command, capture_output=True, text=True, check=False)

    def test_latest_artifacts_are_selected_and_pass_log_is_marked_contradictory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            old_terminal = saved_root / "PhysAnim" / "ProofArtifacts" / "old_terminal.json"
            new_terminal = saved_root / "PhysAnim" / "ProofArtifacts" / "new_terminal.json"
            old_summary = saved_root / "PhysAnim" / "EvidenceSummaries" / "old_evidence_summary.json"
            new_summary = saved_root / "PhysAnim" / "EvidenceSummaries" / "new_evidence_summary.json"
            log_file = saved_root / "Logs" / "PhysAnimUE5.log"

            self._write_json(
                old_terminal,
                {"attempt_uuid": "old", "physical_continuity_validator_passed": False},
                10.0,
            )
            self._write_json(
                new_terminal,
                {
                    "attempt_uuid": "new",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                20.0,
            )
            self._write_json(
                old_summary,
                {"attempt_uuid": "old", "strict_verdict": "BLOCKED", "missing_evidence": False},
                10.0,
            )
            self._write_json(
                new_summary,
                {
                    "attempt_uuid": "new",
                    "strict_verdict": "BLOCKED",
                    "missing_evidence": False,
                    "quality_flags": {"missing_evidence": False, "artifact_log_contradiction": False},
                    "segments": [
                        {"segment_name": "PoseSearch", "state": "Active"},
                        {"segment_name": "PhysicsControl", "state": "NotReached"},
                    ],
                },
                20.0,
            )
            self._write_text(log_file, "Result: PASSED\n", 20.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Actual Evidence", result.stdout)
            self.assertIn("Weak Evidence", result.stdout)
            self.assertIn("Contradictions", result.stdout)
            self.assertIn("Missing Evidence", result.stdout)
            self.assertIn("Next Blocking Segment", result.stdout)
            self.assertIn("Verdict", result.stdout)
            self.assertIn("new_terminal.json", result.stdout)
            self.assertIn("new_evidence_summary.json", result.stdout)
            self.assertIn("CONTRADICTORY", result.stdout)
            self.assertIn("PASSED", result.stdout)

    def test_terminal_proof_pass_log_is_weak_evidence_not_a_contradiction(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "attempt_terminal.json",
                {
                    "attempt_uuid": "attempt",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                20.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "attempt_evidence_summary.json",
                {
                    "attempt_uuid": "attempt",
                    "strict_verdict": "BLOCKED",
                    "missing_evidence": False,
                    "segments": [
                        {"segment_name": "PoseSearch", "state": "Active"},
                        {"segment_name": "PhcPolicy", "state": "NotReached"},
                    ],
                },
                20.0,
            )
            self._write_text(
                saved_root / "Logs" / "PhysAnimUE5.log",
                "PhysAnimProof: AttemptResult uuid=attempt verdict=PASS duration=3.000 terminal_reason=None\n",
                20.0,
            )

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PhysAnimProof: AttemptResult uuid=attempt verdict=PASS", result.stdout)
            self.assertIn("terminal proof evidence:", result.stdout)
            self.assertIn("Weak Evidence", result.stdout)
            self.assertIn("Contradictions\n- none", result.stdout)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)
            self.assertNotIn("CONTRADICTORY", result.stdout)

    def test_missing_summary_is_explicit_and_never_passes(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "one_terminal.json",
                {
                    "attempt_uuid": "one",
                    "physical_continuity_validator_passed": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: PASSED\n", 20.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Missing Evidence", result.stdout)
            self.assertIn("evidence summary", result.stdout.lower())
            self.assertIn("MISSING EVIDENCE", result.stdout)
            self.assertNotIn("PRODUCT SUCCESS", result.stdout)

    def test_attempt_uuid_filters_to_matching_artifacts_and_blocker(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "older_terminal.json",
                {
                    "attempt_uuid": "attempt-old",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "newer_terminal.json",
                {
                    "attempt_uuid": "attempt-new",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                20.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "older_evidence_summary.json",
                {
                    "attempt_uuid": "attempt-old",
                    "strict_verdict": "BLOCKED",
                    "missing_evidence": False,
                    "segments": [
                        {"segment_name": "PoseSearch", "state": "Active"},
                        {"segment_name": "PhcPolicy", "state": "NotReached"},
                    ],
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "newer_evidence_summary.json",
                {
                    "attempt_uuid": "attempt-new",
                    "strict_verdict": "BLOCKED",
                    "missing_evidence": False,
                    "segments": [
                        {"segment_name": "PoseSearch", "state": "Active"},
                        {"segment_name": "PhysicsControl", "state": "NotReached"},
                    ],
                },
                20.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: BLOCKED\n", 20.0)

            result = self._run_cli(repo_root, attempt_uuid="attempt-old")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("older_terminal.json", result.stdout)
            self.assertIn("older_evidence_summary.json", result.stdout)
            self.assertNotIn("newer_terminal.json", result.stdout)
            self.assertNotIn("newer_evidence_summary.json", result.stdout)
            self.assertIn("segment_name=PhcPolicy", result.stdout)
            self.assertNotIn("segment_name=PhysicsControl", result.stdout)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

    def test_attempt_uuid_missing_evidence_does_not_fall_back_to_global_latest(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "other_terminal.json",
                {
                    "attempt_uuid": "attempt-other",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                20.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "other_evidence_summary.json",
                {
                    "attempt_uuid": "attempt-other",
                    "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                    "missing_evidence": False,
                    "quality_flags": {
                        "missing_evidence": False,
                        "terminal_failure": False,
                        "artifact_log_contradiction": False,
                    },
                    "segments": [{"segment_name": "PhcPolicy", "state": "Active"}],
                },
                20.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: PASSED\n", 20.0)

            result = self._run_cli(repo_root, attempt_uuid="attempt-missing")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("terminal artifact for attempt_uuid=attempt-missing", result.stdout)
            self.assertIn("evidence summary for attempt_uuid=attempt-missing", result.stdout)
            self.assertIn("MISSING EVIDENCE", result.stdout)
            self.assertNotIn("other_terminal.json", result.stdout)
            self.assertNotIn("other_evidence_summary.json", result.stdout)
            self.assertNotIn("PRODUCT SUCCESS", result.stdout)

    def test_attempt_uuid_filters_log_claims_to_matching_attempt(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "old_terminal.json",
                {
                    "attempt_uuid": "attempt-old",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "old_evidence_summary.json",
                {
                    "attempt_uuid": "attempt-old",
                    "strict_verdict": "BLOCKED",
                    "missing_evidence": False,
                    "segments": [{"segment_name": "PhcPolicy", "state": "NotReached"}],
                },
                10.0,
            )
            self._write_text(
                saved_root / "Logs" / "PhysAnimUE5.log",
                "attempt-new Result: PASSED\nattempt-old Result: BLOCKED\n",
                20.0,
            )

            result = self._run_cli(repo_root, attempt_uuid="attempt-old")

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("attempt-old Result: BLOCKED", result.stdout)
            self.assertNotIn("attempt-new Result: PASSED", result.stdout)
            self.assertIn("Contradictions\n- none", result.stdout)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)

    def test_default_repo_root_uses_script_location(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "default_terminal.json",
                {
                    "attempt_uuid": "one",
                    "physical_continuity_validator_passed": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "default_evidence_summary.json",
                {
                    "attempt_uuid": "one",
                    "strict_verdict": "BLOCKED",
                    "missing_evidence": False,
                    "segments": [{"segment_name": "Chaos", "state": "NotReached"}],
                },
                10.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: BLOCKED\n", 10.0)

            script_copy = repo_root / "scripts" / "collect_evidence.py"
            script_copy.parent.mkdir(parents=True, exist_ok=True)
            script_copy.write_text(SCRIPT_PATH.read_text(encoding="utf-8"), encoding="utf-8")

            result = subprocess.run(
                [sys.executable, str(script_copy)],
                cwd=str(repo_root),
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("BLOCKED", result.stdout)

    def test_active_segments_are_not_reported_as_next_blocker(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "terminal.json",
                {
                    "attempt_uuid": "one",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "one_evidence_summary.json",
                {
                    "attempt_uuid": "one",
                    "strict_verdict": "DIAGNOSTIC",
                    "missing_evidence": False,
                    "segments": [
                        {"segment_name": "PoseSearch", "state": "Active"},
                        {"segment_name": "PhcPolicy", "state": "NotReached"},
                    ],
                },
                10.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: BLOCKED\n", 10.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("segment_name=PhcPolicy", result.stdout)
            self.assertNotIn("segment_name=PoseSearch, state=Active", result.stdout)
            self.assertIn("DIAGNOSTIC", result.stdout)

    def test_product_success_candidate_is_recognized_as_artifact_success(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "one_terminal.json",
                {
                    "attempt_uuid": "one",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "one_evidence_summary.json",
                {
                    "attempt_uuid": "one",
                    "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                    "missing_evidence": False,
                    "quality_flags": {
                        "missing_evidence": False,
                        "terminal_failure": False,
                        "artifact_log_contradiction": False,
                    },
                    "segments": [
                        {"segment_name": "PoseSearch", "state": "Active"},
                        {"segment_name": "PhcPolicy", "state": "Active"},
                    ],
                },
                10.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: PASSED\n", 10.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("PRODUCT SUCCESS", result.stdout)
            self.assertIn("No blocking segment found.", result.stdout)

    def test_incidental_log_words_do_not_create_contradictions(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "one_terminal.json",
                {
                    "attempt_uuid": "one",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "one_evidence_summary.json",
                {
                    "attempt_uuid": "one",
                    "strict_verdict": "BLOCKED",
                    "missing_evidence": False,
                    "segments": [{"segment_name": "PhcPolicy", "state": "NotReached"}],
                },
                10.0,
            )
            self._write_text(
                saved_root / "Logs" / "PhysAnimUE5.log",
                "LogInit: Engine launched successfully\nLogWindows: Failed to load aqProf.dll\n",
                10.0,
            )

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Contradictions\n- none", result.stdout)
            self.assertIn("Verdict\n- BLOCKED", result.stdout)
            self.assertNotIn("CONTRADICTORY", result.stdout)

    def test_invalid_stability_metrics_are_reported_as_critical(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            # Contradiction: hold duration > 0 but sim bodies = 0
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "bad_terminal.json",
                {
                    "attempt_uuid": "bad",
                    "physical_continuity_validator_passed": True,
                    "hold_duration_sec": 5.0,
                    "runtime_simulating_body_count": 0,
                },
                20.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "bad_evidence_summary.json",
                {
                    "attempt_uuid": "bad",
                    "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                },
                20.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: PASSED\n", 20.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0)
            self.assertIn("CONTRADICTORY", result.stdout)
            self.assertIn("hold_duration_sec=5.0 but runtime_simulating_body_count=0", result.stdout)
            self.assertIn("CRITICAL: Stability metrics are invalid", result.stdout)
            self.assertIn("Route to Artifact Schema Acceptance work", result.stdout)

    def test_missing_critical_stability_fields_are_reported_as_critical(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            # Missing runtime_simulating_body_count
            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "missing_field_terminal.json",
                {
                    "attempt_uuid": "missing",
                    "physical_continuity_validator_passed": True,
                    "hold_duration_sec": 5.0,
                },
                20.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "missing_field_summary.json",
                {
                    "attempt_uuid": "missing",
                    "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                },
                20.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: PASSED\n", 20.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0)
            self.assertIn("MISSING EVIDENCE", result.stdout)
            self.assertIn("stability field: runtime_simulating_body_count", result.stdout)
            self.assertIn("CRITICAL: Stability metrics are invalid", result.stdout)

    def test_explicit_strict_verdict_line_is_a_failure_claim(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "one_terminal.json",
                {
                    "attempt_uuid": "one",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.0,
                    "runtime_simulating_body_count": 22,
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "one_evidence_summary.json",
                {
                    "attempt_uuid": "one",
                    "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                    "missing_evidence": False,
                    "quality_flags": {
                        "missing_evidence": False,
                        "terminal_failure": False,
                        "artifact_log_contradiction": False,
                    },
                },
                10.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "StrictVerdict=BLOCKED\n", 10.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("StrictVerdict=BLOCKED", result.stdout)
            self.assertIn("log claims failure", result.stdout)
            self.assertIn("CONTRADICTORY", result.stdout)

    def test_stability_metrics_are_extracted_and_reported(self) -> None:
        with tempfile.TemporaryDirectory() as tmp_dir:
            repo_root = Path(tmp_dir)
            saved_root = repo_root / "PhysAnimUE5" / "Saved"

            self._write_json(
                saved_root / "PhysAnim" / "ProofArtifacts" / "one_terminal.json",
                {
                    "attempt_uuid": "one",
                    "physical_continuity_validator_passed": True,
                    "terminal_frame_artifact_captured": True,
                    "hold_duration_sec": 3.5,
                    "runtime_simulating_body_count": 22,
                    "max_root_tilt_deg": 12.5,
                    "peak_angular_speed_deg_per_sec": 450.0,
                    "support_churn_hz": 8.0,
                    "proxy_outside_hull_duration_ms": 25.0,
                    "terminal_reason_name": "None",
                },
                10.0,
            )
            self._write_json(
                saved_root / "PhysAnim" / "EvidenceSummaries" / "one_evidence_summary.json",
                {
                    "attempt_uuid": "one",
                    "strict_verdict": "PRODUCT_SUCCESS_CANDIDATE",
                    "quality_flags": {
                        "missing_evidence": False,
                        "terminal_failure": False,
                        "artifact_log_contradiction": False,
                    },
                },
                10.0,
            )
            self._write_text(saved_root / "Logs" / "PhysAnimUE5.log", "Result: PASSED\n", 10.0)

            result = self._run_cli(repo_root)

            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn("Stability Metrics", result.stdout)
            self.assertIn("Hold Duration=3.5", result.stdout)
            self.assertIn("Max Root Tilt=12.5", result.stdout)
            self.assertIn("Peak Angular Speed=450.0", result.stdout)
            self.assertIn("Support Churn=8.0", result.stdout)
            self.assertIn("Proxy Drift=25.0", result.stdout)
            self.assertIn("Terminal Reason=None", result.stdout)


if __name__ == "__main__":
    unittest.main()
