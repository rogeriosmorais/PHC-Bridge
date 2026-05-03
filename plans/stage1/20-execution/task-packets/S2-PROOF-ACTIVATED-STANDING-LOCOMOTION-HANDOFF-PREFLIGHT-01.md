# S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01 - Activated Standing Locomotion Handoff Preflight Proof

## Purpose

Prove the handoff preflight implemented in `S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01`.

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
- `plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01.md`
- `plans/stage1/20-execution/evidence/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01.md`
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

1. Prove the handoff preflight implemented in `S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01`.
2. Do not implement full locomotion activation.
3. Do not tune values.
4. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionHandoffPreflightProof`
5. Required proof cases:
   - `LocomotionRequested` with valid evidence -> `LocomotionHandoffPreflightPassed`
   - `LocomotionRequestDenied` -> `LocomotionHandoffPreflightDenied`
   - no movement intent -> `LocomotionHandoffPreflightDenied`
   - short movement intent pulse -> `LocomotionHandoffPreflightDenied`
   - negative support case -> `LocomotionHandoffPreflightDenied`
   - terminal reason present -> `LocomotionHandoffPreflightDenied`
   - support mode `Airborne` -> `LocomotionHandoffPreflightDenied`
   - support hull area `<= 0` -> `LocomotionHandoffPreflightDenied`
   - active support side count `< 1` -> `LocomotionHandoffPreflightDenied`
   - capsule invalid -> `LocomotionHandoffPreflightDenied`
   - continuity invalid -> `LocomotionHandoffPreflightDenied`
6. Each proof case must record:
   - runtime state before preflight
   - runtime state after preflight
   - request state
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
   - preflight result
   - denial/allow reason
7. Assert `LocomotionHandoffPreflightPassed` does not change:
   - physics ownership
   - standing authority
   - support truth
   - capsule validation behavior
   - continuity validation behavior
   - termination behavior
8. Write proof summary into evidence file.
9. Update `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1`
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
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Proof requires full locomotion activation.
2. Proof requires tuning values.
3. Proof requires changing validators, adapters, pipeline, arbitration, or support truth.
4. Proof requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
5. Handoff preflight passes without `LocomotionRequested`.
6. Handoff preflight passes when the locomotion gate denied.
7. Handoff preflight passes with `terminal_reason != None`.
8. Handoff preflight passes with support mode `Airborne`.
9. Handoff preflight passes with support hull area `<= 0`.
10. Handoff preflight passes with active support side count `< 1`.
11. Handoff preflight passes with invalid capsule or invalid continuity.
12. Handoff preflight changes physics ownership or standing authority.
13. Proof passes without recording preflight result and denial/allow reason.
14. Existing proof/test regression occurs.
15. JSON/audit artifact disagrees with component state.
