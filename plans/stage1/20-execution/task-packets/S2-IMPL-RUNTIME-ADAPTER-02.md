# S2-IMPL-RUNTIME-ADAPTER-02 - Continuity Snapshot Semantics Tests

## Purpose

Add deterministic automation tests for continuity snapshot semantics before any runtime adapter or validator behavior is implemented.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- runtime adapter files
- workflow/process files

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`

## Required Work

1. Add a `PhysAnimValidators.Tests.cpp` automation test file.
2. Add deterministic tests for continuity snapshot/result defaults and field semantics.
3. Prove the tests can construct continuity inputs without live runtime objects.
4. Include only:
   - `PhysAnimValidators.h`
   - `Misc/AutomationTest.h`
   - minimal Core headers if required
5. Do not call a continuity validator in this packet.
6. Do not encode runtime adapter behavior.
7. Do not require PIE, a map, skeletal mesh instances, `UWorld`, `UObject`, `FBodyInstance`, PhysicsControl, or Chaos runtime handles.

## Required Tests

- `PhysAnim.Validators.ContinuitySnapshot`

## Required Build

- `.\\scripts\\build.ps1`

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.Validators.ContinuitySnapshot`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-02.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- continuity snapshot semantics tests pass
- build passes
- no production behavior is changed
- no runtime dependency is introduced
- scope check passes
- one task commit is created
- `execution-log.md` points to `S2-IMPL-RUNTIME-ADAPTER-03`

## Stop Conditions

Stop immediately if:
- tests need a live runtime object
- tests need runtime state-machine or bridge activation includes
- a production behavior change appears necessary
- the test harness is not discoverable
- a matrix expectation cannot be represented with value snapshots
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-02
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
Next task: S2-IMPL-RUNTIME-ADAPTER-03 or blocked
```
