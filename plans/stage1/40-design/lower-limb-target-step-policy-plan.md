# Lower-Limb Target-Step Policy Plan

## Decision

It is still worth continuing in the broader training/runtime alignment direction.

It is **not** worth continuing the narrower sub-direction of repeated lower-limb response-multiplier reshuffling as the main strategy.

Why:

- cadence alignment produced a real improvement
- lower-limb target-range alignment produced a real improvement
- distal explicit target angular-velocity suppression produced a real improvement
- the corrected shell-coupling audit ruled out the wrong pivot

So the direction is still productive, but the next pass should target another lower-limb locomotion-time seam, not more arbitrary gain splitting.

## Current Situation

The current best runtime baseline is:

- `30 Hz` policy cadence
- training-aligned family mass policy
- toe-family reassignment
- knee/ankle-chain target-range policy
- distal range shaping
- locomotion-time distal explicit-only composition with hysteresis/dwell
- distal explicit target angular-velocity suppression
- shared proximal lower-limb response profile

The remaining issue:

- movement smoke is stable
- no fail-stop
- but backward and late locomotion still produce lower-limb outliers, often around `foot_*`, `ball_*`, `calf_*`, and sometimes `thigh_*`

The corrected shell audit shows this is **not** primarily a gameplay-shell coupling problem.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- `FPhysicsControlData`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlData>
- `ACharacter`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/GameFramework/ACharacter>
- `UCharacterMovementComponent`: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/GameFramework/UCharacterMovementComponent>

### Local UE source

- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
- [CharacterMovementComponent.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/Components/CharacterMovementComponent.cpp)
- [SkeletalMeshComponent.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/SkeletalMeshComponent.h)

### ProtoMotions docs and code

- ProtoMotions docs: <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo: <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)
- [smpl_humanoid.xml](/F:/NewEngine/Training/ProtoMotions/protomotions/data/assets/mjcf/smpl_humanoid.xml)
- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)

## Why This Next Pass

The current logs show a useful constraint:

- surviving locomotion outliers still happen even when the written per-step target deltas are already small
- but the bridge still uses one global `MaxAngularStepDegreesPerSecond`
- Manny lower-limb limits are much tighter than the training asset
- ProtoMotions still assumes coherent PD target evolution inside those control intervals

So the remaining mismatch may be that lower-limb targets need a tighter locomotion-time step envelope than the rest of the body.

## Experiment

Add a locomotion-time lower-limb target-step policy that scales down the per-bone angular step cap only for:

- `thigh_*`
- `calf_*`
- `foot_*`
- `ball_*`

The cap should get tighter distally.

## Diagnostics Requirement

Add explicit diagnostics for lower-limb target-step occupancy:

- max occupancy bone
- max occupancy ratio
- effective step limit in degrees
- mean occupancy

This makes the pass falsifiable:

- if occupancy stays low and locomotion spikes do not improve, then target-step smoothing is probably not the next winning seam
- if occupancy is high and the spikes improve materially, keep it

## Success Criteria

- movement smoke stays green with no fail-stop
- forward/backward lower-limb angular peaks improve materially versus the current baseline
- new late idle or strafe regressions do not outweigh the gain

## Failure Criteria

- no material improvement in the remaining lower-limb outliers
- clear regression in forward, backward, idle, or strafe stability
- diagnostics show target-step occupancy is already too low for this seam to matter

## Decision Rule

If this pass is not a clean win, the next step should move away from more lower-limb step/response tuning and toward another representation seam rather than continuing to reshuffle multipliers.
