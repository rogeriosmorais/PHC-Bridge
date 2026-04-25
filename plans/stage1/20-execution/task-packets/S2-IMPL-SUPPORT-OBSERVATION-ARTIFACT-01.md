# S2-IMPL-SUPPORT-OBSERVATION-ARTIFACT-01 — Convert Support Observation Into Artifact Snapshot

## Purpose

Implement a deterministic adapter that converts a validated `FPhysAnimSupportObservationResult` into a `FPhysAnimRunArtifactSnapshot` using the existing canonical terminal failure arbitration path.

This task does not query live Chaos state and does not enforce runtime behavior.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportObservationArtifact.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-SUPPORT-OBSERVATION-ARTIFACT-01.md`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- `PhysAnimValidators.h`
- `PhysAnimValidators.cpp`
- `PhysAnimSupportTruth.h`
- `PhysAnimSupportTruth.cpp`
- `PhysAnimFailureArbitration.h`
- `PhysAnimFailureArbitration.cpp`
- artifact emission/runtime logging files
- workflow/process files except `execution-log.md` through the normal task-completion update

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `PhysAnimRuntimeAdapter.h`
- `PhysAnimRuntimeAdapter.cpp`
- `PhysAnimRuntimeAdapter.SupportObservation.Tests.cpp`
- `PhysAnimArtifactArbitration.Tests.cpp`

Do not read broad Stage 1 docs by default.

## Required Work

1. Add `FPhysAnimSupportObservationArtifactInput`.
2. Add `PhysAnimRuntimeAdapter::BuildSupportObservationArtifactSnapshot`.
3. Build `FPhysAnimRunArtifactSnapshotInput` from:
   - `Input.Observation.Validation`
   - `Input.Values`
   - `Input.AdditionalFailureCandidates`
4. Always add the support terminal reason as an explicit `FPhysAnimFailureCandidate` when `Observation.Validation.TerminalReason != None`.
5. Use `Input.Values.TerminalSubstepTimestamp` as the support terminal timestamp.
6. Call `PhysAnimValidators::BuildRunArtifactSnapshot`.
7. Do not duplicate arbitration logic.
8. Do not duplicate support validation logic.
9. Do not query UWorld, Chaos, FBodyInstance, or live collision state here.

## Required Tests

- `PhysAnim.RuntimeAdapter.SupportObservationArtifact`

Required scenarios:
- valid support observation produces artifact with `TerminalReason = None`
- support failure observation produces artifact with `ActivationSupportFailure`
- proxy failure observation produces artifact with `ActivationProxyOutsideSupportRegion`
- earlier non-support candidate wins by temporal precedence
- simultaneous higher-rank support failure wins over lower-rank authority candidate
- artifact copies support snapshot fields from the support observation

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportObservationArtifact`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportObservation`
- `.\scripts\build.ps1 -Test PhysAnim.Validators.ArtifactArbitration`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-SUPPORT-OBSERVATION-ARTIFACT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- support observation artifact tests pass
- support observation regression test still passes
- artifact arbitration regression test still passes
- build passes
- scope check passes
- no runtime dependency introduced
- no forbidden files touched
- one task commit created
- `execution-log.md` updated through the normal completion path
- handoff block provided

## Stop Conditions

- live Chaos/physics queries are needed
- a support-truth math change is needed
- a validator change is needed
- arbitration logic needs modification
- runtime enforcement is attempted
- state-machine files need edits
- behavior unrelated to support observation artifact assembly is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-SUPPORT-OBSERVATION-ARTIFACT-01
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
