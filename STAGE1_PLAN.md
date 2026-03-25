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
5. determine whether the current RootOn warm-start choreography is both contract-correct and physically viable
6. if not, revise topology / admission margin / hold set / RootOn choreography / tuning using evidence rather than ad hoc workaround changes

## Current Stage 1 truth

The Stage 1 balance-entry investigation has reached this point:

- many earlier failures were contract / ownership / telemetry problems
- those areas are now substantially cleaner
- Phase 1 upper-body hold / LateValidate bookkeeping is no longer the dominant active blocker
- the current active investigation surface is Phase 2 RootOn truthfulness and viability

This is progress, not regression.

The design is now sharp enough to be falsified by runtime at a more specific level.

## Frozen Balance-Mode Rule

Stage 1 treats balance entry as a separate contract from normal bridge startup.

Normal bridge startup may use staged non-root bring-up.

Balance-mode Prepare, LateValidate, and RootOn must not silently reuse normal bring-up semantics as their source of truth.

The authoritative balance-entry contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Required Smoke Outcome

The `PhysAnim.PIE.BalanceModeSmoke` test is successful only if the run ends as one of:

- `BalanceActive`
- explicit safe denial

The test is a failure if the run ends in:

- `BridgeActive`
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
- `plans/stage1/40-design/phase1-late-validate-truth-model.md`
- `plans/stage1/40-design/phase2-rooton-truth-model.md`

## Documentation acceptance rule

The design is considered documented only when all of the following are true:

- the balance-mode entry contract is explicit in `10-specs`
- `STAGE1_PLAN.md` points to that contract
- `40-design` repeats the same contract without divergence
- the docs explicitly distinguish contract correctness from physical viability
- no remaining design text implies that a contract-correct Phase 1 or Phase 2 setup is automatically physically viable
- Phase 2 documents explicitly define the RootOn source-of-truth order and shell/policy suppression semantics

## Long-term architectural direction

The long-term target remains always-on balance.

That means the current split between normal runtime and balance entry should eventually collapse into a single balance-first runtime with subphases for startup, settle, active, recovery, and safe deny.

That future direction does not change the current Stage 1 need to finish the entry investigation cleanly.
