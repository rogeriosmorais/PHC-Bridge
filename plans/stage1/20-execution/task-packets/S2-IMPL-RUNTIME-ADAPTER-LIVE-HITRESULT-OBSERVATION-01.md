# S2-IMPL-RUNTIME-ADAPTER-LIVE-HITRESULT-OBSERVATION-01 — Convert Live UE Hit Results Into Support Observations

## Purpose

Implement a deterministic adapter seam from UE `FHitResult` records into the existing support observation pipeline.

This task connects the support module to live UE collision result shapes without querying Chaos directly, without orchestrating substeps, and without enforcing runtime termination.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.LiveHitResultObservation.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-LIVE-HITRESULT-OBSERVATION-01.md`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- `PhysAnimComponent.h`
- `PhysAnimComponent.cpp`
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
- `PhysAnimRuntimeAdapter.SupportObservationArtifact.Tests.cpp`

Do not read broad Stage 1 docs by default.

## Required Work

1. Add `FPhysAnimSupportHitResultConversionInput`.
2. Add `FPhysAnimSupportHitResultObservationInput`.
3. Add `PhysAnimRuntimeAdapter::ConvertSupportHitResultsToHitRecords`.
4. Add `PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults`.
5. Convert UE `FHitResult` values into `FPhysAnimSupportHitRecord` values.
6. Use `Hit.BoneName` as the body name.
7. Use `Hit.ImpactPoint` as the contact world position.
8. Treat `Hit.bBlockingHit` as the blocking flag.
9. Treat `Hit.Component->Mobility == EComponentMobility::Static` as world-static support when world-static filtering is required.
10. Compose existing deterministic adapters:
    - `ConvertSupportHitResultsToHitRecords`
    - `BuildSupportObservationFromHits`
11. Do not duplicate hull/proxy/support validation/artifact logic.
12. Do not query UWorld, Chaos manifolds, physics scenes, or component overlap state.

## Required Tests

- `PhysAnim.RuntimeAdapter.LiveHitResultObservation`

Required scenarios:
- static blocking `FHitResult` becomes a support hit record
- non-blocking `FHitResult` is ignored
- movable component hit is ignored when world-static filtering is required
- unmapped bone name is ignored
- world-origin rebasing flows through to support observation
- live hit results produce valid support observation
- proxy breach flows from live hit results into support validation

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.LiveHitResultObservation`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportObservationArtifact`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportObservation`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-LIVE-HITRESULT-OBSERVATION-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- live hit-result observation tests pass
- support observation artifact regression test still passes
- support observation regression test still passes
- build passes
- scope check passes
- no runtime enforcement introduced
- no forbidden files touched
- one task commit created
- `execution-log.md` updated through the normal completion path
- handoff block provided

## Stop Conditions

Stop immediately if:
- live Chaos/physics scene queries are needed
- a support-truth math change is needed
- a validator change is needed
- arbitration logic needs modification
- runtime enforcement is attempted
- state-machine files need edits
- behavior unrelated to UE `FHitResult` conversion is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-LIVE-HITRESULT-OBSERVATION-01
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
