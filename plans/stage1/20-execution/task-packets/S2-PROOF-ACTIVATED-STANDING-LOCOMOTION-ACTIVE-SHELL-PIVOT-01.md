# S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-PIVOT-01

## Purpose

Pivot away from proof-routing micro-fixes and mechanically verify that the locomotion-active shell still reaches its deterministic boundary after the latest proxy handoff and proof-failure routing commits.

This is an evidence-only proof packet.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-PIVOT-01.md`
- `plans/stage1/20-execution/evidence/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-PIVOT-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all product/runtime source files
- all tests
- all assets
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files
- all workflow scripts

## Required Work

1. Record `git rev-parse HEAD` as the base.
2. Run the required build and deterministic automation commands.
3. Confirm `PhysAnim.ActivatedStanding.LocomotionActiveShell` passes after the latest proof-routing repairs.
4. Confirm the routing regressions that caused the pivot remain green.
5. Confirm standing live and negative-support proofs remain green.
6. Record concise evidence with command outcomes and the pivot conclusion.
7. Do not change product code or tests in this packet.
8. Update `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProofFailureFailStopRouting`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.RuntimeTermination.Pipeline`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PROOF-ACTIVATED-STANDING-LOCOMOTION-ACTIVE-SHELL-PIVOT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. Any required test fails.
2. Any product-code or test edit appears necessary.
3. The active shell proof regresses after the proof-routing commits.
4. The proof-failure routing regression tests fail.
5. Standing live or negative-support proof regresses.
6. Scope check fails.
