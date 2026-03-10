# Task Packet S1-P1-A2

## Task ID

`S1-P1-A2`

## Goal

Package the G2 comparison so the user can judge Stage 1 quality without inventing the methodology at runtime.

## Frozen Inputs

- accepted one-character Phase 1 result
- [g2-evaluation.md](/F:/NewEngine/plans/stage1/g2-evaluation.md)
- [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md)
- [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- current [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)

## Writable Paths

- [g2-evaluation.md](/F:/NewEngine/plans/stage1/g2-evaluation.md)
- [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)

## Required Work

1. Freeze the comparison sequence.
2. Freeze the capture parity rules for kinematic vs physics-driven clips.
3. Prepare the exact evidence the user must return.
4. Pre-score any obvious blockers before the user is asked to judge G2.

## Definition Of Done

- the user can run G2 with one clear comparison method
- the evidence package can support `pass`, `fail`, or `blocked`
- no hidden setup mismatch remains in the comparison plan

## Escalate To User When

- the comparison sequence cannot be frozen cleanly
- the evidence is not comparable enough for a fair judgment

## Verification

- review against [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- handoff must use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md)

## Next Consumer

- user for `G2`
