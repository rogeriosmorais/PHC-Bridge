# S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01 - Activated Standing Locomotion Handoff Commit Proof

## Purpose

Prove the handoff commit latch implemented in `S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01`.

Do not implement full locomotion activation.
Do not tune values.

## Classification

Runtime proof / evidence validation.

This is not:
- full locomotion activation
- validators redesign
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
- `plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01.md`
- `plans/stage1/20-execution/evidence/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01.md`
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

1. Prove the handoff commit latch implemented in `S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01`.
2. Do not implement full locomotion activation.
3. Do not tune values.
4. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionHandoffCommitProof`
5. Required proof cases:
   - `LocomotionHandoffPreflightPassed` with stable movement intent -> `LocomotionHandoffCommitted`
   - no preflight pass -> `LocomotionHandoffCommitDenied`
   - `LocomotionRequestDenied` -> `LocomotionHandoffCommitDenied`
   - gate denied -> `LocomotionHandoffCommitDenied`
   - movement intent dropped after preflight -> `LocomotionHandoffCommitDenied`
   - negative support case -> `LocomotionHandoffCommitDenied`
   - terminal reason present -> `LocomotionHandoffCommitDenied`
   - support mode `Airborne` -> `LocomotionHandoffCommitDenied`
   - support hull area `<= 0` -> `LocomotionHandoffCommitDenied`
   - active support side count `< 1` -> `LocomotionHandoffCommitDenied`
   - capsule invalid -> `LocomotionHandoffCommitDenied`
   - continuity invalid -> `LocomotionHandoffCommitDenied`
6. Each proof case must record:
   - runtime state before commit evaluation
   - runtime state after commit evaluation
   - request state
   - prior gate result
   - preflight result
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
7. Assert `LocomotionHandoffCommitted` does not change:
   - physics ownership
   - standing authority
   - support truth
   - capsule validation behavior
   - continuity validation behavior
   - termination behavior
8. Assert `LocomotionHandoffCommitted` does not enter locomotion control.
9. Write proof summary into evidence file.
10. Update `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionHandoffCommitProof`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionHandoffCommit`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionHandoffPreflightProof`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionHandoffPreflight`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionRequestStateProof`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionRequestState`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionGateProof`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionGate`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionReadiness`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-COMMIT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Proof requires full locomotion activation.
2. Proof requires tuning values.
3. Proof requires changing validators, adapters, pipeline, arbitration, or support truth.
4. Proof requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
5. LocomotionHandoffCommitted is reached without LocomotionHandoffPreflightPassed.
6. LocomotionHandoffCommitted is reached without LocomotionRequested.
7. LocomotionHandoffCommitted is reached when the locomotion gate denied.
8. LocomotionHandoffCommitted is reached with `terminal_reason != None`.
9. LocomotionHandoffCommitted is reached with support mode `Airborne`.
10. LocomotionHandoffCommitted is reached with support hull area `<= 0`.
11. LocomotionHandoffCommitted is reached with active support side count `< 1`.
12. LocomotionHandoffCommitted is reached with invalid capsule or invalid continuity.
13. LocomotionHandoffCommitted changes physics ownership or standing authority.
14. LocomotionHandoffCommitted starts locomotion control.
15. Existing proof/test regression occurs.
16. JSON/audit artifact disagrees with component state.
