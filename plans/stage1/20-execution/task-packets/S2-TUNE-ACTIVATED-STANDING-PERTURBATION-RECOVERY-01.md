# S2-TUNE-ACTIVATED-STANDING-PERTURBATION-RECOVERY-01 — Activated Standing Perturbation Recovery Tuning

## Purpose

Tune only activated-standing perturbation recovery using the measured baseline from `S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01`.

Do not tune blind. Use the perturbation evidence as the baseline.

## Classification

Tuning / runtime recovery.

This is not:
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
- `plans/stage1/20-execution/task-packets/S2-TUNE-ACTIVATED-STANDING-PERTURBATION-RECOVERY-01.md`
- `plans/stage1/20-execution/evidence/S2-TUNE-ACTIVATED-STANDING-PERTURBATION-RECOVERY-01.md`
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
- all locomotion tuning files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files

## Required Work

1. Tune only activated-standing perturbation recovery using the measured baseline from `S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01`.
2. Do not tune blind. Use the perturbation evidence as baseline.
3. Allowed tuning scope:
   - standing recovery control strength multipliers already owned by `PhysAnimComponent`
   - standing recovery damping ratio / extra damping values already owned by `PhysAnimComponent`
   - standing recovery blend/ramp values already owned by `PhysAnimComponent`
   - rate-limited perturbation recovery guard values already owned by `PhysAnimComponent`
4. Do not change:
   - support geometry
   - support truth
   - validator thresholds
   - runtime adapter behavior
   - termination pipeline behavior
   - activation gates
5. Preserve:
   - `StandingProof.Live` PASS
   - `StandingProof.NegativeSupport` FAIL with `ActivationSupportFailure`
   - `ActivationPath.Wiring` PASS
   - `PIE.G2Presentation` PASS
   - `ActivatedStanding.StabilityMetrics` PASS
   - `ActivatedStanding.Perturbation` PASS or truthful FAIL semantics
6. Add before/after metrics to evidence:
   - recovery duration
   - max root/pelvis displacement
   - max root/pelvis tilt
   - max body angular speed
   - max body linear speed
   - support hull area min/mean/max
   - active support side count min/mean/max
   - terminal reason
   - state after recovery
7. If recovery metrics worsen versus baseline, revert the tuning change.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.RuntimeTermination`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-TUNE-ACTIVATED-STANDING-PERTURBATION-RECOVERY-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. No perturbation baseline from `S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01` is available.
2. Tuning requires changing validators, adapters, pipeline, arbitration, or support truth.
3. Tuning requires changing assets, ONNX, PoseSearch, mass profiles, or PhysicsControl design.
4. `SupportHullAreaCm2` regresses to `0.0`.
5. `StandingProof.Live` regresses.
6. `StandingProof.NegativeSupport` stops failing with `ActivationSupportFailure`.
7. `ActivationPath.Wiring` regresses.
8. `PIE.G2Presentation` regresses.
9. `ActivatedStanding.StabilityMetrics` regresses.
10. `ActivatedStanding.Perturbation` no longer reports truthful PASS/FAIL semantics.
11. Recovery metrics worsen versus baseline and cannot be reverted cleanly.
