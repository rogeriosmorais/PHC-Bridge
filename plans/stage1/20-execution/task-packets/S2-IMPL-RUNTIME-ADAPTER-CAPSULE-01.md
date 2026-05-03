# S2-IMPL-RUNTIME-ADAPTER-CAPSULE-01 - Capsule Contract Snapshot Capture

## Purpose

Extend the runtime adapter with snapshot-only capsule contract capture for later Ready-state validation.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-CAPSULE-01.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- validator behavior files
- validator tests
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- workflow/process files other than `execution-log.md`

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/10-specs/character_capsule_contract.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeAdapter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`

## Required Work

1. Add a small capsule snapshot capture input to `PhysAnimRuntimeAdapter`.
2. Convert live capsule, mesh, and CMC component state into `FPhysAnimCapsuleContractSnapshot`.
3. Keep the adapter snapshot-only:
   - no validator enforcement
   - no terminal routing
   - no artifact emission
   - no public runtime-state transition
4. Do not mutate capsule, mesh, CMC, or actor state.
5. Do not alter validators or tests.

## Required Tests

- not applicable; live runtime/editor behavior is not required for this snapshot shell

## Required Build

- `.\\scripts\\build.ps1`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-CAPSULE-01.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- capsule contract runtime adapter snapshot shell compiles
- no enforcement or terminal routing is added
- no runtime state-machine file is changed
- no artifact emission file is changed
- build passes
- scope check passes
- one task commit is created
- `execution-log.md` advances to the next explicitly created task or `none`

## Stop Conditions

Stop immediately if:
- snapshot capture requires editing runtime behavior files
- snapshot capture requires terminal arbitration or artifact emission
- the adapter needs to mutate component state
- a forbidden runtime file appears in the diff
- a new deterministic contract is discovered but not covered by value-only tests
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-ADAPTER-CAPSULE-01
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
