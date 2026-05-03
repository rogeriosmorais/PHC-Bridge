# S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01 - Activated Standing Locomotion Request State Proof

## Purpose

Prove the locomotion request state implemented in `S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01`.

Do not implement full locomotion activation.

## Classification

Runtime request-state proof / evidence validation.

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
- `plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01.md`
- `plans/stage1/20-execution/evidence/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01.md`
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

1. Prove the locomotion request state using runtime evidence.
2. Do not implement full locomotion activation.
3. Do not tune values.
4. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionRequestStateProof`
5. Required proof cases:
   - stable movement intent after `BalanceActive_Standing` -> `LocomotionRequested`
   - no movement intent -> `LocomotionRequestDenied`
   - short movement intent pulse -> `LocomotionRequestDenied`
   - negative support case -> `LocomotionRequestDenied`
   - terminal reason present -> `LocomotionRequestDenied`
   - capsule invalid -> `LocomotionRequestDenied`
   - continuity invalid -> `LocomotionRequestDenied`
6. Each case must record:
   - runtime state before request evaluation
   - runtime state after request evaluation
   - movement intent magnitude
   - movement intent stable duration
   - support mode
   - support hull area
   - active support side count
   - capsule valid
   - continuity valid
   - terminal reason
   - gate result
   - request state
   - denial/allow reason
7. Assert `LocomotionRequested` does not change:
   - physics ownership
   - standing authority
   - support truth
   - capsule validation behavior
   - continuity validation behavior
   - termination behavior
8. Write proof summary into evidence file.
9. Update execution-log.md.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1`
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
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Proof requires implementing full locomotion activation.
2. Proof requires changing validators, adapters, pipeline, arbitration, or support truth.
3. Proof requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. `LocomotionRequested` can be entered without `BalanceActive_Standing`.
5. `LocomotionRequested` can be entered when the locomotion gate denies.
6. `LocomotionRequested` can be entered with `terminal_reason != None`.
7. `LocomotionRequested` changes physics ownership or standing authority.
8. `LocomotionRequestDenied` does not preserve the exact denial reason.
9. Any existing proof/test regression occurs.
10. JSON/audit artifact disagrees with component state.
