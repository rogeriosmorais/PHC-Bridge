# Self-Observation Channel Breakdown Plan

## Status

- `date`: March 12, 2026
- `owner`: AI orchestrator / implementation pass
- `scope`: bridge trace observability only
- `out of scope`: runtime behavior changes, asset-authored tuning

## Why This Pass Exists

The last direct `self_obs` runtime tweak was falsified:

- movement stayed stable
- but active locomotion maxima regressed broadly
- `self_observation_mean_abs` did not materially improve

That means the next useful move is not another guessed runtime adjustment. We need to know which `self_obs` channel family is actually spiking during locomotion.

## Review Summary

### ProtoMotions

The active `max_coords` self-observation path packs:

1. root height
2. local body positions (root removed)
3. local body rotations as tan-norm
4. local body linear velocities
5. local body angular velocities

Relevant files:

- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)
- [humanoid_obs.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/components/humanoid_obs.py)
- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)

### UE Bridge

The bridge already matches that high-level layout in [PhysAnimBridge.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp), but the current trace only records:

- `self_observation_root_height`
- `self_observation_mean_abs`

That is too coarse to isolate the remaining locomotion-time mismatch.

## Plan

Add per-family `self_obs` breakdown fields to the bridge trace:

- `self_observation_local_position_mean_abs`
- `self_observation_local_rotation_mean_abs`
- `self_observation_local_linear_velocity_mean_abs`
- `self_observation_local_angular_velocity_mean_abs`

Implementation steps:

1. Add the new fields to `FPhysAnimBridgeTraceFrame`.
2. Update the CSV header and row serialization.
3. Add a pure helper in the component to summarize the packed `self_obs` families by fixed index ranges.
4. Populate those fields during policy-step trace emission.
5. Update trace regression tests to lock the new schema.

## Success Criteria

- no runtime behavior change
- build/test/smoke stay green
- movement traces now expose which `self_obs` family spikes during locomotion

## Decision Boundary

Keep this pass if:

- verification stays green
- the new trace fields are present and populated

Reject this pass only if:

- schema/test changes break trace coverage
- or trace emission regresses the current movement smoke baseline
