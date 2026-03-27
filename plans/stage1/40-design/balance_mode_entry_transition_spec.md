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
- the current live work is Phase 2 RootOn truthfulness plus Phase 3 Settle continuity after a much cleaner ownership and topology contract

## 2. Core Rule

Balance Perturbation Mode is not active until the transition succeeds.

A queued request is not active mode.  
Phase 1 is not active mode.  
Phase 2 is not active mode.  
Phase 3 is not active mode.

## 3. Required Runtime States

Minimum authoritative runtime-facing state set:

- `BridgeActive`
- `BalanceStartQueued`
- `BalanceTransition_Preflight`
- `BalanceTransition_Phase1_Prepare`
- `BalanceTransition_Phase1_LateValidate`
- `BalanceTransition_Phase2_RootOn`
- `BalanceEntry_Settle`
- `BalancePerturbationActive`
- `BalanceTransitionFailed`
- `SafeDenied`

Required internal transition phases:

- `BRT_Phase1_Prepare`
- `BRT_Phase1_LateValidate`
- `BRT_Phase2_RootOn`
- `BRT_Phase2_ReadyForPhase3`
- `BRT_Phase3_Settle`

Interpretation rules:

- `BRT_Phase2_ReadyForPhase3` is an ephemeral handoff boundary, not an active phase
- runtime `BalanceEntry_Settle` maps to `BRT_Phase3_Settle`

## 4. Queueing Rules

A balance request must be queued, not rejected, when the runtime is in a valid bridge context but temporarily not eligible.

Queue-worthy blockers include:

- control ramp incomplete
- policy influence below required threshold
- startup handoff incomplete

These are temporary blockers, not failures.

## 5. Ownership Rule

Every transition condition must be one of:

- observed-only
- transition-owned
- external-owned

If a condition is transition-owned, preflight must not reject forever merely because that condition is currently false.

## 6. Phase 1 Truth Model Alignment

Phase 1 behavior is governed by the authoritative [Phase 1 / LateValidate Truth Model](./phase1-late-validate-truth-model.md).

Current important rules:

- a Phase 1 attempt may be contract-correct and still physically non-viable
- the frozen Phase 1 topology record is the contract source of truth once the attempt is accepted
- live readiness reclassification must not silently rewrite that frozen record during the same attempt

## 7. Phase 2 Truth Model Alignment

Phase 2 behavior is governed by the authoritative [Phase 2 / RootOn Truth Model](./phase2-rooton-truth-model.md) and [balance_mode_phase2.md](./balance_mode_phase2.md).

Current important rules:

- a RootOn attempt may be contract-correct and still physically non-viable
- the certified Phase 1 handoff remains the topology source of truth until RootOn explicitly and successfully adds root simulation
- intended ownership, raw body state, modifier-record ownership, and shell / policy influence must not be merged into one notion of done
- hidden shell/reference or policy assistance during the RootOn guard window is a contract failure, not a tuning detail

## 8. Phase 3 Settle Alignment

Phase 3 behavior is governed by [balance_mode_phase3_settle.md](./balance_mode_phase3_settle.md).

Current important rules:

- Settle is a distinct post-RootOn continuity phase, not late RootOn
- runtime `BalanceEntry_Settle` is Phase 3
- current runtime allows policy activity again during Settle, but Settle still fails truthfully on topology regression, lost root simulation, locomotion authority reclaim, resets, shell maintenance loss, material shell correction, or timeout
- Phase 3 execution failures remain classified under the existing owner taxonomy enforced by tests

## 9. Certified Handoff Concept

Phase 1 should emit a certified handoff payload only when the contract conditions are satisfied.

That payload must not imply more than the runtime has actually proven.

In particular:

- a clean topology + suppression + quiet proof does not automatically prove dynamic viability
- the accepted setup must also survive the required admission and validation margins

## 10. Safe Denial Rule

The pipeline must support explicit safe denial.

Safe denial is valid when:

- the entry contract is satisfied enough to make a truthful decision
- but the mode cannot proceed safely

## 11. Current Contract Reality

The entry pipeline has moved through several contract-cleanup steps that are now part of the design, not just debugging:

- frozen Phase 1 topology capture
- next-frame ownership confirmation instead of same-frame overclaiming
- explicit distinction between intended ownership, raw body state, and modifier-record ownership
- suppression of BridgeActive distal re-promotion
- authoritative per-bone movement-type writes for topology-critical Phase 1 bones
- frozen upper-body ownership mode for the LateValidate sustain window
- explicit RootOn diagnostics for policy leak, shell material influence, topology preservation, and raw / modifier disagreement
- explicit Settle diagnostics for root continuity, topology regression, shell maintenance, and timeout

These are no longer optional implementation details. They are part of the real transition contract.

## 12. Investigation Surface (Temporary)

This section reflects the current investigation focus. These details are temporary and expected to change as convergence issues are resolved; they do not form part of the permanent design contract.

### Current live investigation surface

The current entry investigation is focused on:

- preserved-proximal topology during RootOn
- policy suppression truthfulness during the Phase 2 guard window
- shell-state versus shell-influence separation
- same-frame disagreement between raw state and modifier-record ownership during RootOn
- truthful Settle continuity after RootOn without overclaiming Phase 3 suppression behavior

## 13. Logging Contract

Required one-shot logs include:

- request queued
- queue gates satisfied
- preflight begin
- preflight reject / accept
- phase changes
- frozen Phase 1 topology snapshot
- Phase 1 summary
- Phase 2 entry summary
- first Phase 2 failure summary
- `PHASE2_READY_FOR_PHASE3`
- Phase 3 entry / first-failure summaries
- activation
- cleanup summary

The logs must make it possible to tell whether failure was:

- a contract failure
- or a physical-viability failure

And, for RootOn / Settle, whether the mismatch was in:

- certified topology intent
- raw body state
- modifier-record ownership
- policy suppression
- shell influence / shell maintenance

## 14. Recovery Contract

When transition fails, recovery must:

- stop transition-only behaviors
- clear transition-local timers and suppression
- restore coherent `BridgeActive`
- return to one coherent state only:
  - `BridgeActive`
  - `BalanceTransitionFailed`
  - or `SafeDenied`

Retries must not brute-force through an unchanged physical failure with no new evidence.

## 15. Invariants

These must always hold:

- queued requests must have a real future satisfaction path
- transition-owned conditions must not be treated as permanent rejection reasons
- active mode cannot be claimed before real transition success
- failed transition must restore coherent `BridgeActive`
- the frozen Phase 1 record must remain the ownership source of truth until the attempt terminates or advances
- topology-critical ownership must not be silently delegated to broad-set writes that are known not to be authoritative enough
- RootOn must not rely on hidden same-frame policy or shell assistance
- Settle must not be documented as an active phase or silently folded into RootOn

## 16. Acceptance Criteria

This transition spec is satisfied only when:

- balance entry is implemented as a distinct state path
- a dedicated frozen Phase 1 topology record exists
- transition-owned conditions are treated correctly
- contract-correct but physically non-viable safe denial is possible
- logs can distinguish contract failure from physical-viability failure
- RootOn uses an explicit truth model for certified intent vs raw vs modifier vs shell / policy influence
- Settle is explicit as Phase 3, with `BRT_Phase2_ReadyForPhase3` documented as the handoff boundary
- the runtime cannot silently complete the smoke in generic `BridgeActive`
