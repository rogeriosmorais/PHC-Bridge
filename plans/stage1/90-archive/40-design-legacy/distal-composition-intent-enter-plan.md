# Distal Composition Intent-Enter Plan

## Goal

Test whether the remaining phase-start false rows in distal locomotion composition mode are caused by the normal `enterHold` delaying activation after locomotion intent begins.

## Why This Pass

After the recent-intent grace pass:

- within-phase flips dropped from `6 -> 4`
- remaining false rows clustered at the start of `Forward`, `StrafeLeft`, `StrafeRight`, and `Backward`

That suggested an enter-side delay:

- live locomotion intent was already present
- but the mode was still waiting through the normal `0.20s` enter hold

## Proposed Runtime Change

Introduce a separate intent-aware enter hold:

1. if the mode is already active and recent locomotion intent is still present:
   - keep it latched on
2. if the mode is inactive but live locomotion intent is active:
   - use a shorter or zero enter hold
3. otherwise:
   - keep the existing hysteresis and hold behavior

## Initial Experimental Policy

- `DistalLocomotionCompositionPolicyIntentEnterHoldSeconds = 0.00`

## Success Criteria

- build/tests/smoke remain green
- phase-start false rows drop materially
- within-phase flips drop or stay neutral
- active locomotion phase maxima improve or remain neutral

## Failure Criteria

- the mode becomes too eager and stays on for almost all active locomotion rows
- active locomotion maxima regress materially
- late settling regresses

## Result

Failed. This made the mode too eager:

- active locomotion phases became almost entirely `true`
- phase-start false rows mostly disappeared
- but active locomotion maxima regressed sharply enough that the pass should not become the new baseline

The runtime was restored to the previous grace-window baseline after the experiment.
