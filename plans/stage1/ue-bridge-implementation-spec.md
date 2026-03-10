# Stage 1 UE Bridge Implementation Spec

## Purpose

This document is the Unreal-specific implementation spec for the Stage 1 bridge.

It exists because [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md) locks the model-facing contract, but does not by itself freeze the exact UE `5.7.3` classes, functions, ownership model, content paths, and tick/update flow needed to build the runtime safely.

This document is the freeze point for Phase 1 bridge implementation. Do not resume bridge code changes beyond smoke-test maintenance until this spec is reviewed.

## Status

- `Current status`: implementation-ready draft
- `Decision date`: March 10, 2026
- `Target phase`: Phase 1
- `Applies to`: one-character Stage 1 runtime only

## Evidence Base

This spec was written against:

- locked local planning docs:
  - [ENGINEERING_PLAN.md](/F:/NewEngine/ENGINEERING_PLAN.md)
  - [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md)
  - [retargeting-spec.md](/F:/NewEngine/plans/stage1/retargeting-spec.md)
  - [onnx-export-spec.md](/F:/NewEngine/plans/stage1/onnx-export-spec.md)
  - [test-strategy.md](/F:/NewEngine/plans/stage1/test-strategy.md)
- local UE `5.7.3` headers and engine/plugin source:
  - `PhysicsControlComponent.h`
  - `PoseSearchLibrary.h`
  - `PoseSearchResult.h`
  - `PoseSearchAssetSamplerLibrary.h`
  - `NNE.h`
  - `NNERuntimeGPU.h`
  - `NNERuntimeRunSync.h`
  - `NNERuntimeORT.cpp`
- Epic official docs:
  - NNE overview: https://dev.epicgames.com/documentation/en-us/unreal-engine/neural-network-engine-overview-in-unreal-engine
  - `NNERuntimeORT` API page: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/PluginIndex/NNERuntimeORT
  - `INNERuntime` API page: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/NNE/INNERuntime
  - Physics Control plugin API index: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl
  - `UPoseSearchLibrary` API page: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PoseSearch/UPoseSearchLibrary
  - `UPoseSearchAssetSamplerLibrary` API page: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PoseSearch/UPoseSearchAssetSamplerLibrary

Epic's public docs are useful for API existence and intended usage. Exact signatures and call ordering in this spec were verified against the local `5.7.3` headers because some PoseSearch pages lag between `5.6` and `5.7`.

## Locked Design

### 1. Runtime Owner

The live Stage 1 bridge owner is a new plugin component:

- class: `UPhysAnimComponent`
- base class: `UActorComponent`
- host: the controlled character actor

`UPhysAnimComponent` owns:

- startup validation
- PoseSearch query state
- NNE runtime/model/session lifetime
- observation packing
- action unpacking
- Physics Control writes
- runtime logging

It does not own:

- skeletal animation evaluation
- physics simulation
- project-wide orchestration

Those remain owned by UE systems already chosen in the architecture.

#### Why An Actor Component Is Correct

- Stage 1 is explicitly one-character first, not a world service.
- The engineering plan already budgets the bridge as `PhysAnimComponent.h/cpp`.
- The component can declare a direct tick prerequisite on the owning `USkeletalMeshComponent`, which is the critical requirement for safe manual `UPhysicsControlComponent` updates.
- A subsystem is appropriate for smoke harnesses and console-triggered experiments, but it is the wrong owner for the final per-character runtime path.
- An anim instance is the wrong owner because the bridge must also own NNE runtime/session objects and Physics Control state, and because Phase 1 should avoid additional any-thread to game-thread handoff complexity.

#### Rejected Owners

- `UWorldSubsystem`: rejected for production Phase 1; keep only for G1 smoke tests
- `UAnimInstance`: rejected for production Phase 1; too coupled to animation-thread concerns
- `APhysicsControlActor`: rejected; Stage 1 does not need an external controller actor for a single controlled character

#### Smoke-Test Rule

The existing `UPhysAnimMvG102Subsystem` and `UPhysAnimMvG103Subsystem` remain smoke-test-only harnesses. They are not the production runtime owner and must not grow into the Phase 1 bridge.

### 2. Character And Asset Binding

Phase 1 freezes one concrete runtime target:

- runtime character blueprint:
  - `/Game/Characters/Mannequins/Blueprints/BP_PhysAnimCharacter`
- current scaffold character kept only as template reference:
  - `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`

The production runtime character must:

- derive from `ACharacter` or a direct Blueprint child of it
- use Manny as the skeletal mesh
- include one `UPhysAnimComponent`
- include one pre-authored `UPhysicsControlComponent`
- use an Anim Blueprint with a PoseSearch history collector named `PoseHistory_Stage1`

#### Frozen Content Paths

- default map for Phase 1 runtime checks:
  - `/Game/ThirdPerson/Lvl_ThirdPerson`
- skeletal mesh:
  - `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`
- physics asset:
  - `/Game/Characters/Mannequins/Rigs/PA_Mannequin`
- locomotion Anim Blueprint to be created for Phase 1:
  - `/Game/Characters/Mannequins/Animations/ABP_PhysAnim`
- PoseSearch schema to be created for Phase 1:
  - `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion`
- PoseSearch database to be created for Phase 1:
  - `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion`
- imported `UNNEModelData` asset:
  - `/Game/NNEModels/phc_policy`

#### Frozen Bridge Config Policy

Stage 1 does not add a separate bridge config asset.

Use:

- fixed code constants for the mapped subset and tensor contract
- content assets only for:
  - skeletal mesh
  - physics asset
  - Anim Blueprint
  - PoseSearch database
  - NNE model asset

If Phase 1 later needs a tunable config asset, that is a replan item, not an implicit scope expansion.

#### Built-In Systems Intentionally Left Outside The Bridge

The bridge does not take ownership of every Stage 1 built-in system named in the engineering plan.

- `UPhysicalAnimationComponent` remains available as the built-in answer for blend transitions, but locomotion-only Phase 1 does not require bridge-driven blend weights
- knockdown, recovery, and other transition-blending behaviors are out of scope for the one-character locomotion comparison
- if G2 later proves that explicit physics-animation blending is required for a fair comparison, that is a focused scope addition, not something the bridge should preemptively implement now

### 3. PoseSearch Integration

Stage 1 uses one PoseSearch ownership path:

1. the current frame's animated target pose comes from `UPhysicsControlComponent` cached bone data after animation has already evaluated
2. the future reference window for `mimic_target_poses` comes from a direct `UPoseSearchLibrary::MotionMatch(...)` query owned by `UPhysAnimComponent`

This keeps content authoring on built-in PoseSearch assets while avoiding the need to author a full Motion Matching Anim Graph path that does not currently exist in the project.

#### Frozen PoseSearch Owner Model

- The Anim Blueprint remains the owner of the visual locomotion graph.
- `UPhysAnimComponent` is the production owner of the PoseSearch query call.
- `UPhysAnimComponent` owns the Phase 1 `AssetsToSearch` array and it is frozen to:
  - `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion`
- `UPhysAnimComponent` calls `UPoseSearchLibrary::MotionMatch(...)` in the production path.
- The Anim Blueprint does not need an authored Motion Matching node.
- The Anim Blueprint does need a pose-history collector node tagged `PoseHistory_Stage1`.
- The Anim Blueprint does not need a custom `UPhysAnimAnimInstance` parent for the production direct-query path.
- The production path does not depend on an AnimBP wrapper like `GetStage1MotionMatchResult()`.
- No chooser-only divergence is allowed; the direct query must search the same authored database asset that the comparison content is built around.

Startup must validate:

- the live Anim Blueprint class used by the production character is the locomotion Anim Blueprint authored for `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion`
- the live Anim Instance can resolve `PoseHistory_Stage1`
- the production bridge can load `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion` directly
- the authored database must use the Stage 1 locomotion schema at `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion`

#### Frozen Initial Schema Authoring Rule

For first implementation bring-up:

- create one schema asset at `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion`
- choose the Manny/Quinn mannequin skeleton asset `SK_Mannequin` when the schema factory asks for a skeleton
- keep the factory-created default locomotion channels
- leave the mirror data table empty for the initial unmirrored-only database pass

This is the lowest-resistance authored schema path for UE `5.7.3`.

#### Frozen Pose History Tag

- pose history collector name: `PoseHistory_Stage1`

Startup must validate:

- `UPoseSearchLibrary::FindPoseHistoryNode(PoseHistory_Stage1, AnimInstance)` returns non-null

If that lookup fails, startup is `blocked`.

#### Frozen Direct Query Rule

`UPhysAnimComponent` owns one `FPoseSearchBlueprintResult` plus one `FPoseSearchContinuingProperties` state for the production path.

Per tick, the component calls:

- `UPoseSearchLibrary::MotionMatch(...)`

with:

- `AnimInstance = live AnimInstance`
- `AssetsToSearch = { /Game/PoseSearch/Databases/PSDB_Stage1_Locomotion }`
- `PoseHistoryName = PoseHistory_Stage1`
- `ContinuingProperties = previous valid search result converted with the continuing-properties helper`
- `Future = default/empty for locomotion-only Phase 1`

The returned `FPoseSearchBlueprintResult` is the source of truth for future-pose sampling.

#### Current Target Pose Source

The current animation target pose used by the bridge comes from:

1. `PhysicsControl->UpdateTargetCaches(DeltaTime)`
2. `PhysicsControl->GetCachedBoneTransform(...)` or `GetCachedBoneTransforms(...)`

This makes the bridge read the actual post-AnimBP pose that Physics Control will use, instead of re-sampling a separate animation path for the current frame.

#### Future Reference Window Source

The future reference window used for `mimic_target_poses` comes from:

1. `UPoseSearchLibrary::MotionMatch(...)`
2. `UPoseSearchAssetSamplerLibrary::SamplePose(...)`
3. `UPoseSearchAssetSamplerLibrary::GetTransformByName(...)`

#### Frozen PoseSearch Result Consumption

`UPhysAnimComponent` may keep one cached last-valid PoseSearch result only for the one-tick grace period allowed by runtime failure handling.

Per tick:

1. call `UPoseSearchLibrary::MotionMatch(...)`
2. if the result is valid, use it as the source of truth for future-pose sampling and cache it as the last-valid result
3. if the result is invalid for one tick, reuse the cached last-valid result
4. if the result is invalid for `2` consecutive ticks, fail per the runtime failure rules already frozen below

#### Frozen Future Sampling Schedule

The selected checkpoint uses `15` future target poses at `env.dt` spacing.

From the locked ProtoMotions config and simulator code:

- IsaacLab config uses `fps = 120`
- IsaacLab config uses `decimation = 4`
- simulator `dt = decimation / fps`

Therefore the Stage 1 future sample interval is:

- `1 / 30` seconds

Frozen future sample times:

- `SelectedTime + n * (1 / 30)` seconds
- `n = 1..15`
- clamp to the selected asset's play length

This is an inference from the locked local config and simulator code, not an Epic API rule.

#### Frozen Pose Sampling Rules

For each future sample time:

- `Cast<UAnimationAsset>(SearchResult.SelectedAnim)` must succeed
- `FPoseSearchAssetSamplerInput.Animation = Cast<UAnimationAsset>(SearchResult.SelectedAnim)`
- `FPoseSearchAssetSamplerInput.AnimationTime = SampleTime`
- `FPoseSearchAssetSamplerInput.bMirrored = SearchResult.bIsMirrored`
- `FPoseSearchAssetSamplerInput.BlendParameters = SearchResult.BlendParameters`
- `FPoseSearchAssetSamplerInput.RootTransformOrigin = SkeletalMeshComponent->GetComponentTransform()`

Then sample with:

- `UPoseSearchAssetSamplerLibrary::SamplePose(AnimInstance, SamplerInput)`

Then read named transforms with:

- `UPoseSearchAssetSamplerLibrary::GetTransformByName(...)`

#### Debug-Only PoseSearch API

The production Phase 1 handoff path is:

- `UPoseSearchLibrary::MotionMatch(...)`

The old AnimBP-owned Motion Matching wrapper path is now legacy planning only and must not be treated as the production dependency.

The production path must not require:

- a Motion Matching node in the Anim Graph
- `UMotionMatchingAnimNodeLibrary`
- a custom AnimBP callback that copies a search result into bridge-owned state

### 4. NNE Integration

#### Frozen Runtime Preference

Stage 1 runtime order:

1. `NNERuntimeORTDml`
2. `NNERuntimeORTCpu` fallback for correctness/debug only

#### Frozen Owned Objects

`UPhysAnimComponent` owns:

- `TObjectPtr<UNNEModelData>` model asset reference
- runtime weak interface pointer
- model shared pointer
- model-instance shared pointer
- CPU input/output buffers
- input descriptor index map

#### Frozen Load Timing

Model load happens once during startup, before the bridge reports success.

Allowed startup order:

1. resolve model asset `/Game/NNEModels/phc_policy`
2. try `UE::NNE::GetRuntime<INNERuntimeGPU>("NNERuntimeORTDml")`
3. validate `CanCreateModelGPU(ModelData) == Ok`
4. call `CreateModelGPU(ModelData)`
5. call `CreateModelInstanceGPU()`
6. read input/output tensor descs
7. validate descriptor names and counts against the locked bridge contract
8. call `SetInputTensorShapes(...)`

If the DML path fails at runtime creation, log the reason and repeat with:

- `UE::NNE::GetRuntime<INNERuntimeCPU>("NNERuntimeORTCpu")`
- `CanCreateModelCPU(ModelData) == Ok`
- `CreateModelCPU(ModelData)`
- `CreateModelInstanceCPU()`

If both fail, startup is `blocked`.

#### Frozen Descriptor Validation

The bridge must not assume NNE tensor order by source export order alone.

At startup:

- read `GetInputTensorDescs()`
- build an index map by tensor name
- require exactly these named inputs:
  - `self_obs`
  - `mimic_target_poses`
  - `terrain`
- require exactly one output tensor of `69` floats

If descriptor names are missing or duplicated, startup is `blocked`.

#### Frozen Input Shapes

The only accepted Phase 1 shapes are:

- `self_obs`: `[1, 358]`
- `mimic_target_poses`: `[1, 6495]`
- `terrain`: `[1, 256]`
- output: `[1, 69]`

`SetInputTensorShapes(...)` must succeed before the bridge can start.

#### Frozen Inference Cadence

- inference runs once per render tick
- inference runs on the game thread in Phase 1
- no async task, RDG path, or batched multi-character session is allowed in Phase 1

#### Frozen Action Mapping Math

The locked local ProtoMotions helper uses:

- `pd_target = pd_action_offset + pd_action_scale * action`

For the selected `smpl_humanoid` runtime path:

- `action_scale = 1.0`
- `map_actions_to_pd_range = true`
- all `23` policy joints are `3`-DoF groups
- the helper clamps every joint group's scale to `pi`
- every joint group's offset is `0`

So the frozen Stage 1 simplification is:

- `pd_target_i = pi * action_i`

for all `69` output DoFs.

After that:

1. group the `69` DoFs into `23` contiguous `3`-float joints
2. convert each `3`-float group from exponential-map DoF space into a local quaternion
3. convert the quaternion through the frozen SMPL->UE frame-conversion layer
4. map each quaternion onto the Manny control subset
5. compose distal hand targets in parent-first order:
   - `Q_hand_l = Q_L_Wrist * Q_L_Hand`
   - `Q_hand_r = Q_R_Wrist * Q_R_Hand`

#### Frozen Terrain Policy

Stage 1 uses the Third Person flat floor as the primary locomotion comparison environment.

For that environment:

- `terrain` input is a deterministic zero-filled `256` float buffer

If terrain conditioning later becomes necessary for uneven surfaces, that is a separate scope change after the flat-floor comparison path is working.

#### Startup Logs Required

Successful startup must log all of:

- runtime chosen: `NNERuntimeORTDml` or `NNERuntimeORTCpu`
- model asset path
- input tensor count and names
- output tensor count
- `SetInputTensorShapes` success

### 5. Physics Control Integration

#### Frozen Component Ownership

The production Stage 1 character owns one `UPhysicsControlComponent` directly.

The bridge does not create the component dynamically in the production path.
The production component is pre-authored in editor with the required control and body-modifier set.

Startup must fail if the component is missing.

#### Frozen Tick Mode

`UPhysicsControlComponent` must use manual updates in Phase 1.

Required rule from the UE API contract:

- disable Physics Control component ticking
- tick the skeletal mesh first
- explicitly call `UpdateTargetCaches(...)`
- then call `UpdateControls(...)`

#### Frozen Control Space

All bridge-authored controls are parent-space controls.

Rationale:

- the PHC action space is joint-relative
- G1 showed world-space dragging is the wrong mental model for the mapped subset
- the Physics Control target orientation API is explicitly relative to the parent object

#### Frozen Controlled Subset

The root/pelvis is observed but not actuated by the policy.

The pre-authored Physics Control component must already contain named controls for these UE bones:

- `thigh_l`
- `calf_l`
- `foot_l`
- `ball_l`
- `thigh_r`
- `calf_r`
- `foot_r`
- `ball_r`
- `spine_01`
- `spine_02`
- `spine_03`
- `neck_01`
- `head`
- `clavicle_l`
- `upperarm_l`
- `lowerarm_l`
- `hand_l`
- `clavicle_r`
- `upperarm_r`
- `lowerarm_r`
- `hand_r`

That is the full Phase 1 authored control set.

#### Frozen Action Collapse Rule

The selected checkpoint exposes two distal arm joints per side:

- `L_Wrist`
- `L_Hand`
- `R_Wrist`
- `R_Hand`

Manny only provides one practical distal control body per side:

- `hand_l`
- `hand_r`

Phase 1 therefore freezes one explicit many-to-one rule:

- compose `L_Wrist` and `L_Hand` local rotations into one `hand_l` target
- compose `R_Wrist` and `R_Hand` local rotations into one `hand_r` target

Observation-side approximation:

- both `L_Wrist` and `L_Hand` use Manny `hand_l` as their current-state surrogate
- both `R_Wrist` and `R_Hand` use Manny `hand_r` as their current-state surrogate

This is the only accepted Stage 1 collapse approximation. If it materially degrades the locomotion comparison, that is an assumption failure that must be recorded explicitly.

#### Frozen Naming Scheme

All required authored names are deterministic:

- controls: `PACtrl_<BoneName>`
- body modifiers: `PAMod_<BoneName>`

The bridge must never rely on auto-generated names for production controls or modifiers.

#### Frozen Body Modifier Set

Pre-author body modifiers for:

- `pelvis`
- every controlled bone listed above that has a physics body in `PA_Mannequin`

Required startup validation bodies:

- `pelvis`
- `thigh_l`
- `calf_l`
- `foot_l`
- `ball_l`
- `thigh_r`
- `calf_r`
- `foot_r`
- `ball_r`
- `spine_01`
- `spine_02`
- `spine_03`
- `neck_01`
- `head`
- `clavicle_l`
- `upperarm_l`
- `lowerarm_l`
- `hand_l`
- `clavicle_r`
- `upperarm_r`
- `lowerarm_r`
- `hand_r`

If any required body is missing from the active skeletal mesh/physics asset pair, startup is `blocked`.
If any required control or body-modifier name is missing from the pre-authored component, startup is `blocked`.

#### Frozen Runtime Write APIs

Use these functions only:

- set angular gains:
  - `SetControlAngularData(...)`
- set target orientation:
  - `SetControlTargetOrientation(...)`
- cache refresh:
  - `UpdateTargetCaches(...)`
- push controls/modifiers:
  - `UpdateControls(...)`
- runtime reset:
  - `ResetBodyModifiersToCachedBoneTransforms(...)`
  - `SetCachedBoneVelocitiesToZero()`

Phase 1 does not use raw torque writes in the production path.
Phase 1 does not create or destroy production controls dynamically.

#### Frozen Gain Policy

The policy does not emit gains.

Stage 1 uses fixed gains authored in code and tuned manually. The bridge may update them at startup and during explicit tuning passes through:

- `SetControlAngularData(...)`

It must not invent a learned gain head or widen the ONNX contract.

#### Preconditions Before Startup Success

The bridge may report startup success only after all of these are true:

- skeletal mesh component exists
- anim instance exists
- mesh asset is Manny
- physics asset is `PA_Mannequin`
- Physics Control component exists
- all required bodies exist
- pose history node `PoseHistory_Stage1` exists
- model asset exists
- NNE runtime/model/model-instance creation succeeded
- tensor descriptors and shapes matched the locked contract
- all required authored control names were validated
- all required authored body-modifier names were validated
- one initial `UpdateTargetCaches(0.0f)` and `UpdateControls(0.0f)` pass completed without missing-body diagnostics

### 6. Tick And Threading Model

#### Frozen Tick Group

`UPhysAnimComponent` ticks in:

- `TG_PrePhysics`

#### Frozen Prerequisites

The component must add a tick prerequisite on the owning skeletal mesh component so that animation has already evaluated before the bridge calls `UpdateTargetCaches(...)`.

#### Frozen Per-Tick Order

Each render tick executes in this order:

1. skeletal mesh / anim instance tick completes
2. `UPhysAnimComponent::TickComponent(...)` begins in `TG_PrePhysics`
3. `PhysicsControl->UpdateTargetCaches(DeltaTime)`
4. read current cached animation targets from Physics Control
5. read current simulated-body state from the skeletal mesh / primitive component
6. call `UPoseSearchLibrary::MotionMatch(...)` using the live AnimInstance, `PoseHistory_Stage1`, and `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion`
7. sample the `15` future poses with `UPoseSearchAssetSamplerLibrary`
8. pack `self_obs`, `mimic_target_poses`, and `terrain`
9. call `ModelInstance->RunSync(...)`
10. unpack the `69` action floats into the collapsed UE control subset
11. write target orientations and fixed gains
12. `PhysicsControl->UpdateControls(DeltaTime)`

#### Frozen Substep Rule

- inference runs once per render tick
- Chaos substeps reuse the most recent target until the next render tick

That matches the locked Stage 1 architecture.

#### Frozen Threading Assumptions

- all bridge logic runs on the game thread in Phase 1
- the bridge does not read anim-node state directly from any-thread callbacks
- the bridge does not dispatch async inference tasks
- the bridge does not mutate Physics Control from the physics thread

### 7. Current State Read Path

The bridge gathers current runtime state with these UE calls:

- current animated targets:
  - `UPhysicsControlComponent::GetCachedBoneTransform(...)`
- current bone transforms:
  - `USkinnedMeshComponent::GetBoneTransform(...)`
  - `USkinnedMeshComponent::GetBoneQuaternion(...)`
- linear velocity:
  - `UPrimitiveComponent::GetPhysicsLinearVelocity(BoneName)`
- angular velocity:
  - `UPrimitiveComponent::GetPhysicsAngularVelocityInRadians(BoneName)`
- body validation:
  - `USkeletalMeshComponent::GetBodyInstance(BoneName)`

Use direct `FBodyInstance*` access only when the component-level APIs are insufficient for validation or diagnostics.

### 8. Frozen Feature-Construction Algorithms

The UE implementation is not allowed to invent new observation builders. It must reproduce the locked training-side builders.

#### Frozen `self_obs` Build

Per tick:

1. gather the `24` SMPL-ordered bodies from Manny in runtime order
2. use Manny `hand_l` as the surrogate current state for both `L_Wrist` and `L_Hand`
3. use Manny `hand_r` as the surrogate current state for both `R_Wrist` and `R_Hand`
4. express body positions, rotations, linear velocities, and angular velocities in the frozen SMPL ordering
5. on the flat-floor comparison map, use `ground_height = 0`
6. compute heading-inverse rotation from the pelvis/root quaternion
7. emit:
   - root height above ground: `1`
   - root-relative local body positions with root removed: `23 * 3`
   - heading-aligned body rotations in tan-norm form for all `24` bodies: `24 * 6`
   - heading-aligned body linear velocities for all `24` bodies: `24 * 3`
   - heading-aligned body angular velocities for all `24` bodies: `24 * 3`

That produces the locked `358`-float `self_obs`.

#### Frozen `mimic_target_poses` Build

Per tick:

1. call `UPoseSearchLibrary::MotionMatch(...)`
2. sample `15` future poses from the selected animation at:
   - `SelectedTime + n * (1 / 30)` seconds
   - `n = 1..15`
3. clamp each sample time to the selected asset play length
4. sample each future pose with `UPoseSearchAssetSamplerLibrary::SamplePose(...)`
5. read the `24` SMPL-ordered target body transforms from the sampled pose
6. build the reference pose stream by shifting the future stream one step back and inserting the current body state as step `0`
7. compute the locked four feature groups per future step:
   - target body position relative to the previous reference pose
   - target body position relative to the current root
   - target body rotation relative to the previous reference pose
   - target body rotation heading-aligned to the reference root
8. append the future time scalar for each sampled step

That produces the locked `15 * 433 = 6495`-float `mimic_target_poses`.

#### Frozen Frame-Conversion Rule

The bridge must use one explicit frame-conversion layer for both current-state and target-state features.

Frozen vector basis conversion:

- `UE.X = SMPL.Z`
- `UE.Y = SMPL.X`
- `UE.Z = SMPL.Y`

Frozen rotation conversion:

- `B = [[0,0,1],[1,0,0],[0,1,0]]`
- `R_ue = B * R_smpl * B^T`

The implementation must not permute quaternion components directly.

### 9. Failure Handling

#### Startup `blocked`

Treat these as startup blockers:

- missing Manny mesh
- missing `PA_Mannequin`
- missing `UPhysicsControlComponent`
- missing required physics bodies
- missing anim instance
- missing `PoseHistory_Stage1`
- missing model asset
- missing or unsupported NNE runtime
- model/session creation failure
- tensor descriptor mismatch
- `SetInputTensorShapes(...)` failure
- control or body-modifier creation failure

#### Runtime Recoverable For One Tick Only

These may hold the previous valid target for one tick, then escalate:

- a single invalid direct PoseSearch query result frame
- a single sampler failure for one future pose

If the failure persists for `2` consecutive ticks:

- stop the bridge
- reset to cached animation targets
- log runtime `fail`

#### Runtime Immediate Fail-Stop

These stop the bridge immediately:

- `RunSync(...)` failure
- control update call failure caused by missing required control names
- NaN or Inf detected in packed inputs or unpacked actions

Immediate fail-stop behavior:

1. `ResetBodyModifiersToCachedBoneTransforms(...)`
2. `SetCachedBoneVelocitiesToZero()`
3. disable bridge ticking
4. emit a clear error log

#### Comparison-Level Failure

Even if the runtime stays alive, the Stage 1 comparison is failed if:

- motion is consistently unstable
- mapped targets are obviously wrong
- the wrist/hand collapse approximation invalidates the arm chain enough to break the locomotion comparison

That is a Stage 1 viability failure, not a hidden tuning issue.

### 10. Required Test Seams

Phase 1 uses a mixed verification model, not a fake "all of Unreal is TDDable" model.

Implementation must follow this boundary:

- deterministic bridge-core logic is test-first and must exist behind seams that can run without a live Manny world
- live UE integration is verified with automation where practical, then runtime and manual evidence

The bridge is not implementation-complete if deterministic helpers land without tests first.

#### Deterministic Logic That Must Be Tested Before Code

- tensor descriptor name-to-index mapping
- fixed input/output shape validation
- `69 -> 23 x 3` action grouping
- `pd_target_i = pi * action_i` conversion
- future time schedule (`15` steps at `1/30` seconds)
- future sample-time end-of-clip clamping
- wrist-plus-hand collapse composition rule
- SMPL-to-UE basis and rotation conversion helpers
- mapped subset tables and control/body-name lookup tables
- `self_obs` packing from mocked runtime state into the locked `358`-float tensor
- `mimic_target_poses` packing from mocked future samples into the locked `6495`-float tensor
- terrain-zero path
- mapped control-name generation

#### UE Runtime Checks Required

- PoseSearch history collector validation
- direct `UPoseSearchLibrary::MotionMatch(...)` query result retrieval
- model runtime creation
- manual Physics Control update order
- required-body validation
- first successful startup pass

#### Manual Evidence Required

- G1 smoke checks already defined in [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md)
- G2 side-by-side comparison defined in [comparison-sequence-lock.md](/F:/NewEngine/plans/stage1/comparison-sequence-lock.md)

#### Minimum Runtime Logs

The bridge is not considered alive unless logs show all of:

- PoseSearch path alive:
  - pose history node found
  - valid direct search result from `UPoseSearchLibrary::MotionMatch(...)`
- NNE path alive:
  - runtime chosen
  - shapes accepted
  - first `RunSync` succeeded
- retargeting path alive:
  - mapped subset count
  - wrist/hand collapse path active
- Physics Control path alive:
  - required pre-authored control names validated
  - required pre-authored body-modifier names validated
  - first `UpdateControls` pass succeeded
- full loop alive:
  - one tick completed through `MotionMatch -> SamplePose -> RunSync -> UpdateControls`

## Required API Mapping Table

| Responsibility | UE class | UE function(s) | Notes |
|---|---|---|---|
| runtime owner | `UPhysAnimComponent` | `BeginPlay`, `TickComponent`, `EndPlay` | New plugin component; production Stage 1 owner |
| mesh prerequisite | `UActorComponent` / `USkeletalMeshComponent` | `AddTickPrerequisiteComponent` | `UPhysAnimComponent` must tick after the mesh |
| pose history validation | `UPoseSearchLibrary` | `FindPoseHistoryNode` | Must find `PoseHistory_Stage1` at startup |
| PoseSearch production query | `UPoseSearchLibrary` | `MotionMatch` | Production Stage 1 handoff owned by `UPhysAnimComponent` |
| future pose sample | `UPoseSearchAssetSamplerLibrary` | `SamplePose`, `GetTransformByName` | Used to build `mimic_target_poses` |
| current animation target read | `UPhysicsControlComponent` | `UpdateTargetCaches`, `GetCachedBoneTransform`, `GetCachedBoneTransforms` | Authoritative current-frame animated pose |
| live current-state read | `USkinnedMeshComponent`, `UPrimitiveComponent` | `GetBoneTransform`, `GetBoneQuaternion`, `GetPhysicsLinearVelocity`, `GetPhysicsAngularVelocityInRadians`, `GetBodyInstance` | Runtime body state and validation |
| runtime lookup | `UE::NNE` | `GetRuntime<INNERuntimeGPU>`, `GetRuntime<INNERuntimeCPU>` | Try DML first, CPU fallback second |
| model load | `INNERuntimeGPU` / `INNERuntimeCPU` | `CanCreateModelGPU`, `CreateModelGPU`, `CanCreateModelCPU`, `CreateModelCPU` | CPU path is fallback only |
| session creation | `UE::NNE::IModelGPU` / `UE::NNE::IModelCPU` | `CreateModelInstanceGPU`, `CreateModelInstanceCPU` | Owned by `UPhysAnimComponent` |
| input/output validation | `UE::NNE::IModelInstanceRunSync` | `GetInputTensorDescs`, `GetOutputTensorDescs`, `SetInputTensorShapes` | Validate names and fixed shapes before start |
| inference execution | `UE::NNE::IModelInstanceRunSync` | `RunSync` | One synchronous call per render tick |
| control target write | `UPhysicsControlComponent` | `SetControlTargetOrientation` | Use `DeltaTime` as angular velocity delta time |
| gain write | `UPhysicsControlComponent` | `SetControlAngularData` | Fixed gains only |
| manual Physics Control push | `UPhysicsControlComponent` | `UpdateControls` | Must run after all bridge writes |
| runtime reset | `UPhysicsControlComponent` | `ResetBodyModifiersToCachedBoneTransforms`, `SetCachedBoneVelocitiesToZero` | Used on stop/fail-stop |

## Acceptance Criteria

This spec is implementation-ready because it now freezes:

- the runtime owner
- the content paths
- the direct PoseSearch query path
- the future pose sampling path
- the NNE runtime/model/session lifecycle
- the Physics Control manual-update path on a pre-authored control set
- the exact tick ordering
- the only allowed joint-collapse approximation
- the startup, stop, and fail-stop behavior
- the required test seams and runtime evidence

If Phase 1 implementation discovers a need to change any of those decisions, that is not "just coding"; it is a spec change and must be written down before code resumes.
