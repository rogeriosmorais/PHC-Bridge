# S2-IMPL-FAILURE-ARBITRATION-01 — Pure Terminal Failure Arbitration

## Purpose

Implement deterministic terminal-reason arbitration from already-captured failure candidates.

This task is Layer 2.5 pure logic. It does not read live Unreal runtime objects and does not enforce runtime behavior.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimFailureArbitration.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimFailureArbitration.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimFailureArbitration.Tests.cpp`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- `PhysAnimValidators.h`
- `PhysAnimValidators.cpp`
- `PhysAnimRuntimeAdapter.h`
- `PhysAnimRuntimeAdapter.cpp`
- workflow/process files except `execution-log.md` through the normal task-completion update

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`

Do not read broad Stage 1 docs by default.

## Required Tests

- `ARBIT-01`
- `ARBIT-02`
- `ARBIT-03`
- `ARBIT-04`
- `ARBIT-05`

## Required Work

1. Add a pure value-only arbitration module:
   - `PhysAnimFailureArbitration.h`
   - `PhysAnimFailureArbitration.cpp`
   - `PhysAnimFailureArbitration.Tests.cpp`
2. Define `FPhysAnimFailureCandidate`.
3. Define `FPhysAnimFailureArbitrationResult`.
4. Implement rank lookup for every canonical `EPhysAnimTerminalReason`.
5. Implement `ArbitrateFailure`.
6. Apply temporal precedence first:
   - earliest `TerminalSubstepTimestamp` wins.
7. Apply rank precedence only for simultaneous candidates:
   - lower rank number wins when timestamps are equal.
8. Record all other candidates from the winning timestamp in `CoTerminalReasons`.
9. Ignore `EPhysAnimTerminalReason::None`.
10. Do not edit validator, adapter, runtime, or artifact-emission files.

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.FailureArbitration`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-FAILURE-ARBITRATION-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- all mapped arbitration tests pass
- build passes
- scope check passes
- no runtime dependency introduced
- no forbidden files touched
- one task commit created
- `execution-log.md` updated through the normal completion path
- handoff block provided

## Stop Conditions

Stop immediately if:
- arbitration requires live runtime data
- a mapped test cannot be written from the matrix
- a new terminal reason is needed
- timestamp/rank precedence conflicts with `continuous_balance_truth_model.md`
- behavior for validator or runtime adapter files is added
- runtime enforcement is attempted
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-FAILURE-ARBITRATION-01
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
