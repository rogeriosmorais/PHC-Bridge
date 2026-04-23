# Mimic Target Current-Reference Ground Plan

## Goal

Align the current-state reference used to build `mimic_target_poses` with the terrain-relative current-state normalization used by ProtoMotions.

## Direction Check

As of March 12, 2026, it is still worth continuing in the broader training/runtime alignment direction.

Why:

- recent passes are still finding objective contract mismatches
- those passes are cheap to validate and do not require speculative asset retuning

What is not worth returning to:

- more lower-limb write smoothing
- more isolated lower-limb multiplier reshuffling
- more contact-exclusion or step-cap tuning

## Why This Pass

In local ProtoMotions `mimic_obs.py`, before building `mimic_target_poses`, the current body state is normalized by:

1. subtracting terrain height from all current body positions
2. subtracting `respawn_offset_relative_to_data` from current XY

The UE bridge currently builds `mimic_target_poses` from raw current world body samples.

We do not yet have a clean Stage 1 equivalent of Proto’s full `respawn_offset_relative_to_data` concept, so this pass only aligns the terrain-relative current-state part.

## Sources Checked

### Online docs

- UE `UCharacterMovementComponent`:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/GameFramework/UCharacterMovementComponent>
- UE PoseSearch asset sampler library:
  <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PoseSearch/UPoseSearchAssetSamplerLibrary>
- ProtoMotions configuration guide:
  <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo:
  <https://github.com/NVlabs/ProtoMotions>

### Local UE source

- [CharacterMovementComponent.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp)
- [PoseSearchAssetSamplerLibrary.cpp](/E:/UE_5.7/Engine/Plugins/Animation/PoseSearch/Source/Runtime/Private/PoseSearchAssetSamplerLibrary.cpp)

### Local ProtoMotions code

- [mimic_obs.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/components/mimic_obs.py)
- [mimic_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/mimic_utils.py)
- [env.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/mimic/env.py)

## Hypothesis

The current reference body samples used by `BuildMimicTargetPoses(...)` should be terrain-relative in `Z`, just like Proto’s current-state path.

That means:

- copy `CurrentBodySamples`
- subtract the current floor `world Z` from every copied body sample position `Z`
- leave future pose samples unchanged in this pass
- keep self-observation and local action conversion unchanged

## Success Criteria

- `PhysAnim.Component` stays green
- `PhysAnim.PIE.MovementSmoke` stays green
- first-policy/current-reference diagnostics do not regress materially
- if locomotion remains mixed, keep the pass anyway if the contract alignment is objectively correct and non-regressing
