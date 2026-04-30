# S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-01 - Activated Standing Locomotion Active Shell

## Purpose

Implement a locomotion-active shell entered only after `LocomotionHandoffCommitted`.

Do not implement real walking yet.
Do not start root motion, PoseSearch selection, policy-driven locomotion, animation-driven locomotion, or trajectory following.

## Classification

Runtime shell / gating implementation.

This is not:
- full locomotion activation
- real locomotion movement
- validators redesign
- adapter redesign
- runtime pipeline rewrite
- support-truth redesign
- artifact arbitration rewrite
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
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTermination.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTermination.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTerminationState.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationState.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTerminationPipeline.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationPipeline.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimFailureArbitration.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimFailureArbitration.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all files not listed under Allowed Files
- all runtime adapter files
- all runtime orchestrator files
- all assets
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files

## Required Work

1. Implement a locomotion-active shell entered only after `LocomotionHandoffCommitted`.
2. Do not implement real walking yet.
3. Do not start root motion, PoseSearch selection, policy-driven locomotion, animation-driven locomotion, or trajectory following.
4. Add runtime-visible shell states:
   - `LocomotionActiveShell`
   - `LocomotionActiveShellDenied`
5. `LocomotionActiveShell` may be entered only when:
   - `LocomotionHandoffCommitted` is active
   - `LocomotionHandoffPreflightPassed` was recorded
   - `LocomotionRequested` was recorded
   - locomotion gate result was allowed
   - movement intent is still present and stable
   - `terminal_reason = None`
   - support hull area `> 0`
   - active support side count `>= 1`
   - support mode is not `Airborne`
   - capsule validation is valid
   - continuity validation is valid
   - standing authority is still preserved
   - physics ownership has not changed
   - stability metrics are finite
6. `LocomotionActiveShellDenied` must be recorded when any shell-entry precondition fails, with the exact denial reason.
7. `LocomotionActiveShell` must preserve standing control behavior for now.
8. `LocomotionActiveShell` must not:
   - change physics ownership
   - release standing authority
   - alter support truth
   - alter capsule validation
   - alter continuity validation
   - alter termination behavior
   - move the character using locomotion control
   - consume PoseSearch output
   - consume policy locomotion output
9. Add logs:
   - `[PhysAnimLocomotion] LOCOMOTION_ACTIVE_SHELL_ENTERED reason=<...>`
   - `[PhysAnimLocomotion] LOCOMOTION_ACTIVE_SHELL_DENIED reason=<...>`
10. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionActiveShell`
11. Required test cases:
   - valid handoff commit with stable movement intent -> `LocomotionActiveShell`
   - no handoff commit -> `LocomotionActiveShellDenied`
   - no preflight pass -> `LocomotionActiveShellDenied`
   - `LocomotionRequestDenied` -> `LocomotionActiveShellDenied`
   - gate denied -> `LocomotionActiveShellDenied`
   - movement intent dropped after commit -> `LocomotionActiveShellDenied`
   - negative support case -> `LocomotionActiveShellDenied`
   - terminal reason present -> `LocomotionActiveShellDenied`
   - support mode `Airborne` -> `LocomotionActiveShellDenied`
   - invalid capsule -> `LocomotionActiveShellDenied`
   - invalid continuity -> `LocomotionActiveShellDenied`
12. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`
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
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Implementation requires real locomotion control.
2. Implementation requires tuning values.
3. Implementation requires changing validators, adapters, pipeline, arbitration, or support truth.
4. Implementation requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
5. LocomotionActiveShell is reached without `LocomotionHandoffCommitted`.
6. LocomotionActiveShell is reached without `LocomotionRequested`.
7. LocomotionActiveShell is reached when the locomotion gate denied.
8. LocomotionActiveShell is reached with `terminal_reason != None`.
9. LocomotionActiveShell is reached with support mode `Airborne`.
10. LocomotionActiveShell is reached with support hull area `<= 0`.
11. LocomotionActiveShell is reached with active support side count `< 1`.
12. LocomotionActiveShell is reached with invalid capsule or invalid continuity.
13. LocomotionActiveShell changes physics ownership or standing authority.
14. LocomotionActiveShell starts locomotion movement.
15. Existing proof/test regression occurs.
16. JSON/audit artifact disagrees with component state.
