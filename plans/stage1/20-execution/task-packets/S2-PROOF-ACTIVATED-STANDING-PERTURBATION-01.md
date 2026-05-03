# S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01 — Activated Standing Perturbation Proof

## Purpose

Add proof-only perturbation validation for activated standing using the existing runtime evidence path. The perturbation must happen after `BalanceActive_Standing` is reached and stable, and the task must measure how the system recovers without changing tuning.

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
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01.md`
- `plans/stage1/20-execution/evidence/S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01.md`
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

1. Add proof-only perturbation validation for activated standing.
2. Apply one small deterministic perturbation after `BalanceActive_Standing` is reached and stable.
3. Measure:
   - recovery duration
   - max root/pelvis displacement
   - max root/pelvis tilt
   - max body angular speed
   - support hull area min/mean/max
   - active support side count min/mean/max
   - terminal reason
   - whether state remains `BalanceActive_Standing` or transitions safely
4. Do not tune values in this task.
5. Add or extend automation test:
   - `PhysAnim.ActivatedStanding.Perturbation`
6. Test must assert:
   - activation reached before perturbation
   - perturbation was applied once
   - `terminal_reason` is `None` OR a truthful failure reason
   - support hull area remains measured and nonzero before perturbation
   - JSON/audit artifact matches final result
7. Update `execution-log.md` for `S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.RuntimeTermination`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. any validator, adapter, pipeline, arbitration, or support-truth file needs to be edited
2. any asset or ONNX file needs to be edited
3. perturbation validation requires tuning controls
4. activation does not reach `BalanceActive_Standing` before the perturbation
5. perturbation is applied more than once
6. support hull area is `0.0` before perturbation
7. the test passes without asserting terminal reason or truthful failure
8. the JSON/audit artifact disagrees with component state
