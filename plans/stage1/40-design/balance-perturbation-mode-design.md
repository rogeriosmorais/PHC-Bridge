# Balance Perturbation Mode Design

Status: Authoritative feature overview  
Scope: Stage 1 balance-only diagnostic mode

## 1. Purpose

Balance Perturbation Mode is a standing-balance diagnostic mode for Stage 1.

Its job is to measure true articulated-body recovery after controlled perturbations without conflating that recovery with:

- locomotion assistance
- shell/world translation assistance
- capsule dragging
- startup handoff assistance
- hidden movement-system correction
- transition-only shaping that exists only during entry

## 2. Document split

This file is the feature-level overview only.

The normative runtime contracts live in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`
- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`
- `plans/stage1/40-design/balance_mode_phase2.md`

Interpretation rule:

- this file defines what the feature is for
- the `10-specs` documents define the authoritative contract
- the `40-design` documents describe how that contract is carried through implementation and validation

## 3. Activation rule

Balance Perturbation Mode is active only after the entry transition succeeds.

That means:

- not while queued
- not during preflight
- not during Phase 1
- not during Phase 2
- not during settle

## 4. Current design reality

The balance-entry pipeline is now good enough to separate two different questions:

### A. Is the runtime contract correct?
This includes:
- explicit accept / deny behavior
- explicit ownership transitions
- explicit freeze lifetime
- explicit write-routing behavior

### B. Is the accepted setup physically viable?
This includes:
- whether the accepted Phase 1 setup can remain dynamically quiet
- whether contact and tuning destabilize the sim set
- whether the current topology has enough stability margin

This design file must not blur those questions.

## 5. Active-mode requirement

When active:

- standing balance only
- no locomotion entry
- no bridge-owned movement drive
- no CharacterMovement walking authority
- no shell/world translation as a recovery mechanism
- perturbation only when active quiet proof is valid

## 6. Perturbation method

Canonical initial method:

- target body: `pelvis`
- type: single instantaneous linear impulse
- space: world space
- directions: `+X`, `-X`, `+Y`, `-Y`

## 7. Trustworthiness rule

The framework is trustworthy only if it can demonstrate all of the following:

- perturbation reaches the pelvis body
- response is measurable
- response scales with perturbation tier
- shell/world correction is absent or explicitly flagged
- recovery metrics are stable and repeatable
- if control authority is intentionally weakened, recovery worsens

## 8. Summary

Balance Perturbation Mode is a standing-balance diagnostic mode.

Its entry pipeline is now best understood as:

- a solved-or-improving contract problem in some areas
- and an open physical-viability experiment in others

That distinction must remain explicit in all future design and implementation work.
