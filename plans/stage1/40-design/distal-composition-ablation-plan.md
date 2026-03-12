# Distal Composition Ablation Plan

## Why this pass

The cleaned `frames.csv` trace is now emitted only on policy steps, so it is finally a trustworthy input artifact rather than a mixed per-tick log.

That trace shows:

- the worst locomotion-time lower-limb outliers cluster while `distal_locomotion_composition_mode_active=true`
- those outliers are strongest in `Forward`, `StrafeRight`, and `Backward`
- a few ugly transition frames also happen when the mode turns off mid-phase

This matters because the explicit-only distal composition mode is **our runtime heuristic**, not part of the ProtoMotions training contract.

## What the docs/source say

### Unreal Engine

- `UPhysicsControlComponent` controls bodies with damped spring/damper targets.
- `SetControlTargetOrientation(...)` and the skeletal-target/composed path are UE-side target application choices.
- There is no engine-level concept of a "Proto distal locomotion composition mode"; that mode exists only in our bridge policy layer.

### ProtoMotions

- The active checkpoint still uses:
  - `map_actions_to_pd_range = true`
  - `use_biased_controller = false`
  - `self_obs`, `mimic_target_poses`, and `terrain` as the core observation channels
- There is no training-side branch that corresponds to our UE-only explicit-only foot/toe composition toggle.

## Evaluation

It is still worth continuing in the broader training/runtime alignment direction.

It is **not** worth continuing another round of:

- lower-limb multiplier reshuffling
- write smoothing
- step-cap tuning
- contact-exclusion tuning

The next useful test is to falsify whether this UE-only distal composition heuristic is still helping after the recent cadence, world-frame, mimic-target, terrain, and trace fixes.

## Plan

1. Disable the training-aligned distal locomotion composition policy by default.
2. Keep the existing distal target-range policy and distal angular-velocity suppression baseline.
3. Update component automation so the default expectation matches the new baseline.
4. Re-run:
   - `PhysAnim.Component`
   - `PhysAnim.PIE.MovementSmoke`
   - `PhysAnim.PIE.MovementTraceSmoke`
5. Compare the new trace against the latest clean trace on:
   - max/p95 angular speed by movement phase
   - max lower-limb occupancy by movement phase
   - whether the former `distal_locomotion_composition_mode_active=true` spikes disappear

## Success criteria

- no regression in smoke stability
- movement trace remains valid
- lower-limb angular outliers drop materially in the locomotion phases that were previously dominated by distal composition mode

## Failure criteria

- movement smoke regresses materially
- large lower-limb spikes remain unchanged
- disabling the heuristic causes worse forward locomotion than the current baseline

If it fails, restore the current baseline and move to another trace-backed seam.
