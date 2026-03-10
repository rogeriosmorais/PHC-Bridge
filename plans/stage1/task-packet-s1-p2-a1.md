# Task Packet S1-P2-A1

## Task ID

`S1-P2-A1`

## Goal

Turn a passed G2 into a tightly scoped two-character demo package without letting Stage 1 expand into a full game.

## Frozen Inputs

- `G2` marked `pass`
- [phase2-demo-package.md](/F:/NewEngine/plans/stage1/phase2-demo-package.md)
- current [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)
- accepted Phase 1 handoff

## Writable Paths

- [phase2-demo-package.md](/F:/NewEngine/plans/stage1/phase2-demo-package.md)
- [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)

## Required Work

1. Freeze the duplication plan for the second fighter.
2. Freeze the minimal impact-response scope or an explicit defer decision.
3. Freeze the smallest presentation layer needed for G3:
   - player input
   - basic opponent
   - camera
   - HUD
4. Keep demo scope focused on motion quality, not feature completeness.

## Definition Of Done

- the Phase 2 demo can be built without broad new planning
- every major demo feature is either explicitly included or explicitly deferred
- the package is narrow enough to preserve the Stage 1 thesis

## Escalate To User When

- demo expectations expand past proof-of-concept scope
- impact response or presentation decisions become product-scope choices

## Verification

- static review against [phase2-demo-package.md](/F:/NewEngine/plans/stage1/phase2-demo-package.md)
- consistency review against [g3-evaluation.md](/F:/NewEngine/plans/stage1/g3-evaluation.md)
- handoff must use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md)

## Next Consumer

- implementation owner for Phase 2
- then `S1-P2-A2`
