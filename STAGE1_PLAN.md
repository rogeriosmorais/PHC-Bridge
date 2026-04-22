# Stage 1 Plan

## Purpose

This document is the Stage 1 index and control document.

It defines the frozen document hierarchy, the active execution focus, and the rule for which document wins when two documents overlap.

## Stage 1 Document Hierarchy

Authority order:

1. `plans/stage1/10-specs/*`
2. `STAGE1_PLAN.md`
3. `plans/stage1/40-design/*`

Interpretation rules:

- if a `40-design` document conflicts with a `10-specs` document, the `10-specs` document wins
- if `STAGE1_PLAN.md` conflicts with a `10-specs` document, the `10-specs` document wins
- `40-design` may explain or sequence implementation work, but it may not introduce a runtime contract absent from `10-specs`
- balance-mode entry rules are contract rules and must exist in `10-specs`

## Current Execution Focus

Phase 1 bridge implementation and stabilization remain active, but the investigation surface has moved.

Current focus:

1. preserve the now-clean balance-entry state machine contract
2. preserve the accepted Phase 1 write-routing and freeze contracts
3. distinguish contract failures from physical-viability failures
4. preserve a truthful RootOn truth model in Phase 2
5. preserve a truthful Settle continuity contract in Phase 3
6. treat truthful deny and failure taxonomy as observability only, not as product success
7. require balance entry to reach `BalanceActive_Standing` and hold it for `3.0` seconds before any run counts as success
8. isolate why shell maintenance becomes materially corrective during the current latest Phase 3 Settle frontier after an otherwise truthful RootOn handoff
9. determine whether the current Settle `phase3_material_shell_correction` frontier is a contract-level mismatch, a physical-viability limit, or both
10. if needed, revise Settle shell behavior / continuity checks / tuning using evidence rather than ad hoc workaround changes

## Current Stage 1 Truth

The Stage 1 balance-entry investigation has reached this point:

- many earlier failures were contract / ownership / telemetry problems
- those areas are now substantially cleaner
- Phase 1 upper-body hold / LateValidate bookkeeping is no longer the dominant active blocker
- truthful safe denial is now treated as a useful forensic outcome, not a successful smoke outcome
- Phase 1 Prepare / LateValidate and Phase 2 RootOn may now pass truthfully without that implying product success
- the active benchmark is now stricter than RootOn truth:
  balance entry must reach `BalanceActive_Standing` and remain there for `3.0` continuous seconds
- the current active blocker in the latest saved live smoke on `2026-04-22` remains Phase 3 Settle shell-maintenance continuity:
  `phase3_material_shell_correction`
- the current audited shape is an early Settle shell-correction frontier at `tick=2` with `shellVelocityDelta=33.20/10.00` under preserved shell lock after truthful RootOn
- further grace-window refinement is now suspect unless it moves the standing-hold benchmark
- the next engineering slice is truthful standing-benchmark evidence and then Settle shell-maintenance truth, not restart cleanup and not the older Phase 1 readiness-margin frontier

This is progress, not regression.

The design is now sharp enough to be falsified by runtime at a more specific level.

## Frozen Balance-Mode Rule

Stage 1 treats balance entry as a separate contract from normal bridge startup.

Normal bridge startup may use staged non-root bring-up.

Balance-mode Prepare, LateValidate, RootOn, and Settle must not silently reuse normal bring-up semantics as their source of truth.

The authoritative balance-entry contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Required Smoke Outcome

The `PhysAnim.PIE.BalanceModeSmoke` test is successful only if the run ends as one of:

- `BalanceActive_Standing`

The test is a failure if the run ends in:

- `BridgeActive`
- `BalanceActive_Recovery`
- explicit safe denial
- unresolved entry ambiguity
- misleading success caused by hidden same-frame assistance

## Planning Bundle Freeze

The planning bundle under `plans/stage1/` remains frozen except for contract corrections and design updates required to reflect proven evidence.

Only these categories may change during the current stabilization loop:

- `10-specs` documents that define runtime contract
- `40-design` documents that explain the implementation/design consequences of that contract
- evidence documents that record results

## Planning Bundle Index

### Contract documents

- `plans/stage1/10-specs/bridge-spec.md`
- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`

### Derived design documents

- `plans/stage1/40-design/balance-perturbation-mode-design.md`
- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`
- `plans/stage1/40-design/balance_mode_phase2.md`
- `plans/stage1/40-design/balance_mode_phase3_settle.md`
- `plans/stage1/40-design/phase1-late-validate-truth-model.md`
- `plans/stage1/40-design/phase1-transactional-auto-calibration-harness.md`
- `plans/stage1/40-design/phase2-rooton-truth-model.md`

## Documentation Acceptance Rule

The design is considered documented only when all of the following are true:

- the balance-mode entry contract is explicit in `10-specs`
- `STAGE1_PLAN.md` points to that contract
- `40-design` repeats the same contract without divergence
- the docs explicitly distinguish contract correctness from physical viability
- no remaining design text implies that a contract-correct Phase 1, Phase 2, or Phase 3 setup is automatically physically viable
- Phase 2 documents explicitly define the RootOn source-of-truth order and shell / policy suppression semantics
- Phase 3 documents explicitly define the Settle continuity contract, runtime mapping, timers, and emitted failure taxonomy
- the docs explicitly state that truthful safe deny is not a passing outcome
- the docs explicitly state the active benchmark: `BalanceActive_Standing` held for `3.0` continuous seconds

## Long-Term Architectural Direction

The long-term target remains always-on balance.

That means the current split between normal runtime and balance entry should eventually collapse into a single balance-first runtime with subphases for startup, settle, active, recovery, and safe deny.

That future direction does not change the current Stage 1 need to finish the entry investigation cleanly.
