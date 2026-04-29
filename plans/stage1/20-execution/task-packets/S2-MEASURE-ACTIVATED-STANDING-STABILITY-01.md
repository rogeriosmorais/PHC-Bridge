# S2-MEASURE-ACTIVATED-STANDING-STABILITY-01 — Activated Standing Stability Measurement

## Purpose

Add measurement-only stability telemetry for activated standing and expose it to automated tests without changing tuning or activation semantics.

## Classification

Measurement / telemetry.

This is not:
- tuning
- validator refactoring
- adapter refactoring
- termination pipeline rewrite
- support-truth rewrite
- failure-arbitration rewrite
- activation rewrite
- asset authoring

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-MEASURE-ACTIVATED-STANDING-STABILITY-01.md`
- `plans/stage1/20-execution/evidence/S2-MEASURE-ACTIVATED-STANDING-STABILITY-01.md`
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
- all control tuning files
- all locomotion tuning files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files

## Required Work

1. Add measurement-only stability telemetry for activated standing.
2. Do not tune any values.
3. Measure during `BalanceActive_Standing`:
   - root/pelvis world position drift
   - root/pelvis vertical drift
   - root/pelvis angular drift
   - max body angular speed
   - max body linear speed
   - support hull area min/mean/max
   - active support side count min/mean/max
   - terminal reason
   - fail-stop count
   - activation duration
4. Add a measurement result accessible to automated tests.
5. Extend or add automation test `PhysAnim.ActivatedStanding.StabilityMetrics`.
6. The test must run the activated standing proof for at least 30 seconds and assert:
   - activation reached `BalanceActive_Standing`
   - `terminal_reason = None`
   - `support_hull_area > 0`
   - `active_support_side_count >= 1`
   - no fail-stop
   - metrics are finite and non-empty
7. Write metrics summary into the evidence file.
8. Update `execution-log.md` for `S2-MEASURE-ACTIVATED-STANDING-STABILITY-01`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-MEASURE-ACTIVATED-STANDING-STABILITY-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Definition Of Done

- measurement telemetry is collected during activated standing
- the stability measurement is accessible to automated tests
- the new stability automation test passes
- required standing-proof regression tests pass
- build passes
- scope check passes
- strict workflow check passes
- one task commit is created
- `execution-log.md` is updated

## Stop Conditions

Stop immediately if:
- any tuning value needs to be changed
- any validator, adapter, pipeline, arbitration, or support-truth file needs to be edited
- `SupportHullAreaCm2` regresses to `0.0`
- `StandingProof.Live` regresses
- `StandingProof.NegativeSupport` stops failing with `ActivationSupportFailure`
- `ActivationPath.Wiring` regresses
- `PIE.G2Presentation` regresses
- metrics are fabricated instead of measured from live runtime
- the test passes without asserting activation state and `terminal_reason`

## Required Handoff

```text
Summary:
Task: S2-MEASURE-ACTIVATED-STANDING-STABILITY-01
Base:
Head:
Commit:
Build:
Tests:
Scope:
Workflow:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task:
```
