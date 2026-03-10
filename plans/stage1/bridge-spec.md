# Stage 1 PHC Bridge Spec

## Purpose

This document defines the Stage 1 `PoseSearch -> PHC -> Physics Control` bridge contract for the currently selected local ProtoMotions checkpoint.

The planning-level assumptions in this file have now been checked against the frozen local MaskedMimic SMPL config and code path used for Phase 0.

## Scope

This spec covers:

- what information Stage 1 must gather from UE5
- what information Stage 1 must feed into the PHC model
- what the PHC model is expected to output
- how outputs are converted into `UPhysicsControlComponent` targets
- what must be confirmed during Phase 0 before implementation starts

This spec does not invent a new policy format or change the locked architecture.

## Confirmed Selected Local Policy

- simulator path: `IsaacLabSimulator`
- robot/body type: `smpl_humanoid`
- config source: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\config.yaml`
- checkpoint source: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\last.ckpt`
- agent class: `protomotions.agents.masked_mimic.agent.MaskedMimic`
- model class: `protomotions.agents.masked_mimic.model.VaeDeterministicOutputModel`
- simulator-side control type: `built_in_pd`
- action mapping mode: `map_actions_to_pd_range: true`

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

### Confirmed Runtime Input Set

The selected MaskedMimic checkpoint does not consume one monolithic observation tensor at inference time. `model.act()` consumes a keyed runtime input dictionary.

The required inference-time keys are:

- `self_obs`: `358` floats
- `masked_mimic_target_poses`: `2024` floats
- `masked_mimic_target_poses_masks`: `11` bools
- `historical_pose_obs`: `5385` floats
- `motion_text_embeddings`: `512` floats
- `motion_text_embeddings_mask`: `1` bool
- `terrain`: `256` floats
- `vae_noise`: `64` floats

### Confirmed Grouping And Field Order

`self_obs` is produced by `compute_humanoid_observations_max(...)` and is grouped as:

- root height: `1`
- root-relative local body positions with root position removed: `23 * 3 = 69`
- heading-aligned body rotations in tan-norm form for all `24` bodies: `24 * 6 = 144`
- heading-aligned body linear velocities for all `24` bodies: `24 * 3 = 72`
- heading-aligned body angular velocities for all `24` bodies: `24 * 3 = 72`

Total: `358`

`masked_mimic_target_poses` is the sparse conditioning tensor used by the prior network:

- `(10 near-future sparse poses + 1 far target pose) * 184 = 2024`
- conditionable-body order:
  - `Pelvis`
  - `L_Ankle`
  - `R_Ankle`
  - `L_Hand`
  - `R_Hand`
  - `Head`
  - heading / planar-velocity pseudo-entry
- each sparse pose entry encodes masked target translation and rotation features plus mask bits and time information

`historical_pose_obs` is:

- `15` subsampled historical `self_obs` frames
- each historical frame appends `1` time scalar
- total: `15 * (358 + 1) = 5385`

`terrain` is a `16 x 16` height-sample map flattened to `256` floats.

`motion_text_embeddings` is a `512`-float vector plus a `1`-bit visibility mask.
Stage 1 should supply a zero vector and false mask unless text-conditioned runtime control is explicitly added later.

`vae_noise` is a `64`-float latent-noise vector.
For deterministic Stage 1 inference, use the eval path behavior already frozen in the config and runtime wrapper.

### Planning Assumptions

- Use root-relative coordinates where the PHC config expects invariance to world translation.
- Use the pelvis as the root anchor unless the chosen config proves otherwise.
- Do not include finger or twist-bone state in the PHC observation. Those remain outside the mapped SMPL subset.
- Unmapped UE5 bones are visual followers of animation/PoseSearch rather than policy-driven control variables.

### Training-Only Inputs Not Required At Runtime

The local code path distinguishes between training-time encoder inputs and inference-time prior inputs.

These are present in training but not required by `model.act()` inference:

- `mimic_target_poses`: `6495` floats
- `masked_mimic_target_bodies_masks`: `140` bools

This matters for Stage 1 because the UE bridge only needs to reproduce the inference-time contract, not the training-only encoder contract.

## Action Contract

### Confirmed Action Shape And Meaning

The selected checkpoint outputs a single action tensor of `69` floats.

- model output head: `torch.tanh(...)`
- numeric range before PD mapping: `[-1, 1]`
- semantic meaning: per-DoF position targets in the robot's exponential-map DoF space
- simulator application: `_action_to_pd_targets(offset + scale * action)` and then built-in PD position targets

The action tensor is ordered by `robot.dof_names`, which groups `23` joints at `3` DoFs each:

- `L_Hip`
- `L_Knee`
- `L_Ankle`
- `L_Toe`
- `R_Hip`
- `R_Knee`
- `R_Ankle`
- `R_Toe`
- `Torso`
- `Spine`
- `Chest`
- `Neck`
- `Head`
- `L_Thorax`
- `L_Shoulder`
- `L_Elbow`
- `L_Wrist`
- `L_Hand`
- `R_Thorax`
- `R_Shoulder`
- `R_Elbow`
- `R_Wrist`
- `R_Hand`

### Planning Assumptions

- The selected checkpoint does not emit gains.
- Stage 1 must use fixed editor-configured or hand-tuned `UPhysicsControlComponent` gains.
- The Stage 1 plugin does not emit raw torques directly unless `UPhysicsControlComponent` proves unusable and the project explicitly accepts the fallback path already documented in `ENGINEERING_PLAN.md`.

## Pose Representation Rules

- The bridge must use the representation required by the selected local config, not the representation most convenient in UE5.
- `self_obs` and target-pose features use heading-aligned tan-norm rotation features derived from quaternion state.
- The action head represents joint targets in exponential-map DoF space before PD-range mapping.
- The bridge must keep conversion code isolated so the representation decision does not leak into unrelated plugin code.

## Coordinate Frame Rules

- UE5 and SMPL frame conversion must be explicit and tested.
- Do not rely on a simple axis permutation alone because handedness is unresolved at planning level.
- All bridge code must pass through a named frame-conversion layer before writing model inputs or consuming model outputs.
- The selected IsaacLab simulator uses `w_last: false` internally, but the common observation-building path converts data before feature construction and the relevant observation builders here operate with `w_last=True`.
- Stage 1 should treat the confirmed model-facing quaternion/tan-norm path as `xyzw` / `w_last=True` at the feature-construction boundary.

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

- runtime observation input set shape and field grouping
- action tensor shape and field order
- required pose representation
- required frame assumptions
- gain-output policy
- exact SMPL joint order used by the model

## Confirmed Assumptions

- the selected local pretrained path is the MaskedMimic SMPL checkpoint under the repo's `data/pretrained_models` tree
- inference uses a keyed input dictionary rather than one flattened observation tensor
- the runtime bridge must provide `self_obs`, sparse masked-mimic target poses, historical pose context, terrain, text-conditioning placeholders, and `vae_noise`
- the model outputs `69` normalized action values that become built-in PD position targets after deterministic offset/scale mapping
- gains are fixed outside the model

## Rejected Assumptions

- the runtime bridge does not need to reproduce every training-time encoder input
- the selected checkpoint does not output direct joint quaternions
- the selected checkpoint does not output per-joint stiffness or damping values

## Deliverable For S1-PLAN-02

The implementation-ready output from this planning task should be:

- this document updated with exact tensor and representation details from the selected ProtoMotions config
- one explicit "confirmed assumptions" section added during Phase 0
- one explicit "rejected assumptions" section added if the initial planning assumptions were wrong
