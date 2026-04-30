# S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01 - Activated Standing Locomotion Request State

## Purpose

Implement the state wiring that records a locomotion request after the proven locomotion gate allows transition.

Do not implement full locomotion control yet.

## Classification

Runtime request-state wiring / proof-driven locomotion admission.

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
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnim.SmokeTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01.md`
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

1. Implement the state wiring that records a locomotion request after the proven locomotion gate allows transition.
2. Do not implement full locomotion control yet.
3. Do not start walking, root motion, PoseSearch selection, policy-driven locomotion, or animation-driven locomotion.
4. Add an explicit runtime-visible state or substate representing:
   - `BalanceActive_Standing`
   - `LocomotionRequested`
   - `LocomotionRequestDenied`
5. `LocomotionRequested` may be entered only when:
   - `BalanceActive_Standing` is active
   - locomotion gate result is allowed
   - `terminal_reason = None`
   - support hull area `> 0`
   - active support side count `>= 1`
   - capsule validation is valid
   - continuity validation is valid
6. `LocomotionRequestDenied` must be recorded when the gate denies, with the exact denial reason.
7. `LocomotionRequested` must preserve standing authority and must not change physics ownership, support truth, capsule rules, continuity rules, or termination behavior.
8. Add logs:
   - `[PhysAnimLocomotion] LOCOMOTION_REQUESTED reason=<...>`
   - `[PhysAnimLocomotion] LOCOMOTION_REQUEST_DENIED reason=<...>`
9. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionRequestState`
10. Required test cases:
   - stable movement intent after `BalanceActive_Standing` -> `LocomotionRequested`
   - no movement intent -> `LocomotionRequestDenied`
   - short movement intent pulse -> `LocomotionRequestDenied`
   - negative support case -> `LocomotionRequestDenied`
   - terminal reason present -> `LocomotionRequestDenied`
   - invalid capsule -> `LocomotionRequestDenied`
   - invalid continuity -> `LocomotionRequestDenied`
11. Update evidence and execution log.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionRequestState`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionGate`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionReadiness`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Request-state wiring requires changing validators, adapters, pipeline, arbitration, or support truth.
2. Request-state wiring requires full locomotion activation.
3. Request-state wiring requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. Request state allows without `BalanceActive_Standing`.
5. Request state allows with no movement intent.
6. Request state allows with short movement intent below minimum duration.
7. Request state allows with `terminal_reason != None`.
8. Request state allows with `support hull area <= 0`.
9. Request state allows with invalid capsule or invalid continuity validation.
10. Request state allows from the negative support case.
11. Any existing proof/test regression occurs.
12. JSON/audit artifact disagrees with component state.
