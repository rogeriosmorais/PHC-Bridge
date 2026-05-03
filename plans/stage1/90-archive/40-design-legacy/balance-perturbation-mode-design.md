# Balance Perturbation Mode Design

Status: Authoritative feature overview  
Scope: Stage 1 balance-only diagnostic mode

## 1. Purpose

Balance Perturbation Mode is a standing-balance diagnostic mode for Stage 1.

Its job is to measure true articulated-body recovery after controlled perturbations without conflating that recovery with:

- locomotion assistance
- shell or world translation assistance
- capsule dragging
- hidden movement-system correction
- transition-only shaping that exists only during activation

## 2. Document Split

This file is the feature-level overview only.

The normative runtime contracts live in:

- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`
- `./balance_mode_entry_transition_spec.md`
- `./balance_mode_phase2.md`
- `./balance_mode_phase3_settle.md`
- `./phase1-late-validate-truth-model.md`

## 3. Activation Rule

Balance Perturbation Mode is active only after:

- the bridge reaches `BridgeActive_Physical`
- controller blend-in succeeds
- standing validation succeeds
- `BalanceActive_Standing` then persists continuously for `3.0` seconds

It is not active:

- while queued
- during preflight
- during physical readiness checks
- during controller blend-in
- during standing validation

## 4. Current Design Reality

The current entry pipeline is now best understood as three separate questions:

### A. Is the runtime contract correct?

This includes:

- explicit accept and deny behavior
- continuous physical ownership of the balance-critical chain
- explicit controller blend rules
- explicit terminal outcomes

### B. Is the accepted setup physically viable?

This includes:

- whether the already-physical chain remains stable
- whether controller blend-in destabilizes it
- whether standing validation can complete truthfully
- whether `BalanceActive_Standing` can persist for the benchmark hold window

### C. Is the active balance behavior itself good once entry succeeds?

This includes:

- perturbation delivery
- measurable recovery
- repeatable metrics
- no hidden locomotion assist

This file must not blur those questions.

## 5. Active-Mode Requirement

When active:

- standing balance only
- no locomotion entry
- no bridge-owned movement drive
- no CharacterMovement walking authority
- no shell or world translation as a recovery mechanism
- perturbation only when active quiet proof is valid

## 6. Perturbation Method

Canonical initial method:

- target body: `pelvis`
- type: single instantaneous linear impulse
- space: world space
- directions: `+X`, `-X`, `+Y`, `-Y`

## 7. Trustworthiness Rule

The framework is trustworthy only if it can demonstrate all of the following:

- perturbation reaches the pelvis body
- response is measurable
- response scales with perturbation tier
- shell or world correction is absent or explicitly flagged
- recovery metrics are stable and repeatable
- if control authority is intentionally weakened, recovery worsens

For entry itself, the framework is trustworthy only if it can also demonstrate:

- the balance-critical chain stayed continuously physical
- controller authority blended gradually
- entry reached `BalanceActive_Standing`
- that state held continuously for `3.0` seconds
- safe deny remained a diagnostic failure outcome, not a passing outcome

## 8. Summary

Balance Perturbation Mode is a standing-balance diagnostic mode.

Its activation pipeline is now best understood as:

- a balance-first contract on a continuously physical chain
- a gradual controller blend-in
- a standing-validation hold
- an investigation whose only passing benchmark is sustained `BalanceActive_Standing`
