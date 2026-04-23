# Stage 1 UE Bridge Implementation Spec

## Purpose

This document freezes the Unreal-side implementation contract for the Stage 1 bridge and for balance activation.

It is implementation-facing and must match the runtime contract in the balance-mode design docs.

Primary operational references:

- `continuous_balance_truth_model.md`
- `authority_matrix.md`
- `instrumentation_and_acceptance.md`

## Runtime Owner

The live Stage 1 runtime owner is:

- `UPhysAnimComponent`

`UPhysAnimComponent` owns:

- startup validation
- PoseSearch query state
- NNE runtime/model/session lifetime
- observation packing
- action unpacking
- Physics Control writes
- bridge runtime state
- balance-activation state
- diagnostic snapshots
- smoke-visible terminal outcome

## Runtime States

The implementation must expose distinct runtime states or equivalent explicit sub-states for:

- `BridgeActive`
- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Standing`
- `BalanceActive_Recovery`
- `SafeDenied`
- `Failed`

Compatibility note:

- existing code symbols may still contain legacy `Phase1`, `Phase2`, `Phase3`, `RootOn`, or `Settle` names
- this documentation pass does not require renaming those symbols

## Required Balance-Activation Data

On balance request acceptance, the runtime must create a dedicated activation record containing:

- attempt-active state
- request-accepted timestamp
- balance-critical chain definition
- support-set definition
- ownership-continuity snapshot
- controller-authority blend state
- standing-validation timer state
- shell bookkeeping state
- shell influence diagnostics
- shell helper used flag
- topology change event count
- authority conflict count
- terminal outcome flag
- failure reason if any

## Required Truth Sources

The implementation must keep these sources separate:

- intended ownership
- raw body simulation state
- modifier-record or control-layer ownership bookkeeping
- controller-authority alpha
- shell bookkeeping state
- shell influence materiality

These are different signals.

The implementation must also surface authority conflict events explicitly rather than letting subsystem fights remain implicit.

## Required Continuity Snapshot

Balance activation must use a dedicated authoritative post-update snapshot for truth-sensitive decisions.

That snapshot must at minimum be able to report:

- raw simulation state for the balance-critical chain
- raw simulation state for the support set
- worst-body linear and angular stability metrics
- controller-authority alpha / blend progress
- shell offset and velocity deltas
- shell influence materiality
- locomotion or reset authority contamination
- standing-validation accumulated hold time

## Required Ownership Rule

Under the target Stage 1 design, the implementation must treat the balance-critical chain as continuously simulated through activation.

The implementation must not silently change that contract under the guise of runtime tuning or diagnostics work.

If balance-critical ownership semantics change, the implementation spec and design docs must change in the same commit.

## Required Blend Rule

During `BalanceActivation_BlendIn`:

- controller authority must ramp gradually
- abrupt activation of full authority is not the intended path
- the default alpha is `ControlAuthorityAlpha`
- the default blend duration is `0.75` seconds
- the alpha is global across the balance-critical chain and support set in `V0`
- support-set targets use the same alpha in `V0`
- damping and strength scale with the same alpha in `V0`
- target history is rebased once on blend entry
- target discontinuity greater than `15.0` degrees on the balance-critical chain is terminal
- diagnostics may record blend instability, but may not reclassify that instability as success

## Required Shell Rule

The runtime must distinguish:

- shell bookkeeping (`locked`, `reanchored`, `reseeded`, or equivalent)
- shell influence on the balance-critical chain

The presence of shell bookkeeping is not itself a failure.

Material shell influence on the balance-critical chain during activation is a failure.

For `V0`:

- shell helper use on the balance-critical chain or support set is forbidden
- any such use must emit a helper-used event and fail the run

## Required Standing-Validation Behavior

`BalanceActivation_Validate` may begin only after the bridge is in a physically ready state, the controller blend has reached its required activation range, and support truth remains valid.

Standing validation must:

- require contiguous readiness for the configured hold duration
- reset its hold timer on non-ready frames
- end truthfully on the first terminal failure

## Smoke-Test Evaluation Rule

The automation smoke must evaluate the final state using the balance-activation terminal state, not the generic bridge-running state.

Passing outcome:

- `BalanceActive_Standing`

Failing outcomes:

- `BridgeActive`
- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Recovery`
- `SafeDenied`
- unresolved activation state
- ambiguous failure state

## Documentation Alignment Rule

No implementation detail is allowed that requires a hidden balance-activation rule absent from the current design or spec docs.

If implementation adds or removes a real:

- ownership rule
- blend rule
- standing-validation rule
- diagnostic truth source
- terminal reason

the design and spec docs must be updated in the same change.

## Acceptance Criteria

This implementation spec is satisfied only when:

- balance activation is implemented as a distinct state path
- the balance-critical chain is explicit in runtime data
- a dedicated post-update truth snapshot exists
- ownership, bookkeeping, and shell influence remain distinct
- controller blend is explicit
- standing validation is explicit
- smoke evaluation uses terminal balance outcomes
- truthful safe deny is not a passing smoke result
