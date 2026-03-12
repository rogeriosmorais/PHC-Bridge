# Lower-Limb Chain Locomotion Composition Plan

## Situation

The current locomotion-time target-alignment work has established:

- ProtoMotions is producing absolute PD targets in simulator joint space.
- UE PhysicsControl combines skeletal-animation targets with explicit control targets when `bUseSkeletalAnimation = true`.
- Distal-only explicit target mode for `foot_*` / `ball_*` during locomotion materially improves backward motion, but strafe remains mixed.
- In the latest deterministic movement smoke, several of the remaining peaks shift up the lower-limb chain into `calf_*` and occasionally `thigh_*`, rather than disappearing.

That means the previous `foot_*` / `ball_*`-only pass was directionally correct, but too narrow.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent::SetControlsUseSkeletalAnimation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/SetControlsUseSk->
- `UPhysicsControlComponent::UpdateControls`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/UpdateControls>
- `Set Control Use Skeletal Animation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/PhysicsControl/SetControlUseSkeletalAnimation>

### UE local source

- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
  - when `bUseSkeletalAnimation` is enabled, the target transform and extracted target velocities are built from tracked skeletal pose data before the explicit control target is applied
- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
  - `SetControlUseSkeletalAnimation(...)` is a supported per-control runtime toggle

### ProtoMotions online docs

- ProtoMotions docs
  - <https://nvlabs.github.io/ProtoMotions/>
- ProtoMotions GitHub
  - <https://github.com/NVlabs/ProtoMotions>

### ProtoMotions local code

- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)
  - `map_actions_to_pd_range = true`
  - `use_biased_controller = false`
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
  - actions are mapped into PD targets in simulator joint space
- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)
  - `3-DoF` joints use symmetric `1.2x` range expansion

## Current Read

The remaining locomotion instability is no longer broad lower-limb occupancy. The evidence now points at representation mismatch inside the full knee/ankle/toe chain during dynamic locomotion:

- `calf_*` still sits on the composed skeletal-target path
- `foot_*` / `ball_*` already moved to explicit-only mode in the previous pass
- under strafe and later backward samples, peaks now commonly migrate upward into `calf_*`

That pattern is consistent with a partial representation split across one articulated chain:

- parent joint still using skeletal-animation-composed targets
- child joints using explicit-only targets

This is likely still a mismatch relative to ProtoMotions, where the full lower-limb joint chain receives simulator-space PD targets coherently.

## Hypothesis

During active locomotion, `calf_*`, `foot_*`, and `ball_*` should all switch together to explicit-only target mode.

Keeping `calf_*` on the composed path while `foot_*` / `ball_*` are explicit-only is probably leaving a mixed target representation inside one coupled lower-limb chain.

## Experiment

Keep the existing locomotion threshold and widen the affected set:

- activation threshold:
  - planar owner speed `> 50 cm/s`
- affected controls:
  - `calf_l`
  - `foot_l`
  - `ball_l`
  - `calf_r`
  - `foot_r`
  - `ball_r`
- behavior:
  - above threshold:
    - force those controls to `bUseSkeletalAnimation = false`
  - below threshold:
    - preserve the current global control mode

Do not combine this pass with:

- further gain tuning
- further target attenuation changes
- constraint authoring edits
- mass changes

## Success Criteria

The pass is good if deterministic movement smoke shows:

- no fail-stop
- no regression in movement completion
- lower strafe and backward lower-limb peaks than the current `foot_*` / `ball_*`-only baseline
- no new locomotion-start discontinuity

## Failure Interpretation

- if strafe and backward both improve:
  - keep the widened lower-limb composition policy
- if peaks merely migrate further up into `thigh_*`:
  - the next surface is locomotion transition handling or proximal lower-limb composition
- if the pass introduces a locomotion-start jump:
  - composition is the correct surface, but transition hysteresis or ramping is needed

## Result

Implemented and tested:

- locomotion threshold unchanged at `50 cm/s`
- affected controls widened to:
  - `calf_l`
  - `foot_l`
  - `ball_l`
  - `calf_r`
  - `foot_r`
  - `ball_r`

Measured result in deterministic movement smoke:

- no fail-stop
- movement completion remains green
- no new locomotion-start discontinuity
- but this is not a clean improvement over the previous `foot_*` / `ball_*`-only pass

Observed runtime behavior:

- forward is mixed and includes a materially worse outlier:
  - one forward sample spikes `ball_l` to about `10104 deg/s`
- backward remains mixed:
  - some samples are still lower than the older attenuation baseline
  - but large late backward peaks remain, including `ball_r` around `9176 deg/s`
- lower-limb occupancy remains centered on `calf_*` around `~1.1x - 1.4x` during active backward/forward phases

Current read:

- widening explicit-only mode to the whole knee/ankle/toe chain does not finish the problem
- the representation surface is still real, but the remaining issue is probably not just which lower-limb bones are toggled
- the next pass should inspect locomotion transition handling or more proximal lower-limb target composition, not just more distal-set expansion
