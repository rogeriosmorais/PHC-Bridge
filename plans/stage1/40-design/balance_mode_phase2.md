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
- RootOn failure and retry semantics
- the explicit handoff into Phase 3

## 2. Relationship To Phase 1 And Phase 3

Phase 2 consumes a still-valid Phase 1 output.

If Phase 1 is contract-correct but physically non-viable, Phase 2 must not pretend otherwise.

Phase 2 is not allowed to use RootOn to rescue a Phase 1 setup that never had credible stability margin.

Phase 2 is also not Settle:

- `RootOn` means Phase 2 only
- `Settle` means Phase 3 only
- the Phase 2 success boundary is `BRT_Phase2_ReadyForPhase3`, not activation

Phase 3 continuity rules are defined in [balance_mode_phase3_settle.md](./balance_mode_phase3_settle.md).

## 3. Core Rule

Phase 2 is not "turn pelvis sim on and hope."

Phase 2 may begin only from a still-valid handoff and only after the explicit readiness proof is satisfied.

If the proof is absent, false, or incoherent, Phase 2 must deny safely before RootOn.

## 4. Required Entry Preconditions

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

## 5. Safe Denial Path

If any required Phase 2 precondition is false, Phase 2 must deny before any RootOn attempt.

Denial is valid and preferable to a dishonest RootOn attempt.

## 6. Warm-Start Contract

RootOn must be executed as a warm start, not a blind flip.

Required behavior:

- do not seed pelvis from arbitrary animation frame assumptions
- seed from the live physics-consistent state where possible
- validate root / proximal continuity before enabling root simulation
- zero and reseed velocities around the sim flip when required
- abort before RootOn if continuity is incoherent

## 7. RootOn Truth Model

The authoritative RootOn truth model is defined in [phase2-rooton-truth-model.md](./phase2-rooton-truth-model.md).

Minimum required interpretation during RootOn:

- certified topology intent remains the contract source of truth
- same-tick raw end state is the deciding proof of technical RootOn success or failure
- modifier-record ownership is still required routing evidence
- same-frame disagreement among those layers must be classified truthfully rather than collapsed into a single fake success state

## 8. Authority During Phase 2

During the Phase 2 guard window:

- policy writes to the transition set are forbidden
- cached resets are forbidden
- topology expansion is forbidden unless explicitly defined by the certified handoff + RootOn choreography
- shell and CharacterMovement correction influence on simulated bodies is forbidden
- move-smoke / locomotion re-entry is forbidden
- hold/reference state may persist only where the contract explicitly allows it

### Shell-state versus shell-influence rule

During Phase 2, distinguish:

- shell state (`locked`, `reanchored`, `reseeded`)
- shell influence on simulated bodies

The shell may remain locked or reanchored without that, by itself, being a violation.

A violation exists only if shell/reference behavior materially influences the simulated transition set during the guard window in a way the contract forbids.

## 9. RootOn Frame Sequence

Phase 2 executes in this order:

1. capture entry snapshot
2. freeze hazards
3. execute RootOn choreography
4. immediately re-read and validate same-tick root simulation state
5. enter the guard window if technical RootOn succeeded
6. hand off to `BRT_Phase2_ReadyForPhase3` once guard-window duration elapses without terminal failure

Implementation note:

- current runtime still treats modifier disagreement as routing evidence worth logging, but the deciding proof at RootOn remains the same-tick raw end state rather than modifier state alone

## 10. Failure And Retry Taxonomy

Use the following current taxonomy:

| Failure reason | Meaning | Retry status |
| :--- | :--- | :--- |
| `phase2_topology_not_preserved` | RootOn failed to preserve the certified topology after the grace window | retryable only with recovery completion, material state change, fresh quiet proof, cooldown, and retry budget |
| `phase2_root_on_spike` | RootOn produced a truthful instability / spike failure | not retryable |
| `phase2_policy_write_leak` | forbidden policy writes occurred during RootOn / guard window | not retryable |
| `phase2_reset_violation` | forbidden reset state leaked into the active attempt | terminal |
| `phase2_shell_correction_material` | shell influence became materially active on simulated bodies | terminal |
| `phase2_root_simulation_dropped` | root simulation was not preserved after RootOn | terminal |

Do not collapse these into one generic no-convergence label.

## 11. Handoff To Phase 3

Phase 2 guard-window success is not activation.

The Phase 2 success boundary is:

- RootOn succeeded technically
- guard-window checks remained truthful through `BalancePhase2GuardWindowDuration`
- runtime enters `BRT_Phase2_ReadyForPhase3`
- runtime immediately advances to `BRT_Phase3_Settle`

Phase 3 then owns post-RootOn continuity and activation gating.

## 12. Acceptance Criteria

This spec is satisfied only when Phase 2 performs a true warm-start RootOn from a still-valid handoff, can deny safely before RootOn, distinguishes shell state from shell influence, uses the canonical RootOn truth-order, and hands off explicitly into Phase 3 instead of silently treating Settle as late RootOn.
