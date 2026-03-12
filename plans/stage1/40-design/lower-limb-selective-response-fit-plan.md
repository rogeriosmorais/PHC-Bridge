# Lower-Limb Selective Response Fit Plan

## Situation

The current best measured locomotion baseline is:

- distal locomotion-time composition only for `foot_*` / `ball_*`
- hysteresis+dwell on that mode switch
- explicit target angular-velocity suppression only for those distal controls
- a first-pass locomotion response profile on both `thigh_*` and `calf_*`

That first-pass proximal response profile is a keepable improvement, but it still treats thighs and calves the same.

The latest movement smoke says that is too coarse:

- lower-limb limit occupancy is still usually keyed by `calf_*`
- the worst linear offender is still often `calf_*`
- the worst angular spill is often `thigh_*`

So the remaining issue does not look like “more damping everywhere in the proximal chain.” It looks like a selective coupling problem inside the knee/hip segment of the locomotion chain.

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
  - runtime multipliers are still the intended seam
  - damping ratio and extra damping are converted into the effective angular spring/damping every update

### ProtoMotions docs/code

- ProtoMotions docs
  - <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo
  - <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
- [robots.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/isaaclab/utils/robots.py)

Important training-side detail:

- the SMPL IsaacLab actuator table groups hips, knees, and ankles into the same leg family
- toes are weaker than the rest of the leg chain
- that supports keeping toes distinct, but it does not force thighs and calves to be identical in the UE bridge once runtime evidence shows calf-first occupancy pressure

## Hypothesis

The current locomotion response profile is still over-coupling the thigh and calf response.

If that is true, then:

- `calf_*` should keep the stronger locomotion-time damping profile
- `thigh_*` should move to a milder profile
- forward/backward/strafe should stay stable
- the worst `thigh_*` angular spill should come down without reopening the old distal toe/foot spikes

## Experiment

Keep unchanged:

- cadence alignment
- lower-limb target-range shaping
- distal locomotion-time explicit-only composition
- hysteresis+dwell thresholds
- distal explicit target angular-velocity suppression

Change only this:

- split the locomotion-time proximal response profile
- `calf_*`:
  - keep or slightly increase the current response fit
- `thigh_*`:
  - reduce to a milder locomotion-time response fit

First-pass selective profile:

- `thigh_*`
  - damping ratio scale `1.10`
  - extra damping scale `1.15`
- `calf_*`
  - damping ratio scale `1.25`
  - extra damping scale `1.45`

## Success Criteria

- `PhysAnim.PIE.MovementSmoke` stays green with no fail-stop
- no locomotion-start discontinuity
- lower-limb occupancy remains near the current `~0.9x - 1.1x` range
- the worst `thigh_*` angular spill comes down relative to the shared-profile baseline
- late idle stays better than the old distal-only baseline

## Failure Interpretation

- if distal spikes reopen, the calf-priority split is too aggressive
- if proximal spikes stay unchanged, the remaining seam is probably not just per-bone response magnitude
- if it helps without regressions, keep it and only then revisit perturbation/G2 under movement
