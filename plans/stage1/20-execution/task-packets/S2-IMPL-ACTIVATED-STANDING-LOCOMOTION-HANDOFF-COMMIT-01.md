# S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01 - Activated Standing Locomotion Handoff Commit

## Purpose

Add a commit-only latch after locomotion handoff preflight for activated standing.

Do not implement locomotion activation.
Do not widen scope beyond the handoff commit path.

## Classification

Implementation task with proof coverage.

This is not:
- full locomotion activation
- validator redesign
- adapter redesign
- runtime pipeline rewrite
- artifact arbitration rewrite
- support-truth redesign
- asset authoring
- ONNX/model changes
- PoseSearch tuning
- mass tuning
- PhysicsControl redesign

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Locomotion.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all files not listed under Allowed Files
- all runtime adapter files
- all runtime orchestrator files
- all runtime termination files
- all runtime termination-state files
- all runtime termination-pipeline files
- all validators files
- all support-truth files
- all failure-arbitration files
- all assets
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files

## Required Work

1. Add a commit-only handoff latch after preflight for activated standing locomotion.
2. Keep the runtime bridge small and reuse existing runtime evidence.
3. Do not change locomotion activation behavior.
4. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionHandoffCommit`
5. Required commit cases:
   - valid preflight pass with stable intent -> committed
   - no preflight pass -> denied or pending, depending on runtime evidence
   - request denied -> denied
   - gate denied -> denied
   - movement intent dropped after preflight -> denied
   - negative support -> denied
   - terminal reason present -> denied
   - support mode `Airborne` -> denied
   - invalid capsule -> denied
   - invalid continuity -> denied
6. Each case must record:
   - runtime state before/after
   - request state
   - preflight state
   - commit state
   - prior gate result
   - movement intent magnitude
   - movement intent stable duration
   - support mode
   - support hull area
   - active support side count
   - capsule valid
   - continuity valid
   - terminal reason
   - standing authority preserved
   - physics ownership unchanged
   - stability metrics finite
   - commit result
   - denial/allow reason
7. Write proof summary into evidence file.
8. Update `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionHandoffCommit`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Commit requires full locomotion activation.
2. Commit requires tuning values.
3. Commit requires changing validators, adapters, pipeline, arbitration, or support truth.
4. Commit requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
5. Commit state changes locomotion activation behavior.
6. Commit changes physics ownership or standing authority.
7. Commit passes without recording commit result and denial/allow reason.
8. Existing proof/test regression occurs.
9. JSON/audit artifact disagrees with component state.
