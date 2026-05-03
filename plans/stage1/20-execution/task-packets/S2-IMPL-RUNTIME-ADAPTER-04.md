# S2-IMPL-RUNTIME-ADAPTER-04 - Runtime Adapter Snapshot Shell

## Purpose

Add the first runtime adapter snapshot shell that converts live runtime state into the continuity value snapshot without enforcing outcomes.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.Readiness.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.LateValidation.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.Certification.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.PolicyAndShell.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.Diagnostics.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComparisonSubsystem.cpp`
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- validator behavior files
- test C++ files
- workflow/process files

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/20-execution/balance_first_rollout_protocol.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/20-execution/balance_first_refactor_plan.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`

## Required Work

1. Add a small runtime adapter owning surface for snapshot capture.
2. Convert live runtime/body state into `FPhysAnimContinuitySnapshot` values only.
3. Keep the adapter snapshot-only:
   - no validator enforcement
   - no terminal routing
   - no artifact emission
   - no public runtime-state transition
4. Do not rewrite or call the balance-entry state machine.
5. Do not alter PhysicsControl setup, capsule/CMC behavior, support truth logic, or validator behavior.
6. Build only; deterministic validator coverage comes from `S2-IMPL-RUNTIME-ADAPTER-03`.

## Required Tests

- not applicable; live runtime/editor behavior is not required for this snapshot shell

## Required Build

- `.\\scripts\\build.ps1`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-04.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- runtime adapter snapshot shell compiles
- no enforcement or terminal routing is added
- no runtime state-machine file is changed
- no artifact emission file is changed
- build passes
- scope check passes
- one task commit is created
- `execution-log.md` advances to the next explicitly created task or `none`

## Stop Conditions

Stop immediately if:
- snapshot capture requires editing the balance-entry state machine
- snapshot capture requires terminal arbitration or artifact emission
- the adapter needs to tune runtime behavior to pass
- a forbidden runtime file appears in the diff
- a new deterministic contract is discovered but not covered by value-only tests
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-04
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
Next task: next explicit packet, blocked, or none
```
