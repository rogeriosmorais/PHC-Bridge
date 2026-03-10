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
- at least one combat-style action if the combat path is ready

## Primary G2 Sequence

The default G2 comparison sequence is:

1. relaxed idle / ready stance
2. walk forward
3. jog or short run
4. stop / brake
5. turn or pivot
6. short recovery / rebalance step

This is the minimum sequence required for G2.

## Combat Add-On Sequence

If the combat path is already viable by the end of Phase 1, add this short second sequence:

1. guard pose
2. jab
3. cross or hook
4. dodge / evade
5. front kick or round kick
6. return to stance

## Lock Rules

Before G2:

- every motion actually used in the comparison must be `locked` in [motion-source-lock-table.md](/F:/NewEngine/plans/stage1/motion-source-lock-table.md)
- the comparison capture must name the exact clips or motion sources used
- kinematic and physics-driven versions must use the same sequence order

## Minimum Acceptable G2 Scope

G2 may proceed with locomotion-only evidence only if:

- the primary sequence above is fully locked
- the combat path is explicitly marked not ready yet
- the orchestrator writes that limitation into [g2-evaluation.md](/F:/NewEngine/plans/stage1/g2-evaluation.md)

Do not silently downgrade G2 to a weaker sequence.

## Not Allowed

Do not use any of these as the sole G2 basis:

- idle only
- one isolated punch only
- a hand-picked best-looking fragment with no braking or recovery
- a sequence that differs between baseline and physics-driven capture

## Required Handoff Fields

The comparison-sequence handoff must include:

- exact motion list in order
- exact clip/file references once known
- whether combat add-on is included
- reason if combat add-on is omitted
- confirmation that the same sequence is used in both captures
