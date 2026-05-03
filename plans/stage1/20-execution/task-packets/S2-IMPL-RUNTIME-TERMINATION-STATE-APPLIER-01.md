# S2-IMPL-RUNTIME-TERMINATION-STATE-APPLIER-01 — Deterministic Runtime Termination State Applier

## Purpose

Implement a deterministic state applier that converts `FPhysAnimRuntimeTerminationCommand` into component-owned runtime termination state.

This task still does not mutate live UE components. It creates the exact state transition that the later `PhysAnimComponent` integration will call.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTerminationState.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationState.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationState.Tests.cpp`

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
- `PhysAnimRuntimeTermination.h`
- `PhysAnimRuntimeOrchestrator.h`
- `PhysAnimValidators.h`

## Required Work

1. Add `FPhysAnimRuntimeTerminationState`.
2. Add `FPhysAnimRuntimeTerminationStateApplyInput`.
3. Add `FPhysAnimRuntimeTerminationStateApplyResult`.
4. Add `PhysAnimRuntimeTerminationState::ApplyTerminationCommand`.
5. The state must track: `bTerminated`, `TerminalReason`, `TerminalSubstepTimestamp`, `bTerminalFrameArtifactCaptured`, `bPolicyEvaluationEnabled`, `bBridgeOutputFrozen`, `bPhysicsFailStopRequested`, `bMovementReclaimRequested`, `TerminalArtifact`.
6. Applying a non-terminal command must preserve previous state except for copying the latest non-terminal artifact.
7. Applying a terminal command must: set `bTerminated`, copy terminal reason and timestamp, copy terminal artifact, set policy/bridge/fail-stop/reclaim flags according to the command.
8. Applying a terminal command to an already terminated state must be idempotent: keep the first terminal reason, timestamp, and artifact; report `bIgnoredBecauseAlreadyTerminated = true`.
9. Do not mutate live components.
10. Do not duplicate command construction logic.
11. Do not evaluate validators.
12. Do not query UWorld, Chaos, FBodyInstance, or live collision state.

## Required Tests

- `PhysAnim.RuntimeTermination.State`

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeTermination.State`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeTermination.Command`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-TERMINATION-STATE-APPLIER-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- runtime termination state tests pass
- runtime termination command regression test still passes
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
- validator logic needs modification
- arbitration logic needs modification
- support-truth math needs modification
- orchestrator production code needs modification
- behavior unrelated to runtime termination state application is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-TERMINATION-STATE-APPLIER-01
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
