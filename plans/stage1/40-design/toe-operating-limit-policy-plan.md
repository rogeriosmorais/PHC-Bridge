# Toe Operating-Limit Policy Plan

## Purpose

The manual toe-constraint authoring audit passed.

That means the next lower-limb pass should stop treating `ball_l` / `ball_r` as malformed authoring and instead test whether the current toe constraints are simply too permissive for Stage 1 runtime stability.

## Current Evidence

- `ball_l` and `ball_r` are structurally sane and left/right symmetric.
- The current Manny toe constraints are very permissive:
  - twist `Free / 45`
  - swing1 `Free / 45`
  - swing2 `Free / 45`
- `ball_*` remains a frequent peak angular offender during deterministic movement.
- Simple toe-only gain changes were worse than the current toe-family mapping baseline.

## Engine / Runtime Basis

This pass uses UE constraint data directly:

- `UPhysicsAsset::FindConstraintIndex(...)`
- `UPhysicsConstraintTemplate::DefaultInstance`
- `FConstraintInstance::SetAngularTwistLimit(...)`
- `FConstraintInstance::SetAngularSwing1Limit(...)`
- `FConstraintInstance::SetAngularSwing2Limit(...)`

The bridge already applies temporary runtime policy state on activation and restores it on teardown for mass scaling, so toe-limit policy will follow the same pattern.

## Policy

Temporary runtime operating limits for:

- `ball_l <- foot_l`
- `ball_r <- foot_r`

First fitted target:

- twist: `Limited / 20`
- swing1: `Limited / 20`
- swing2: `Limited / 20`

This is intentionally conservative for Stage 1.

## Implementation

1. Add stabilization settings and cvars:
   - `bApplyTrainingAlignedToeLimitPolicy`
   - `TrainingAlignedToeLimitPolicyBlend`
2. Save the original Manny toe constraint motions and angles on activation.
3. Apply the temporary toe operating-limit policy before `RecreatePhysicsState()`.
4. Restore the original Manny toe constraint state on teardown.
5. Add helper coverage in `PhysAnim.Component`.

## Success Criteria

The first pass is good if:

- build passes
- `PhysAnim.Component` passes
- `PhysAnim.PIE.MovementSmoke` passes
- movement diagnostics show:
  - no fail-stop
  - lower peak toe angular speed than the current committed baseline

## Decision Rule

- If the movement smoke improves:
  - keep the toe operating-limit policy as the next baseline candidate
- If it does not improve:
  - restore the old behavior
  - move the next pass away from toe limits and toward a different lower-limb mismatch surface

## Current Result

Completed first runtime sweep.

Current measured result:

- `blend=1.00` is too aggressive
- `blend=0.50` is the first viable toe operating-limit baseline candidate

Current conclusion:

- the manual `ball_*` authoring is not the leading issue
- the toe operating-limit mismatch is real
- the live baseline should use a half-blend toe-limit policy, not the full clamp
