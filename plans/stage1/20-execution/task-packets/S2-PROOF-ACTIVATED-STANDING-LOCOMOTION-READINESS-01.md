# S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-READINESS-01 — Activated Standing Locomotion Readiness Proof

## Purpose

Add proof-only locomotion-readiness validation from the already-activated standing state.

Do not implement locomotion activation yet.

## Classification

Proof / runtime validation.

This is not:
- tuning
- support-truth redesign
- validator refactoring
- adapter refactoring
- termination pipeline rewrite
- artifact arbitration rewrite
- asset authoring
- ONNX/model changes

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-READINESS-01.md`
- `plans/stage1/20-execution/evidence/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-READINESS-01.md`
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

1. Add proof-only locomotion-readiness validation from the already-activated standing state.
2. Do not implement locomotion activation yet.
3. Apply a small deterministic movement-intent signal after `BalanceActive_Standing` is reached and stable.
4. Measure:
   - whether standing state remains valid during intent
   - root/pelvis displacement
   - root/pelvis tilt
   - max body angular speed
   - support hull area min/mean/max
   - active support side count min/mean/max
   - terminal reason
   - whether locomotion transition would be allowed or denied by current evidence
5. Do not tune values in this task.
6. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.LocomotionReadiness`
7. Test must assert:
   - activation reached `BalanceActive_Standing` before movement intent
   - movement intent was applied once
   - `terminal_reason` is `None` OR a truthful failure reason
   - support hull area remains measured and nonzero before intent
   - current system does not enter unsupported locomotion state
   - JSON/audit artifact matches final result
8. Update `execution-log.md` for `S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-READINESS-01`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionReadiness`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-READINESS-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Locomotion readiness requires changing validators, adapters, pipeline, arbitration, or support truth.
2. Locomotion readiness requires tuning controls.
3. Locomotion readiness requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. Activation does not reach `BalanceActive_Standing` before movement intent.
5. Movement intent is applied more than once.
6. `SupportHullAreaCm2` is `0` before movement intent.
7. Test passes without asserting `terminal_reason` or truthful failure.
8. System enters a locomotion state without an explicit locomotion transition task.
9. JSON/audit artifact disagrees with component state.
