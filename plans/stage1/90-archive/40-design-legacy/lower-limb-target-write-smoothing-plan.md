# Lower-Limb Target-Write Smoothing Plan

## Direction Check

It is still worth continuing in the broader training/runtime alignment direction.

It is not worth continuing the already-falsified sub-directions as the primary lever:
- lower-limb contact-exclusion alignment
- lower-limb target-step caps
- more isolated thigh/calf multiplier reshuffling
- startup cache-order cleanup as the main locomotion fix

## Situation

Current committed baseline:
- fixed `30 Hz` policy cadence
- lower-limb target-range shaping
- distal locomotion composition hysteresis/dwell
- distal explicit target angular-velocity suppression
- shared proximal lower-limb response profile
- PhysicsControl startup cache-prewarm

What the latest movement-smoke baseline still shows:
- no fail-stop
- lower-limb limit occupancy mostly around `~0.9x - 1.1x`
- but locomotion still produces intermittent lower-limb spikes, especially in strafe/backward and late samples

That means:
- hard limit overshoot is no longer the dominant story
- cache invalidity is no longer the dominant story
- the remaining seam is likely how often and how abruptly lower-limb targets are written during locomotion

## Sources Consulted

### UE online docs

- `UPhysicsControlComponent`
- `SetControlTargetOrientation`
- `UpdateControls`
- `FPhysicsControlData`

### Local UE 5.7 source

- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
- [PhysicsControlComponent.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponent.cpp)

Key UE-side read:
- `SetControlTargetOrientation(...)` directly updates the explicit target state
- PhysicsControl does not provide a built-in target-write smoothing layer for this seam
- `UpdateControls(...)` then applies those targets on the next control update

### ProtoMotions docs/code

- ProtoMotions config docs: <https://nvlabs.github.io/ProtoMotions/user_guide/configuration.html>
- ProtoMotions repo: <https://github.com/NVlabs/ProtoMotions>
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
- [config.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/config.py)
- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)

Key ProtoMotions-side read:
- control is still `30 Hz`
- actions are mapped into PD ranges
- the policy is trained under smooth sequential control updates, not with an extra UE-side explicit target write every variable frame

## Hypothesis

The remaining lower-limb spikes are partly caused by high-frequency lower-limb target write jitter during locomotion.

Even when individual target steps are not huge, repeated lower-limb target rewrites can still excite the articulated chain.

## Narrow Experiment

Apply locomotion-time lower-limb target-write smoothing after the existing range/composition/delta-time logic, but before the final explicit target write.

Initial policy:
- activate only while owner planar speed is above the locomotion threshold
- smooth only lower-limb bones:
  - `thigh_*`
  - `calf_*`
  - `foot_*`
  - `ball_*`
- use a family-weighted blend:
  - proximal (`thigh_*`, `calf_*`) = mild smoothing
  - distal (`foot_*`, `ball_*`) = stronger smoothing

Do not change:
- global action smoothing
- control cadence
- lower-limb target-range policy
- distal composition hysteresis
- mass policy

## Success Criteria

- build passes
- `PhysAnim.Component` passes
- `PhysAnim.PIE.MovementSmoke` passes
- no fail-stop
- forward/backward/strafe lower-limb spikes improve or at least one difficult phase improves without materially regressing the others

## Failure Criteria

- forward regresses materially
- backward regresses materially
- strafe/idle reopen larger outliers
- movement smoke becomes less stable

If that happens, the next pass should move off write smoothing and onto a different locomotion-time representation seam.
