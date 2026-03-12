# Terrain Input Alignment Plan

## Direction Check

- Keep going in the broader training/runtime alignment direction.
- Do not reopen already-falsified sub-directions like lower-limb write smoothing, step-cap tuning, or more multiplier reshuffling.
- The next useful seam is `terrain`, because the active ProtoMotions checkpoint enables terrain observations while the Stage 1 bridge still emits a zero terrain tensor.

## Sources To Re-check Before Coding

- Official UE docs:
  - `UPhysicsControlComponent`
  - `UCharacterMovementComponent::PerformMovement`
- Local UE 5.7 source:
  - `PhysicsControlComponentImpl.cpp`
  - `CharacterMovementComponent.cpp`
- ProtoMotions docs/code:
  - ProtoMotions configuration guide
  - `terrain.py`
  - `terrain_utils.py`
  - `terrain_obs.py`
  - active checkpoint `config.yaml`

## Current Understanding

- Active checkpoint config has `terrain: true`.
- Active checkpoint expects `terrain_obs_num_samples = 256`.
- ProtoMotions terrain observations are:
  - a `16 x 16` local grid
  - spanning `[-1m, +1m]` on both local axes
  - rotated by yaw with the character heading
  - encoded as `root_height - sampled_ground_height`
- The current Stage 1 bridge still calls `BuildZeroTerrain(...)`, so the model never receives the terrain channel it was trained with.

## Implementation Plan

1. Add a bridge helper that exposes Proto-compatible terrain sample offsets:
   - `16 x 16`
   - `[-1, +1]` meters
   - flatten order matching Proto `meshgrid(...).flatten()`
2. Add a bridge helper that converts sampled ground heights into the final terrain tensor:
   - `terrain[i] = root_world_z - ground_height[i]`
3. In `UPhysAnimComponent`, replace the zero-terrain path with a real world sampler:
   - use current root body sample position and yaw
   - line trace each sample point against static world geometry
   - ignore the owning actor
   - fall back to the existing floor-derived ground height if a trace misses
4. Keep the pass intentionally minimal:
   - no terrain normalization changes
   - no gameplay-shell redesign
   - no lower-limb retuning in the same patch

## Success Criteria

- Build passes.
- `PhysAnim.Component` passes.
- `PhysAnim.PIE.MovementSmoke` passes.
- No new fail-stop or movement regression.
- The bridge no longer emits an all-zero terrain tensor during inference.

## Failure Criteria

- Compile/runtime regression.
- Movement smoke becomes less stable.
- Terrain packing order/count diverges from Proto's `256`-sample contract.

## Expected Outcome

- This should be a keepable contract fix.
- It may not produce a dramatic locomotion breakthrough by itself.
- If movement remains mixed afterward, the next pass should stay on observation/representation seams rather than return to falsified lower-limb heuristics.
