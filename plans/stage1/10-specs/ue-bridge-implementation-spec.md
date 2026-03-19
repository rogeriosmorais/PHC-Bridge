# Stage 1 UE Bridge Implementation Spec

## Purpose

This document freezes the Unreal-side implementation contract for the Stage 1 bridge and for balance-mode entry.

It is implementation-facing and must match the runtime contract in:

- `plans/stage1/10-specs/bridge-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`

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
- balance-mode entry state
- late-validation topology snapshot
- smoke-visible terminal outcome

## Runtime States

The implementation must expose distinct runtime states for:

- `BridgeActive`
- `BalanceEntryPending`
- `BalanceLateValidate`
- `BalanceActive`
- `SafeDenied`

Implementation must not leave balance entry represented only as plain `BridgeActive`.

## Normal Bring-Up Implementation

Normal bring-up may continue to use staged non-root groups.

The staged bring-up path may own:

- simulation handoff
- group unlock sequence
- control-authority ramps
- policy-influence ramp

These normal bring-up values are not the balance-entry topology source of truth.

## Required Balance Entry Data

On balance request acceptance, the runtime must create a dedicated balance-entry record.

That record must contain:

- balance-entry state
- request-accepted timestamp
- dedicated late-validation topology snapshot
- quiet-proof timer state
- terminal outcome flag
- terminal denial flag
- terminal denial reason if denied

## Required Late-Validation Snapshot

Before late validation starts counting quiet-proof time, the implementation must freeze and store:

- root ownership mode
- proximal ownership mode
- distal ownership mode
- upper-body ownership mode
- `simCount`
- `upperBodySimCount`
- policy suppression state for late validation

This stored snapshot is then used for:

- late-validation logs
- late-validation resets
- smoke outcome evaluation

The late-validation snapshot must not be recomputed from the live normal bring-up path on every frame.

## Required Upper-Body Rule

The balance-entry path must explicitly force the intended upper-body ownership before late validation begins.

Late validation must read `upperBodySimCount` from the dedicated balance-entry snapshot.

The implementation must not let `upperBodySimCount=4` leak in from the ordinary bring-up topology when late validation is supposed to run under a different ownership model.

## Entry State Flow

Required flow:

1. `BridgeActive`
2. balance request accepted
3. `BalanceEntryPending`
4. dedicated balance-entry topology frozen
5. `BalanceLateValidate`
6. terminal outcome:
   - `BalanceActive`, or
   - `SafeDenied`

Disallowed flow:

1. `BridgeActive`
2. balance request accepted
3. repeated late-validation resets
4. smoke exits in plain `BridgeActive`

## Late-Validation Timing Rule

Quiet-proof timing starts only after:

- the dedicated late-validation topology is frozen
- the upper-body ownership for late validation is established
- the runtime has entered `BalanceLateValidate`

Quiet-proof timing must not start while the balance path is still carrying unresolved normal bring-up ownership.

## Reset Rule

When late validation fails a frame:

- the runtime may reset the quiet-proof timer
- the runtime may remain in `BalanceLateValidate`
- the runtime must preserve the balance-entry state machine
- the runtime must not collapse back to ambiguous plain `BridgeActive`

If the implementation decides to deny balance entry, it must transition to:

- `SafeDenied`

## Smoke-Test Evaluation Rule

The automation smoke must evaluate the final state using the balance-entry terminal state, not the generic bridge-running state.

Required passing outcomes:

- `BalanceActive`
- `SafeDenied`

Required failing outcomes:

- `BridgeActive`
- unresolved `BalanceEntryPending`
- unresolved `BalanceLateValidate`

## Logging Requirements

The implementation must emit logs for:

- balance request accepted
- transition to `BalanceEntryPending`
- late-validation topology frozen
- transition to `BalanceLateValidate`
- late-validation reset reason
- terminal transition to `BalanceActive` or `SafeDenied`

Logs during late validation must show:

- the dedicated late-validation topology values
- the dedicated `upperBodySimCount`
- the current quiet-proof duration
- the required quiet-proof threshold

## Documentation Alignment Rule

No implementation detail is allowed that requires a hidden balance-mode rule absent from `10-specs`.

If implementation adds a new balance-entry gate, ownership mode, or terminal state, the `10-specs` balance-entry doc must be updated in the same change.

## Acceptance Criteria

This implementation spec is satisfied only when:

- balance entry is implemented as a distinct state path
- a dedicated late-validation topology snapshot exists
- upper-body ownership is explicitly frozen for late validation
- smoke evaluation uses terminal balance outcomes
- the runtime cannot silently end the smoke in plain `BridgeActive`
