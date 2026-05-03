# S2-IMPL-RUNTIME-ADAPTER-SUPPORT-CONTACTS-01 — Convert Contact Samples Into Support Snapshot

## Purpose

Implement a pure deterministic contact-sample adapter that converts runtime-like contact samples into `FPhysAnimSupportContractSnapshot` using existing `SupportTruth` math.

This bridges raw contact detections into the high-level support contract validator.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportContacts.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-CONTACTS-01.md`

## Forbidden Files

- `PhysAnimComponent.*`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- `PhysAnimValidators.*`
- `PhysAnimSupportTruth.*`
- `PhysAnimFailureArbitration.*`
- artifact emission/runtime logging files
- workflow/process files except `execution-log.md` through the normal task-completion update

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`

## Required Work

1. Add `FPhysAnimSupportContactSample` to `PhysAnimRuntimeAdapter.h`.
2. Add `FPhysAnimSupportContactsSnapshotCaptureInput` to `PhysAnimRuntimeAdapter.h`.
3. Add `CaptureSupportSnapshotFromContacts` to `PhysAnimRuntimeAdapter.h/cpp`.
4. Group valid contact samples by BodyName + SupportSide.
5. Convert each group into `FPhysAnimSupportPatch` via `ExtractPatchHull`.
6. Build frame hull via `BuildFrameHull`.
7. Derive `bSupportStateL` / `bSupportStateR` from valid non-empty patches.
8. Update `SupportGapTimerMs` (0.0 if any side active, else increment).
9. Classify `SupportMode` via `ClassifySupportMode`.
10. Run `AdjudicateProxy`.
11. Add `PhysAnimRuntimeAdapter.SupportContacts.Tests.cpp`.

## Required Tests

- `PhysAnim.RuntimeAdapter.SupportContacts`

Required scenarios:
- SUPPORT-CONTACTS-01: left-foot square contacts -> one active side, positive area, SingleFootSurvival
- SUPPORT-CONTACTS-02: left + right contacts -> two active sides, TwoFootStable
- SUPPORT-CONTACTS-03: no contacts -> increment support gap, TransientRecovery under limit
- SUPPORT-CONTACTS-04: no contacts beyond gap limit -> Airborne, ValidateSupport fails
- SUPPORT-CONTACTS-05: proxy inside hull -> reset outside timer
- SUPPORT-CONTACTS-06: proxy outside hull beyond limit -> ActivationProxyOutsideSupportRegion
- SUPPORT-CONTACTS-07: invalid contacts are ignored

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportContacts`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.Support`
- `.\scripts\build.ps1 -Test PhysAnim.Validators.Support`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-CONTACTS-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- support contact adapter tests pass
- build passes
- scope check passes
- no runtime dependency introduced (beyond existing adapter dependencies)
- no forbidden files touched
- one task commit created
- `execution-log.md` updated through the normal completion path
- handoff block provided

## Stop Conditions

Stop immediately if:
- capture requires complex new geometry math (should use existing support truth)
- runtime enforcement is attempted
- same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-SUPPORT-CONTACTS-01
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
