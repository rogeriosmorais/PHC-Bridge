# Stage 1 PHC Bridge Spec

## Purpose

This document defines the Stage 1 `PoseSearch -> PHC -> Physics Control` runtime contract.

It covers the model-facing bridge contract and the runtime state-machine contract needed for normal bridge operation and balance activation.

## Scope

This spec covers:

- the runtime input/output contract
- the bridge runtime phases
- the relationship between normal bridge startup and balance activation
- the allowed terminal states for the balance smoke
- the frozen rules implementation must follow
- the required distinction between contract correctness and physical viability

## Runtime Phases

Stage 1 uses these target runtime phases:

1. `Uninitialized`
2. `RuntimeReady`
3. `WaitingForPoseSearch`
4. `BridgeActive`
5. `BridgeActive_Physical`
6. `BalanceActivation_BlendIn`
7. `BalanceActivation_StandingValidation`
8. `BalanceActive_Standing`
9. `BalanceActive_Recovery`
10. `SafeDenied`
11. `Failed`

Rules:

- `BridgeActive` means the normal bridge is alive
- `BridgeActive_Physical` means the bridge is alive with the balance-critical chain already simulated
- active balance is not claimed until standing validation succeeds

## Normal Bridge Contract

Normal bridge startup may use staged non-root bring-up.

Normal staged bring-up may:

- promote non-critical simulation groups gradually
- ramp control authority gradually
- ramp policy influence gradually

Normal bring-up is not itself the balance-activation contract.

## Balance Activation Contract

Balance activation is a separate contract layered on top of an already running bridge.

When balance mode is requested:

- the runtime must enter an explicit balance-activation attempt
- the balance-critical chain must already be or become continuously simulated before controller blend-in is treated as active activation
- controller authority must ramp onto that already-physical state
- the runtime must resolve to either `BalanceActive_Standing`, `SafeDenied`, or `Failed`

The authoritative activation contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Balance-Critical Chain

The default Stage 1 balance-critical chain is:

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

Interpretation rules:

- these bodies must stay continuously simulated through activation
- temporary kinematic re-ownership of this chain is not the intended activation mechanism
- distal and upper-body tuning may still evolve, but not by violating balance-critical continuity

## Observation Contract

The required inference-time inputs remain:

- `self_obs`: `358` floats
- `mimic_target_poses`: `6495` floats
- `terrain`: `256` floats

These tensors are unchanged by the balance-activation documentation rewrite.

## Action Contract

The model output remains:

- one action tensor of `69` floats

The action-to-target mapping contract is unchanged by this rewrite.

## Contract Correctness Vs Physical Viability

These are different questions.

### Contract correctness means

- queueing and acceptance are explicit
- balance-critical ownership continuity is explicit
- controller-blend behavior is explicit
- diagnostic sources are explicit
- terminal outcomes are explicit and truthful

### Physical viability means

- the already-physical chain remains dynamically quiet enough to survive activation
- the controller blend does not destabilize the live physical state
- the standing-validation window can complete without hidden support

The docs and code must preserve this distinction.

## Engine-Grounded Rule

Stage 1 is built on UE physics and Physics Control behavior that are not equivalent to a turnkey humanoid-balance stack.

Therefore:

- `Kinematic` vs `Simulated` is a hard ownership distinction
- sub-step regime is part of the evidence context
- damping is not a substitute for a correct ownership and control contract
- Physics Control must be treated as a control layer, not as proof of physical viability by itself

## Diagnostics Rule

Balance-activation diagnostics are measurement-only.

At minimum, docs and logs must distinguish:

- ownership continuity
- controller-authority alpha / blend progress
- shell bookkeeping state
- shell influence materiality
- raw physical stability metrics

No diagnostic, grace window, or classification rule may convert instability into success.

## Balance Smoke Terminal-State Contract

`PhysAnim.PIE.BalanceModeSmoke` passes only if the runtime finishes in:

- `BalanceActive_Standing`

The smoke fails if the runtime ends in:

- `BridgeActive`
- `BridgeActive_Physical`
- `BalanceActivation_BlendIn`
- `BalanceActivation_StandingValidation`
- `BalanceActive_Recovery`
- `SafeDenied`
- unresolved entry state
- ambiguous recovery state

## Safe Denial Contract

Safe denial is an explicit outcome.

Safe denial is allowed only when the runtime emits an explicit terminal denial result and leaves the ambiguous entry path.

Safe denial is not:

- stalling in `BridgeActive`
- repeating retries without terminal resolution
- a silent fallback

## Logging Contract

Balance-mode logs must distinguish:

- normal bridge bring-up
- physical-readiness state
- controller blend-in
- standing validation
- active standing
- safe denial or failure

At minimum, logs during activation must make it possible to tell:

- whether the balance-critical chain stayed continuously simulated
- how controller authority ramped
- whether shell influence was absent or materially active
- whether the failure was contract-level or physical-level

## Acceptance Criteria

This spec is satisfied only when all of the following are true:

- balance activation is documented as a separate runtime contract
- the balance-critical chain is explicit
- the target activation flow is explicit
- diagnostics are explicitly observational only
- contract correctness and physical viability are explicitly distinguished
- the smoke passes only on sustained `BalanceActive_Standing`
