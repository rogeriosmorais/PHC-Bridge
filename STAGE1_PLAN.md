# Stage 1 Plan

## Purpose

This document is the Stage 1 index and control document.

It defines the frozen document hierarchy, the active execution focus, and the rule for which document wins when two documents overlap.

## Stage 1 Document Hierarchy

Stage 1 uses a strict document hierarchy.

Authority order:

1. `plans/stage1/10-specs/*`
2. `STAGE1_PLAN.md`
3. `plans/stage1/40-design/*`

Interpretation rules:

- if a `40-design` document conflicts with a `10-specs` document, the `10-specs` document wins
- if `STAGE1_PLAN.md` conflicts with a `10-specs` document, the `10-specs` document wins
- `40-design` may explain or sequence implementation work, but it may not introduce a runtime contract that is absent from `10-specs`
- balance-mode entry rules are contract rules, so they must exist in `10-specs`, not only in `40-design`

## Current Execution Focus

Phase 1 bridge implementation and stabilization remain active.

Current focus:

1. freeze the balance-mode entry contract
2. align normal bridge bring-up and balance-mode late validation
3. make the PIE balance smoke pass with either:
   - active balance mode, or
   - a safe denial
4. eliminate the invalid end state where the smoke finishes in plain `BridgeActive`

## Frozen Balance-Mode Rule

Stage 1 now treats balance entry as a separate contract from normal bridge startup.

Normal bridge startup is allowed to use staged non-root bring-up.

Balance-mode late validation is not allowed to reuse the normal bring-up topology as its validation topology.

Late validation must operate on a frozen balance-entry topology defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Required Smoke Outcome

The `PhysAnim.PIE.BalanceModeSmoke` test is successful only if the run ends as one of:

- `BalanceActive`
- explicit safe denial

The test is a failure if the run ends in:

- `BridgeActive`

## Planning Bundle Freeze

The planning bundle under `plans/stage1/` remains frozen except for contract corrections required to align code and design.

Only these categories may change during the current stabilization loop:

- `10-specs` documents that define the balance-mode entry contract
- `40-design` documents that must be rewritten to match that contract
- execution/evidence documents that record the result

## Planning Bundle Index

### Contract documents

- `plans/stage1/10-specs/bridge-spec.md`
- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`

### Derived design documents

- `plans/stage1/40-design/balance-mode-design.md`
- `plans/stage1/40-design/balance-mode-smoke-design.md`

## Balance-Mode Alignment Rule

All balance-mode design text in the repo must align to these statements:

- late validation uses a dedicated balance-entry topology
- late validation does not inherit upper-body simulation ownership from normal bring-up
- `upperBodySimCount` for late validation must reflect the late-validation topology, not the transient normal bring-up topology
- the smoke must not terminate while balance entry is still unresolved inside plain `BridgeActive`

## Acceptance Rule For Documentation

The design is considered documented only when all of the following are true:

- the balance-mode entry contract is explicit in `10-specs`
- `STAGE1_PLAN.md` points to that contract
- `40-design` repeats the same contract without divergence
- no remaining design text describes late validation as a continuation of normal bring-up simulation ownership
