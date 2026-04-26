# S2-PROOF-LIVE-COMPONENT-EVIDENCE-HOOK-01 — Live Runtime Evidence Hook (Proof)

## Purpose

Implement a non-mutating "Proof of Life" hook in `PhysAnimComponent` that captures real Chaos collision evidence and evaluates it through the deterministic `PhysAnimRuntimeTerminationPipeline`.

This task verifies the pipeline logic against live engine data without modifying the bridge activation state machine or mutating character physics.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`

## Forbidden Files

- `PhysAnimRuntimeTerminationPipeline.h/cpp`
- `PhysAnimRuntimeOrchestrator.h/cpp`
- `PhysAnimRuntimeAdapter.h/cpp`
- `PhysAnimValidators.h/cpp`

## Required Work

1. Add `bEnableLiveRuntimeEvidenceProof` and related sweep settings to `UPhysAnimComponent` header.
2. Implement `TickLiveRuntimeEvidenceProof` to orchestrate:
    - Sweep capture (FHitResult)
    - Observation building
    - Pipeline evaluation
    - Progress logging (LogPhysAnimBridge)
3. Implement `CaptureLiveRuntimeEvidenceHitResults` using `World->SweepSingleByObjectType`.
4. Implement `EmitLiveRuntimeEvidenceTerminalArtifactOnce` to dump high-fidelity evidence on failure.
5. Hook `TickLiveRuntimeEvidenceProof` into `UPhysAnimComponent::TickComponent` (via `PhysAnimComponent.Core.cpp`).
6. Ensure no live state is mutated.

## Required Tests

- None (Live Engine Verification).
- Definition of Done: COMPILING + Correct Log Output in Editor.

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PROOF-LIVE-COMPONENT-EVIDENCE-HOOK-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- `PhysAnimComponent` compiles with new properties and methods.
- Bridge logs show "AttemptStart" and "StandingProgress" when enabled.
- Terminal reason is correctly logged on failure.
- No forbidden files touched.
- Execution log updated.

## Required Handoff

```text
Summary:
Task: S2-PROOF-LIVE-COMPONENT-EVIDENCE-HOOK-01
Base: 8ea0c70
Head:
Commit:
Build:
Tests: n/a (Proof)
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task:
```
