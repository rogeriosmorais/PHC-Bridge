# Balance Mode Phase 2 Root-On Spec

Status: Authoritative implementation design  
Scope: Stage 1 behavior for `BalanceTransition_Phase2_RootOn` and the immediate guard window

## 1. Purpose

This document defines the root-on choreography for Phase 2 of the Balance Mode entry transition.

It is authoritative for:

- Phase 2 entry preconditions
- safe denial before root-on
- warm-start root-on requirements
- root-on frame order
- guard-window rules
- root-on spike classification
- recovery and retry rules

## 2. Relationship to Phase 1

Phase 2 consumes a still-valid Phase 1 output.

If Phase 1 is contract-correct but physically non-viable, Phase 2 must not pretend otherwise.

In other words:
Phase 2 is not allowed to use root-on to “rescue” a Phase 1 setup that never had credible stability margin.

## 3. Core rule

Phase 2 is not “turn pelvis sim on and hope.”

Phase 2 may begin only from a still-valid handoff and only after the explicit readiness proof is satisfied.

If the proof is absent, false, or incoherent, Phase 2 must deny safely before root-on.

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

If any required Phase 2 precondition is false, Phase 2 must deny before any root-on attempt.

Denial is valid and preferable to a dishonest root-on attempt.

## 6. Warm-start contract

Root-on must be executed as a warm start, not a blind flip.

Required behavior:

- do not seed pelvis from arbitrary animation frame assumptions
- seed from the live physics-consistent state where possible
- validate root/proximal continuity before enabling root simulation
- zero and reseed velocities around the sim flip when required
- abort before root-on if continuity is incoherent

## 7. Authority during Phase 2

During Phase 2:

- policy writes to root/proximal/distal are forbidden
- the established hold/reference state is preserved
- cached resets are forbidden
- shell and CharacterMovement correction must remain suppressed

## 8. Root-on frame sequence

Phase 2 executes in this order:

1. capture entry snapshot
2. freeze hazards
3. execute root-on
4. immediately re-read and validate root simulation state
5. enter guard window if technical root-on succeeded

## 9. Guard window

The guard window begins immediately after technical root-on success.

During the guard window, the following remain forbidden:

- policy writes into the transition set
- cached resets
- topology expansion
- locomotion entry
- shell assistance
- shell-reference reseed

## 10. Topology during Phase 2

The required topology must be whatever the current certified handoff / Phase 2 design states explicitly.

Do not change topology implicitly during root-on under the guise of tuning.

## 11. Failure classes

Examples include:

- missing handoff
- invalidated handoff
- policy leak
- reset violation
- same-frame conflicting authority
- root-on spike
- root simulation dropped
- material shell correction
- no convergence path

## 12. Acceptance criteria

This spec is satisfied only when Phase 2 performs a true warm-start root-on from a still-valid handoff, can deny safely before root-on, and does not depend on hidden same-frame assistance or brute-force retry loops.
