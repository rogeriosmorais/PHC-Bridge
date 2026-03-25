# Balance Mode Phase 2 Root-On Spec

Status: Authoritative implementation design  
Scope: Stage 1 behavior for `BalanceTransition_Phase2_RootOn` and the immediate guard window

## 1. Purpose

This document defines the RootOn choreography for Phase 2 of the Balance Mode entry transition.

It is authoritative for:

- Phase 2 entry preconditions
- safe denial before RootOn
- warm-start RootOn requirements
- RootOn frame order
- guard-window rules
- RootOn spike classification
- shell/policy suppression semantics
- recovery and retry rules

## 2. Relationship to Phase 1

Phase 2 consumes a still-valid Phase 1 output.

If Phase 1 is contract-correct but physically non-viable, Phase 2 must not pretend otherwise.

In other words:
Phase 2 is not allowed to use RootOn to “rescue” a Phase 1 setup that never had credible stability margin.

## 3. Core rule

Phase 2 is not “turn pelvis sim on and hope.”

Phase 2 may begin only from a still-valid handoff and only after the explicit readiness proof is satisfied.

If the proof is absent, false, or incoherent, Phase 2 must deny safely before RootOn.

## 4. Required entry preconditions

Phase 2 may begin only if all are true:

- certified Phase 1 handoff exists
- handoff is still valid
- LateValidate completed successfully
- topology still matches the payload
- target continuity still matches the payload
- no reset is pending
- no topology flip is pending
- no fail-stop precursor is active
- entry shell / root conditions remain within bounds

## 5. Safe denial path

If any required Phase 2 precondition is false, Phase 2 must deny before any RootOn attempt.

Denial is valid and preferable to a dishonest RootOn attempt.

## 6. Warm-start contract

RootOn must be executed as a warm start, not a blind flip.

Required behavior:

- do not seed pelvis from arbitrary animation frame assumptions
- seed from the live physics-consistent state where possible
- validate root/proximal continuity before enabling root simulation
- zero and reseed velocities around the sim flip when required
- abort before RootOn if continuity is incoherent

## 7. Authority during Phase 2

During Phase 2 guard window:

- policy writes to the transition set are forbidden unless the spec explicitly allows a later release point
- cached resets are forbidden
- topology expansion is forbidden unless explicitly defined by the certified handoff + RootOn choreography
- shell and CharacterMovement correction influence on simulated bodies is forbidden
- hold/reference state may persist only where the contract explicitly allows it

### Shell-state versus shell-influence rule

During Phase 2, distinguish:

- shell state (`locked`, `reanchored`, `reseeded`)
- shell influence on simulated bodies

The shell may remain locked or reanchored without that, by itself, being a violation.

A violation exists only if shell/reference behavior materially influences the simulated transition set during the guard window in a way the contract forbids.

## 8. RootOn frame sequence

Phase 2 executes in this order:

1. capture entry snapshot
2. freeze hazards
3. execute RootOn choreography
4. immediately re-read and validate root simulation state
5. enter guard window if technical RootOn succeeded

## 9. RootOn truth model

The authoritative RootOn truth model is defined in `phase2-rooton-truth-model.md`.

Minimum required interpretation during RootOn:

- certified topology intent remains the contract source of truth
- modifier-record ownership is a separate observable
- raw body sim state is a separate observable
- same-frame disagreement among those three layers must be classified truthfully rather than collapsed into a single fake success state

## 10. Topology during Phase 2

The required topology must be whatever the current certified handoff / Phase 2 design states explicitly.

Do not change topology implicitly during RootOn under the guise of tuning.

If the design says the proximal Phase 1 sim set is preserved during RootOn, then that preserved set must remain coherent at all three layers:

- intended ownership
- modifier-record ownership
- raw body state

## 11. Guard window

The guard window begins immediately after technical RootOn success.

During the guard window, the following remain forbidden:

- policy writes into the transition set
- cached resets
- topology expansion
- locomotion entry
- shell assistance
- shell-reference reseed used as support

## 12. Failure classes

Examples include:

- missing handoff
- invalidated handoff
- policy leak
- reset violation
- same-frame conflicting authority
- RootOn spike
- root simulation dropped
- material shell correction
- topology not preserved
- modifier/raw disagreement over preserved sim bones
- no convergence path

## 13. Acceptance criteria

This spec is satisfied only when Phase 2 performs a true warm-start RootOn from a still-valid handoff, can deny safely before RootOn, distinguishes shell state from shell influence, and does not depend on hidden same-frame assistance or brute-force retry loops.
