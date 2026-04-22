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

## 2. Document Split

This file is the feature-level overview only.

The normative runtime contracts live in:

- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`
- `./balance_mode_entry_transition_spec.md`
- `./balance_mode_phase2.md`
- `./balance_mode_phase3_settle.md`
- `./phase1-late-validate-truth-model.md`

Interpretation rule:

- this file defines what the feature is for
- the specs define the authoritative contract
- the design files describe how that contract is carried through implementation and validation

## 3. Activation Rule

Balance Perturbation Mode is active only after the entry transition succeeds.

That means:

- not while queued
- not during preflight
- not during Phase 1
- not during Phase 2 / RootOn
- not during Phase 3 / Settle

Current runtime note:

- Settle success now publishes `BalanceActive_Standing`
- that state is intentionally separate from any future perturbation-time recovery substate
- entry is not considered successful until `BalanceActive_Standing` persists continuously for `3.0` seconds

## 4. Current Design Reality

The project has moved past several false blockers.

The current entry pipeline is now good enough to separate three questions:

### A. Is the runtime contract correct?

This includes:

- explicit accept / deny behavior
- explicit ownership transitions
- explicit freeze lifetime
- explicit write-routing behavior
- correct frozen topology capture

### B. Is the accepted setup physically viable?

This includes:

- whether the accepted Phase 1 setup can remain dynamically quiet
- whether RootOn can warm-start truthfully
- whether post-RootOn Settle continuity can hold long enough to reach `BalanceActive_Standing`
- whether `BalanceActive_Standing` can then persist for the benchmark hold window
- whether contact and tuning destabilize the sim set

### C. Is the active balance behavior itself good once entry succeeds?

This includes:

- perturbation delivery
- measurable recovery
- repeatable metrics
- no hidden locomotion assist

This file must not blur those questions.

## 5. Current Accepted Entry Shape

The current accepted Phase 1 design now assumes:

- root = kinematic
- proximal = simulated
- distal = kinematic
- upper = kinematic during LateValidate hold

That is a deliberate pre-root-on staging topology, not a final active-mode topology.

## 6. Current Resolved Contract Problems

These are now treated as resolved or largely-resolved contract issues, not the main open problem:

- same-frame overclaiming of ownership violations
- BridgeActive distal re-promotion thrash
- premature release of upper-body LateValidate hold due to wrong frozen ownership capture
- dependence on broad `"All"` movement-type writes for topology-critical Phase 1 bones

## 7. Investigation Surface (Temporary)

This section reflects the current investigation focus. These details are temporary and expected to change frequently; they do not form part of the timeless design contract.

### Last Confirmed Failure Mode

- `phase1_late_validate_upper_body_instability`

This is why convergence failure during LateValidate was more important than the earlier contract bugs.

The current balance-entry investigation surface has since moved into:

- truthful RootOn execution in Phase 2
- truthful Settle continuity in Phase 3
- real `BalanceActive_Standing` entry and hold, rather than truthful safe-deny or RootOn-only progress

## 8. Active-Mode Requirement

When active:

- standing balance only
- no locomotion entry
- no bridge-owned movement drive
- no CharacterMovement walking authority
- no shell/world translation as a recovery mechanism
- perturbation only when active quiet proof is valid

## 9. Perturbation Method

Canonical initial method:

- target body: `pelvis`
- type: single instantaneous linear impulse
- space: world space
- directions: `+X`, `-X`, `+Y`, `-Y`

## 10. Trustworthiness Rule

The framework is trustworthy only if it can demonstrate all of the following:

- perturbation reaches the pelvis body
- response is measurable
- response scales with perturbation tier
- shell/world correction is absent or explicitly flagged
- recovery metrics are stable and repeatable
- if control authority is intentionally weakened, recovery worsens

For entry itself, the framework is trustworthy only if it can also demonstrate:

- entry reaches `BalanceActive_Standing`
- that state holds continuously for `3.0` seconds
- safe-deny remains a diagnostic failure outcome, not a passing outcome

## 11. Summary

Balance Perturbation Mode is a standing-balance diagnostic mode.

Its entry pipeline is now best understood as:

- a much cleaner and more explicit contract than before
- a still-open physical-viability experiment in Phase 2 RootOn and Phase 3 Settle
- an investigation whose only passing benchmark is sustained `BalanceActive_Standing`, not truthful deny classification

That distinction must remain explicit in all future design and implementation work.
