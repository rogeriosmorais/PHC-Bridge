# Balance Mode Entry / Transition Spec

Status: Authoritative implementation design  
Scope: Stage 1 runtime balance activation path

## Legacy Filename Note

This filename is retained for compatibility.

The target design is no longer a multi-phase handoff ritual. This document defines balance-first activation onto an already-physical state.

Primary rewrite docs live in:

- `../10-specs/continuous_balance_architecture.md`
- `../10-specs/continuous_balance_truth_model.md`
- `../10-specs/authority_matrix.md`
- `../10-specs/instrumentation_and_acceptance.md`
- `./rewrite_migration_plan.md`
- `./legacy_handoff_contract.md`

## 1. Purpose

This document specifies the balance activation design for Stage 1.

It replaces the core assumption behind the old transition state machine rather than extending it.

This file is migration-facing and compatibility-facing. It must not become the place where the rewrite drifts back into “Prepare/LateValidate/RootOn/Settle, but cleaner.”

It defines:

- request handling
- queueing
- preflight
- physical-readiness requirements
- controller blend-in
- standing validation
- failure and recovery
- activation boundary

## 2. Core Rule

Balance mode is not active until standing validation succeeds.

A queued request is not active mode.  
`BridgeActive_Physical` is not active mode.  
`BalanceActivation_BlendIn` is not active mode.  
`BalanceActivation_StandingValidation` is not active mode.

## 3. Required Runtime States

Minimum authoritative runtime-facing state set:

- `BridgeActive`
- `BalanceStartQueued`
- `BalanceTransition_Preflight`
- `BridgeActive_Physical`
- `BalanceActivation_BlendIn`
- `BalanceActivation_StandingValidation`
- `BalanceActive_Standing`
- `BalanceTransitionFailed`
- `SafeDenied`

Interpretation rules:

- `BridgeActive_Physical` means the balance-critical chain is already simulated
- `BalanceActivation_BlendIn` means controller authority is ramping
- `BalanceActivation_StandingValidation` means the bridge is measuring sustained physical standing

## 4. Queueing Rules

A balance request must be queued, not rejected, when the runtime is in a valid bridge context but temporarily not eligible.

Queue-worthy blockers include:

- startup bring-up incomplete
- balance-critical chain not yet continuously simulated
- control ramp prerequisites incomplete
- policy or shell contamination still active

These are temporary blockers, not failures.

## 5. Preflight Rule

Preflight decides whether the runtime can begin balance activation truthfully.

Preflight must confirm:

- a valid bridge context exists
- the balance-critical chain is defined
- no immediate conflicting authority owns the balance-critical chain
- there is a real path to entering `BridgeActive_Physical`

Preflight must not certify a later ownership flip as the intended path to success.

Preflight must also not assume shell-maintained containment will hide controller weakness later in the run.

## 6. Physical-Readiness Rule

Before blend-in begins, the runtime must establish `BridgeActive_Physical`.

That means:

- the balance-critical chain is already simulated
- raw state confirms that continuity
- the runtime is not depending on a future topology flip to become physically real
- shell bookkeeping and shell influence are measured separately

## 7. Controller Blend Rule

`BalanceActivation_BlendIn` exists to ramp controller authority gradually onto the already-physical balance-critical chain.

Required rules:

- controller authority rises gradually
- abrupt assertion of full authority is not the intended path
- topology flips are not the intended success mechanism
- diagnostics may classify instability, but may not widen grace until instability looks acceptable

Interpretation rule:

- when instability appears here, the runtime must first consider controller strength, damping, target representation, action scaling, latency, pose discontinuity, and authority conflicts before blaming a missing handoff ritual

## 8. Standing-Validation Rule

`BalanceActivation_StandingValidation` is the continuity window before active standing.

Required rules:

- validation succeeds only after contiguous readiness over the required hold duration
- non-ready frames reset the hold timer
- first truthful terminal failure ends the attempt
- diagnostics remain continuity-facing evidence only

## 9. Logging Contract

Required one-shot logs include:

- request queued
- queue gates satisfied
- preflight begin
- preflight reject or accept
- transition to `BridgeActive_Physical`
- blend-in entry summary
- first blend failure summary
- standing-validation entry summary
- first standing-validation failure summary
- activation
- cleanup summary

The logs must make it possible to tell whether failure was:

- a contract failure
- or a physical-viability failure

And whether the deciding mismatch was in:

- ownership continuity
- controller blend
- shell influence
- standing stability

The activation logs should also make it possible to tell whether failure is dominated by:

- controller effort or target-shaping weakness
- poor contact quality
- COM drift behavior
- long-lived oscillation
- hidden authority fights between policy, Physics Control, locomotion, and startup logic

## 10. Recovery Contract

When activation fails, recovery must:

- stop transition-only behaviors
- clear transition-local timers and blend state
- restore coherent `BridgeActive`
- return to one coherent state only:
  - `BridgeActive`
  - `BalanceTransitionFailed`
  - or `SafeDenied`

Retries must not brute-force through an unchanged physical failure with no new evidence.

## 11. Invariants

These must always hold:

- queued requests must have a real future satisfaction path
- active mode cannot be claimed before real standing-validation success
- failed transition must restore coherent `BridgeActive`
- balance-critical ownership must not silently depend on hidden topology flips
- diagnostics must not manufacture a pass

## 12. Acceptance Criteria

This transition spec is satisfied only when:

- balance activation is implemented as a distinct state path
- queueing and preflight lead toward an already-physical bridge state
- controller blend-in is explicit
- standing validation is explicit
- logs distinguish contract failure from physical-viability failure
- the runtime cannot silently complete the smoke in generic `BridgeActive`
