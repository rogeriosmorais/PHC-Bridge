# S2-IMPL-RUNTIME-ADAPTER-01 - Value-Only Runtime Snapshot Structs

## Purpose

Introduce the value-only validator snapshot structs needed before any runtime adapter reads live Unreal state.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- runtime adapter files
- test C++ files
- workflow/process files

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/20-execution/balance_first_tdd_strategy.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/20-execution/balance_first_refactor_plan.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimTruthTypes.h`

## Required Work

1. Add `PhysAnimValidators.h` and `PhysAnimValidators.cpp` as compile-only pure validator scaffolding.
2. Define value-only continuity snapshot/result structs for adapter-fed validators.
3. Keep field names traceable to artifact fields where practical:
   - topology change count
   - continuity bookkeeping mismatch
   - pelvis sleep duration in milliseconds
   - physical continuity validator passed
   - terminal reason
4. Use only value types and the existing `EPhysAnimTerminalReason` from `PhysAnimTruthTypes.h`.
5. Do not read live runtime state.
6. Do not include `UWorld`, `UObject`, `FBodyInstance`, components, actors, PhysicsControl, or Chaos runtime handles.
7. Do not implement validator behavior in this packet.

## Required Tests

- not applicable; compile/build proof only

## Required Build

- `.\\scripts\\build.ps1`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-01.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- value-only validator scaffold compiles
- continuity snapshot/result structs exist
- no runtime object dependency is introduced
- no tests or runtime files are changed
- build passes
- scope check passes
- one task commit is created
- `execution-log.md` points to `S2-IMPL-RUNTIME-ADAPTER-02`

## Stop Conditions

Stop immediately if:
- a snapshot field requires a live runtime object instead of a value
- a forbidden include becomes necessary
- the build requires module dependency changes outside this packet
- validator behavior must be implemented to compile
- a runtime state-machine or bridge activation file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-01
Base:
Head:
Commit:
Build:
Tests: not applicable
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task: S2-IMPL-RUNTIME-ADAPTER-02 or blocked
```
