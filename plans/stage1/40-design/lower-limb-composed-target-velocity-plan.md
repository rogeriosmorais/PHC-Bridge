# Lower-Limb Composed Target Velocity Plan

## Situation

The current best measured locomotion-time baseline is:

- `foot_*` / `ball_*` only
- explicit-only locomotion composition under hysteresis+dwell
- explicit target angular velocity suppressed for those distal controls while that mode is active

That pass was a real improvement:

- no fail-stop in deterministic movement smoke
- first forward distal spikes dropped materially
- lower-limb occupancy stayed mostly around `~0.9x - 1.1x`

But the remaining worst spikes now often migrate proximally into:

- `calf_*`
- `thigh_*`
- occasional `foot_*`

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- `SetControlsUseSkeletalAnimation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/SetControlsUseSk->
- `SetControlTargetPositionAndOrientation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/SetControlTarget->

### UE local source

- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
  - `SetControlTargetOrientation(...)` synthesizes explicit target angular velocity from target-orientation deltas unless `AngularVelocityDeltaTime == 0`
- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
  - skeletal-target angular velocity is one path
  - explicit target angular velocity is added separately through `Target.TargetAngularVelocity`
  - that explicit term is still added even when skeletal-animation targets are being used

### ProtoMotions docs/code

- ProtoMotions docs
  - <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo/docs
  - <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
  - PD-target torque is computed from:
    - target position
    - current DoF position
    - current DoF velocity
  - the PD-target path does not expose a separate explicit target angular-velocity term analogous to UE’s synthesized `Target.TargetAngularVelocity`

## Current Read

The last pass only suppressed synthesized explicit target angular velocity for the distal explicit-only controls:

- `foot_*`
- `ball_*`

That reduced the distal spikes.

The remaining large locomotion spikes now cluster more often in:

- `calf_*`
- `thigh_*`

Those controls are still using the composed skeletal-target path, but UE still adds explicit target angular velocity for them through the control target record.

So the next mismatch may be:

- not target magnitude
- not locomotion composition membership
- but the remaining explicit target angular-velocity term on the proximal lower-limb controls

## Hypothesis

The remaining locomotion-time lower-limb spikes are partly driven by synthesized explicit target angular velocity on the rest of the lower-limb chain.

If that is true, then:

- keep the current locomotion composition baseline unchanged
- suppress synthesized explicit target angular velocity for:
  - `thigh_*`
  - `calf_*`
  - `foot_*`
  - `ball_*`
  while locomotion mode is active
- and movement smoke should stay stable while reducing the worst proximal lower-limb spikes

## Experiment

Keep unchanged:

- `foot_*` / `ball_*` explicit-only locomotion composition
- speed hysteresis+dwell
- distal range shaping
- current cadence baseline

Change only this:

- when locomotion mode is active
- zero explicit target angular-velocity synthesis for the whole lower-limb chain:
  - `thigh_*`
  - `calf_*`
  - `foot_*`
  - `ball_*`

This means `SetControlTargetOrientation(...)` should get `AngularVelocityDeltaTime = 0.0f` for those bones during locomotion.

## Success Criteria

The pass is a win if deterministic movement smoke shows:

- no fail-stop
- no locomotion-start discontinuity
- forward/backward peaks no worse than the current distal-suppression baseline
- reduced worst-case `calf_*` / `thigh_*` spikes

## Failure Interpretation

- if it improves proximal spikes without reintroducing distal regressions:
  - keep it and move to perturbation/G2 re-evaluation under movement
- if it regresses or shifts spikes upward without a net win:
  - revert and move to another seam, most likely per-bone target-write smoothing rather than more angular-velocity suppression

## Result

- implemented exactly the planned seam:
  - kept the committed `foot_*` / `ball_*` hysteresis+dwell baseline
  - widened explicit target angular-velocity suppression to:
    - `thigh_*`
    - `calf_*`
    - `foot_*`
    - `ball_*`
- verification:
  - UE build passed
  - `PhysAnim.Component` passed
  - `PhysAnim.PIE.MovementSmoke` passed
  - no fail-stop
- measured runtime result:
  - forward was mixed:
    - some distal peaks improved a little
    - but `thigh_r` and `calf_r` still produced large forward spikes
  - backward regressed:
    - large `ball_l` / `ball_r` spikes remained
    - one backward sample rose to roughly `ball_l ~ 3545 deg/s`
  - late idle/strafe also showed new large `ball_*` outliers
- current read:
  - broad lower-limb angular-velocity suppression is not a clean new baseline
  - the runtime code should stay on the narrower distal-only suppression baseline
  - the next pass should move to another seam, most likely per-bone target-write smoothing or another proximal locomotion-time transition policy
