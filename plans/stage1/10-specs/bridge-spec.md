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
- the frozen rules implementation must follow
- the required distinction between contract correctness and physical viability

## Runtime phases

Stage 1 uses these runtime phases:

1. `Uninitialized`
2. `RuntimeReady`
3. `WaitingForPoseSearch`
4. `BridgeActive`
5. balance-entry subphases
6. `BalanceActive`
7. `SafeDenied`
8. `Failed`

Rules:

- `BridgeActive` means the normal bridge is alive
- balance-entry is not represented by plain `BridgeActive`
- active balance is not claimed until entry succeeds

## Normal bridge contract

Normal bridge startup may use staged non-root bring-up.

Normal staged bring-up may:

- promote non-root simulation groups gradually
- ramp control authority gradually
- ramp policy influence gradually

Normal bring-up is not itself the balance-entry contract.

## Balance-mode entry contract

Balance-mode entry is a separate contract layered on top of an already running bridge.

When balance mode is requested:

- the runtime must leave plain `BridgeActive` logically, even if the outer runtime owner remains the same
- the runtime must enter an explicit balance-entry attempt
- Prepare and LateValidate must use the dedicated balance-entry contract
- the runtime must resolve to either active balance or explicit safe denial

The authoritative balance-entry contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Observation contract

The required inference-time inputs remain:

- `self_obs`: `358` floats
- `mimic_target_poses`: `6495` floats
- `terrain`: `256` floats

These tensors are unchanged by the balance-entry documentation update.

## Action contract

The model output remains:

- one action tensor of `69` floats

The action-to-target mapping contract is unchanged by the balance-entry documentation update.

## Contract correctness vs physical viability

These are different questions.

### Contract correctness means

- queueing / acceptance are explicit
- topology ownership is explicit
- suppression rules are explicit
- target-write routing is explicit
- convergence snapshots use the authoritative timing/source
- terminal outcomes are explicit and truthful

### Physical viability means

- the accepted setup remains dynamically quiet enough to survive entry
- the contact/tuning/ownership combination is stable enough to pass LateValidate
- the accepted setup has enough margin to survive beyond superficial quiet windows

A Phase 1 attempt may be contract-correct and still physically non-viable.

The docs and code must preserve this distinction.

## Engine-grounded rule

Stage 1 is built on UE physics and Physics Control behavior that are not equivalent to a turnkey humanoid-balance stack.

Therefore:

- `Kinematic` vs `Simulated` is a hard ownership distinction
- sub-step regime is part of the evidence context
- damping is not a substitute for a correct ownership and control contract
- Physics Control must be treated as a control layer, not as proof of physical viability by itself

## State-ownership rule

The bridge must track two ownership domains:

### Normal bridge ownership

Used for:

- startup
- staged non-root bring-up
- control-authority ramps
- policy influence ramps

### Balance-entry ownership

Used for:

- request acceptance
- entry gating
- Prepare
- LateValidate
- final transition to `BalanceActive` or `SafeDenied`

Implementation must not reuse the normal bring-up counters as the authoritative balance-entry ownership state.

## Balance smoke terminal-state contract

`PhysAnim.PIE.BalanceModeSmoke` passes only if the runtime finishes in one of:

- `BalanceActive`
- `SafeDenied`

The smoke fails if the runtime ends in:

- `BridgeActive`
- unresolved entry state
- ambiguous recovery state

## Safe denial contract

Safe denial is an explicit outcome.

Safe denial is allowed only when the runtime emits an explicit terminal denial result and leaves the ambiguous entry path.

Safe denial is not:

- stalling in `BridgeActive`
- repeating resets without terminal resolution
- a silent fallback

## Logging contract

Balance-mode logs must distinguish:

- normal bridge bring-up
- balance-entry attempt
- Prepare
- LateValidate
- active balance
- safe denial

At minimum, logs during entry must make it possible to tell:

- what topology was accepted
- what suppression state held
- what target-write path was active
- what convergence snapshot was used
- whether the failure was contract-level or physical-level

## Acceptance criteria

This spec is satisfied only when all of the following are true:

- balance entry is documented as a separate runtime contract
- contract correctness and physical viability are explicitly distinguished
- the smoke terminal states are limited to `BalanceActive` or `SafeDenied`
- implementation and design docs do not assume that contract correctness automatically implies a viable Phase 1 setup
