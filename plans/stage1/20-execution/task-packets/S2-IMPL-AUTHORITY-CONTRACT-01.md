# S2-IMPL-AUTHORITY-CONTRACT-01 - Pure Authority And Interference Validators

## Purpose

Add value-only authority, movement reclaim, and shell-helper validators for `VALID-04A` through `VALID-04D`, `VALID-06A`, and `VALID-06B` before runtime wiring.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-IMPL-AUTHORITY-CONTRACT-01.md`
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
- `plans/stage1/10-specs/authority_matrix.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Required Work

1. Add value-only authority, movement reclaim, and shell-helper snapshot/result structs.
2. Add deterministic tests for:
   - `VALID-04A`
   - `VALID-04B`
   - `VALID-04C`
   - `VALID-04D`
   - `VALID-06A`
   - `VALID-06B`
3. Implement `ValidateAuthority`, `ValidateMovementReclaim`, and `ValidateShellHelper` over value snapshots only.
4. Map authority contamination to `ActivationAuthorityConflict`.
5. Map movement reclaim to `ActivationMovementReclaim`.
6. Map shell-helper writes to `ActivationShellHelperViolation`.
7. Do not read live runtime state.
8. Do not alter bridge, state-machine, adapter, PhysicsControl, capsule/CMC, or artifact behavior.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.Validators.Authority`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-AUTHORITY-CONTRACT-01.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- all mapped authority/interference tests pass
- build passes
- validators remain pure and value-only
- no runtime behavior file is changed
- scope check passes
- one task commit is created
- `execution-log.md` advances to the next explicitly created task or `none`

## Stop Conditions

Stop immediately if:
- a mapped row cannot be represented with value snapshots
- a runtime include becomes necessary
- a runtime file appears in the diff
- artifact emission is needed
- runtime wiring is needed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-AUTHORITY-CONTRACT-01
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
