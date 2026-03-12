# Lower-Limb Proximal Composition Plan

## Situation

The latest lower-limb locomotion passes established:

- Locomotion-time explicit-only target mode is a real mismatch surface.
- The best measured baseline so far is:
  - `foot_*`
  - `ball_*`
  - explicit-only only after locomotion-speed hysteresis and dwell
- Widening explicit-only mode to the full `calf_*` / `foot_*` / `ball_*` chain was not a clean win.

The remaining movement-smoke failures are no longer dominated by pure distal outliers. In the current transition-policy baseline, some of the larger locomotion spikes migrate proximally into `thigh_*`, especially during backward and strafe phases.

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- `UPhysicsControlComponent::SetControlUseSkeletalAnimation`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/SetControlsUseSk->
- `UPhysicsControlComponent::UpdateControls`
  - <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent/UpdateControls>

### UE local source

- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)
  - `SetControlUseSkeletalAnimation(...)` toggles the mode directly. There is no internal blend.
- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
  - when skeletal animation is enabled, the explicit target is applied in the space of the computed skeletal target transform
  - in the current bridge baseline, skeletal target velocity feedforward is already disabled globally (`0.0 / 0.0`)

### ProtoMotions docs/code

- ProtoMotions docs
  - <https://nvlabs.github.io/ProtoMotions/>
- ProtoMotions GitHub
  - <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
  - actions are mapped directly into PD targets in simulator joint space
  - the controller does not perform a locomotion-time representation flip between additive animation-composed and explicit-only modes
- [humanoid_utils.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/env_utils/humanoid_utils.py)
  - lower-limb 3-DoF joints use the symmetric `1.2x` action-range expansion

## Current Read

The remaining mismatch is still about target semantics, not authoring:

- current bridge baseline already disables skeletal target velocity feedforward globally
- the remaining runtime difference is the target-frame composition itself
- full `calf_*` / `foot_*` / `ball_*` explicit-only switching overshoots
- current transition-policy logs show residual peaks migrating into `thigh_*`

That makes `thigh_*` the next narrow structural experiment.

## Hypothesis

The lower-limb locomotion mismatch is now split across the chain:

- `foot_*` / `ball_*` benefit from explicit-only locomotion-time target mode
- `calf_*` do not
- `thigh_*` may still be paying the remaining skeletal-target composition mismatch during locomotion

So the next pass should test:

- keep the current transition-policy baseline
- add `thigh_*` to the explicit-only locomotion composition set
- leave `calf_*` on the current composed path

## Experiment

Apply explicit-only locomotion composition, under the existing hysteresis+dwell policy, to:

- `thigh_l`
- `foot_l`
- `ball_l`
- `thigh_r`
- `foot_r`
- `ball_r`

Do **not** include:

- `calf_l`
- `calf_r`

## Success Criteria

The pass is a win if deterministic movement smoke shows:

- no fail-stop
- no locomotion-start discontinuity
- forward and backward spikes no worse than the current transition-policy baseline
- reduced proximal `thigh_*` spikes in the later movement phases

## Failure Interpretation

- if this improves the proximal spikes without reintroducing the distal explosions:
  - keep it and re-evaluate perturbation/G2 under movement
- if it regresses badly:
  - revert and move to a different surface, most likely lower-limb target write timing or another policy-side locomotion transition seam

## Result

Implemented and tested, then reverted.

Measured result in deterministic movement smoke:

- no fail-stop
- movement completion remained green
- but the pass was materially worse than the transition-policy baseline

Observed runtime behavior:

- forward regressed sharply:
  - early forward already rose to about `ball_r:1003 deg/s`
  - later forward samples reached about `ball_r:6177-7009 deg/s`
- backward also regressed:
  - repeated `calf_r` linear spikes rose into the `~2200-3080 cm/s` range
  - repeated `foot_r` / `ball_r` angular spikes rose into the `~8700-11979 deg/s` range
- the remaining peaks did not become cleaner proximal-only signals; they became larger mixed lower-limb and upper-body bursts

Current read:

- simply adding `thigh_*` to the explicit-only locomotion composition set is not the right next baseline
- the next pass should move to another locomotion-time mismatch surface, most likely lower-limb target write timing or another policy-side transition seam
