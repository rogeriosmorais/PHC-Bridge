# Self-Observation Root-Height Alignment Plan

## Summary

The broader training/runtime alignment direction is still worth continuing. The narrower lower-limb heuristic branch is not.

The next justified pass is a self-observation contract correction, but only at the lowest-risk seam:

- keep the current stabilized runtime baseline
- do not reopen full world-frame conversion yet
- correct the self-observation root-height scalar so it matches ProtoMotions' runtime convention

## What The Review Confirmed

### ProtoMotions runtime convention

- The pretrained checkpoint uses `use_max_coords_obs = true`, `local_root_obs = true`, and `root_height_obs = true`.
- `compute_humanoid_observations_max(...)` in ProtoMotions builds `self_obs` as:
  - root height
  - local body positions
  - local body tan-norm rotations
  - local body linear velocities
  - local body angular velocities
- The observation size for SMPL is `358`, which matches the current UE bridge.
- ProtoMotions' simulator runtime is `z-up`, not `y-up`, for the actual observation path:
  - Isaac Gym simulator setup explicitly uses `up_axis = z`
  - self-observation height is read from `root_pos[:, 2]`

### UE bridge state

- The current UE `BuildSelfObservation(...)` packing order and size already match ProtoMotions' `max_coords` structure closely.
- The current caller still passes `ground_height = 0.0f` every frame.
- That is a clear contract violation relative to ProtoMotions, which queries ground height under the root every frame.

## Why This Pass Is Narrow

There is a larger unresolved frame-contract question around the bridge's world-space vector conversion. That is real, but changing all world-space position, velocity, and future-pose conversions in one pass would be too wide and too risky.

The root-height scalar is different:

- it is a standalone scalar channel
- ProtoMotions behavior is explicit
- UE exposes floor state directly through `CharacterMovement`
- we can correct it without changing the rest of the observation packing

## Planned Implementation

1. Derive a real world-space ground height from the gameplay shell when a valid walkable floor exists.
2. Derive the desired root-height observation as:
   - `root_world_z - ground_world_z`
3. Convert that into the existing `BuildSelfObservation(...)` call without changing tensor shape or packing order:
   - synthesize the `GroundHeight` argument so the emitted scalar equals the desired world-space root-height observation
4. Keep all other observation channels unchanged in this pass.

## Helper Policy

- Prefer `CharacterMovement->CurrentFloor.HitResult.ImpactPoint.Z` when a walkable floor blocking hit is valid.
- Fallback to `capsule_center_z - capsule_half_height - floor_distance` when floor data is walkable but the impact point is not usable.
- Fallback to `0.0` only when no trustworthy walkable floor data exists.

## Verification

- Add helper-level automation coverage for:
  - floor-world-z resolution
  - synthetic observation-ground-height resolution
- Rebuild
- Run:
  - `PhysAnim.Component`
  - `PhysAnim.PIE.MovementSmoke`

## Success Criteria

- Build and tests stay green.
- Movement smoke stays stable.
- The self-observation root-height channel is no longer hardcoded to a flat `0.0`-ground assumption.
- This pass remains low-risk and keepable even if it is only a partial locomotion improvement.

## Sources Reviewed

- UE docs:
  - `UCharacterMovementComponent::FindFloor`
  - `UPhysicsControlComponent`
  - `FPhysicsControlData`
- UE 5.7 source:
  - `CharacterMovementComponent.h/.cpp`
  - `PhysicsControlComponent.cpp`
  - `PhysicsControlComponentImpl.cpp`
- ProtoMotions docs/code:
  - configuration docs
  - `humanoid_obs.py`
  - `humanoid_utils.py`
  - `simulator.py`
  - pretrained `config.yaml`
