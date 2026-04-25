# S2-IMPL-CONTROLLER-STABILITY-01 - Pure Controller Stability Validator

## Purpose

Add a value-only controller stability validator for `VALID-05A` through `VALID-05H` before runtime wiring.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-IMPL-CONTROLLER-STABILITY-01.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- runtime adapter files
- PhysicsControl setup files
- capsule/CMC runtime behavior files
- artifact emission files
- workflow/process files other than `execution-log.md`

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Required Work

1. Add value-only controller stability snapshot/result structs.
2. Add deterministic tests for `VALID-05A` through `VALID-05H`.
3. Implement `PhysAnimValidators::ValidateControllerStability` over value snapshots only.
4. Use the named thresholds from `instrumentation_and_acceptance.md` and `balance_first_test_matrix.md`.
5. Do not read live runtime state.
6. Do not alter bridge, state-machine, adapter, PhysicsControl, capsule/CMC, or artifact behavior.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.Validators.ControllerStability`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-CONTROLLER-STABILITY-01.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- all `VALID-05*` controller stability tests pass
- build passes
- validator remains pure and value-only
- no runtime behavior file is changed
- scope check passes
- one task commit is created
- `execution-log.md` advances to the next explicitly created task or `none`

## Stop Conditions

Stop immediately if:
- a `VALID-05*` row cannot be represented with value snapshots
- a runtime include becomes necessary
- a runtime file appears in the diff
- artifact emission is needed
- runtime wiring is needed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-CONTROLLER-STABILITY-01
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
Next task: next explicit packet, blocked, or none
```
