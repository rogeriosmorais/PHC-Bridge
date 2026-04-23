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
- balance activation rules are contract rules and must exist in `10-specs`

## Current Execution Focus

The active Stage 1 direction is now balance-first activation.

Current focus:

1. keep the balance-critical chain continuously simulated as much as possible
2. minimize topology and ownership flips during activation
3. ramp controller authority gradually onto an already-physical state
4. preserve truthful diagnostics as observability only
5. distinguish contract failures from physical-viability failures
6. require balance activation to reach `BalanceActive_Standing` and hold it for `3.0` seconds before any run counts as success
7. treat shell bookkeeping and shell influence as separate observables
8. defer legacy flip-path refinement except where needed for temporary backward-compat notes

## Current Stage 1 Truth

The Stage 1 balance investigation now assumes:

- the earlier flip-based `Prepare -> LateValidate -> RootOn -> Settle` model is conceptually flawed as the target design
- the new target design is activation onto a continuously physical balance-critical chain
- truthful diagnostics are still required, but diagnostics must not justify grace-based passing
- truthful safe denial remains useful forensics, not product success
- the only passing benchmark remains `BalanceActive_Standing` held continuously for `3.0` seconds
- the next engineering slice is balance-first activation implementation, not further certification of ownership flips

## Frozen Balance Rule

Stage 1 treats balance activation as a separate contract from normal bridge startup.

Normal bridge startup may still use staged bring-up for non-critical systems.

Balance activation must not silently treat the old multi-phase handoff model as its target source of truth.

The authoritative balance-activation contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Required Smoke Outcome

The `PhysAnim.PIE.BalanceModeSmoke` test is successful only if the run ends as:

- `BalanceActive_Standing`

The test is a failure if the run ends in:

- `BridgeActive`
- `BridgeActive_Physical`
- `BalanceActivation_BlendIn`
- `BalanceActivation_StandingValidation`
- `BalanceActive_Recovery`
- explicit safe denial
- unresolved entry ambiguity
- misleading success caused by hidden assistance

## Planning Bundle Freeze

The planning bundle under `plans/stage1/` remains frozen except for contract corrections and design updates required to reflect proven evidence.

Only these categories may change during the current stabilization loop:

- `10-specs` documents that define runtime contract
- `40-design` documents that explain the implementation and design consequences of that contract
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

- the balance-activation contract is explicit in `10-specs`
- `STAGE1_PLAN.md` points to that contract
- `40-design` repeats the same contract without divergence
- the docs explicitly distinguish contract correctness from physical viability
- no authoritative document implies a flip-based handoff is the intended activation mechanism
- the docs explicitly define continuous physical ownership of the balance-critical chain as the target design
- the docs explicitly define controller authority as a gradual blend onto an already-physical state
- the docs explicitly state that diagnostics are observational and cannot justify grace-based passing
- the docs explicitly state that truthful safe deny is not a passing outcome
- the docs explicitly state the active benchmark: `BalanceActive_Standing` held for `3.0` continuous seconds

## Architectural Direction

The active direction and the long-term target are now the same:

- a balance-first runtime
- continuously physical balance-critical bodies
- gradual controller blend-in
- standing validation before active mode
- recovery or denial based on truthful physical outcomes
