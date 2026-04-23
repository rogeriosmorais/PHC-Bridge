# Balance Mode Phase 3 Settle Spec

Status: Authoritative implementation design  
Scope: Stage 1 standing-validation hold semantics

## Legacy Filename Note

This filename is retained for compatibility.

The target design is no longer a post-flip `Settle` phase. This document now defines standing-validation behavior before publishing `BalanceActive_Standing`.

Primary rewrite docs live in:

- `../10-specs/continuous_balance_truth_model.md`
- `../10-specs/instrumentation_and_acceptance.md`
- `./legacy_handoff_contract.md`

## 1. Purpose

This document defines the standing-validation hold window for balance activation.

It is authoritative for:

- the boundary between blend-in and active standing
- success and timeout timing
- failure reasons during standing validation
- the rule that diagnostics remain first-failure evidence only

## 2. Phase Boundary

Standing validation begins only after all of the following are true:

- physical readiness was established truthfully
- controller blend-in completed without terminal failure
- the runtime entered `BalanceActivation_StandingValidation`

Interpretation rules:

- standing validation is not active balance mode
- `BalanceActive_Standing` is published only after the validation hold succeeds

## 3. Standing-Validation Contract

Standing validation exists to prove that the continuously physical balance-critical chain can remain stable under the blended controller long enough to enter active standing truthfully.

Required rules:

- success requires contiguous readiness over the required hold duration
- non-ready frames reset the hold timer
- first truthful terminal failure ends the attempt
- diagnostics may classify failure, but may not repair the state into success

Because the old transition guards are no longer the intended protection layer, early standing-validation runs may look worse than the old ritualized path. That is expected if the runtime is now exposing controller weakness or long-lived oscillation more honestly.

## 4. Required Observables

During standing validation, the runtime must keep these observables separate:

1. balance-critical ownership continuity
2. raw physical stability
3. modifier-record or control-layer bookkeeping
4. shell bookkeeping state
5. shell influence materiality
6. locomotion or reset authority state
7. control effort and controller saturation when available
8. COM behavior, contact quality, and long-lived oscillation when available

Interpretation rules:

- raw continuity is the deciding proof for ownership continuity and physical standing
- bookkeeping disagreement under preserved raw continuity is diagnostic evidence, not success
- shell bookkeeping is not shell influence

## 5. Timer Rules

Standing validation timing is:

- success only after `BalanceActive_Standing` readiness holds for `3.0` continuous seconds
- failure on timeout if that hold never completes

Interpretation rules:

- surviving for a while is not enough
- the hold duration is the benchmark

## 6. Failure Classification

Standing validation must reject truthfully when the first material failure is one of:

- balance-critical ownership continuity lost
- instability exceeds standing thresholds
- shell influence becomes materially active
- gameplay or reset authority reclaims control
- timeout with no completed standing hold

Suggested emitted reasons:

- `standing_validation_ownership_lost`
- `standing_validation_instability`
- `standing_validation_controller_strength_or_representation_failure`
- `standing_validation_shell_influence_material`
- `standing_validation_authority_conflict`
- `standing_validation_authority_reclaimed`
- `standing_validation_timeout`

## 7. Logging Rule

Standing-validation documentation must preserve the current expectation of one-shot transition evidence:

- validation entry summary
- first failure summary
- success log before activation

These logs are continuity-facing evidence, not proof that active standing already exists.

## 8. Acceptance Criteria

This spec is satisfied only when:

- standing validation is documented as a first-class pre-activation hold
- the `3.0`-second benchmark is explicit
- emitted failure reasons are tied to truthful physical causes
- diagnostics remain observational only
