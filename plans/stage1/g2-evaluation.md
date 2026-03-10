# Stage 1 Gate G2 Evaluation

## Purpose

This document defines how Gate G2 is evaluated.

Gate text from the engineering plan:

> Side-by-side - physics-driven vs kinematic PoseSearch. Must look noticeably more natural.

This package exists so the user does not have to invent the comparison method at evaluation time.

Use [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md) as the tie-breaker when the verdict feels vague.

## Gate Status

- `Current status`: blocked
- `Decision owner`: user, using orchestrator-prepared evidence
- `Final verdict`: pending

Use only:

- `pass`
- `fail`
- `blocked`

## Entry Criteria

Do not run G2 until:

- Phase 1 is complete enough for comparison
- the one-character bridge runs end to end
- the comparison sequence is chosen and frozen
- the orchestrator confirms no `red` assumption blocks the quality comparison

## Comparison Setup

Compare two versions of the same motion sequence:

1. **Kinematic baseline**
   - Manny driven by the animation / PoseSearch path without the physics-driven PHC bridge taking over body motion
2. **Physics-driven version**
   - Manny driven by the full Stage 1 bridge using PHC outputs and `UPhysicsControlComponent`

Keep these fixed between both captures:

- same character
- same animation source / comparison sequence
- same camera framing
- same arena or environment setup
- same playback speed

## What G2 Is Actually Judging

The goal is not perfect realism. The goal is whether the physics-driven version is clearly better on the thesis criteria:

- weight
- momentum
- balance recovery
- contact response
- overall non-robotic feel

## Evaluation Rubric

### Pass

Choose `pass` if the physics-driven version is **noticeably more natural** on most of these:

- weight transfer looks more believable
- motion carries momentum instead of stopping abruptly
- body reacts to contact or balance shifts in a physically convincing way
- the result looks less robotic than the kinematic baseline

### Fail

Choose `fail` if any of these are true:

- the difference is negligible
- the physics-driven version looks worse overall
- the physics-driven version is too unstable or noisy to support the thesis
- the comparison is dominated by artifacts instead of improved physicality

### Blocked

Choose `blocked` if:

- the captures are not comparable
- the sequence is not frozen
- the user cannot make a fair judgment from the available evidence

## Required Evidence

- one side-by-side clip or two clearly comparable clips
- one short written verdict from the user
- notes on which rubric points drove the decision

## Manual Check Mapping

- Primary checkpoint: `MV-G2-01`

## Evidence Template

- `Comparison sequence`:
- `Kinematic clip`:
- `Physics-driven clip`:
- `Camera/setup parity confirmed?`: yes/no
- `User verdict`: pending
- `Why this verdict was chosen`:

## Orchestrator Review Before User Decision

The orchestrator must confirm:

- the captures are comparable
- the evidence is sufficient for a user judgment
- no major setup mismatch invalidates the comparison

## Final Decision Section

- `Final verdict`: pending
- `Decision date`:
- `Decision summary`:
- `Can Phase 2 begin?`: no

## If Verdict Is Not Pass

- `Failure or block reason`:
- `Need rework?`: yes/no
- `Fallback or stop decision`:
- `Next task or replan action`:
