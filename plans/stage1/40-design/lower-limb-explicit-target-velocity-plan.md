# Lower-Limb Explicit Target Velocity Plan

## Situation

The current best locomotion-time lower-limb baseline is:

- `foot_*` / `ball_*` only
- explicit-only target composition under speed hysteresis and dwell
- no fail-stop in deterministic movement smoke

That baseline is better than:

- whole-chain `calf_*` / `foot_*` / `ball_*` explicit-only switching
- `thigh_* + foot_* + ball_*` explicit-only switching

So the next pass should not widen the affected bone set again.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- `SetControlUseSkeletalAnimation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/SetControlsUseSk->
- `UpdateControls`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/UpdateControls>

### UE local source

- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
  - `SetControlTargetOrientation(...)` computes `ControlTarget.TargetAngularVelocity` from quaternion delta and `AngularVelocityDeltaTime`
  - if `AngularVelocityDeltaTime == 0`, the explicit target angular velocity is zeroed
- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
  - skeletal target velocity feedforward is a separate path
  - the current bridge baseline already zeros skeletal target velocity multipliers globally
  - explicit target angular velocity is still added through `Target.TargetAngularVelocity`

### ProtoMotions docs/code

- ProtoMotions docs
  - <https://nvlabs.github.io/ProtoMotions/>
- ProtoMotions GitHub
  - <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
  - the built-in PD controller computes torque from:
    - PD target position
    - current DoF position
    - current DoF velocity
  - it does not synthesize a separate target angular velocity from target-orientation deltas when using the PD-target path

## Current Read

The remaining mismatch is likely not just target frame composition.

For locomotion-time explicit-only lower-limb controls, the bridge currently does this:

1. compute a new target orientation
2. pass a nonzero `AngularVelocityDeltaTime` into `SetControlTargetOrientation(...)`
3. let UE synthesize a new explicit target angular velocity from the orientation delta

ProtoMotions' PD-target path does not appear to have an equivalent explicit target angular velocity term for this control mode.

So lower-limb explicit-only mode may still be injecting an untrained velocity-like feedforward term during locomotion.

## Hypothesis

The remaining lower-limb locomotion spikes are partly caused by synthesized explicit target angular velocity on `foot_*` / `ball_*`.

If that is true, then:

- keep the current explicit-only composition transition policy
- but zero explicit target angular velocity for the affected lower-limb controls
- and movement smoke should stay stable while reducing the worst locomotion spikes

## Experiment

Keep the existing locomotion-time transition policy unchanged:

- `foot_*`
- `ball_*`
- enter `50 cm/s`
- exit `100 cm/s`
- enter hold `0.20 s`
- exit hold `0.20 s`

Change only this:

- when writing target orientations for those lower-limb explicit-only controls while the mode is active
- pass `AngularVelocityDeltaTime = 0.0f`
- so UE does not synthesize explicit target angular velocity from the orientation delta

All other controls keep the existing target-write timing behavior.

## Success Criteria

The pass is a win if deterministic movement smoke shows:

- no fail-stop
- no locomotion-start discontinuity
- forward and backward spikes no worse than the current transition-policy baseline
- reduced worst-case `foot_*` / `ball_*` angular spikes

## Failure Interpretation

- if this improves the distal spikes:
  - keep it and re-evaluate perturbation/G2 under movement
- if it regresses or only shifts peaks upward without reducing them:
  - revert and move to another locomotion-time seam, most likely per-bone target smoothing or write timing rather than explicit target angular velocity

## Result

- implemented exactly the planned seam:
  - kept the committed `foot_*` / `ball_*` hysteresis+dwell locomotion-composition baseline
  - zeroed `AngularVelocityDeltaTime` only for those distal controls while that mode is active
- verification:
  - UE build passed
  - `PhysAnim.Component` passed
  - `PhysAnim.PIE.MovementSmoke` passed
  - no fail-stop
- measured runtime result:
  - first forward spikes dropped materially into the low-thousands range instead of the prior high-four-digit baseline
  - deterministic movement smoke completed cleanly
  - lower-limb limit occupancy stayed mostly around `~0.9x - 1.1x`
  - remaining peaks still migrate proximally in some forward/backward samples, especially `calf_*`, `thigh_*`, and occasional `foot_*`
- current read:
  - synthesized explicit target angular velocity was a real remaining mismatch surface
  - this is a keepable improvement to the locomotion-time distal baseline
  - the next pass should inspect per-bone write smoothing or another proximal lower-limb seam, not re-open whole-chain explicit-only switching
