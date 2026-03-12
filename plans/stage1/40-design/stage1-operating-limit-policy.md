# Stage 1 Operating-Limit Policy

## Purpose

This file defines the current Stage 1 limit policy after the first explicit:

- SMPL-vs-Manny limit audit
- SMPL-vs-Manny mass-distribution audit

It exists to stop ad hoc runtime retuning.

The policy answers one narrow question:

How should Stage 1 interpret training-side action ranges when Manny's current UE constraint limits are tighter and differently distributed than the ProtoMotions SMPL asset?

## Inputs

- [smpl-to-manny-limit-table.md](/F:/NewEngine/plans/stage1/40-design/smpl-to-manny-limit-table.md)
- [smpl-to-manny-mass-table.md](/F:/NewEngine/plans/stage1/40-design/smpl-to-manny-mass-table.md)
- [training-runtime-alignment-plan.md](/F:/NewEngine/plans/stage1/40-design/training-runtime-alignment-plan.md)

## Policy

### 1. Do not broaden Manny hard limits blindly

Stage 1 should **not** respond to the audit by copying broad SMPL training ranges into the Manny physics asset.

Reason:

- ProtoMotions SMPL ranges are broad and simulator-friendly
- Manny's current UE asset is much tighter in spine, knees, shoulders, and elbows
- broadening those constraints without a family-by-family operating policy would create new instability and reduce interpretability

### 2. Separate hard safety limits from intended operating limits

For Stage 1, use two layers conceptually:

- `hard safety limit`
  - the authored UE physics-asset constraint
- `operating limit`
  - the smaller action/target envelope we intentionally drive inside that hard limit

Current status:

- hard safety limits exist in the Manny physics asset today
- operating limits are still implicit and need to become explicit in a later runtime policy pass

### 3. Treat the current Manny asset as the hard-limit baseline

Until a deliberate retune is authored, the current Manny constraints remain the Stage 1 hard safety envelope.

That means:

- no broad asset-level constraint widening yet
- no silent “training said 180 degrees so UE should too” changes

### 4. Use family-level operating intent, not per-bone improvisation

Stage 1 should reason in these families:

- pelvis / root-adjacent torso base
- leg chain
- spine chain
- neck / head
- shoulder / arm / hand

This is the right level because:

- the ProtoMotions SMPL asset already has family-level stiffness hierarchy
- the Manny mass and limit mismatches also show up most clearly at family level

## Family Policy

### Legs

- Current read:
  - Manny legs are materially lighter than SMPL
  - Manny knees and ankles are materially tighter than SMPL
- Stage 1 policy:
  - do **not** broaden hard limits yet
  - prioritize mass alignment first
  - keep leg-chain operating motion conservative until mass distribution is closer

Reason:

- movement and perturbation sensitivity are already lower-body dominated
- changing ranges before mass/inertia alignment would mix variables

### Spine

- Current read:
  - Manny mid/upper spine limits are much tighter than SMPL
  - Manny torso mass is concentrated higher in the chain than SMPL
- Stage 1 policy:
  - do **not** broaden spine hard limits yet
  - treat current spine operating range as intentionally conservative
  - revisit only after family-level mass adjustment

Reason:

- torso-heavy + tight-spine is already a meaningful training/runtime mismatch
- changing both range and mass at once would make the next regression hard to read

### Neck / Head

- Current read:
  - bridge controls exist
  - direct Manny one-to-one constraint pairs are missing in the current audit path
- Stage 1 policy:
  - keep current behavior
  - do not author limit changes until the actual control/constraint relationship is made explicit

Reason:

- action-range semantics are not transparent enough there yet

### Shoulders / Arms / Hands

- Current read:
  - Manny shoulders and elbows are much tighter than SMPL
  - Manny upper limbs are materially heavier than SMPL
- Stage 1 policy:
  - do **not** widen shoulder/elbow hard limits yet
  - let mass alignment happen first
  - keep upper-limb operating range conservative

Reason:

- the arm chain was already a sensitive instability path earlier in stabilization
- widening tight arm constraints before mass adjustment would be high-risk

## What This Means For The Next Runtime Pass

The next runtime-side alignment change should not be a limit edit.

It should be:

1. family-level mass adjustment policy
2. measured movement regression
3. only then, if needed, operating-range clamps or authored limit revisions

## Frozen Rule

Until a later planning update explicitly changes this:

- Manny physics-asset constraints remain the Stage 1 hard limits
- runtime alignment work should move to mass distribution next
- any range adjustment must be justified family-by-family against:
  - movement smoke
  - movement soak
  - G2 presentation regression

## Current Orchestrator Read

The audits now make one thing clear:

The current training/runtime mismatch is not just “wrong gains.”

It is:

- narrower Manny hard limits
- torso-heavy Manny mass distribution
- leg-light Manny mass distribution

So the next meaningful correction is mass-family alignment, not blind limit widening.
