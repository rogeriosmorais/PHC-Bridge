# Stage 1 PHC Bridge Spec

## Purpose

This document defines the Stage 1 `PoseSearch -> PHC -> Physics Control` runtime contract.

It covers the model-facing bridge contract and the runtime state-machine contract needed for normal bridge operation and balance-mode entry.

## Scope

This spec covers:

- the runtime input/output contract
- the bridge runtime phases
- the relationship between normal bridge startup and balance-mode entry
- the allowed terminal states for the balance smoke
- the frozen rules that implementation must follow

## Runtime Phases

Stage 1 uses these runtime phases:

1. `Uninitialized`
2. `RuntimeReady`
3. `WaitingForPoseSearch`
4. `BridgeActive`
5. `BalanceEntryPending`
6. `BalanceLateValidate`
7. `BalanceActive`
8. `SafeDenied`
9. `Failed`

Rules:

- `BridgeActive` means the normal bridge is alive
- `BalanceEntryPending`, `BalanceLateValidate`, and `BalanceActive` are balance-mode states
- balance-mode entry is not represented by plain `BridgeActive`

## Normal Bridge Contract

Normal bridge startup may use staged non-root bring-up.

Normal staged bring-up is allowed to:

- promote non-root simulation groups gradually
- ramp control authority gradually
- ramp policy influence gradually

Normal bring-up is not itself the balance-mode contract.

## Balance-Mode Entry Contract

Balance-mode entry is a separate contract layered on top of an already running bridge.

When balance mode is requested:

- the runtime must leave plain `BridgeActive`
- the runtime must enter `BalanceEntryPending`
- late validation must use the frozen balance-entry topology
- late validation must not inherit upper-body simulation ownership from the transient normal bring-up topology

The authoritative balance-entry contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Frozen Late-Validation Topology Rule

Late validation must use a dedicated topology snapshot.

That topology snapshot must be established before quiet-proof timing starts.

The late-validation topology is the source of truth for:

- `simCount`
- `upperBodySimCount`
- proximal/distal ownership
- upper-body ownership mode
- policy suppression state during late validation

The late-validation topology must not be read from the ordinary bring-up counters after the balance request has entered balance-entry processing.

## Upper-Body Ownership Rule

For late validation:

- upper-body ownership must be explicitly frozen
- upper-body ownership must not be inferred from the current staged bring-up group
- `upperBodySimCount` must describe the late-validation topology only

Any configuration that logs late validation while still carrying normal bring-up upper-body simulation ownership is contract-invalid.

## Balance Smoke Terminal-State Contract

`PhysAnim.PIE.BalanceModeSmoke` passes only if the runtime finishes in one of:

- `BalanceActive`
- `SafeDenied`

The smoke fails if the runtime ends in:

- `BridgeActive`
- `BalanceEntryPending`
- `BalanceLateValidate`
- `Failed`

## Safe Denial Contract

Safe denial is an explicit outcome.

Safe denial is allowed only when the runtime emits an explicit terminal denial result and leaves the ambiguous entry path.

Safe denial is not:

- stalling in `BridgeActive`
- repeating late-validation resets without terminal resolution

## Observation Contract

The required inference-time inputs remain:

- `self_obs`: `358` floats
- `mimic_target_poses`: `6495` floats
- `terrain`: `256` floats

These tensors are unchanged by the balance-mode documentation update.

## Action Contract

The model output remains:

- one action tensor of `69` floats

The action-to-target mapping contract is unchanged by the balance-mode documentation update.

## State-Ownership Rule

The bridge must track two different ownership domains:

### Normal bridge ownership

Used for:

- startup
- staged non-root simulation bring-up
- control-authority ramps
- policy influence ramps

### Balance-entry ownership

Used for:

- request acceptance
- entry gating
- late validation
- quiet-proof timing
- final transition to `BalanceActive` or `SafeDenied`

Implementation must not reuse the normal bring-up counters as the authoritative balance-entry ownership state.

## Logging Contract

Balance-mode logs must distinguish:

- normal bridge bring-up
- balance-entry pending
- late validation
- active balance
- safe denial

At minimum, logs during late validation must reflect:

- dedicated balance-entry state
- dedicated topology snapshot
- `upperBodySimCount` from the late-validation topology
- explicit terminal outcome when entry ends

## Rejected Contract Shapes

The following are explicitly rejected:

- treating late validation as a plain continuation of normal staged bring-up
- allowing late validation to inherit `upperBodySimCount` from normal bring-up
- ending the smoke while still in plain `BridgeActive`
- using repeated late-validation resets as a terminal outcome

## Acceptance Criteria

This spec is satisfied only when all of the following are true:

- balance entry is documented as a separate runtime contract
- late validation uses its own topology snapshot
- upper-body ownership is frozen for late validation
- the smoke terminal states are limited to `BalanceActive` or `SafeDenied`
