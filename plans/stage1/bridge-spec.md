# Stage 1 PHC Bridge Spec

## Purpose

This document defines the Stage 1 `PoseSearch -> PHC -> Physics Control` bridge contract for the currently selected local ProtoMotions checkpoint.

The planning-level assumptions in this file are now checked against the frozen local `motion_tracker/smpl` config and code path selected for the locomotion-only Stage 1 runtime.

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
- config source: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\config.yaml`
- checkpoint source: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`
- agent class: `protomotions.agents.mimic.agent.Mimic`
- model class: `protomotions.agents.ppo.model.PPOModel`
- simulator-side control type: `built_in_pd`
- action mapping mode: `map_actions_to_pd_range: true`

## Stage 1 Bridge Contract

### Upstream Source

- `PoseSearch` remains the source of the target motion intent.
- Stage 1 assumes `PoseSearch` provides one current selected animation state that can be sampled each render tick for the currently selected clip/state.
- Stage 1 uses a dense `15`-step future reference window derived from that selected animation state at the locked simulator cadence, not a separate custom trajectory predictor invented in UE5.

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

The selected motion-tracker checkpoint still uses a keyed runtime input dictionary, but it is materially simpler than the previous MaskedMimic path.

The required inference-time keys are:

- `self_obs`: `358` floats
- `mimic_target_poses`: `6495` floats
- `terrain`: `256` floats

### Confirmed Grouping And Field Order

`self_obs` is produced by `compute_humanoid_observations_max(...)` and is grouped as:

- root height: `1`
- root-relative local body positions with root position removed: `23 * 3 = 69`
- heading-aligned body rotations in tan-norm form for all `24` bodies: `24 * 6 = 144`
- heading-aligned body linear velocities for all `24` bodies: `24 * 3 = 72`
- heading-aligned body angular velocities for all `24` bodies: `24 * 3 = 72`

Total: `358`

`mimic_target_poses` is the dense future target-pose tensor:

- `15 future target poses * 433 floats = 6495`
- each target pose is built from the `max-coords-future-rel` path plus time
- per-future-pose grouping:
  - target relative body positions: `24 * 3 = 72`
  - target body positions relative to current root: `24 * 3 = 72`
  - target relative body rotations in tan-norm form: `24 * 6 = 144`
  - target body rotations in heading-aligned tan-norm form: `24 * 6 = 144`
  - future-time scalar: `1`

`terrain` is a `16 x 16` height-sample map flattened to `256` floats.

### Planning Assumptions

- Use root-relative coordinates where the PHC config expects invariance to world translation.
- Use the pelvis as the root anchor unless the chosen config proves otherwise.
- Do not include mannequin finger or twist-bone state in the PHC observation.
- The runtime still needs `24` SMPL bodies. `L_Hand` and `R_Hand` are therefore not dropped; they are approximated from Manny `hand_l` and `hand_r` on the UE side.
- Unmapped UE5 bones are visual followers of animation/PoseSearch rather than policy-driven control variables.

### Runtime-Simplification Note

The selected checkpoint deliberately avoids the extra runtime inputs that were present in the earlier MaskedMimic plan:

- no sparse masked target-pose tensor
- no target-pose mask tensor
- no historical pose conditioning tensor
- no motion-text embedding input
- no `vae_noise` input

This is the main reason it is now the preferred Stage 1 runtime target.

## Action Contract

### Confirmed Action Shape And Meaning

The selected checkpoint outputs a single action tensor of `69` floats.

- model output head: PPO actor mean over `69` action values
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

### Exact PD-Range Mapping

The selected runtime path uses the simulator helper:

- `pd_target = pd_action_offset + pd_action_scale * action`

Frozen Stage 1 inputs to that helper:

- `map_actions_to_pd_range: true`
- `action_scale: 1.0`

The offset/scale vectors are produced by `build_pd_action_offset_scale(...)`.

For the selected `smpl_humanoid` asset:

- every policy joint is a `3`-DoF group
- each group is first expanded to a symmetric range:
  - `joint_scale = min(1.2 * max(abs(low_xyz), abs(high_xyz)), pi)`
  - `joint_offset = 0`
- the final per-DoF mapping is the group scale copied across the `x/y/z` entries

Because the selected `smpl_humanoid.xml` ranges are all at least `[-180 deg, 180 deg]` and several are wider, every Stage 1 joint group reaches the clamp:

- `joint_scale = pi`
- `joint_offset = 0`

So the frozen Stage 1 simplification is:

- `pd_target_i = pi * action_i`

for all `69` action dimensions.

### Exact Action-To-Local-Rotation Conversion

After PD-range mapping:

1. group the `69` DoFs into `23` joints in `robot.dof_names` order
2. use the implicit `3`-DoF offsets:
   - `0, 3, 6, ..., 69`
3. convert each `3`-float group from exponential-map DoF space into a local quaternion with `w_last=True`
4. pass that quaternion through the frozen SMPL->UE frame-conversion layer
5. map the resulting local parent-space rotation onto the UE control target

### Exact Distal-Hand Collapse Rule

The selected checkpoint has:

- `L_Wrist`
- `L_Hand`
- `R_Wrist`
- `R_Hand`

Manny has only one practical distal hand control body per side:

- `hand_l`
- `hand_r`

Frozen Stage 1 collapse:

- `Q_hand_l = Q_L_Wrist * Q_L_Hand`
- `Q_hand_r = Q_R_Wrist * Q_R_Hand`

where the multiplication order is parent first, child second.

Observation-side surrogate:

- both `L_Wrist` and `L_Hand` read from Manny `hand_l`
- both `R_Wrist` and `R_Hand` read from Manny `hand_r`

This is the only approved Stage 1 distal-hand approximation.

## Pose Representation Rules

- The bridge must use the representation required by the selected local config, not the representation most convenient in UE5.
- `self_obs` and target-pose features use heading-aligned tan-norm rotation features derived from quaternion state.
- The action head represents joint targets in exponential-map DoF space before PD-range mapping.
- The bridge must keep conversion code isolated so the representation decision does not leak into unrelated plugin code.

## Coordinate Frame Rules

- UE5 and SMPL frame conversion must be explicit and tested.
- Do not shuffle quaternion components directly.
- All bridge code must pass through a named frame-conversion layer before writing model inputs or consuming model outputs.
- The selected IsaacLab simulator uses `w_last: false` internally, but the common observation-building path converts data before feature construction and the relevant observation builders here operate with `w_last=True`.
- Stage 1 should treat the confirmed model-facing quaternion/tan-norm path as `xyzw` / `w_last=True` at the feature-construction boundary.
- Freeze the basis conversion at the vector boundary as:
  - `UE.X = SMPL.Z`
  - `UE.Y = SMPL.X`
  - `UE.Z = SMPL.Y`
- Freeze the rotation conversion as a basis change, not a quaternion component permutation:
  - `B = [[0,0,1],[1,0,0],[0,1,0]]`
  - `R_ue = B * R_smpl * B^T`

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
- Distal-hand policy rule:
  - `L_Wrist` and `L_Hand` collapse onto UE `hand_l`
  - `R_Wrist` and `R_Hand` collapse onto UE `hand_r`
- Unmapped bones:
  - fingers
  - twist bones
  - any mannequin extras not represented by SMPL

Unmapped bones keep their animation-driven pose unless a later spec says otherwise.

## Physics Control Output Rules

- Orientation targets are written through `SetControlTargetOrientation()`.
- Strength / damping updates are written through `SetControlAngularData()` if the bridge has per-joint gain data.
- Control targets are parent-space targets derived from the mapped local joint quaternions, not world-space drags.
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

- the selected local pretrained path is the `motion_tracker/smpl` checkpoint under the repo's `data/pretrained_models` tree
- inference uses a small keyed input dictionary rather than one flattened observation tensor
- the runtime bridge must provide `self_obs`, dense future `mimic_target_poses`, and `terrain`
- the model outputs `69` normalized action values that become built-in PD position targets after deterministic offset/scale mapping
- gains are fixed outside the model

## Rejected Assumptions

- the Stage 1 runtime bridge does not need MaskedMimic-specific sparse conditioning inputs
- the selected checkpoint does not output direct joint quaternions
- the selected checkpoint does not output per-joint stiffness or damping values

## Deliverable For S1-PLAN-02

The implementation-ready output from this planning task should be:

- this document updated with exact tensor and representation details from the selected ProtoMotions config
- one explicit "confirmed assumptions" section added during Phase 0
- one explicit "rejected assumptions" section added if the initial planning assumptions were wrong
