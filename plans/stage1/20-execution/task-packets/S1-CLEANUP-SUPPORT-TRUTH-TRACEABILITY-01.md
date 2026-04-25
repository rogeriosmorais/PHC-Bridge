# S1-CLEANUP-SUPPORT-TRUTH-TRACEABILITY-01 — Support Truth Test Traceability Cleanup

## Purpose

Align Slice 1 support-truth test labels and comments with `balance_first_test_matrix.md` without changing production behavior.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimTruthTypes.h`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- workflow/process files

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`

Do not read broad Stage 1 docs by default.

## Required Work

1. Compare every `LOGIC-*` label/comment in `PhysAnimSupportTruth.Tests.cpp` against `balance_first_test_matrix.md`.
2. Rename comments and assertion labels so the test file uses the same LOGIC IDs and scenario meanings as the matrix.
3. Preserve the existing test intent where it is correct.
4. If an existing assertion is valid but mislabeled, relabel it only.
5. If a matrix row is missing coverage, add the smallest test case needed inside the existing support-truth test file.
6. Do not change production code.
7. Do not change public structs/enums.
8. Do not change thresholds or expected behavior unless the matrix itself is impossible to satisfy.

## Required Tests

- `.\\scripts\\build.ps1 -Test PhysAnim.SupportTruth`

## Required Build

- `.\\scripts\\build.ps1`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S1-CLEANUP-SUPPORT-TRUTH-TRACEABILITY-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- test labels/comments match the matrix
- any missing matrix-row coverage is added in the test file only
- production code is unchanged
- required test command passes
- build passes
- scope check passes
- one cleanup task commit is created
- `execution-log.md` is advanced with `scripts/complete_task.ps1`
- forbidden files untouched

## Stop Conditions

Stop immediately if:
- a required fix needs production code changes
- a matrix row contradicts implemented behavior
- a public API change appears necessary
- a runtime file appears in the diff
- the test suite fails for behavior unrelated to traceability
- the same conceptual failure happens twice

If a matrix row contradicts the implemented behavior, classify it as `contract gap` and stop.

## Required Handoff

```text
Summary:
Task: S1-CLEANUP-SUPPORT-TRUTH-TRACEABILITY-01
Base:
Head:
Commit:
Build:
Tests:
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task: S2-DESIGN-RUNTIME-ADAPTER-01 or blocked
```
