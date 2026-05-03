# Lower-Limb Locomotion Response Fit Plan

## Situation

The current best measured locomotion baseline is:

- `foot_*` / `ball_*` only
- explicit-only locomotion composition under hysteresis+dwell
- synthesized explicit target angular velocity suppressed only for those distal controls

That baseline is materially better than earlier locomotion passes:

- no fail-stop in deterministic movement smoke
- lower-limb occupancy usually stays near `~0.9x - 1.1x`
- first forward distal spikes are much lower than the old high-four-digit baseline

But the remaining worst spikes now often migrate into:

- `calf_*`
- `thigh_*`
- occasional `foot_*`

while target occupancy remains near `1.0x`.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- `FPhysicsControlData`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlData>
- `FPhysicsControlMultiplier`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlMultiplier>

### UE local source

- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
  - control multipliers are converted into the effective spring/damping every update
  - runtime multipliers are the intended safe seam for response fitting

### ProtoMotions docs/code

- ProtoMotions docs
  - <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo
  - <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
  - PD torque is computed from:
    - target position
    - current DoF position
    - current DoF velocity
- local pretrained SMPL config shows control gains are part of the training-side controller contract even when represented differently than UE PhysicsControl

### Existing local evidence

- [stabilization-stress-test.md](/F:/NewEngine/plans/stage1/40-design/stabilization-stress-test.md)
- [stabilization-stress-test-questions.md](/F:/NewEngine/plans/stage1/40-design/stabilization-stress-test-questions.md)

The strongest measured result from the stress matrix was:

- damping ratio is the most sensitive stability lever
- movement narrows the safe envelope much more than idle

## Current Read

The remaining movement problem no longer looks like target-range overflow first:

- lower-limb occupancy is near `1.0x`
- target deltas are already small
- the worst spikes are now response spikes in the proximal lower-limb chain

That points to a response-fit seam:

- not more target composition widening
- not more broad angular-velocity suppression
- but locomotion-time lower-limb control response

## Hypothesis

The remaining locomotion spikes are partly caused by an under-damped proximal lower-limb response in UE PhysicsControl relative to what the ProtoMotions-trained policy expects.

If that is true, then:

- keep the current distal locomotion baseline unchanged
- increase locomotion-time damping only for the proximal lower-limb chain:
  - `thigh_*`
  - `calf_*`
- leave distal feet/toes on the current baseline for now
- and movement smoke should stay stable while reducing the worst `calf_*` / `thigh_*` spikes

## Experiment

Keep unchanged:

- cadence alignment (`30 Hz`)
- lower-limb target-range shaping
- distal locomotion composition hysteresis+dwell
- distal explicit target angular-velocity suppression

Change only this:

- when locomotion mode is active
- apply a modest proximal lower-limb response profile:
  - `thigh_*`
  - `calf_*`
- first pass target:
  - no strength increase
  - damping ratio multiplier > `1.0`
  - extra damping multiplier > `1.0`

This should be implemented through runtime control multipliers, not asset authoring.

## Success Criteria

The pass is a win if deterministic movement smoke shows:

- no fail-stop
- no locomotion-start discontinuity
- reduced worst-case `calf_*` / `thigh_*` spikes
- no material regression in the improved distal `foot_*` / `ball_*` behavior

## Failure Interpretation

- if proximal spikes come down without reintroducing distal regressions:
  - keep it and re-evaluate perturbation/G2 under movement
- if it just moves spikes elsewhere or worsens distal behavior:
  - revert and move to another seam, most likely per-bone target-write smoothing or a more selective locomotion-time response profile

## Result

- implemented a locomotion-time proximal lower-limb response profile:
  - `thigh_*`
  - `calf_*`
- response change:
  - no strength increase
  - damping ratio scale `1.20`
  - extra damping scale `1.35`
- verification:
  - UE build passed
  - `PhysAnim.Component` passed
  - `PhysAnim.PIE.MovementSmoke` passed
  - no fail-stop
- measured runtime result:
  - forward remained mixed, with one sample still reaching roughly `thigh_r ~ 2803 deg/s`
  - backward stayed difficult, but the worst samples remained in the same broad range instead of exploding further
  - strafe improved versus the prior distal-only baseline:
    - `thigh_l` peaks stayed around `~3053 deg/s` instead of the prior `~3224 deg/s`
    - several strafe samples also reduced distal `ball_*` / `foot_*` spikes
  - late idle improved materially:
    - previous late-idle worst case had `ball_l ~ 3499 deg/s`
    - this pass reduced late-idle peaks to roughly `foot_* ~ 763 - 835 deg/s`
- current read:
  - locomotion-time proximal response fitting is a real remaining alignment seam
  - this is a keepable improvement on top of the distal-only target-velocity baseline
  - the next pass should stay in response fitting, most likely a more selective per-bone locomotion response profile rather than another target-semantics experiment
