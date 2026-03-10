# Stage 1 PHC Bridge Spec

## Purpose

This document defines the planning-level contract for the Stage 1 `PoseSearch -> PHC -> Physics Control` bridge. It is intentionally conservative: it locks the bridge shape tightly enough for implementation planning, while reserving exact tensor dimensions for confirmation against the chosen ProtoMotions/PHC config during Phase 0.

## Scope

This spec covers:

- what information Stage 1 must gather from UE5
- what information Stage 1 must feed into the PHC model
- what the PHC model is expected to output
- how outputs are converted into `UPhysicsControlComponent` targets
- what must be confirmed during Phase 0 before implementation starts

This spec does not invent a new policy format or change the locked architecture.

## Stage 1 Bridge Contract

### Upstream Source

- `PoseSearch` remains the source of the target motion intent.
- Stage 1 assumes `PoseSearch` provides a target pose that can be sampled each render tick for the currently selected clip/state.
- Planning assumption: Stage 1 uses a single current target pose plus any immediately available derived kinematic quantities, not a long custom trajectory window invented in UE5.

### Runtime Flow

Per render tick, the Stage 1 plugin does this:

1. Read current articulated-body state from Manny's simulated skeleton.
2. Read the current target pose from the active `PoseSearch` result.
3. Convert UE5 state and target data into the PHC observation format required by the chosen ProtoMotions config.
4. Run PHC inference through UE5 NNE.
5. Convert the PHC action output into mapped UE5 bone target orientations and angular-drive values.
6. Push those targets into `UPhysicsControlComponent`.

## Observation Contract

### Required Observation Categories

The final observation tensor must be confirmed from the selected ProtoMotions config, but the Stage 1 bridge must be able to provide these categories:

- root position or root-relative translation state
- root orientation
- root linear velocity
- root angular velocity
- mapped joint rotations for the SMPL-covered body
- mapped joint angular velocities for the SMPL-covered body
- contact-related state if the chosen PHC config requires it
- target-pose features derived from the current `PoseSearch` result if the chosen PHC variant conditions on a reference pose

### Planning Assumptions

- Use root-relative coordinates where the PHC config expects invariance to world translation.
- Use the pelvis as the root anchor unless the chosen config proves otherwise.
- Do not include finger or twist-bone state in the PHC observation. Those remain outside the mapped SMPL subset.
- Unmapped UE5 bones are visual followers of animation/PoseSearch rather than policy-driven control variables.

### Phase 0 Confirmation Required

Before implementation starts, confirm:

- exact observation tensor field order
- exact observation tensor dimension
- whether the policy expects quaternions, 6D rotations, axis-angle, or another representation
- whether the policy expects only current-frame targets or some short history / future reference
- whether ground-contact bits or normals are required

Phase 0 should confirm those fields from these exact local sources first:

- `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\config.yaml`
- `F:\NewEngine\Training\ProtoMotions\protomotions\eval_agent.py`
- `F:\NewEngine\Training\ProtoMotions\protomotions\envs\mimic\components\mimic_obs.py`

## Action Contract

### Required Action Categories

The PHC output must be treated as producing:

- desired joint orientation targets for the SMPL-covered joints
- optional stiffness / damping or other PD-gain-like values if the chosen PHC head exposes them

### Planning Assumptions

- If the PHC head does not emit gains, Stage 1 uses fixed editor-configured or hand-tuned `UPhysicsControlComponent` gains.
- If the PHC head emits gains, Stage 1 maps them into UE5 angular-drive strength/damping ranges through a deterministic normalization layer.
- The Stage 1 plugin does not emit raw torques directly unless `UPhysicsControlComponent` proves unusable and the project explicitly accepts the fallback path already documented in `ENGINEERING_PLAN.md`.

### Phase 0 Confirmation Required

Before implementation starts, confirm:

- exact action tensor field order
- exact action tensor dimension
- joint order used by the policy head
- output numeric range and normalization assumptions
- whether the output is local-joint space, parent-relative space, or another frame

Phase 0 should confirm those facts from these exact local sources first:

- `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\config.yaml`
- `F:\NewEngine\Training\ProtoMotions\protomotions\agents\masked_mimic\agent.py`
- `F:\NewEngine\Training\ProtoMotions\protomotions\agents\ppo\agent.py`

## Pose Representation Rules

- The bridge must use the representation required by the chosen PHC config, not the representation most convenient in UE5.
- Stage 1 planning assumes conversions may be required between UE5 transforms and SMPL-friendly policy inputs.
- The bridge must keep conversion code isolated so the representation decision does not leak into unrelated plugin code.

## Coordinate Frame Rules

- UE5 and SMPL frame conversion must be explicit and tested.
- Do not rely on a simple axis permutation alone because handedness is unresolved at planning level.
- All bridge code must pass through a named frame-conversion layer before writing model inputs or consuming model outputs.

## Mapping Rules

- The policy-controlled subset is the SMPL-covered body defined in `.agents/skills/smpl-skeleton/SKILL.md`.
- Mapped core bones:
  - pelvis
  - thighs
  - calves
  - feet / balls if required by the chosen control subset
  - spine chain
  - clavicles
  - upper arms
  - lower arms
  - hands
  - neck / head
- Unmapped bones:
  - fingers
  - twist bones
  - any mannequin extras not represented by SMPL

Unmapped bones keep their animation-driven pose unless a later spec says otherwise.

## Physics Control Output Rules

- Orientation targets are written through `SetControlTargetOrientation()`.
- Strength / damping updates are written through `SetControlAngularData()` if the bridge has per-joint gain data.
- PHC inference runs once per render tick.
- Physics substeps reuse the most recent target until the next render-tick inference result arrives.

## Failure Modes And Fallbacks

### Bridge-Level Failure Modes

- PHC config cannot be matched to the available UE5 state
- coordinate conversion produces mirrored or unstable motion
- action output range cannot be mapped to stable control targets
- `UPhysicsControlComponent` cannot reproduce a stable approximation of the policy intent

### Approved Stage 1 Fallbacks

- locomotion-only scope if fight-specific fine-tuning fails
- fixed gains if learned gain outputs are unavailable
- re-evaluate Stage 1 viability rather than introducing a new inference runtime
- raw torque fallback only if the project explicitly chooses the already-documented Physics Control fallback path

## Phase 0 Acceptance Criteria

This spec is considered locked only when all of the following are written down from the selected ProtoMotions config:

- observation tensor shape and field order
- action tensor shape and field order
- required pose representation
- required frame assumptions
- gain-output policy
- exact SMPL joint order used by the model

## Deliverable For S1-PLAN-02

The implementation-ready output from this planning task should be:

- this document updated with exact tensor and representation details from the selected ProtoMotions config
- one explicit "confirmed assumptions" section added during Phase 0
- one explicit "rejected assumptions" section added if the initial planning assumptions were wrong
