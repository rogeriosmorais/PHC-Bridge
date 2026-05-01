## Purpose
Supersede the blocked proof-failure routing surface and route proof-triggered startup / WaitingForPoseSearch / standing-entry failures through the normal fail-stop side-effect path without widening the failure architecture.

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
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimRuntimeTerminationPipeline.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationPipeline.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeTerminationPipeline.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-FIX-P1N-PROOF-FAILURE-FAILSTOP-ROUTING-PIPELINE-WIDEN-01.md`
- `plans/stage1/20-execution/evidence/S2-FIX-P1N-PROOF-FAILURE-FAILSTOP-ROUTING-PIPELINE-WIDEN-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Locomotion.cpp`
- `PhysAnimRuntimeAdapter.*`
- `PhysAnimRuntimeOrchestrator.*`
- `PhysAnimValidators.*`
- `PhysAnimSupportTruth.*`
- `PhysAnimFailureArbitration.*`
- `assets`
- `ONNX files`
- `PoseSearch tuning files`
- `mass tuning files`
- `PhysicsControl redesign files`

## Required Work
1. Supersede the blocked routing surface from `S2-FIX-P1L-PROOF-FAILURE-FAILSTOP-ROUTING-01`.
2. Fix only proof-triggered fail-stop routing for proof-enabled startup / WaitingForPoseSearch / standing-entry failure.
3. Explicitly widen scope to `PhysAnimRuntimeTerminationPipeline.h/.cpp`.
4. Use `PhysAnimRuntimeTerminationPipeline.*` only to route proof-triggered failures through the same fail-stop side-effect path as normal terminal failures.
5. Locate every proof-triggered path that directly assigns `RuntimeState = FailStopped`.
6. Replace each direct assignment with the normal fail-stop routing path.
7. Preserve original terminal reason, audit terminal reason, runtime termination state, and log-visible terminal reason.
8. Required side effects: deactivate runtime physics control, reset bridge physics state, stop trace session, disable tick when normal fail-stop would disable it.
9. Do not make `ProofNotSatisfied` pass by accepting early `FailStopped`.
10. Do not change `ProofSatisfied`.
11. Do not weaken `StandingProof.Live` or `StandingProof.NegativeSupport`.
12. Add or extend automation covering routed proof failure, no direct `FailStopped`, side effects, preserved terminal reason, `ActivationPath.Wiring`, and `StandingProof.NegativeSupport`.
13. Required logs:
    - `[PhysAnim] Proof failure routed through fail-stop helper reason=<...> state=<...>`
    - `[PhysAnim] Proof failure fail-stop side effects complete reason=<...>`
    - `[PhysAnim] Proof failure terminal reason preserved reason=<...>`
14. Evidence must include the exact scope blocker that required widening, before/after direct-assignment vs helper/path, and proofs of the side effects plus regressions.
15. Update `execution-log.md`.

## Tests
- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.ActivationReview.ProofFailureFailStopRouting`
- `.\scripts\build.ps1 -Test PhysAnim.RuntimeTerminationPipeline`
- `.\scripts\build.ps1 -Test PhysAnim.StandingProof.Live`
- `.\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-FIX-P1N-PROOF-FAILURE-FAILSTOP-ROUTING-PIPELINE-WIDEN-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `.\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions
- Stop if the requested proof-failure routing would require an unlisted file.
- Stop if the proof path starts accepting early `FailStopped`.
- Stop if the normal fail-stop side effects are not preserved.
- Stop after build, tests, scope, commit, evidence, and strict workflow validation are complete.
