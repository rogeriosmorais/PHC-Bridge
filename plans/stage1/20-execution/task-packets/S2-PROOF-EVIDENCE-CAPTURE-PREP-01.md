# Task Packet: S2-PROOF-EVIDENCE-CAPTURE-PREP-01

## Purpose
Prepare the evidence extraction and validation checklist for the Cycle 16 Editor-runtime proof. No C++ changes allowed.

## Allowed Files
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/30-evidence/S2-PROOF-EDITOR-STANDING-ATTEMPT-01.md`
- `plans/stage1/20-execution/task-packets/S2-PROOF-EVIDENCE-CAPTURE-PREP-01.md`

## Forbidden Files
- All runtime C++ files
- All test files

## Required Work
1. **Define Log Patterns**: Specify the exact strings to search for in the Editor log.
2. **PIE Run Checklist**: Document the step-by-step instructions for the human operator.
3. **Validation Criteria**: Explicitly define what constitutes a PASS vs. a FAIL in the telemetry.
4. **Evidence Prefill**: Update the evidence markdown with the checklist and placeholders.

## Required Tests/Build
- Build must remain clean.
- Mechanical verification of `PhysAnim.RuntimeTermination` (Baseline) must pass.

## Definition of Done
- Checklist and patterns documented in the evidence file.
- Handoff report with the `grep` command for log extraction.
- Execution log updated.

## Stop Conditions
- Attempting to modify C++ logic.
- Workflow state inconsistency.
