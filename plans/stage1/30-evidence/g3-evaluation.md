# Stage 1 Gate G3 Evaluation

## Purpose

This document defines how Gate G3 is evaluated.

Gate text from the engineering plan:

> Show to observers - "Does this look robotic?" Subjective but critical.

This package exists so observer feedback is collected consistently instead of casually.

Use [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/10-specs/acceptance-thresholds.md) as the minimum standard for calling the demo compelling.

## Gate Status

- `Current status`: blocked
- `Decision owner`: user, with observer feedback and orchestrator-prepared evidence
- `Final verdict`: pending

Use only:

- `pass`
- `fail`
- `blocked`

## Entry Criteria

Do not run G3 until:

- Phase 2 demo package is complete enough to show
- the demo is stable enough to watch without major technical distractions
- the orchestrator confirms the evidence package is understandable by outside observers

## Observer Prompt

Use a short prompt like this:

> You are looking only at the motion quality. Does this look robotic, or does it feel weighty and physically believable enough to be interesting?

Do not overload observers with technical explanation before the first impression.

## User Procedure

1. Confirm the orchestrator has frozen the exact demo build or clip to show.
2. Show the demo without a long technical preface.
3. Ask the observer prompt exactly once before discussing implementation details.
4. Record the first reaction in the observer's own words.
5. Repeat for at least `3` observers when practical.
6. If you only have `1` or `2` observers, record the smaller sample honestly.
7. Summarize the common positive and negative reactions.
8. Only after that choose the final `pass`, `fail`, or `blocked` verdict.

## What G3 Is Actually Judging

G3 is not measuring engineering completeness. It is measuring whether the demo is compelling enough to justify more work.

The judgment criteria are:

- non-robotic feel
- physical believability
- visible weight and momentum
- overall interest / distinctiveness

## Required Evidence

- demo clip or live demo notes
- short observer reactions
- user summary of whether the demo justifies continuing

## Observer Capture Template

- `Observer label`:
- `First reaction`:
- `Did they call it robotic?`: yes/no
- `Did they mention weight or momentum?`: yes/no
- `Anything they found distracting?`:

## Manual Check Mapping

- Primary checkpoint: `MV-G3-01`

## Evidence Template

- `Demo version shown`:
- `Observers`:
- `Observer notes`:
- `Common positive reactions`:
- `Common negative reactions`:
- `User summary`:

## Verdict Rules

### Pass

Choose `pass` if observers and the user consistently conclude that the motion feels physically interesting and non-robotic enough to justify more work.

### Fail

Choose `fail` if the demo is judged robotic, unconvincing, or not meaningfully better than ordinary animation.

### Blocked

Choose `blocked` if the demo quality cannot be fairly judged because instability, missing presentation, or poor evidence capture gets in the way.

## Final Decision Section

- `Final verdict`: pending
- `Decision date`:
- `Decision summary`:
- `Should Stage 2 planning/execution continue?`: no

## If Verdict Is Not Pass

- `Failure or block reason`:
- `Stop at Stage 1?`: yes/no
- `Need narrower Stage 1 polish pass?`: yes/no
- `Next task or replan action`:
