# S2-TUNE-ACTIVATED-STANDING-STABILITY-01 — Activated Standing Stability Tuning

## Purpose

Tune activated standing stability using the measured baseline from `S2-MEASURE-ACTIVATED-STANDING-STABILITY-01` without widening scope into adapters, validators, arbitration, or support-truth changes.

## Classification

Tuning / runtime parameter adjustment.

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
- `plans/stage1/20-execution/task-packets/S2-TUNE-ACTIVATED-STANDING-STABILITY-01.md`
- `plans/stage1/20-execution/evidence/S2-TUNE-ACTIVATED-STANDING-STABILITY-01.md`
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

1. Tune only activated standing stability using the measured baseline from `S2-MEASURE-ACTIVATED-STANDING-STABILITY-01`.
2. Do not tune blind.
3. Use the existing runtime-owned standing control settings only.
4. Do not change support geometry, support truth, validator thresholds, runtime adapter behavior, or pipeline behavior.
5. Preserve:
   - `StandingProof.Live` PASS
   - `StandingProof.NegativeSupport` FAIL with `ActivationSupportFailure`
   - `ActivationPath.Wiring` PASS
   - `PIE.G2Presentation` PASS
   - `ActivatedStanding.StabilityMetrics` PASS
6. Record before/after metrics in the evidence file:
   - root/pelvis position drift
   - vertical drift
   - angular drift
   - max body angular speed
   - max body linear speed
   - support hull area min/mean/max
   - active support side count min/mean/max
   - activation duration
   - terminal reason
7. Update `execution-log.md` for `S2-TUNE-ACTIVATED-STANDING-STABILITY-01`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-TUNE-ACTIVATED-STANDING-STABILITY-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. any validator, adapter, pipeline, arbitration, or support-truth file needs to be edited
2. any asset or ONNX file needs to be edited
3. any tuning change would regress `StandingProof.Live`
4. any tuning change would regress `StandingProof.NegativeSupport`
5. any tuning change would regress `ActivationPath.Wiring`
6. any tuning change would regress `PIE.G2Presentation`
7. any tuning change would regress `ActivatedStanding.StabilityMetrics`
8. support hull area regresses to `0.0`
9. the evidence cannot show before/after metrics from the measured baseline

