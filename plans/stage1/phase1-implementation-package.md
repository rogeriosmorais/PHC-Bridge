# Stage 1 Phase 1 Implementation Package

## Purpose

This package defines the implementation-ready scope for Phase 1: one physics-driven character in UE5, using the locked Stage 1 architecture and only the minimum custom bridge code required.

Phase 1 exists to answer the next question after G1:

Can one Manny character run the full `PoseSearch -> PHC -> Physics Control -> Chaos` loop well enough to support a credible quality comparison for G2?

The bridge is not considered fully specced for implementation until [ue-bridge-implementation-spec.md](/F:/NewEngine/plans/stage1/ue-bridge-implementation-spec.md) is completed and reviewed.

The human-owned UE Editor asset wiring and startup path for the current implementation is documented in [phase1-ue-bridge-bringup-runbook.md](/F:/NewEngine/plans/stage1/phase1-ue-bridge-bringup-runbook.md).

## Frozen Phase 1 Decisions

These decisions are now frozen for the current Phase 1 implementation pass.

### Runtime Model Decision

- `Decision`: `pretrained`
- `Chosen runtime model`: `motion_tracker/smpl`
- `Checkpoint path`: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`
- `Why this is frozen now`:
  - G1 passed on the locomotion-only thesis without requiring a fine-tuned model first
  - the selected checkpoint already matches the locked bridge contract in [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md)
  - no fine-tuned Stage 1 checkpoint currently exists, so waiting for one would reopen Phase 1 instead of starting it

### ONNX Export / Import Freeze

- export entry point:
  - `F:\NewEngine\Training\scripts\export_onnx.py`
- export output:
  - `F:\NewEngine\Training\output\phc_policy.onnx`
- UE import source path:
  - `F:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx`
- accepted opset:
  - `17`
- offline validation:
  - `onnxruntime 1.24.3` CPU session parity against the PyTorch export wrapper
  - max abs diff: `1.64e-7`
  - mean abs diff: `3.36e-8`
- frozen tensor interface:
  - `self_obs = 358`
  - `mimic_target_poses = 6495`
  - `terrain = 256`
  - output `actions = 69`

### Minimal PoseSearch Content For G2

For the first one-character comparison path, Phase 1 content stays frozen to the locomotion set already named in [phase1-ue-bridge-bringup-runbook.md](/F:/NewEngine/plans/stage1/phase1-ue-bridge-bringup-runbook.md):

- `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle`
- every `MF_Unarmed_Walk_*` clip in `/Game/Characters/Mannequins/Anims/Unarmed/Walk`
- every `MF_Unarmed_Jog_*` clip in `/Game/Characters/Mannequins/Anims/Unarmed/Jog`

Do not widen the Phase 1 comparison content beyond this locomotion core unless the orchestrator explicitly reopens the sequence lock.

### Stable Enough For G2

For the current Phase 1 pass, `stable enough for G2` means:

- the character starts the full bridge without startup-blocking asset or control-name failures
- the imported PHC model loads through NNE in UE5 and runs at least one successful inference pass
- the one-character runtime remains controllable for about `30` seconds at the frozen synchronous substep settings in `DefaultEngine.ini`
- instability or bridge faults do not dominate the capture to the point that the user cannot make a fair side-by-side judgment

### Current Runtime Truth

Current local Phase 1 truth on March 10, 2026:

- startup now succeeds through `NNERuntimeORTDml`
- the current blocker is no longer model loading
- the current blocker is uncontrolled post-startup runtime behavior (`flying` / `spinning uncontrollably`)

This means Phase 1 is now in stabilization/tuning, not in export discovery and not yet in G2 packaging.

### Frozen Stabilization Order

The Phase 1 stabilization pass must proceed in this order:

1. keep the working startup path fixed:
   - same model asset
   - same character Blueprint
   - same PoseSearch content
   - same physics settings unless the task explicitly says otherwise
2. reduce action influence before changing mapping assumptions
3. adjust fixed Physics Control gains/damping next
4. inspect mapping / frame-assumption faults only if low-influence tuning still produces pathological motion
5. do not ask the user to run G2 until the runtime-stability threshold passes

### Frozen Stabilization Surface

The first implementation pass uses a thin UE-native stabilization layer, not a second control system.

- action conditioning lives in `UPhysAnimComponent`
- control strength/damping scaling stays on `UPhysicsControlComponent`
- live iteration happens through component defaults plus runtime console variables

Frozen first-pass live knobs:

- `physanim.ForceZeroActions`
- `physanim.ActionScale`
- `physanim.ActionClampAbs`
- `physanim.ActionSmoothingAlpha`
- `physanim.StartupRampSeconds`
- `physanim.MaxAngularStepDegPerSec`
- `physanim.AngularStrengthMultiplier`
- `physanim.AngularDampingRatioMultiplier`
- `physanim.AngularExtraDampingMultiplier`
- `physanim.EnableInstabilityFailStop`
- `physanim.MaxRootHeightDeltaCm`
- `physanim.MaxRootLinearSpeedCmPerSec`
- `physanim.MaxRootAngularSpeedDegPerSec`
- `physanim.InstabilityGracePeriodSeconds`

Frozen automated guardrail for this pass:

- the bridge monitors the root body (`pelvis`) every tick
- the bridge auto-fail-stops if one or more instability thresholds stay exceeded longer than the grace window
- default automatic fail-stop thresholds:
  - `root height delta > 120 cm`
  - `root linear speed > 1200 cm/s`
  - `root angular speed > 720 deg/s`
  - `grace window = 0.25 s`
- this automated fail-stop is now the default objective Phase 1 stop condition for obvious launch / spin failures

## Entry Criteria

Do not start Phase 1 implementation until all of these are true:

- Gate G1 is explicitly `pass` in [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md)
- `bridge-spec.md` is locked for the chosen PHC config
- `ue-bridge-implementation-spec.md` is implementation-ready for the UE `5.7.3` runtime path
- `retargeting-spec.md` is locked for the Stage 1 mapped subset
- `motion-set.md` is locked for the Stage 1 locomotion-only core
- `comparison-sequence-lock.md` is locked for the intended G2 sequence
- `test-strategy.md` is stable enough to define Phase 1 evidence
- the deterministic bridge-core behaviors that must be TDD-first are explicitly frozen in `test-strategy.md`
- the orchestrator has reviewed `assumption-ledger.md` and confirmed there is no `red` assumption blocking one-character implementation

## Phase 1 Deliverables

1. One-character implementation package for the Stage 1 bridge
2. ONNX / NNE integration plan for the selected PHC model
3. PoseSearch content-integration plan for the locked locomotion clips
4. PD tuning plan for one-character stability and comparison readiness
5. A clear setup for the G2 side-by-side evaluation package
6. An ONNX export/import path that no longer requires discovery work during Phase 1
7. A UE integration spec that freezes exact runtime owner classes, API calls, and tick/update flow before further bridge code changes

## Scope

Phase 1 includes:

- a single Manny character
- the runtime bridge from `PoseSearch` output to PHC input
- PHC inference through UE5 NNE
- action-to-`UPhysicsControlComponent` mapping
- PoseSearch content setup sufficient for comparison
- stability / tuning work needed to make G2 meaningful

Phase 1 does not include:

- two-character gameplay
- impact response policy
- camera / HUD / demo packaging
- any Stage 2 GPU work

## Work Breakdown

### P1-01: Freeze Phase 1 Inputs

- Owner: orchestrator
- Goal: freeze the exact assumptions and artifacts Phase 1 is allowed to build on
- Inputs:
  - `plans/stage1/g1-evidence.md`
  - `plans/stage1/bridge-spec.md`
  - `plans/stage1/retargeting-spec.md`
  - `plans/stage1/test-strategy.md`
  - `plans/stage1/assumption-ledger.md`
- Output:
  - short frozen-input note in the Phase 1 handoff

### P1-02: Plugin Bridge Implementation Scope

- Owner: AI worker
- Goal: define the exact bridge responsibilities for `PhysAnimPlugin`
- Required reference:
  - [ue-bridge-implementation-spec.md](/F:/NewEngine/plans/stage1/ue-bridge-implementation-spec.md)
- Output:
  - implementation-ready breakdown for:
    - state gathering
    - observation packing
    - NNE inference call
    - action unpacking
    - physics-control writes
- Constraints:
  - keep custom code to the low hundreds where feasible
  - do not add a new inference runtime
  - do not widen scope into Stage 2 systems
  - split deterministic bridge helpers away from live UE glue so they can be tested first
  - do not implement deterministic bridge logic before its tests exist

### P1-03: ONNX / NNE Integration Scope

- Owner: AI worker
- Goal: define how the chosen PHC model becomes a UE5-usable NNE asset
- Inputs:
  - locked bridge spec
  - [onnx-export-spec.md](/F:/NewEngine/plans/stage1/onnx-export-spec.md)
- Output:
  - model import / validation plan
  - expected runtime evidence for successful loading and inference
- Escalate if:
  - the selected model export path no longer fits the Stage 1 assumptions

### P1-03b: Pretrained Versus Fine-Tuned Runtime Decision

- Owner: orchestrator
- Goal: decide whether Phase 1 should run on the pretrained model directly or on a fine-tuned version
- Inputs:
  - G1 evidence
  - motion-set coverage review
- Output:
  - explicit `pretrained`, `fine-tuned`, or `stop/replan` decision
- Default:
  - use pretrained only if it is good enough for the locked motion set
  - otherwise fine-tune on the locked motion set before expecting G2 to be meaningful
- Frozen outcome for the current pass:
  - `pretrained`

### P1-04: PoseSearch Content Scope

- Owner: AI worker
- Goal: define the minimal PoseSearch setup required for the one-character comparison
- Inputs:
  - `ENGINEERING_PLAN.md`
  - G1 outcome
  - [comparison-sequence-lock.md](/F:/NewEngine/plans/stage1/comparison-sequence-lock.md)
- Output:
  - clip / database setup plan
  - clear statement of which locomotion content is needed in Phase 1
- Constraint:
  - keep content scope small enough that G2 is a quality test, not a content-production project

### P1-05: PD Tuning Scope

- Owner: AI worker
- Goal: define the tuning work needed to make the one-character loop stable and comparable
- Inputs:
  - G1 evidence
  - substep evidence
- Output:
  - tuning checklist for gains, damping, and stability settings
  - definition of what "stable enough for G2" means
- Escalate if:
  - tuning work would effectively require architectural changes
- Frozen immediate purpose:
  - eliminate the current post-startup flight/spin failure mode before any G2 capture packaging

### P1-06: Optional Visual Bonus Decision

- Owner: orchestrator
- Goal: decide whether Chaos Flesh + ML Deformer belongs in Phase 1 or should be deferred
- Output:
  - explicit `include` or `defer` note
- Default:
  - defer unless it materially improves the G2 comparison without destabilizing scope

### P1-07: G2 Readiness Handoff

- Owner: orchestrator
- Goal: package the one-character implementation result so G2 can be evaluated cleanly
- Inputs:
  - all Phase 1 outputs
  - current assumption ledger
- Output:
  - handoff to `plans/stage1/g2-evaluation.md`

## Required Evidence For Phase 1 Completion

Before Phase 1 can be considered complete enough for G2 packaging, there must be evidence for:

- one-character bridge works end to end
- PHC model runs through NNE in UE5
- PoseSearch content is available for the chosen comparison sequence
- the UE integration path is frozen well enough that no new UE API discovery work is still blocking implementation
- the comparison sequence is frozen in `comparison-sequence-lock.md`
- tuning reaches a stable-enough state for comparison
- no unresolved `red` assumptions remain for G2
- deterministic bridge-core tests were written before implementation and remain passing

## Failure Handling

If Phase 1 stalls or fails:

- update `assumption-ledger.md`
- decide whether the failure is:
  - tuning-related and recoverable
  - bridge-related and recoverable
  - content-related and recoverable by narrowing scope
  - a stop condition for Stage 1
- do not advance to G2 packaging until that decision is written down

## Handback Requirements

The worker or orchestrator closing Phase 1 must produce a handoff that includes:

- `Task ID`: `S1-P1-A1`
- decision summary
- produced artifact(s)
- open risks
- blocked by user? yes/no
- verification summary
- next consumer

Use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md) exactly.
