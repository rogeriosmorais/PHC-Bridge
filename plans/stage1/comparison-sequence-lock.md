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

1. relaxed idle / ready stance
2. walk forward
3. jog or short run
4. stop / brake
5. turn or pivot
6. short recovery / rebalance step

This is the minimum sequence required for G2.

## Lock Rules

Before G2:

- every motion actually used in the comparison must be `locked` in [motion-source-lock-table.md](/F:/NewEngine/plans/stage1/motion-source-lock-table.md)
- the comparison capture must name the exact clips or motion sources used
- kinematic and physics-driven versions must use the same sequence order

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
