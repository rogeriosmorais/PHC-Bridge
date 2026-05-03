# S2-IMPL-RUNTIME-TERMINATION-PIPELINE-01 — Deterministic Runtime Termination Pipeline

## Purpose

Implement a single deterministic pipeline seam that composes:

1. `PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep`
2. `PhysAnimRuntimeTermination::BuildTerminationCommand`
3. `PhysAnimRuntimeTerminationState::ApplyTerminationCommand`

This task still does not mutate live UE components. It creates the one-call seam that `PhysAnimComponent` can later call.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTerminationPipeline.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationPipeline.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationPipeline.Tests.cpp`

## Forbidden Files

- `PhysAnimComponent.h`
- `PhysAnimComponent.cpp`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- `PhysAnimRuntimeAdapter.h`
- `PhysAnimRuntimeAdapter.cpp`
- `PhysAnimRuntimeOrchestrator.h`
- `PhysAnimRuntimeOrchestrator.cpp`
- `PhysAnimRuntimeTermination.h`
- `PhysAnimRuntimeTermination.cpp`
- `PhysAnimRuntimeTerminationState.h`
- `PhysAnimRuntimeTerminationState.cpp`
- `PhysAnimValidators.h`
- `PhysAnimValidators.cpp`
- `PhysAnimSupportTruth.h`
- `PhysAnimSupportTruth.cpp`
- `PhysAnimFailureArbitration.h`
- `PhysAnimFailureArbitration.cpp`
- artifact emission/runtime logging files
- workflow/process files except `execution-log.md` through the normal task-completion update

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `PhysAnimRuntimeTerminationState.h`
- `PhysAnimRuntimeTermination.h`
- `PhysAnimRuntimeOrchestrator.h`
- `PhysAnimValidators.h`

## Required Work

1. Add `FPhysAnimRuntimeTerminationPipelineInput`.
2. Add `FPhysAnimRuntimeTerminationPipelineResult`.
3. Add `PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline`.
4. The pipeline must:
   - call the orchestrator to get the substep result
   - call the command builder to get the termination command
   - call the state applier to update the termination state
5. Return all three intermediate/final results in the result struct.
6. The pipeline must be pure/deterministic: it takes previous state and input, returns new state and metadata.
7. No mutation of live components.
8. No evaluators or validators logic duplication.
9. No direct UWorld or Chaos queries.

## Required Tests

- `PhysAnim.RuntimeTermination.Pipeline`

Required scenarios:
- clean run (no failure, no termination)
- failure-to-terminal run (failure detected, command built, state terminated)
- idempotent terminal run (already terminated, later failure ignored)
- movement reclaim suppression respects input flag
- termination command suppression respects input flag

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeTermination.Pipeline`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeTermination.State`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-TERMINATION-PIPELINE-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- runtime termination pipeline tests pass
- runtime termination state/command regression tests pass
- build passes
- scope check passes
- no live runtime mutation introduced
- one task commit created
- `execution-log.md` updated through the normal completion path
- handoff block provided

## Stop Conditions

Stop immediately if:
- live component mutation is needed
- `PhysAnimComponent` must be edited
- orchestrator/command/state logic needs modification
- behavior unrelated to deterministic pipeline composition is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-TERMINATION-PIPELINE-01
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
