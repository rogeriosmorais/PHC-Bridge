# S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HIT-SNAPSHOT-01 — Convert Support Hit Records Directly Into Support Snapshots

## Purpose

Implement a deterministic adapter that converts support hit records directly into `FPhysAnimSupportContractSnapshot` through the existing hit-record, contact-sample, support-truth, and proxy-adjudication chain.

This task does not query live Chaos state and does not enforce runtime behavior.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportHitSnapshot.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HIT-SNAPSHOT-01.md`

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
- `PhysAnimRuntimeAdapter.SupportHits.Tests.cpp`
- `PhysAnimRuntimeAdapter.SupportContacts.Tests.cpp`

Do not read broad Stage 1 docs by default.

## Required Work

1. Add `FPhysAnimSupportHitSnapshotCaptureInput`.
2. Add `PhysAnimRuntimeAdapter::CaptureSupportSnapshotFromHits`.
3. Implement the function by composing existing adapters:
   - `ConvertSupportHitsToContactSamples`
   - `CaptureSupportSnapshotFromContacts`
4. Copy all temporal, threshold, proxy, COM, support-body, and world-origin fields through the chain.
5. Do not duplicate hit filtering logic outside `ConvertSupportHitsToContactSamples`.
6. Do not duplicate hull/proxy/support-mode logic outside `CaptureSupportSnapshotFromContacts`.
7. Add `PhysAnimRuntimeAdapter.SupportHitSnapshot.Tests.cpp`.
8. Do not query UWorld, Chaos, FBodyInstance, or live collision state here.

## Required Tests

- `PhysAnim.RuntimeAdapter.SupportHitSnapshot`

Required scenarios:
- valid square hit records produce one active side and `SingleFootSurvival`
- world-origin rebasing works through the full hit-to-snapshot path
- no valid hits increments support gap through the full path
- no valid hits beyond gap limit produces `Airborne` and fails `ValidateSupport`
- proxy outside hull beyond limit emits `ActivationProxyOutsideSupportRegion`
- invalid/unmapped/non-world-static hits are ignored through the full path

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportHitSnapshot`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportHits`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportContacts`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HIT-SNAPSHOT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- hit-snapshot tests pass
- support hit regression test still passes
- support contact regression test still passes
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
- runtime enforcement is attempted
- state-machine files need edits
- behavior unrelated to hit-record-to-support-snapshot composition is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HIT-SNAPSHOT-01
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
