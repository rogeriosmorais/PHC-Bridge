# Ankle Constraint Authoring Audit Plan

## Purpose

The verified `0.50` toe operating-limit baseline is now the best measured lower-limb fit, but movement still peaks in the ankle chain:

- `calf_l(sim)` remains the peak body linear offender
- `ball_l(sim)` remains the peak body angular offender

That makes the direct ankle constraints the next lower-limb surface to audit before any broader runtime change.

## Audit Scope

Direct Manny constraints:

- `foot_l <- calf_l`
- `foot_r <- calf_r`

Questions:

1. Do both direct ankle constraints exist?
2. Do left/right motions and limit angles match?
3. Are left/right reference frames symmetric by magnitude?
4. Are the primary and secondary axes normalized and non-degenerate?
5. Is there any obvious angular rotation offset mismatch?

## Implementation

1. Add `PhysAnim.Component.MannyAnkleConstraintAuthoring`.
2. Load the Manny physics asset.
3. Resolve both direct ankle constraints.
4. Log motions, limits, axes, positions, and angular rotation offsets.
5. Assert left/right symmetry and basic axis sanity using the same tolerances as the toe audit.

## Success Criteria

- build passes
- `PhysAnim.Component` passes
- the audit clearly tells us whether the ankle chain is malformed or merely sensitive

## Decision Rule

- If the ankle authoring is malformed:
  - fix authoring before more runtime tuning
- If the ankle authoring is sane:
  - treat the next pass as a lower-limb operating-limit or response-fit problem, not a manual authoring problem
