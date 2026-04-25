# S2-IMPL-RUNTIME-TERMINATION-COMMAND-01 — Deterministic Runtime Termination Command

## Purpose

Implement a deterministic command adapter that converts `FPhysAnimRuntimeSubstepResult` into a runtime-safe termination command.

This task does not mutate live runtime components. It defines the exact command that a later component integration task may apply.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTermination.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTermination.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTermination.Tests.cpp`

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
- `PhysAnimRuntimeOrchestrator.h`
- `PhysAnimValidators.h`

Do not read broad Stage 1 docs by default.

## Required Work

1. Add `FPhysAnimRuntimeTerminationCommand`.
2. Add `FPhysAnimRuntimeTerminationCommandInput`.
3. Add `PhysAnimRuntimeTermination::BuildTerminationCommand`.
4. The command must expose:
   - `bTerminate`
   - `TerminalReason`
   - `TerminalSubstepTimestamp`
   - `bCaptureTerminalArtifact`
   - `bDisablePolicyEvaluation`
   - `bFreezeBridgeOutput`
   - `bRequestPhysicsFailStop`
   - `bRequestMovementReclaim`
   - `Artifact`
5. The command must preserve the full artifact snapshot from the orchestrator result.
6. If `Input.bEnableTerminationCommand == false`, the command must not terminate even if the orchestrator result is terminal.
7. If `Input.bAllowMovementReclaimOnTermination == false`, the command must not request movement reclaim.
8. The command must not mutate live components.
9. Do not duplicate arbitration logic.
10. Do not evaluate validators.
11. Do not query UWorld, Chaos, FBodyInstance, or live collision state.

## Required Tests

- `PhysAnim.RuntimeTermination.Command`

Required scenarios:
- clean orchestrator result produces no termination command
- terminal orchestrator result produces termination command
- disabled termination command preserves artifact but does not terminate
- terminal command requests policy shutdown and bridge output freeze
- terminal command requests physics fail-stop
- movement reclaim flag respects `bAllowMovementReclaimOnTermination`
- command preserves terminal reason, timestamp, and artifact fields

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeTermination.Command`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeOrchestrator.Substep`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-TERMINATION-COMMAND-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- runtime termination command tests pass
- runtime orchestrator regression test still passes
- build passes
- scope check passes
- no live runtime mutation introduced
- no forbidden production files touched
- no forbidden files touched
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
- behavior unrelated to runtime termination command assembly is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-TERMINATION-COMMAND-01
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
