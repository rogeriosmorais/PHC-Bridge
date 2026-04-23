# Runtime World-Frame Alignment Plan

## Goal

Test whether the remaining locomotion-time lower-limb instability is partly caused by using the old SMPL authoring-frame axis remap for ProtoMotions runtime world data.

This pass is intentionally narrow:

- change observation/future-target world-frame conversion only
- keep local action-to-control rotation conversion unchanged
- verify with component automation and movement smoke

## Direction Check

As of March 12, 2026, it is still worth continuing in the broader training/runtime alignment direction.

What is no longer worth treating as the primary lever:

- more lower-limb write smoothing
- more lower-limb step-cap tuning
- more contact-exclusion tweaks
- more isolated thigh/calf multiplier reshuffling

The current highest-value open seam is the runtime world-frame contract for:

- `self_obs`
- `mimic_target_poses`

## Why This Pass

ProtoMotions runtime world data is clearly `z-up`:

- Isaac Gym simulator config sets `up_axis = z`
- gravity is on `z`
- ground plane normal is `(0, 0, 1)`
- `compute_humanoid_observations_max(...)` uses `root_pos[:, 2]` as height and works in the XY heading plane

The local ProtoMotions state-conversion path reorders quaternions and bodies, but it does not perform any world-axis remap.

Our UE bridge still uses the older SMPL-authoring remap helpers for runtime world data:

- `UeVectorToSmpl(...)`
- `UeQuaternionToSmpl(...)`

Those helpers were originally introduced to bridge the SMPL local-joint authoring frame, not necessarily the Proto runtime world frame.

## Sources Checked

### Online docs

- UE `UPhysicsControlComponent`:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- UE `FPhysicsControlData`:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlData>
- ProtoMotions config guide:
  <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo:
  <https://github.com/NVlabs/ProtoMotions>

### Local UE source

- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)

### Local ProtoMotions code

- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)
- [mimic_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/mimic_utils.py)
- [robot_state.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/robot_state.py)
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/isaacgym/simulator.py)
- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)

## Hypothesis

Proto runtime world data should not use the old `Y-up -> Z-up` remap.

The first narrow experiment is:

- add explicit Proto-runtime-world conversion helpers
- make them identity-frame helpers for positions, world rotations, and world velocities
- apply them only to:
  - `GatherCurrentBodySamples(...)`
  - `SampleFuturePoses(...)`
- leave local action rotation conversion on the existing SMPL authoring helpers

## Success / Failure Criteria

Keep the change only if all of the following are true:

1. `PhysAnim.Component` stays green
2. `PhysAnim.PIE.MovementSmoke` stays green
3. locomotion-time lower-limb outliers improve or at least do not materially worsen
4. no new startup discontinuity or fail-stop appears

If forward/backward/strafe materially regress, restore the last safe baseline and keep only the documentation of the failed pass.
