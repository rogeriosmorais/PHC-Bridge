# S2-IMPL-ARTIFACT-ARBITRATION-INTEGRATION-01 — Wire Arbitration Into Artifact Snapshot Assembly

## Purpose

Make `BuildRunArtifactSnapshot` consume canonical terminal failure arbitration instead of using local fixed-order terminal-reason selection.

This task keeps the work value-only. It does not read live Unreal runtime objects and does not enforce runtime behavior.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimArtifactArbitration.Tests.cpp`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- `PhysAnimRuntimeAdapter.h`
- `PhysAnimRuntimeAdapter.cpp`
- `PhysAnimFailureArbitration.h`
- `PhysAnimFailureArbitration.cpp`
- `PhysAnimFailureArbitration.Tests.cpp`
- workflow/process files except `execution-log.md` through the normal task-completion update

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `PhysAnimFailureArbitration.h`

Do not read broad Stage 1 docs by default.

## Required Work

1. Add `FailureCandidates` to `FPhysAnimRunArtifactSnapshotInput`.
2. Update `BuildRunArtifactSnapshot` to call `PhysAnimFailureArbitration::ArbitrateFailure`.
3. Populate:
   - `TerminalReason`
   - `CoTerminalReasons`
   - `TerminalSubstepTimestamp`
4. Preserve existing value copying from validator results into the artifact snapshot.
5. If no explicit `FailureCandidates` are provided, build fallback candidates from existing validation results using `Input.Values.TerminalSubstepTimestamp`.
6. Add an isolated automation test file:
   - `PhysAnimArtifactArbitration.Tests.cpp`
7. Do not edit runtime adapter or runtime state-machine files.

## Required Tests

- `PhysAnim.Validators.ArtifactArbitration`

Required scenarios:
- artifact uses temporal precedence from explicit failure candidates
- artifact records co-terminal reasons for same timestamp
- artifact returns `None` when no candidates or validation failures exist

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.Validators.ArtifactArbitration`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ARTIFACT-ARBITRATION-INTEGRATION-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- artifact arbitration tests pass
- build passes
- scope check passes
- no runtime dependency introduced
- no forbidden files touched
- one task commit created
- `execution-log.md` updated through the normal completion path
- handoff block provided

## Stop Conditions

Stop immediately if:
- artifact arbitration requires live runtime data
- a new terminal reason is needed
- runtime enforcement is attempted
- validator behavior unrelated to artifact assembly is changed
- `PhysAnimFailureArbitration` itself needs modification
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-ARTIFACT-ARBITRATION-INTEGRATION-01
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
Next task:
```
