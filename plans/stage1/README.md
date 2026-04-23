# Stage 1 Planning Bundle

> **FROZEN** as of March 11, 2026. See freeze policy below.

## Freeze Policy

The planning phase is complete. Gate G1 has passed. Phase 1 implementation and stabilization are active.

**The original planning bundle was frozen, but the balance-first rewrite has reopened the Stage 1 contract docs listed in `STAGE1_PLAN.md`.**

**Living or actively rewritten documents are:**

- `20-execution/execution-log.md` — active task state
- `20-execution/assumption-ledger.md` — risk tracking
- `30-evidence/g2-evaluation.md` — G2 gate evidence
- the balance-first rewrite docs under `10-specs` and `40-design` that are now named in `STAGE1_PLAN.md`

**Everything else in this bundle is frozen reference material unless the balance-first rewrite explicitly makes it active again.** Do not update specs, task packets, orchestration docs, delegation specs, handoff formats, or user runbooks during active execution unless a code change directly invalidates a frozen contract or the rewrite has explicitly made that doc active.

If a frozen document becomes wrong because of a code change, update it in the same commit as the code change.

## Folder Guide

- `00-control` — **Frozen.** Orchestration model, delegation spec, handoff format.
- `10-specs` — **Mixed during rewrite.** Continuous-balance rewrite specs are active; older lock docs remain frozen.
- `20-execution` — **Mixed.** `execution-log.md` and `assumption-ledger.md` are living. Everything else is frozen.
- `30-evidence` — **Mixed.** `g2-evaluation.md` is living. `g1-evidence.md` and `g3-evaluation.md` are frozen.
- `40-design` — **Mixed during rewrite.** Continuous-balance rewrite design docs are active; older design docs are legacy compatibility docs unless otherwise noted.
- `40-tasks` — **Frozen.** Task graph and task packets.
- `50-content` — **Frozen.** Motion, checkpoint, and scaffold locks.
- `60-user` — **Frozen.** User runbooks and verification instructions.

## Source-of-Truth Rules

- [`STAGE1_PLAN.md`](/F:/NewEngine/STAGE1_PLAN.md) is the Stage 1 index and control document.
- Files in `10-specs` define frozen contracts.
- The 3 living documents record active execution state, not implementation intent.
- Files in `60-user` remain operator-facing and easy to follow.
