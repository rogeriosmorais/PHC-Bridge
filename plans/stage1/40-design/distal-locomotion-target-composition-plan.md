# Distal Locomotion Target Composition Plan

## Situation

The lower-limb alignment work has established:

- policy cadence is aligned to the pretrained ProtoMotions control cadence
- mass distribution and family PD profile now have explicit Stage 1 baselines
- lower-limb target-range shaping helps materially
- locomotion-time distal attenuation helps somewhat in early forward motion
- the remaining large lower-limb spikes still concentrate in `foot_*` / `ball_*`, especially during backward and strafe phases

So the current problem is no longer broad lower-limb occupancy. It is the way distal lower-limb targets are being composed during locomotion.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`:
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- `FPhysicsControlData`:
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlData>

### UE local source

- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
  - explicit control targets are applied in the space of the skeletal target transform
  - `bUseSkeletalAnimation` changes whether the control target is combined with animation or used directly
- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
  - `SetControlUseSkeletalAnimation(...)` exists and can be changed per control at runtime

### ProtoMotions online docs

- ProtoMotions docs:
  - <https://nvlabs.github.io/ProtoMotions/>
- ProtoMotions GitHub:
  - <https://github.com/NVlabs/ProtoMotions>

### ProtoMotions local code

- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)
  - `map_actions_to_pd_range = true`
  - `use_biased_controller = false`
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
  - actions are converted into PD targets in simulator joint space
- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)
  - `build_pd_action_offset_scale(...)` works in joint DoF space before simulator PD targets are applied

### Current bridge code

- [PhysAnimStage1InitializerComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStage1InitializerComponent.cpp)
  - authored controls default to `bUseSkeletalAnimation = false`
- [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp)
  - live policy currently switches all controls into skeletal-animation target representation when policy influence is active
  - locomotion-time distal attenuation currently reduces `foot_*` / `ball_*` target magnitude but still leaves them in the same target composition mode

## Current Read

ProtoMotions policy targets are absolute PD targets in simulator joint space.

UE PhysicsControl, when `bUseSkeletalAnimation = true`, applies the authored explicit target on top of the skeletal animation target transform. That means:

- if the bridge provides an absolute lower-limb policy target as the explicit target
- and PhysicsControl also composes it with a locomotion animation target
- then distal lower-limb controls can be effectively double-driven during dynamic locomotion

This would be most visible in `foot_*` / `ball_*`, which is exactly where the remaining movement spikes concentrate.

## Hypothesis

The remaining distal locomotion mismatch is target composition mode, not just target magnitude.

Specifically:

- `foot_*` and `ball_*` should not remain in skeletal-animation-composed control mode during active locomotion
- they should switch to explicit-only target mode while locomotion is active
- the rest of the body can keep the current policy-active skeletal-target composition

## First Experiment

Add a movement-time distal composition policy:

- activation threshold:
  - planar owner speed `> 50 cm/s`
- affected controls:
  - `foot_l`
  - `ball_l`
  - `foot_r`
  - `ball_r`
- behavior:
  - when below threshold:
    - keep current global skeletal-target composition behavior
  - when above threshold:
    - force those distal controls to `bUseSkeletalAnimation = false`
    - leave the rest of the controls unchanged

Do not combine this pass with more gain changes, mass changes, or limit changes.

## Success Criteria

The pass is good if deterministic movement smoke shows:

- no fail-stop
- no regression in total movement completion
- lower backward/strafe distal `ball_*` angular spikes than the current locomotion-time attenuation baseline
- no new first-frame discontinuity when locomotion starts

## Failure Interpretation

- if backward/strafe distal spikes drop:
  - keep the per-control composition policy and re-evaluate G2 locomotion-coupled perturbation
- if the pass introduces jumps at locomotion start:
  - target composition is likely the right surface, but transition handling needs its own follow-up
- if there is little or no change:
  - the next pass should inspect a deeper distal representation mismatch, not more runtime composition toggles

## First Pass Result

Implemented and tested:

- locomotion threshold: `50 cm/s`
- affected controls:
  - `foot_l`
  - `ball_l`
  - `foot_r`
  - `ball_r`
- behavior:
  - when above threshold, those controls force `bUseSkeletalAnimation = false`
  - the rest of the body keeps the current policy-active skeletal-target composition

Measured result in deterministic movement smoke:

- no fail-stop
- movement completion remains green
- backward peaks improve materially versus the locomotion-time attenuation baseline
- strafe remains mixed:
  - some distal spikes reduce
  - some peaks shift up the chain into `foot_*`, `calf_*`, or even `thigh_*`

Current read:

- target composition mode is a real part of the remaining lower-limb mismatch
- distal explicit-only mode under locomotion is a useful direction
- but `foot_*` / `ball_*` alone are not enough for a clean new baseline
- the next pass should inspect whether the explicit-only composition policy should expand to the full knee/ankle/toe chain or whether the locomotion transition itself needs dedicated handling
