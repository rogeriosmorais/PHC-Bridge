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

- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `./phase1-late-validate-truth-model.md`
- `./balance_mode_entry_transition_spec.md`
- `./balance_mode_phase1_stabilization_spec.md`

Interpretation rule:

- this file defines what the feature is for
- the specs define the authoritative contract
- the design files describe how that contract is carried through implementation and validation

## 3. Activation rule

Balance Perturbation Mode is active only after the entry transition succeeds.

That means:

- not while queued
- not during preflight
- not during Phase 1
- not during Phase 2
- not during settle

## 4. Current design reality

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
- whether contact and tuning destabilize the sim set
- whether the current topology has enough stability margin
- whether live sim coverage matches the frozen expected sim set

### C. Is the active balance behavior itself good once entry succeeds?
This includes:
- perturbation delivery
- measurable recovery
- repeatable metrics
- no hidden locomotion assist

This file must not blur those questions.

## 5. Current accepted entry shape

The current accepted Phase 1 design now assumes:

- root = kinematic
- proximal = simulated
- distal = kinematic
- upper = kinematic during LateValidate hold

That is a deliberate pre-root-on staging topology, not a final active-mode topology.

## 6. Current resolved contract problems

These are now treated as resolved or largely-resolved contract issues, not the main open problem:

- same-frame overclaiming of ownership violations
- BridgeActive distal re-promotion thrash
- premature release of upper-body LateValidate hold due to wrong frozen ownership capture
- dependence on broad `"All"` movement-type writes for topology-critical Phase 1 bones

## 7. Last Confirmed Problem

The last confirmed major blocker is:

- `phase1_late_validate_upper_body_instability`

This is why convergence failure during LateValidate is currently more important than the earlier contract bugs.

## 8. Active-mode requirement

When active:

- standing balance only
- no locomotion entry
- no bridge-owned movement drive
- no CharacterMovement walking authority
- no shell/world translation as a recovery mechanism
- perturbation only when active quiet proof is valid

## 9. Perturbation method

Canonical initial method:

- target body: `pelvis`
- type: single instantaneous linear impulse
- space: world space
- directions: `+X`, `-X`, `+Y`, `-Y`

## 10. Trustworthiness rule

The framework is trustworthy only if it can demonstrate all of the following:

- perturbation reaches the pelvis body
- response is measurable
- response scales with perturbation tier
- shell/world correction is absent or explicitly flagged
- recovery metrics are stable and repeatable
- if control authority is intentionally weakened, recovery worsens

## 11. Summary

Balance Perturbation Mode is a standing-balance diagnostic mode.

Its entry pipeline is now best understood as:

- a much cleaner and more explicit contract than before
- but still an open physical-viability experiment in Phase 1

That distinction must remain explicit in all future design and implementation work.
