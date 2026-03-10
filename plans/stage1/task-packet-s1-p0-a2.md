# Task Packet S1-P0-A2

## Purpose

This is the second execution-ready AI packet for Stage 1 Phase 0.

Use it only after `S1-P0-A1` has been accepted and the required Phase 0 evidence has been captured.

## Task ID

`S1-P0-A2`

## Goal

Collect the Phase 0 results, judge them against the explicit thresholds, and produce the Gate G1 go / no-go package.

## Frozen Inputs

- accepted handoff from `S1-P0-A1`
- [phase0-execution-package.md](/F:/NewEngine/plans/stage1/phase0-execution-package.md)
- [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md)
- [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)
- user evidence for `MV-G1-01`, `MV-G1-02`, and `MV-G1-03`

## Writable Paths

- [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md)
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)
- [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)

## Required Work

1. Fill each G1 criterion with concrete evidence.
2. Mark each criterion `pass`, `fail`, or `blocked` using `acceptance-thresholds.md`.
3. Update the assumption ledger for every assumption materially changed by Phase 0 evidence.
4. Recommend one final G1 verdict:
   - `pass`
   - `fail`
   - `blocked`
5. Record the next task or replan action in the evidence package and execution log.

## Definition Of Done

This task is done only if:

- `g1-evidence.md` contains evidence, verdicts, and a final orchestrator recommendation
- no G1-critical assumption remains silently stale
- the next Stage 1 step is explicit:
  - proceed to Phase 1
  - rework inside Phase 0
  - stop / replan

## Escalate To User When

- a required visual judgment is missing or too ambiguous to score
- the evidence clips or notes are not comparable enough to decide
- a product-level decision is required between fallback and stop

## Verification

- evidence review against [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- consistency review against [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)
- handoff must use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md)

## Next Consumer

- orchestrator for G1 decision
- then `S1-P1-A1` only if G1 passes
