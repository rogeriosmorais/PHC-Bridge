# Balance Mode Entry / Transition Spec

Status: Authoritative implementation design  
Scope: Stage 1 runtime entry path into Balance Perturbation Mode

## 1. Purpose

This document specifies the entry and transition design for Balance Perturbation Mode.

It defines:

- request handling
- queueing
- state machine
- preflight
- ownership rules
- transition phases
- failure and recovery
- activation boundary

It also reflects the current state of the investigation:

- many earlier problems were contract problems
- the current leading remaining issue is Phase 1 physical viability after a now-much-cleaner ownership and topology contract

## 2. Core rule

Balance Perturbation Mode is not active until the transition succeeds.

A queued request is not active mode.  
Phase 1 is not active mode.  
Phase 2 is not active mode.

## 3. Required runtime states

Minimum authoritative state set:

- `BridgeActive`
- `BalanceStartQueued`
- `BalanceTransition_Preflight`
- `BalanceTransition_Phase1_Prepare`
- `BalanceTransition_Phase1_LateValidate`
- `BalanceTransition_Phase2_RootOn`
- `BalancePerturbationActive`
- `BalanceTransitionFailed`
- `SafeDenied`

## 4. Queueing rules

A balance request must be queued, not rejected, when the runtime is in a valid bridge context but temporarily not eligible.

Queue-worthy blockers include:

- control ramp incomplete
- policy influence below required threshold
- startup handoff incomplete

These are temporary blockers, not failures.

## 5. Ownership rule

Every transition condition must be one of:

- observed-only
- transition-owned
- external-owned

If a condition is transition-owned, preflight must not reject forever merely because that condition is currently false.

## 6. Phase 1 Truth Model Alignment

Phase 1 behavior is governed by the authoritative [Phase 1 / LateValidate Truth Model](./phase1-late-validate-truth-model.md).

Current important rules:

- A Phase 1 attempt may be contract-correct and still physically non-viable.
- The frozen Phase 1 topology record is the contract source of truth once the attempt is accepted.
- Live readiness reclassification must not silently rewrite that frozen record during the same attempt.

## 7. Certified handoff concept

Phase 1 should emit a certified handoff payload only when the contract conditions are satisfied.

That payload must not imply more than the runtime has actually proven.

In particular:

- a clean topology + suppression + quiet proof does not automatically prove dynamic viability
- the accepted setup must also survive the required admission and validation margins

## 8. Safe denial rule

The pipeline must support explicit safe denial.

Safe denial is valid when:

- the entry contract is satisfied enough to make a truthful decision
- but the mode cannot proceed safely

## 9. Current Phase 1 contract reality

The entry pipeline has moved through several contract-cleanup steps that are now part of the design, not just debugging:

- frozen Phase 1 topology capture
- next-frame ownership confirmation instead of same-frame overclaiming
- explicit distinction between intended ownership, modifier-record ownership, and raw body state
- suppression of BridgeActive distal re-promotion
- authoritative per-bone movement-type writes for topology-critical Phase 1 bones
- frozen upper-body ownership mode for the LateValidate sustain window

These are no longer optional implementation details. They are part of the real transition contract.

## 10. Last Confirmed Blocker

The current first meaningful remaining blocker after those contract fixes is:

- `phase1_late_validate_upper_body_instability`

The entry-transition design must now explicitly allow that the accepted topology can be correct while the physical-viability proof (convergence) still fails during LateValidate. This is now driven by the consolidated observed-violation gate for upper-body bones.

## 11. Logging contract

Required one-shot logs include:

- request queued
- queue gates satisfied
- preflight begin
- preflight reject / accept
- phase changes
- frozen Phase 1 topology snapshot
- Phase 1 summary
- Phase 2 deny / root-on summary
- activation
- cleanup summary

The logs must make it possible to tell whether failure was:

- a contract failure
- or a physical-viability failure

And, for Phase 1, whether the mismatch was in:

- frozen ownership capture
- intended ownership
- modifier-record ownership
- raw body state
- live sim-coverage vs expected sim-coverage

## 12. Recovery contract

When transition fails, recovery must:

- stop transition-only behaviors
- clear transition-local timers and suppression
- restore coherent `BridgeActive`
- return to one coherent state only:
  - `BridgeActive`
  - `BalanceTransitionFailed`
  - or `SafeDenied`

Retries must not brute-force through an unchanged physical failure with no new evidence.

## 13. Invariants

These must always hold:

- queued requests must have a real future satisfaction path
- transition-owned conditions must not be treated as permanent rejection reasons
- active mode cannot be claimed before real transition success
- failed transition must restore coherent `BridgeActive`
- the frozen Phase 1 record must remain the ownership source of truth until the attempt terminates or advances
- topology-critical ownership must not be silently delegated to broad-set writes that are known not to be authoritative enough

## 14. Acceptance criteria

This transition spec is satisfied only when:

- balance entry is implemented as a distinct state path
- a dedicated frozen Phase 1 topology record exists
- transition-owned conditions are treated correctly
- contract-correct but physically non-viable safe denial is possible
- logs can distinguish contract failure from physical-viability failure
- the runtime cannot silently complete the smoke in generic `BridgeActive`
