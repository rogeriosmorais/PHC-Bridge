# Self-Observation Velocity Alignment Plan

## Status

- `date`: March 12, 2026
- `owner`: AI orchestrator / implementation pass
- `scope`: Stage 1 runtime bridge only
- `out of scope`: [asset-authored-physics-tuning-design.md](/F:/NewEngine/plans/stage1/40-design/asset-authored-physics-tuning-design.md)

## Why This Pass Exists

The current movement traces suggest `self_obs` is the more likely remaining mismatch surface than `mimic_target_poses`.

Observed pattern from the current movement-trace baseline:

- `mimic_target_poses_mean_abs` stays relatively stable across locomotion phase starts
- `self_observation_mean_abs` spikes sharply at locomotion onset
- lower-limb outliers remain concentrated in active locomotion phases, not passive idle

That combination points to current-state packing, especially body velocities, as the next seam worth testing.

## UE / ProtoMotions Review

### ProtoMotions

ProtoMotions `self_obs` in the active `max_coords` path consumes:

- `rigid_body_pos`
- `rigid_body_rot`
- `rigid_body_vel`
- `rigid_body_ang_vel`

Relevant local files:

- [humanoid_obs.py](/F:/NewEngine/Training/ProtoMotions/protomotions/envs/base_env/components/humanoid_obs.py)
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
- [isaaclab/simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/isaaclab/simulator.py)
- [isaacgym/simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/isaacgym/simulator.py)
- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)

The important detail is that these velocities come from simulator body state for all bodies, not only for a subset of actively simulated runtime bodies.

### UE

UE `USkeletalMeshComponent::GetPhysicsLinearVelocity(BoneName)` and `GetPhysicsAngularVelocityInRadians(BoneName)` resolve through `GetBodyInstance(BoneName)` and return body-instance physics velocity.

Relevant local engine files:

- [PrimitiveComponentPhysics.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/PrimitiveComponentPhysics.cpp)
- [BodyInstance.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/PhysicsEngine/BodyInstance.cpp)
- [SkeletalMeshComponentPhysics.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/SkeletalMeshComponentPhysics.cpp)

That is correct for actively simulating bodies, but it is a plausible mismatch for bridge-observation bones that are currently kinematic or otherwise not contributing simulator-style body velocities.

## Current Hypothesis

`GatherCurrentBodySamples(...)` should not treat all observation bones the same.

- If the bone is simulating, physics-body velocity remains the right source.
- If the bone is kinematic / non-simulating, the bridge should derive velocity from world-transform deltas over time.

This should move UE current-state packing closer to the simulator-style body-state contract used by ProtoMotions without changing authoring, gains, or target semantics.

## Implementation Plan

1. Add a small observation-velocity cache to `UPhysAnimComponent`.
   - per-observation-bone previous world transform
   - previous sample timestamp
   - validity bit

2. Add pure helpers for velocity derivation.
   - linear velocity from translation delta / dt
   - angular velocity from quaternion delta / dt
   - zero output for first sample or invalid dt

3. Change `GatherCurrentBodySamples(...)`.
   - simulating body: keep physics velocity path
   - non-simulating body: use transform-delta velocity path

4. Reset the cache on runtime resets.
   - bridge activation
   - fail-stop / teardown
   - stabilization runtime reset

5. Do not change `mimic_target_poses`, control writes, or lower-limb policy in this pass.

## Verification

Automated:

- component tests for:
  - linear velocity derivation
  - angular velocity derivation
  - first-sample zeroing / invalid-dt zeroing

Runtime:

- `PhysAnim.Component`
- `PhysAnim.PIE.MovementSmoke`
- `PhysAnim.PIE.MovementTraceSmoke`

Success criterion:

- no smoke regression or fail-stop
- movement trace stays valid
- `self_observation_mean_abs` and active-phase lower-limb maxima improve materially versus the current grace-window baseline

## Decision Boundary

Keep this pass if:

- movement remains stable
- at least one of:
  - active-phase `self_observation_mean_abs` drops materially
  - active-phase lower-limb angular outliers drop materially

Reject this pass if:

- smoke regresses
- phase maxima worsen broadly
- trace evidence shows no meaningful change

## Outcome

- `status`: rejected as a new runtime baseline
- `verification`:
  - `Build.bat PhysAnimUE5Editor ...`
  - `PhysAnim.Component`
  - `PhysAnim.PIE.MovementTraceSmoke`
- `result`:
  - movement remained stable
  - but active locomotion maxima worsened broadly relative to the current grace-window baseline
  - `self_observation_mean_abs` did not materially improve in active phases
- `follow-up`:
  - keep investigating `self_obs`
  - do not keep the mixed transform-delta velocity policy
