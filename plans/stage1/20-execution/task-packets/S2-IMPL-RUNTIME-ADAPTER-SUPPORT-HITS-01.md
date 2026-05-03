# S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HITS-01 — Convert Hit Records Into Contact Samples

## Purpose

Implement a pure deterministic adapter that converts UE-style hit records into `FPhysAnimSupportContactSample` arrays. This bridges raw collision data into the internal support truth model.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportHits.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HITS-01.md`

## Forbidden Files

- `PhysAnimComponent.*`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- `PhysAnimValidators.*`
- `PhysAnimSupportTruth.*`
- `PhysAnimFailureArbitration.*`
- artifact emission/runtime logging files

## Required Work

1. Add `FPhysAnimSupportBodyMapping`, `FPhysAnimSupportHitRecord`, and `FPhysAnimSupportHitConversionInput` to `PhysAnimRuntimeAdapter.h`.
2. Add `ConvertSupportHitsToContactSamples` to `PhysAnimRuntimeAdapter.h/cpp`.
3. Implement filtering:
   - Ignore hits where `bBlockingHit == false`.
   - Ignore hits where `bFromWorldStatic == false`.
   - Ignore hits whose `BodyName` is not in `SupportBodies`.
4. Implement mapping:
   - Map `BodyName` to `EPhysAnimSupportSide` using `SupportBodies`.
5. Implement planar projection:
   - `X = WorldPositionCm.X - WorldOriginCm.X`
   - `Y = WorldPositionCm.Y - WorldOriginCm.Y`
6. Add `PhysAnimRuntimeAdapter.SupportHits.Tests.cpp`.

## Required Tests

- `PhysAnim.RuntimeAdapter.SupportHits`

Required scenarios:
- SUPPORT-HITS-01: valid world-static blocking hit becomes one contact sample
- SUPPORT-HITS-02: non-blocking hit is ignored
- SUPPORT-HITS-03: non-world-static hit is ignored
- SUPPORT-HITS-04: unmapped body is ignored
- SUPPORT-HITS-05: left/right body mapping is preserved
- SUPPORT-HITS-06: world origin rebasing produces expected planar contact position
- SUPPORT-HITS-07: converted samples feed `CaptureSupportSnapshotFromContacts` successfully

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportHits`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportContacts`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HITS-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- support hit adapter tests pass
- build passes
- scope check passes
- no runtime dependency introduced
- handoff block provided

## Stop Conditions

- same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-SUPPORT-HITS-01
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
