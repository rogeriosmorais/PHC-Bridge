# S2-IMPL-RUNTIME-ADAPTER-03 - Pure Continuity Validator

## Purpose

Implement the pure continuity validator over value snapshots and cover `VALID-01A` through `VALID-01D`.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Forbidden Files

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
- `plans/stage1/20-execution/balance_first_tdd_strategy.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Required Work

1. Add failing deterministic tests locally for the `VALID-01A` through `VALID-01D` continuity rows.
2. Implement `PhysAnimValidators::ValidateContinuity` over value snapshots only.
3. Map continuity failures exactly:
   - physics disabled -> `ActivationContinuousSimulationLost`
   - pelvis sleep duration greater than 100 ms -> `ActivationContinuousSimulationLost`
   - body instance loss/topology change -> `ActivationTopologyChange`
   - bookkeeping delta only -> no terminal reason and diagnostic mismatch set
4. Set the physical continuity pass field from the validator result.
5. Treat raw continuity loss as authoritative over bookkeeping mismatch.
6. Keep bookkeeping mismatch diagnostic-only unless another failure exists.
7. Do not read live runtime state.
8. Do not emit artifacts or route terminal state.

## Required Tests

- `VALID-01A`
- `VALID-01B`
- `VALID-01C`
- `VALID-01D`
- suite: `PhysAnim.Validators.Continuity`

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.Validators.Continuity`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-03.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- all `VALID-01*` continuity tests pass
- build passes
- validator remains pure and value-only
- no runtime object dependency is introduced
- no runtime state-machine, bridge activation, or artifact emission file is changed
- scope check passes
- one task commit is created
- `execution-log.md` points to `S2-IMPL-RUNTIME-ADAPTER-04`

## Stop Conditions

Stop immediately if:
- a `VALID-01*` row cannot be represented with value snapshots
- a forbidden include becomes necessary
- terminal precedence beyond continuity is needed
- artifact emission is needed
- runtime adapter capture is needed
- a runtime state-machine or bridge activation file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-03
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
Next task: S2-IMPL-RUNTIME-ADAPTER-04 or blocked
```
