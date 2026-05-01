# S2-FIX-P1M-PROOF-FAILURE-FAILSTOP-ROUTING-SURFACE-WIDEN-01

## Purpose
- Fix proof-triggered fail-stop routing without redesigning termination.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.StartStop.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTermination.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTermination.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTerminationState.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationState.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-FIX-P1M-PROOF-FAILURE-FAILSTOP-ROUTING-SURFACE-WIDEN-01.md`
- `plans/stage1/20-execution/evidence/S2-FIX-P1M-PROOF-FAILURE-FAILSTOP-ROUTING-SURFACE-WIDEN-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Locomotion.cpp`
- `PhysAnimRuntimeAdapter.*`
- `PhysAnimRuntimeOrchestrator.*`
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
1. Fix only the proof-failure fail-stop routing issue.
2. Do not work on proof-disabled safe-deny, startup proxy handoff timing, proof gate truth criteria, support truth generation, post-entry capsule/continuity hardening, locomotion chain, or tuning.
3. Locate every proof-enabled startup / WaitingForPoseSearch / standing-entry branch that directly assigns `RuntimeState = FailStopped`.
4. Replace direct `RuntimeState = FailStopped` assignment with the existing fail-stop helper/path used by normal terminal failures.
5. Preserve the original terminal reason and keep fail-stop side effects intact.
6. Do not make `ProofNotSatisfied` pass by accepting early `FailStopped`.
7. Do not change `ProofSatisfied` behavior.
8. Do not weaken `StandingProof.Live` or `StandingProof.NegativeSupport`.
9. Add or extend automation coverage for routed proof fail-stop, preserved terminal reason, and regression coverage for `ProofSatisfied`, `ProofNotSatisfied`, and `StandingProof.NegativeSupport`.
10. Update `execution-log.md`.

## Tests
- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProofFailureFailStopRouting`
- `.\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring`
- `.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProofSatisfiedProxyHandoffSourceOfTruthProof`
- `.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProofSatisfiedProxyHandoffSourceOfTruth`
- `.\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `.\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-FIX-P1M-PROOF-FAILURE-FAILSTOP-ROUTING-SURFACE-WIDEN-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `.\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions
- Fix requires changing locomotion files.
- Fix requires changing runtime adapter, support truth, validators, failure arbitration, orchestrator, or termination pipeline.
- Fix requires tuning values.
- Any proof-triggered startup or standing-entry failure still assigns `RuntimeState = FailStopped` directly.
- Routed fail-stop does not deactivate runtime physics control.
- Routed fail-stop does not reset bridge physics state.
- Routed fail-stop does not stop trace session.
- Routed fail-stop loses or rewrites the terminal reason.
- `ProofSatisfied` regresses.
- `ProofNotSatisfied` no longer observes `WaitingForPoseSearch` before fail-stop.
- `StandingProof.Live` regresses.
- `StandingProof.NegativeSupport` no longer fails with `ActivationSupportFailure`.
- JSON/audit artifact disagrees with component state.
