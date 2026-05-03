# S2-IMPL-PLANT-CONTRACT-01 - Pure Plant Contract Validator

## Purpose

Add a value-only plant admissibility validator for `VALID-03A` through `VALID-03D` before any runtime wiring.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-IMPL-PLANT-CONTRACT-01.md`
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
- `plans/stage1/10-specs/physics_asset_contract.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Required Work

1. Add value-only plant contract snapshot/result structs.
2. Add deterministic tests for:
   - `VALID-03A`
   - `VALID-03B`
   - `VALID-03C`
   - `VALID-03D`
3. Implement `PhysAnimValidators::ValidatePlant` over value snapshots only.
4. Map every plant contract violation to `ActivationPhysicsAssetContractViolation`.
5. Do not read live runtime state.
6. Do not alter bridge, state-machine, adapter, PhysicsControl, capsule/CMC, or artifact behavior.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.Validators.Plant`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-PLANT-CONTRACT-01.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- all `VALID-03*` plant tests pass
- build passes
- validator remains pure and value-only
- no runtime behavior file is changed
- scope check passes
- one task commit is created
- `execution-log.md` advances to the next explicitly created task or `none`

## Stop Conditions

Stop immediately if:
- a `VALID-03*` row cannot be represented with value snapshots
- a runtime include becomes necessary
- a runtime file appears in the diff
- artifact emission is needed
- Ready-state wiring is needed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-PLANT-CONTRACT-01
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
