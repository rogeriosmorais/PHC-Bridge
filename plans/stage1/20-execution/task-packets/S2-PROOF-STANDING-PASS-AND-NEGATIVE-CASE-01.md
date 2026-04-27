# Task Packet: S2-PROOF-STANDING-PASS-AND-NEGATIVE-CASE-01

## Purpose
Prove two things in automation:
1. Positive path: StandingProof.Live reaches >= 3.0s with terminal_reason=None.
2. Forced negative path: A deliberate support/capsule/continuity violation fails with ActivationSupportFailure.

## Allowed Files
- PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h
- PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp
- PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp
- plans/stage1/20-execution/task-packets/S2-PROOF-STANDING-PASS-AND-NEGATIVE-CASE-01.md
- plans/stage1/20-execution/evidence/S2-PROOF-STANDING-PASS-AND-NEGATIVE-CASE-01.md
- plans/stage1/20-execution/execution-log.md

## Forbidden Files
- PhysAnimRuntimeAdapter.*
- PhysAnimRuntimeOrchestrator.*
- PhysAnimRuntimeTermination.*
- PhysAnimRuntimeTerminationState.*
- PhysAnimRuntimeTerminationPipeline.*
- PhysAnimValidators.*
- PhysAnimSupportTruth.*
- PhysAnimFailureArbitration.*
- control tuning
- locomotion tuning
- PoseSearch tuning
- mass tuning
- PhysicsControl redesign
- assets

## Required Work
1. Positive path: StandingProof.Live reaches >= 3.0s with terminal_reason=None, support_hull_area > 0, support_mode=TwoFootStable or SingleFootSurvival, JSON audit artifact validated.
2. Negative path: Implement a new test `PhysAnim.StandingProof.NegativeSupport` that forces support loss (e.g., by making foot sweeps miss) and asserts `ActivationSupportFailure`.
3. Vocabulary Update: Rename "None success output" to "success audit artifact" in code/comments.

## Definition of Done
- Positive path PASSED with explicit assertions.
- Negative path FAILED with ActivationSupportFailure as expected.
- Audit JSON and Failure JSON exist and are validated.
- All required tests in AGENTS.md pass.

## Stop Conditions
- Positive path only passes because assertions are weak.
- Negative path does not fail.
- Negative path fails with an invented/manual reason.
- JSON disagrees with component state.
- Support hull area regresses to 0 in the positive path.
- Negative case requires validator/adapter/pipeline changes.
- Activation path wiring becomes necessary.
