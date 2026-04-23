# Lower-Limb Composition Transition Policy Plan

## Situation

The latest lower-limb composition passes established:

- `foot_*` / `ball_*` explicit-only locomotion composition improves backward movement versus the older attenuation baseline.
- Expanding explicit-only mode to the full `calf_*` / `foot_*` / `ball_*` chain is not a clean win:
  - it keeps movement smoke stable
  - but it produces mixed forward/backward results and one materially worse forward outlier

So the next pass should not widen the affected bone set again.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- `UPhysicsControlComponent::SetControlsUseSkeletalAnimation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/SetControlsUseSk->
- `UPhysicsControlComponent::UpdateControls`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/UpdateControls>
- Blueprint `Set Controls Use Skeletal Animation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/PhysicsControl/SetControlsUseSkeletalAnimation>

### UE local source

- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
  - `SetControlUseSkeletalAnimation(...)` writes the control mode directly; there is no built-in transition blend
- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
  - if `bUseSkeletalAnimation` is enabled, the skeletal target transform and skeletal target velocities are built from cached skeletal pose data
  - the explicit target is then applied in the space of that skeletal target transform

### ProtoMotions online docs

- ProtoMotions docs
  - <https://nvlabs.github.io/ProtoMotions/>
- ProtoMotions GitHub
  - <https://github.com/NVlabs/ProtoMotions>

### ProtoMotions local code

- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
  - actions are mapped into PD targets in simulator joint space
  - there is no runtime representation flip between animation-composed and explicit-only target modes
- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)
  - PD target ranges are derived directly in joint DoF space

## Current Read

UE PhysicsControl mode switching is binary:

- skeletal-animation-composed target mode on
- or off

There is no built-in blend for that runtime mode switch.

ProtoMotions does not perform an equivalent runtime representation flip at locomotion onset. It stays in one control representation and only changes the PD targets themselves.

So the remaining issue is likely not just *which* lower-limb controls are in explicit-only mode. It is also *when and how* the bridge flips them.

## Hypothesis

The current locomotion composition policy is too abrupt.

A raw speed-threshold flip can:

- switch target composition on the same tick that locomotion intent appears
- instantly stop using skeletal-derived target velocities
- instantly change the target frame relationship for the affected controls

That is a plausible explanation for the mixed and sometimes explosive locomotion samples.

## Experiment

Revert the affected set to the last better structure:

- `foot_l`
- `ball_l`
- `foot_r`
- `ball_r`

Then add stateful transition handling for locomotion composition:

- keep the existing activation threshold:
  - enter threshold: `50 cm/s`
- exit threshold: `100 cm/s`
- enter hold: `0.20 s`
- exit hold: `0.20 s`

Behavior:

- explicit-only locomotion composition becomes active only after sustained motion above the enter threshold
- it remains active until motion has stayed below the exit threshold for the exit hold
- the rest of the body stays on the current policy-active skeletal-target composition path

## Success Criteria

The pass is good if deterministic movement smoke shows:

- no fail-stop
- no regression in movement completion
- lower forward and backward outliers than the full-chain composition pass
- no locomotion-start discontinuity
- no rapid mode flapping near low-speed transitions

## Failure Interpretation

- if this improves the worst outliers:
  - keep the transition policy and re-evaluate perturbation/G2 under movement
- if results remain mixed with stable mode occupancy:
  - the next pass should inspect more proximal lower-limb composition or target-velocity handling
- if the main issue becomes mode flapping anyway:
  - increase hysteresis or hold times before trying another structural change

## Result

Implemented and tested:

- affected controls reverted to:
  - `foot_l`
  - `ball_l`
  - `foot_r`
  - `ball_r`
- stateful locomotion composition transition policy added:
  - enter threshold `50 cm/s`
  - exit threshold `100 cm/s`
  - enter hold `0.20 s`
  - exit hold `0.20 s`

Measured result in deterministic movement smoke:

- no fail-stop
- movement completion remains green
- no locomotion-start discontinuity
- materially better than the failed full-chain composition pass

Observed runtime behavior:

- forward is much more bounded than the full-chain pass:
  - first forward sample peaks around `ball_l:509.5 deg/s`
  - later forward samples still show some large peaks, for example `foot_r:1716.2 deg/s`
- backward is also improved versus the full-chain pass:
  - late backward still has large peaks, but now commonly in the `~1485 - 3203 deg/s` range rather than the prior `~9176 deg/s` distal outlier
- the remaining spikes often migrate proximally into `thigh_*` or non-lower-limb bodies rather than disappearing entirely

Current read:

- the stateful transition policy is the best measured locomotion-composition result so far
- abrupt runtime representation flips were part of the problem
- the next pass should inspect proximal lower-limb composition or target-velocity handling, not widen the distal explicit-only set again
