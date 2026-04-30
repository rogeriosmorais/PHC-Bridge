# S2-FIX-P1A-PROOF-DISABLED-SAFE-DENY-01 - Fix Proof-Disabled Safe-Deny Regression

## Purpose

Fix only the proof-disabled safe-deny bug.

## Classification

Runtime guardrail fix.

This is not:
- fail-stop routing
- capsule/continuity post-entry validation
- locomotion gate/request/preflight/commit/shell
- tuning

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-FIX-P1A-PROOF-DISABLED-SAFE-DENY-01.md`
- `plans/stage1/20-execution/evidence/S2-FIX-P1A-PROOF-DISABLED-SAFE-DENY-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Locomotion.cpp`
- `PhysAnimRuntimeAdapter.*`
- `PhysAnimRuntimeOrchestrator.*`
- `PhysAnimRuntimeTermination.*`
- `PhysAnimRuntimeTerminationState.*`
- `PhysAnimRuntimeTerminationPipeline.*`
- `PhysAnimValidators.*`
- `PhysAnimSupportTruth.*`
- `PhysAnimFailureArbitration.*`
- `assets`
- `ONNX files`
- `PoseSearch tuning files`
- `mass tuning files`
- `PhysicsControl redesign files`

## Required Work

1. Fix only the proof-disabled safe-deny bug.

2. Do not work on:
   - fail-stop routing
   - capsule/continuity post-entry validation
   - locomotion gate/request/preflight/commit/shell
   - tuning

3. In `ShouldExitStandingToSafeDeny`, do not treat default Airborne / zero-support artifacts as terminal when `bEnableLiveRuntimeEvidenceProof` is false.

4. In `BalanceEntry_Settle`, do not route to `BalanceSafeDeny` from stale default proof artifacts when `bEnableLiveRuntimeEvidenceProof` is false.

5. Proof-disabled standing must be allowed to remain in `BalanceActive_Standing` if no other real failure exists.

6. Add or extend automation coverage:
   - proof disabled + standing entry does not immediately safe-deny
   - proof disabled + `BalanceEntry_Settle` does not safe-deny from stale default Airborne / zero-support artifact
   - proof enabled + real no-support still safe-denies as before

7. Evidence must include:
   - before behavior
   - after behavior
   - exact tests run
   - confirmation that proof-enabled negative support behavior still works

8. Update `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivationReview.ProofDisabledSafeDeny`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-FIX-P1A-PROOF-DISABLED-SAFE-DENY-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Fix requires changing runtime adapter, validators, support truth, arbitration, or termination pipeline.
2. Fix requires changing locomotion files.
3. Fix requires tuning values.
4. Proof-disabled mode still transitions to `BalanceSafeDeny` from default Airborne / zero-support artifacts.
5. `BalanceEntry_Settle` still calls the stale proof safe-deny path when proof is disabled.
6. Proof-enabled negative support no longer fails with `ActivationSupportFailure`.
7. `StandingProof.Live` regresses.
8. `ActivationPath.Wiring` regresses.
