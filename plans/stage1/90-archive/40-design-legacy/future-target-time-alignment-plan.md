# Future Target Time Alignment Plan

## Goal

Align the `mimic_target_poses` time channel with the actual clamped future sample times used by ProtoMotions.

## Direction Check

As of March 12, 2026, it is still worth continuing in the broader training/runtime alignment direction.

What is still worth doing:

- objective contract fixes between ProtoMotions observation/target packing and the UE bridge
- narrow runtime representation fixes that are directly grounded in ProtoMotions code

What is not worth treating as the next primary lever:

- more lower-limb write smoothing
- more lower-limb step-cap tuning
- more isolated thigh/calf/toe multiplier reshuffling

## Why This Pass

ProtoMotions `mimic_target_pose.with_time` is enabled in the active pretrained checkpoint.

In the local ProtoMotions implementation:

- future motion times are clipped to the motion length
- the final appended per-step time value is the actual clamped time-to-target, not the nominal schedule

In the current UE bridge:

- future animation sample time is clamped to animation length
- but `FutureTimeSeconds` still stores the nominal offset

That means late future steps near the end of a clip can carry:

- repeated/clamped target transforms
- but still report the original larger future time delta

That is a direct contract mismatch.

## Sources Checked

### Online docs

- UE PoseSearch asset sampler input:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Utilities/Struct/MakePoseSearchAssetSamplerInput>
- UE PoseSearch asset sampler library:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PoseSearch/UPoseSearchAssetSamplerLibrary>
- ProtoMotions configuration guide:
  <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo:
  <https://github.com/NVlabs/ProtoMotions>

### Local UE source

- [PoseSearchAssetSampler.cpp](/E:/UE_5.7/Engine/Plugins/Animation/PoseSearch/Source/Runtime/Private/PoseSearchAssetSampler.cpp)
- [PoseSearchAssetSamplerLibrary.cpp](/E:/UE_5.7/Engine/Plugins/Animation/PoseSearch/Source/Runtime/Private/PoseSearchAssetSamplerLibrary.cpp)

### Local ProtoMotions code/config

- [mimic_obs.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/components/mimic_obs.py)
- [mimic_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/mimic_utils.py)
- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)

## Hypothesis

The UE bridge should emit:

`FutureTimeSeconds = ClampedSampleTime - CurrentSelectedTime`

not:

`FutureTimeSeconds = NominalFutureOffset`

## Implementation

1. Add a pure helper that resolves the effective future sample time after end-of-clip clamping.
2. Use that helper in `SampleFuturePoses(...)`.
3. Leave the actual future offset schedule unchanged.
4. Add automation coverage for:
   - unclamped case
   - end-clamped case
5. Rebuild and rerun:
   - `PhysAnim.Component`
   - `PhysAnim.PIE.MovementSmoke`

## Success Criteria

- bridge/runtime remains stable
- the time channel in `mimic_target_poses` matches ProtoMotions semantics
- no regression in movement smoke

Even if the locomotion gains are small, keep the fix if the contract alignment is objectively correct and non-regressing.
