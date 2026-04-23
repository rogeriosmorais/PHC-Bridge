# Mimic Target Data-Origin Alignment Plan

## Goal

Align `mimic_target_poses` more closely with ProtoMotions' data-relative target-pose contract by removing the remaining UE world-origin dependency from the current reference and future target samples.

## Direction Check

As of March 12, 2026, it is still worth continuing in the broader training/runtime alignment direction.

Why:

- recent passes are still finding objective representation-contract mismatches
- those fixes are cheap to verify and do not require speculative asset retuning

What is not worth returning to:

- lower-limb write smoothing
- target-step caps
- more isolated thigh/calf multiplier reshuffling
- more contact-exclusion tweaking

## Why This Pass

In local ProtoMotions `mimic_obs.py`, the current reference used to build `mimic_target_poses` is normalized back into the motion-data frame by:

1. subtracting terrain height from current body positions
2. subtracting `respawn_offset_relative_to_data` from current XY

The future target poses themselves come from `motion_lib` in that same data-relative frame.

The UE bridge was still sampling future pose targets with:

- `RootTransformOrigin = SkeletalMesh->GetComponentTransform()`

which makes the future targets world-origin dependent. After the previous pass, the current reference was terrain-relative in `Z` but the future targets were still sampled in world space, so the two sides were still not fully in the same frame.

## Sources Checked

### Online docs

- UE PoseSearch asset sampler library:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PoseSearch/UPoseSearchAssetSamplerLibrary>
- UE `UCharacterMovementComponent`:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/GameFramework/UCharacterMovementComponent>
- ProtoMotions configuration guide:
  <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo:
  <https://github.com/NVlabs/ProtoMotions>

### Local UE source

- [PoseSearchAssetSamplerLibrary.cpp](/E:/UE_5.7/Engine/Plugins/Animation/PoseSearch/Source/Runtime/Private/PoseSearchAssetSamplerLibrary.cpp)
- [PoseSearchAssetSampler.cpp](/E:/UE_5.7/Engine/Plugins/Animation/PoseSearch/Source/Runtime/Private/PoseSearchAssetSampler.cpp)

### Local ProtoMotions code

- [mimic_obs.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/components/mimic_obs.py)
- [mimic_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/mimic_utils.py)
- [env.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/env.py)

## Hypothesis

The Stage 1 bridge should make `mimic_target_poses` data-relative in the same broad way ProtoMotions does:

- sample future target poses with `RootTransformOrigin = Identity`
- derive a Stage 1 proxy for Proto's `respawn_offset_relative_to_data` from the current selected pose:
  - sample the selected current animation root once in world-origin mode
  - sample the same selected current animation root once in identity-origin mode
  - subtract the two to get the XY data-origin offset UE sampling was adding
- subtract that XY offset from the current body reference samples
- keep the already-correct terrain-relative `Z` normalization for the current reference

This is a Stage 1 proxy, not a claim that UE has Proto's full respawn system.

## Success Criteria

- `PhysAnim.Component` stays green
- `PhysAnim.PIE.MovementSmoke` stays green
- if movement remains mixed, keep the pass anyway if it is an objective contract improvement and non-regressing
