# S2-IMPL-RUNTIME-ADAPTER-SUPPORT-01 — Runtime Support Snapshot Capture

## Purpose

Capture runtime support snapshot inputs from Unreal Engine data and feed them into the existing pure support validator.

This task bridges live Unreal data into the validator-facing snapshot format.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.Support.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-01.md`

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

1. Add `FPhysAnimSupportSnapshotCaptureInput` to `PhysAnimRuntimeAdapter.h`.
2. Add `CaptureSupportSnapshot` to `PhysAnimRuntimeAdapter.h/cpp`.
3. Map all input fields from `FPhysAnimSupportSnapshotCaptureInput` to `FPhysAnimSupportContractSnapshot`.
4. Ensure `ProxyTerminalReason` is passed through.
5. Ensure `TOptional` fields are preserved.
6. Add `PhysAnimRuntimeAdapter.Support.Tests.cpp`.

## Required Tests

- `PhysAnim.RuntimeAdapter.Support`

Required scenarios:
- SUPPORT-ADAPTER-01: copies left/right support states
- SUPPORT-ADAPTER-02: copies support hull area and active side count
- SUPPORT-ADAPTER-03: preserves nullable proxy fields
- SUPPORT-ADAPTER-04: passes proxy terminal reason through
- SUPPORT-ADAPTER-05: captured snapshot validates through ValidateSupport

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.Support`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-SUPPORT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- support adapter tests pass
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
Task: S2-IMPL-RUNTIME-ADAPTER-SUPPORT-01
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
