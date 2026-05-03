# S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01 - Activated Standing Locomotion Handoff Preflight

## Purpose

Implement a preflight-only handoff check after `LocomotionRequested`.

Do not implement full locomotion activation.

## Classification

Runtime preflight / evidence validation.

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
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01.md`
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

1. Implement a preflight-only handoff check after `LocomotionRequested`.
2. Do not implement full locomotion activation.
3. Do not start walking, root motion, PoseSearch selection, policy-driven locomotion, or animation-driven locomotion.
4. Add runtime-visible handoff preflight result states:
   - `LocomotionHandoffPreflightPassed`
   - `LocomotionHandoffPreflightDenied`
5. Handoff preflight may pass only when:
   - runtime state is `LocomotionRequested`
   - prior gate result is allowed
   - `terminal_reason = None`
   - support hull area `> 0`
   - active support side count `>= 1`
   - support mode is not `Airborne`
   - capsule validation is valid
   - continuity validation is valid
   - standing authority is still preserved
   - physics ownership has not changed
   - current measured stability metrics are finite
6. Handoff preflight must deny when:
   - runtime state is not `LocomotionRequested`
   - prior gate result is denied
   - `terminal_reason != None`
   - support hull area `<= 0`
   - active support side count `< 1`
   - support mode is `Airborne`
   - capsule validation is invalid
   - continuity validation is invalid
   - standing authority was lost
   - physics ownership changed unexpectedly
   - required metrics are missing, `NaN`, or `Inf`
7. Add logs:
   - `[PhysAnimLocomotion] LOCOMOTION_HANDOFF_PREFLIGHT_PASSED reason=<...>`
   - `[PhysAnimLocomotion] LOCOMOTION_HANDOFF_PREFLIGHT_DENIED reason=<...>`
8. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionHandoffPreflight`
9. Required test cases:
   - `LocomotionRequested` with valid evidence -> preflight passed
   - `LocomotionRequestDenied` -> preflight denied
   - no movement intent -> preflight denied
   - short movement intent pulse -> preflight denied
   - negative support case -> preflight denied
   - terminal reason present -> preflight denied
   - invalid capsule -> preflight denied
   - invalid continuity -> preflight denied
10. Update evidence and execution log.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1`
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
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-HANDOFF-PREFLIGHT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Implementation requires full locomotion activation.
2. Implementation requires changing validators, adapters, pipeline, arbitration, or support truth.
3. Implementation requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. Handoff preflight passes without `LocomotionRequested`.
5. Handoff preflight passes when the locomotion gate denied.
6. Handoff preflight passes with `terminal_reason != None`.
7. Handoff preflight passes with support hull area `<= 0`.
8. Handoff preflight passes with active support side count `< 1`.
9. Handoff preflight passes with invalid capsule or invalid continuity.
10. Handoff preflight changes physics ownership or standing authority.
11. Existing proof/test regression occurs.
12. JSON/audit artifact disagrees with component state.
