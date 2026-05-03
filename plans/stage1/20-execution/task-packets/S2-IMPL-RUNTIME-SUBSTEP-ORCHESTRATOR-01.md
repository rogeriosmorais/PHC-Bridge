# S2-IMPL-RUNTIME-SUBSTEP-ORCHESTRATOR-01 — Deterministic Runtime Substep Orchestrator

## Purpose

Implement a deterministic runtime substep orchestrator that combines already-captured validator outputs and support observations into one canonical artifact-arbitrated termination decision.

This task enables termination decision logic without mutating live runtime state, without editing `PhysAnimComponent`, and without changing the bridge activation state machine.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeOrchestrator.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeOrchestrator.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeOrchestrator.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.LiveHitResultObservation.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportContacts.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportHits.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportHitSnapshot.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportObservation.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.SupportObservationArtifact.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-SUBSTEP-ORCHESTRATOR-01.md`

## Allowed Test Maintenance

The six `PhysAnimRuntimeAdapter.*.Tests.cpp` files are allowed only for UE5 Unity-build helper-name collision cleanup.

Permitted edits in those files:
- rename anonymous-namespace helper functions with file-specific prefixes
- rename local variables if needed to avoid Unity-build ambiguity
- preserve existing test behavior
- preserve existing test names
- preserve existing assertions unless required to fix compile-only naming issues

Forbidden edits in those files:
- changing production logic
- changing validator expectations
- changing support math expectations
- changing arbitration semantics
- deleting regression coverage

## Forbidden Files

- `PhysAnimComponent.h`
- `PhysAnimComponent.cpp`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- `PhysAnimRuntimeAdapter.h`
- `PhysAnimRuntimeAdapter.cpp`
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
- `PhysAnimRuntimeAdapter.h`
- `PhysAnimValidators.h`
- `PhysAnimFailureArbitration.h`

Do not read broad Stage 1 docs by default.

## Required Work

1. Add `FPhysAnimRuntimeSubstepInput`.
2. Add `FPhysAnimRuntimeSubstepResult`.
3. Add `PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep`.
4. Collect terminal candidates from:
   - plant validation
   - capsule validation
   - continuity validation
   - support observation validation
   - authority validation
   - movement reclaim validation
   - shell helper validation
   - controller stability validation
   - additional failure candidates
5. Use `Input.Values.TerminalSubstepTimestamp` as the timestamp for all validators captured during the current substep.
6. Build a canonical `FPhysAnimRunArtifactSnapshot` through `PhysAnimValidators::BuildRunArtifactSnapshot`.
7. Set:
   - `bShouldTerminate`
   - `TerminalReason`
   - `TerminalSubstepTimestamp`
   - `bTerminalFrameArtifactCaptured`
8. Respect `bEnableTermination`.
9. Do not duplicate arbitration logic.
10. Do not mutate live runtime components.
11. If UE5 Unity build helper collisions occur in existing runtime adapter tests, fix helper names using file-specific prefixes only.

## Required Tests

- `PhysAnim.RuntimeOrchestrator.Substep`

Required scenarios:
- clean substep does not terminate
- support failure terminates with `ActivationSupportFailure`
- plant and support failure in same substep terminates with plant reason by rank
- earlier additional candidate wins by temporal precedence
- termination disabled preserves artifact terminal reason but does not terminate
- terminal artifact captured flag is true only when termination is enabled and terminal reason exists
- artifact copies support observation fields

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeOrchestrator.Substep`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.LiveHitResultObservation`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportContacts`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportHits`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportHitSnapshot`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportObservation`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeAdapter.SupportObservationArtifact`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-SUBSTEP-ORCHESTRATOR-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- runtime orchestrator substep tests pass
- live hit-result observation regression test still passes
- support contacts regression test still passes
- support hits regression test still passes
- support hit snapshot regression test still passes
- support observation regression test still passes
- support observation artifact regression test still passes
- build passes
- scope check passes
- no live runtime mutation introduced
- no forbidden production files touched
- no adapter production files touched
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
- adapter production code needs modification
- behavior unrelated to runtime substep decision assembly or Unity-build test helper collision cleanup is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-RUNTIME-SUBSTEP-ORCHESTRATOR-01
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
