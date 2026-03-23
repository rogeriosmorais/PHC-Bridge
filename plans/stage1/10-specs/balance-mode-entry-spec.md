# Balance Mode Entry Spec

## Purpose

This document defines the authoritative Stage 1 balance-entry contract.

It exists to separate:

- normal bridge startup behavior
- balance-entry state-machine behavior
- Phase 1 ownership and write-routing behavior
- the still-open physical-viability question

## Core interpretation

Balance entry is a distinct runtime contract layered on top of a running bridge.

The current leading question is no longer whether the runtime can represent entry at all.

The current leading question is whether the accepted Phase 1 frozen setup is physically viable once the contract is enforced correctly.

## Phase 1 Truth Model Alignment

Phase 1 behavior is governed by the authoritative [Phase 1 / LateValidate Truth Model](file:///f:/NewEngine-AgentB/plans/stage1/40-design/phase1-late-validate-truth-model.md).

The accepted Phase 1 topology is:

- `root = kinematic`
- `proximal = simulated`
- `distal = kinematic`
- `upper = kinematic`

Interpretation rules:

- The root side may remain kinematic during Phase 1.
- `pelvisSimulating=false` is not, by itself, a Phase 1 deny condition.
- Phase 1 topology intent and raw body sim state are not guaranteed to be frame-synchronous; violations must be tracked according to the truth-model confirmation rules.
- Topology changes are ownership changes, not mere tuning changes.

## Phase 1 write-routing contract

During Prepare and LateValidate:

- normal policy target writes over the accepted Phase 1 set are suppressed
- only the explicit allowed hold path may publish to the allowed kinematic bones
- simulated Phase 1 bones must not receive held target writes
- diagnostics must distinguish `normal`, `held`, and `total`

## Phase 1 freeze contract

On transition accept:

- freeze is acquired

During the full Phase 1 attempt:

- freeze remains active
- Prepare / LateValidate bounces do not release it

Freeze releases only when the attempt reaches terminal success, terminal safe deny, explicit abort, or teardown.

## Convergence-snapshot contract

Prepare and LateValidate decisions must use an authoritative post-update convergence snapshot.

That snapshot is the source of truth for:

- authoritative root tilt
- root validity
- body-motion instability metrics
- target-delta metrics
- shell/reference deltas used by gating

## Contract correctness vs physical viability

### Contract correctness

Phase 1 is contract-correct when:

- topology is accepted correctly
- suppression is correct
- hold-only semantics are correct
- freeze lifetime is correct
- convergence timing/source is correct
- terminal reasons are truthful

### Physical viability

Phase 1 is physically viable only if the accepted setup remains dynamically quiet enough to survive entry under current:

- control tuning
- contact behavior
- sub-step regime
- hold/reference behavior

A run may satisfy contract correctness and still fail physical viability.

## Required terminal truthfulness

When the accepted Phase 1 setup fails because the sim set is dynamically unstable, the deny/reset path should identify that explicitly rather than collapsing everything to a generic no-convergence label.

## Acceptance criteria

This spec is satisfied only when:

- the accepted Phase 1 topology is explicit
- the write-routing contract is explicit
- the freeze contract is explicit
- the convergence snapshot contract is explicit
- the docs explicitly allow a contract-correct but physically non-viable Phase 1 result
