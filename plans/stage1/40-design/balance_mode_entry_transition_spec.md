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
many earlier problems were contract problems; the remaining leading issue is the physical viability of the accepted Phase 1 setup.

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

## 6. Phase 1 interpretation

Phase 1 exists to produce a valid pre-root-on state.

Current important rule:

- a Phase 1 attempt may be contract-correct and still physically non-viable

This document therefore treats Phase 1 output as both:

- a contract product
- and a viability test result

## 7. Certified handoff concept

Phase 1 should emit a certified handoff payload only when the contract conditions are satisfied.

That payload must not imply more than the runtime has actually proven.

In particular:
a clean topology + suppression + quiet proof does not automatically prove dynamic viability unless the accepted setup has survived the required admission and validation margins.

## 8. Safe denial rule

The pipeline must support explicit safe denial.

Safe denial is valid when:

- entry contract is satisfied enough to make a truthful decision
- but the mode cannot proceed safely

## 9. Recovery contract

When transition fails, recovery must:

- stop transition-only behaviors
- clear transition-local timers and suppression
- restore coherent `BridgeActive`
- return to one coherent state only:
  - `BridgeActive`
  - `BalanceTransitionFailed`
  - or `SafeDenied`

## 10. Logging contract

Required one-shot logs include:

- request queued
- queue gates satisfied
- preflight begin
- preflight reject / accept
- phase changes
- Phase 1 summary
- Phase 2 deny / root-on summary
- activation
- cleanup summary

The logs must make it possible to tell whether failure was:

- a contract failure
- or a physical-viability failure

## 11. Invariants

These must always hold:

- queued requests must have a real future satisfaction path
- transition-owned conditions must not be treated as permanent rejection reasons
- active mode cannot be claimed before real transition success
- failed transition must restore coherent `BridgeActive`
- retries must not brute-force through unchanged physical failure
