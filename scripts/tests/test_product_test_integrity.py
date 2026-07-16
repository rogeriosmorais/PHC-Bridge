from __future__ import annotations

import json
import re
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
PRIVATE = (
    REPO_ROOT
    / "PhysAnimUE5"
    / "Plugins"
    / "PhysAnimPlugin"
    / "Source"
    / "PhysAnimPlugin"
    / "Private"
)


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def function_body(source: str, signature: str, next_marker: str) -> str:
    start = source.index(signature)
    end = source.index(next_marker, start)
    return source[start:end]


class ProductTestIntegrityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.helpers = read(PRIVATE / "PhysAnimBalance.TestHelpers.h")
        cls.standing = read(PRIVATE / "PhysAnimStandingProof.FunctionalTests.cpp")
        cls.emitter = read(PRIVATE / "PhysAnimProofArtifactEmitter.cpp")
        cls.collector = read(REPO_ROOT / "scripts" / "collect_evidence.py")

    def test_balance_smoke_has_no_stage1_or_terminal_failure_as_pass_path(self) -> None:
        self.assertNotIn("&& !bIsStage1", self.helpers)
        terminal_branch = function_body(
            self.helpers,
            "if (IsTruthfulBalanceSmokeTerminalReason(TerminalReason))",
            "if (RuntimeState == EPhysAnimRuntimeState::FailStopped",
        )
        self.assertNotRegex(terminal_branch, r"\breturn\s+true\s*;")

    def test_live_policy_control_and_physics_assertions_cannot_be_disabled(self) -> None:
        self.assertNotIn("GPhysAnimStrictLivePolicyProofQuality", self.standing)
        self.assertNotIn("p.PhysAnim.StrictLivePolicyProofQuality", self.standing)
        self.assertNotIn("strict proof quality is opt-in", self.standing)

    def test_product_tests_cannot_mutate_thresholds_or_unlock_groups(self) -> None:
        self.assertNotIn("EAutomationTestFlags::ProductFilter", self.standing)
        enable_body = function_body(
            self.standing,
            "bool FEnableStandingProofCommand::Update()",
            "DEFINE_LATENT_AUTOMATION_COMMAND(FOverrideLoadTestSettingsCommand)",
        )
        self.assertNotIn("StabilizationSettings", enable_body)
        self.assertNotRegex(enable_body, r"Settings\.[A-Za-z0-9_]+\s*=")
        self.assertNotIn("UnlockBringUpGroup", enable_body)

    def test_complex_diagnostics_are_not_registered_with_an_empty_case_list(self) -> None:
        cases = (
            (
                "void FPhysAnimRawSimulationOwnershipBisectDiagnosticTest::GetTests",
                "bool FPhysAnimRawSimulationOwnershipBisectDiagnosticTest::RunTest",
            ),
            (
                "void FPhysAnimControlIsolationMatrixDiagnosticTest::GetTests",
                "bool FPhysAnimControlIsolationMatrixDiagnosticTest::RunTest",
            ),
            (
                "void FPhysAnimThighRestoreDiagnosticTest::GetTests",
                "bool FPhysAnimThighRestoreDiagnosticTest::RunTest",
            ),
        )
        for signature, next_marker in cases:
            with self.subTest(signature=signature):
                get_tests = function_body(self.standing, signature, next_marker)
                self.assertIn("OutBeautifiedNames.Add", get_tests)
                self.assertIn("OutTestCommands.Add", get_tests)

    def test_runtime_artifact_contains_facts_not_authoritative_product_fields(self) -> None:
        forbidden_json_fields = (
            'TEXT("product_success")',
            'TEXT("strict_verdict")',
            'TEXT("artifact_pass")',
            'TEXT("physical_continuity_validator_passed")',
            'TEXT("controller_stability_passed")',
        )
        for field in forbidden_json_fields:
            with self.subTest(field=field):
                self.assertNotIn(field, self.emitter)

    def test_unobservable_setup_override_count_is_not_serialized_as_zero(self) -> None:
        self.assertNotIn(
            'Json->SetNumberField(TEXT("setup_override_count")',
            self.emitter,
        )
        self.assertIn(
            'PhysAnimProof_SetOptionalNumber(Json, TEXT("setup_override_count")',
            self.emitter,
        )

    def test_current_state_reports_observed_behavior_without_receipt_dependency(self) -> None:
        current_state = read(REPO_ROOT / "docs" / "evidence" / "CURRENT_STATE_ANALYSIS.md")
        self.assertRegex(current_state, r"(?i)product behavior is \*\*FAIL\*\*")
        self.assertIn("## Latest Observed Attempt", current_state)
        self.assertIn("phase1_late_validate_upper_body_instability", current_state)
        self.assertIn("The evaluator returned\n`FAIL`, not `INVALID` or `BLOCKED`", current_state)
        self.assertIn("not blocked by a signer", current_state)
        self.assertNotRegex(current_state, r"(?i)signed receipt:\s*\*\*ABSENT\*\*")
        self.assertNotIn("Proceed immediately", current_state)
        self.assertNotIn("technically complete and verified", current_state)

    def test_local_evidence_collector_cannot_issue_a_product_verdict(self) -> None:
        self.assertNotIn('return "PRODUCT SUCCESS"', self.collector)
        self.assertNotIn('summary.get("strict_verdict")', self.collector)
        self.assertIn('"Local Diagnostic Classification"', self.collector)

    def test_workflow_test_gates_execute_real_suites(self) -> None:
        package = json.loads(read(REPO_ROOT / "package.json"))
        scripts = package.get("scripts", {})

        self.assertIn("python -m pytest scripts/tests", scripts.get("test:blast", ""))
        self.assertIn("python -m pytest scripts/tests", scripts.get("test:fast", ""))
        self.assertIn("scripts/test_runtime.ps1", scripts.get("test:runtime", ""))
        self.assertIn("scripts/run_causal_standing.ps1 -Mode Milestone", scripts.get("test:product", ""))
        self.assertIn("scripts/test_full.ps1", scripts.get("test", ""))
        self.assertTrue((REPO_ROOT / "scripts" / "test_runtime.ps1").is_file())
        self.assertTrue((REPO_ROOT / "scripts" / "test_full.ps1").is_file())

    def test_project_mcp_uses_the_guarded_globalmcp2_runtime(self) -> None:
        config = json.loads(read(REPO_ROOT / ".mcp.json"))["mcpServers"]["mcp-graph"]
        self.assertEqual(
            config["command"],
            r"C:\Users\roger\AppData\Local\Volta\tools\image\node\22.22.1\node.exe",
        )
        self.assertEqual(
            config["args"],
            [
                r"F:\GlobalMCP2\mcp-graph-workflow\compat\mcp-graph-13.27-gate\dist\proxy.js"
            ],
        )


if __name__ == "__main__":
    unittest.main()
