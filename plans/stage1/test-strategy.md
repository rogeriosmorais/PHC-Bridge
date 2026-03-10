# Stage 1 Test Strategy

## Purpose

This document defines how Stage 1 claims are validated across unit tests, integration tests, UE5 automation, and manual checks. It keeps TDD as the implementation rule while recognizing that not every Stage 1 gate is fully automatable.

## Validation Layers

Stage 1 validation is split into four layers:

1. **Python/unit tests** for training-side conversion, export, and data-shape logic
2. **Bridge/integration tests** for observation packing, action unpacking, and retargeting correctness
3. **UE5 automation or scripted checks** for runtime control-path behavior where automation is practical
4. **Manual ELI5 verification** for motion quality, editor-only setup, and gate decisions requiring human judgment

## Test Ownership By Area

| Area | Primary Mode | Secondary Mode | Manual Check Needed |
|---|---|---|---|
| SMPL mapping tables | Python/unit | static review | only if transforms remain ambiguous |
| Coordinate conversion | Python/unit | bridge smoke test | yes for visual confirmation in UE5 |
| PHC observation packing | Python/unit or bridge unit | integration review | no if config is confirmed |
| PHC action unpacking | Python/unit or bridge unit | integration review | no if output ranges are known |
| ONNX export shape validity | Python/unit | runtime load check | no |
| NNE model loading in UE5 | UE5 scripted/runtime check | manual confirmation | yes if automation is absent |
| Physics Control response | UE5 scripted/runtime check | manual confirmation | yes |
| Chaos substep stability | UE5 runtime check | manual confirmation | yes |
| End-to-end Manny smoke test | integration + manual | none | yes |
| G2 quality comparison | manual | evidence review | yes |
| G3 demo quality decision | manual | observer notes | yes |

## Stage 1 Test Matrix

### Phase 0

| Claim | Verification Mode | Evidence |
|---|---|---|
| PHC baseline produces plausible locomotion | manual (`MV-G1-01`) | clip + pass/fail note |
| chosen ProtoMotions config is understood | static review | bridge spec updated with confirmed fields |
| SMPL mapping table is coherent | unit/static review | mapping review + validation cases |
| NNE dummy model loads | UE5 runtime check | runtime log or screenshot |
| Physics Control responds to targets | UE5 runtime + manual (`MV-G1-02`) | clip + log or screenshot |
| substep rate is stable | UE5 runtime + manual | clip + short stability note |
| minimal PHC-style output can drive Manny | integration + manual (`MV-G1-03`) | clip + pose note |

### Phase 1

| Claim | Verification Mode | Evidence |
|---|---|---|
| plugin bridge writes correct observations and outputs | unit/integration | tests plus code review |
| PHC ONNX runs through NNE in UE5 | runtime/automation if available | logs and test result |
| PoseSearch and PHC bridge cooperate on one character | integration | runtime capture |
| PD tuning is stable enough to compare quality | runtime + manual | comparison capture |
| physics-driven motion looks noticeably better than kinematic | manual (`MV-G2-01`) | side-by-side verdict |

### Phase 2

| Claim | Verification Mode | Evidence |
|---|---|---|
| locomotion showcase packaging is clear enough to judge | integration + runtime | demo capture |
| final demo is compelling enough for further investment | manual (`MV-G3-01`) | observer notes + final decision |

## Mixed Validation Rule

Stage 1 uses a mixed strategy on purpose:

- strict TDD for deterministic bridge logic
- runtime or automation checks for live UE system integration
- manual verification for subjective quality and gate decisions

The implementation rule is:

- if a behavior can be expressed as pure math, fixed packing, table lookup, fixed-shape validation, or deterministic conversion, it must be written test-first
- if a behavior depends on live UE runtime state, editor-authored assets, physics stability, or visual judgment, it is not required to be written test-first but it still needs scripted/runtime/manual evidence

This is not optional guidance. Phase 1 implementation must obey it.

## Deterministic Logic That Must Be TDD-First

Before implementation code is added, tests must be written first for any new deterministic bridge logic in Python or C++.

This includes:

- SMPL joint ordering and Stage 1 mapped-subset tables
- Manny target-body and control-name lookup tables derived from the locked mapping
- `69 -> 23 x 3` action grouping
- the frozen PD mapping `pd_target_i = pi * action_i`
- wrist-plus-hand collapse composition
- basis and frame-conversion helpers between SMPL and UE
- future sample schedule generation (`15` steps at `1/30` seconds) and end-of-clip clamping
- fixed tensor descriptor name-to-index mapping
- fixed input/output shape validation
- `self_obs` packing from mocked UE-style inputs into the locked tensor layout
- `mimic_target_poses` packing from mocked future-pose samples into the locked tensor layout
- terrain-zero and other fixed Stage 1 filler paths
- startup validation helpers that only inspect provided descriptors, names, or mapping tables

Recommended test level by area:

- Python/unit tests for training-side conversion, export, and parity fixtures
- C++ unit or automation tests for pure bridge helpers that do not require a live world
- bridge-level fixture tests for full packed input/output buffers built from mocked data

Subjective quality gates are not a reason to skip TDD for deterministic helpers.

## Live UE Integration That Is Not TDD-First

These areas still require evidence, but they are not strong candidates for pure red-green-refactor:

- NNE runtime creation and model/session startup in UE
- PoseSearch history collector lookup and live `MotionMatch` behavior
- `UPoseSearchAssetSamplerLibrary` sampling against authored assets
- `UPhysicsControlComponent` control creation, cache updates, and manual-update ordering
- required-body discovery on Manny's physics asset
- Chaos stability under runtime stepping
- visual correctness on Manny and side-by-side quality judgment

For these areas, use UE automation where practical, then runtime logs and manual verification for the rest.

## What Stays Manual

These remain manual by design:

- "looks alive" judgments in training visualization
- editor-only setup validation
- side-by-side quality comparison for G2
- observer-facing demo decision for G3

Manual checks must use the checkpoint IDs in `plans/stage1/manual-verification.md`.

## Required Evidence Format

- Automated checks: test name, pass/fail result, and any relevant log or summary
- Runtime scripted checks: command or scene used plus a short result note
- Manual checks: checkpoint ID, clip/screenshot, and one-sentence verdict
- Gate decisions: explicit `pass`, `fail`, or `blocked`

## Blocking Conditions

Escalate to the user if:

- a critical Stage 1 claim has no credible automated or scripted verification path
- a required manual check lacks a clear observable result
- a gate decision would otherwise rely on vague verbal impressions only

## Deliverable For S1-PLAN-04

The implementation-ready output from this planning task should be:

- this test strategy
- future task specs that point back to the correct verification mode per claim
- eventual execution packages that bind each claim to a named test or manual checkpoint
