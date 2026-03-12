# Stage 1 Comparison Sequence Lock

## Purpose

This document locks the exact comparison shape for Gate G2 so Phase 1 does not have to improvise what gets compared.

## Stage 1 G2 Rule

G2 is not allowed to compare arbitrary clips.

It must compare one locked sequence that tests the Stage 1 thesis:

- weight
- momentum
- braking
- turning
- recovery

## Primary G2 Sequence

The default G2 comparison sequence is:

1. scripted locomotion-coupled perturbation / push
2. relaxed idle / ready stance
3. walk forward
4. jog or short run
5. stop / brake
6. turn or pivot
7. short recovery / rebalance step

This is the minimum sequence required for G2.

## Preferred G2 Format

The preferred Stage 1 G2 format is now:

- one live side-by-side PIE comparison
- same map
- same camera framing
- same session
- same moment-to-moment action sequence

Preferred actor roles:

- `Physics-Driven`: the active bridge Manny
- `Kinematic`: a comparison Manny spawned as the baseline

Preferred trigger path:

- enter `BridgeActive`
- run `PhysAnim.G2.StartPresentation`
- compare both actors through the same scripted sequence and fixed camera
- the first phase is now a short scripted walking perturbation with visible pusher boxes
- the `Physics-Driven` actor receives the physical contact disturbance
- the `Kinematic` actor stays on the same scripted locomotion path without receiving that contact
- the presentation camera uses a closer perturbation framing first, then returns to the wider locomotion framing

Fallback trigger path:

- enter `BridgeActive`
- run `PhysAnim.G2.StartSideBySide`
- use it only for ad hoc sanity checks when the scripted presentation harness is unavailable

If the live side-by-side path is unavailable, two clearly comparable recordings are still acceptable, but they are now the fallback, not the default.

## Lock Rules

Before G2:

- every motion actually used in the comparison must be `locked` in [motion-source-lock-table.md](/F:/NewEngine/plans/stage1/50-content/motion-source-lock-table.md)
- the comparison capture must name the exact clips or motion sources used
- kinematic and physics-driven versions must use the same sequence order
- if live side-by-side is used, both actors must be driven by the same frozen live sequence or mirrored input path
- the two actors must not physically interfere with each other
- labels or other role markers must make it obvious which actor is `Kinematic` and which is `Physics-Driven`

## Not Allowed

Do not use any of these as the sole G2 basis:

- idle only
- one isolated locomotion fragment with no transitions
- a hand-picked best-looking fragment with no braking or recovery
- a sequence that differs between baseline and physics-driven capture

## Required Handoff Fields

The comparison-sequence handoff must include:

- exact motion list in order
- exact clip/file references once known
- confirmation that the same sequence is used in both captures
