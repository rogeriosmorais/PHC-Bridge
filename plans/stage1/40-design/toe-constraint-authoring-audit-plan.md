# Toe Constraint Authoring Audit Plan

## Purpose

The current lower-limb fitting work has isolated `ball_l` and `ball_r` as frequent peak angular offenders during deterministic movement.

Before making more runtime tuning changes, Stage 1 needs to answer a simpler question:

- are the manually created Manny toe constraints obviously mis-authored?

This plan exists to answer that with asset evidence rather than more gain experiments.

## Why This Is The Next Step

Current evidence says:

- `ball_*` exists in the Manny physics asset
- left/right toe masses are sane and symmetric
- the toe constraints are present and permissive
- isolated toe-only runtime gain tweaks did not improve the movement case

That means the next likely mismatch surface is authoring, not another small runtime gain change.

## Audit Questions

1. Do `ball_l` and `ball_r` both expose direct Manny constraint instances?
2. Are the two toe constraints symmetric in:
   - angular motion modes
   - limit angles
   - reference-frame positions
   - reference-frame axes
   - angular rotation offsets
3. Are the toe reference axes normalized and non-degenerate?
4. Are the toe constraints materially more permissive than the rest of the locomotion chain?

## Implementation

Add a dedicated automation test:

- `PhysAnim.Component.MannyToeConstraintAuthoring`

The test should:

- load `PA_Mannequin`
- read the direct constraint instances for:
  - `ball_l <- foot_l`
  - `ball_r <- foot_r`
- log the raw constraint data for both sides
- assert left/right symmetry within a reasonable tolerance
- assert reference axes are normalized enough to be sane

This is an audit test, not a runtime fix.

## Success Criteria

The audit should let us sort the problem into one of two buckets:

1. `gross authoring issue`
   - left/right asymmetry
   - bad/degenerate axes
   - obviously inconsistent frame setup
2. `no gross authoring issue`
   - toe constraints are structurally sane
   - the remaining problem is likely a higher-level runtime mismatch

## Next Decision Rule

- If the audit fails:
  - stop tuning runtime gains
  - fix the toe constraint authoring first
- If the audit passes:
  - keep the current toe-family runtime baseline
  - move the next alignment pass to a different lower-limb mismatch surface

## Current Result

Completed.

Current measured result from `PhysAnim.Component.MannyToeConstraintAuthoring`:

- both toe constraints exist
- left/right motion modes match
- left/right limit angles match
- left/right reference-frame magnitudes are symmetric
- reference axes are normalized and non-degenerate
- angular rotation offsets are zero on both sides

Current conclusion:

- the manually created `ball_*` constraints do not look grossly malformed
- the remaining issue is more likely permissive/sensitive toe behavior than basic bad authoring
