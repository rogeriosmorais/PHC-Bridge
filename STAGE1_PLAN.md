# Stage 1 Plan

## Purpose

This document is the Stage 1 index and control document.

It defines:

- the Stage 1 document hierarchy
- the current execution focus
- the distinction between contract correctness and physical viability
- the rule for which document wins when two documents overlap

## Stage 1 document hierarchy

Stage 1 uses a strict document hierarchy.

Authority order:

1. `plans/stage1/10-specs/*`
2. `STAGE1_PLAN.md`
3. `plans/stage1/40-design/*`

Interpretation rules:

- if a `40-design` document conflicts with a `10-specs` document, the `10-specs` document wins
- if `STAGE1_PLAN.md` conflicts with a `10-specs` document, the `10-specs` document wins
- `40-design` may explain or sequence implementation work, but it may not introduce a runtime contract absent from `10-specs`
- balance-entry rules are runtime-contract rules, so they must exist in `10-specs`, not only in `40-design`

## Current execution focus

Phase 1 bridge implementation and stabilization remain active.

Current focus, in order:

1. keep the Phase 1 runtime contract truthful and explicit
2. preserve the accepted Phase 1 topology and write-suppression contract
3. keep the balance smoke ending only in explicit balance outcomes
4. evaluate whether the accepted Phase 1 frozen setup is physically viable

## The key distinction for current work

Stage 1 must now separate two kinds of work.

### Contract correctness

Contract correctness means:

- request / accept path is explicit
- runtime states are explicit
- accepted topology is explicit
- hold-only vs policy writes are separated correctly
- authoritative post-update telemetry drives convergence checks
- freeze lifetime covers the full Phase 1 attempt
- terminal outcomes are explicit and truthful

### Physical viability

Physical viability means:

- the accepted frozen Phase 1 setup actually remains dynamically quiet enough to survive Prepare and LateValidate
- contact, tuning, and ownership assumptions are strong enough for the setup to hold

A Phase 1 attempt can be contract-correct and still be physically non-viable.

That is the current working assumption behind the remaining stabilization loop.

## Frozen balance-mode rule

Stage 1 treats balance entry as a separate contract from normal bridge startup.

Normal bridge startup is allowed to use staged non-root bring-up.

Balance-mode validation is not allowed to silently inherit ordinary startup ownership rules.

The authoritative balance-entry contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Current accepted Phase 1 topology rule

Under the current design, the accepted Phase 1 topology is:

- root: kinematic
- proximal: simulated
- distal: simulated
- upper body: kinematic

This means the current design does **not** require pelvis/root simulation as a prerequisite.

Any implementation or design text that treats `pelvis_not_simulating` as a terminal Phase 1 violation under `root=kin` is contract-invalid.

## Required smoke outcome

The `PhysAnim.PIE.BalanceModeSmoke` test is successful only if the run ends as one of:

- `BalanceActive`
- explicit safe denial

The test is a failure if the run ends in:

- `BridgeActive`
- unresolved balance-entry intermediate states
- plain `Failed`

## What is currently considered solved enough to keep

The following areas are now considered substantially aligned and should not be casually reopened without new evidence:

- explicit balance-entry state path
- accepted topology snapshot
- hold-only write behavior in Prepare / LateValidate
- split diagnostics for normal vs held target writes
- post-update convergence snapshot timing
- freeze lifetime contract
- root tilt source correction
- specific body-motion terminal deny reasons

## What remains active

The active problem is no longer “make the state machine explicit.”

The active problem is:

> can the accepted Phase 1 frozen setup survive physically under current control, tuning, and contact conditions?

That means the remaining loop should prioritize:

1. stronger admission viability checks
2. clearer separation between blocked admission and terminal deny
3. better physical diagnosis of why the accepted sim set destabilizes
4. revision of tuning / contact / hold-set / topology assumptions only when the logs prove the current setup is non-viable

## Planning bundle freeze

The planning bundle under `plans/stage1/` remains frozen except for:

- contract corrections required to align code and design
- viability clarifications required to document what is proven vs still hypothetical
- execution/evidence updates that describe current runtime truth

## Planning bundle index

### Contract documents

- `plans/stage1/10-specs/bridge-spec.md`
- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`

### Derived design documents

- `plans/stage1/40-design/balance-mode-design.md`
- `plans/stage1/40-design/balance-mode-smoke-design.md`

## Balance-mode alignment rules

All balance-mode design text in the repo must align to these statements:

- balance entry is a separate runtime contract layered on top of a running bridge
- Prepare and LateValidate use a dedicated accepted topology snapshot
- Prepare and LateValidate are hold-only from the control-write side
- root/pelvis may remain kinematic under the accepted Phase 1 topology
- convergence admission must use authoritative post-update telemetry
- body-motion instability of the accepted sim set is a first-class physical failure, not just a generic fallback reason
- the smoke must not terminate while balance entry is still unresolved inside plain `BridgeActive`

## Acceptance rule for documentation

Stage 1 documentation is considered aligned only when all of the following are true:

- the balance-entry contract is explicit in `10-specs`
- `STAGE1_PLAN.md` points to that contract clearly
- `40-design` repeats the same contract without divergence
- the docs explicitly distinguish contract correctness from physical viability
- no remaining design text describes LateValidate as a continuation of normal bring-up ownership or treats root/pelvis simulation as required under `root=kin`
