# S2-IMPL-ACTIVATED-STANDING-TO-LOCOMOTION-GATE-01 - Activated Standing To Locomotion Gate

## Purpose

Implement only the gate from `BalanceActive_Standing` to the conceptual locomotion-ready / locomotion-requested state.

Do not implement full locomotion activation yet.

## Classification

Runtime gate / proof-driven locomotion admission.

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
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-TO-LOCOMOTION-GATE-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-ACTIVATED-STANDING-TO-LOCOMOTION-GATE-01.md`
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

1. Implement only the decision gate from `BalanceActive_Standing` to locomotion-requested / locomotion-ready.
2. Do not implement full locomotion activation.
3. Put locomotion-specific gate logic in `PhysAnimComponent.Locomotion.cpp`.
4. The gate may allow locomotion transition only when:
   - `BalanceActive_Standing` is active
   - movement intent is present
   - movement intent remains stable for the configured minimum duration
   - `terminal_reason = None`
   - support hull area `> 0`
   - active support side count `>= 1`
   - capsule validation remains valid
   - continuity validation remains valid
   - perturbation/stability metrics remain within currently accepted bounds
5. The gate must deny locomotion transition when:
   - standing is not active
   - proof is incomplete
   - `terminal_reason != None`
   - support mode is `Airborne`
   - support hull area `<= 0`
   - capsule invalid
   - continuity invalid
   - movement intent is absent or below threshold
   - movement intent has not persisted long enough
6. Add logs:
   - `[PhysAnimLocomotion] LOCOMOTION_GATE_ALLOWED reason=<...>`
   - `[PhysAnimLocomotion] LOCOMOTION_GATE_DENIED reason=<...>`
7. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionGate`
8. Required cases:
   - no movement intent -> denied
   - short movement intent pulse -> denied
   - stable movement intent after standing activation -> allowed
   - negative support case -> denied
   - terminal reason present -> denied
9. Update evidence and execution log.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionGate`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionReadiness`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-TO-LOCOMOTION-GATE-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Gate implementation requires changing validators, adapters, pipeline, arbitration, or support truth.
2. Gate implementation requires full locomotion activation.
3. Gate implementation requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. Locomotion gate allows transition before `BalanceActive_Standing` is reached.
5. Locomotion gate allows transition with `terminal_reason != None`.
6. Locomotion gate allows transition with `SupportHullAreaCm2 <= 0`.
7. Locomotion gate allows transition with invalid capsule or continuity validation.
8. Locomotion gate allows transition from the negative support case.
9. Any existing proof/test regression occurs.
10. JSON/audit artifact disagrees with component state.
