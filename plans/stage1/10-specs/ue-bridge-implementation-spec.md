# Stage 1 UE Bridge Implementation Spec

## Purpose

This document freezes the Unreal-side implementation contract for the Stage 1 bridge and for balance-mode entry.

It is implementation-facing and must match the runtime contract in:

- `plans/stage1/10-specs/bridge-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Runtime owner

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
- balance-entry state
- convergence snapshots
- smoke-visible terminal outcome

## Runtime states

The implementation must expose distinct runtime states or equivalent explicit balance-entry sub-states for:

- `BridgeActive`
- balance-entry Prepare
- balance-entry LateValidate
- `BalanceActive`
- `SafeDenied`

Implementation must not leave balance entry represented only as plain `BridgeActive`.

## Normal bring-up implementation

Normal bring-up may continue to use staged non-root groups.

The staged bring-up path may own:

- simulation handoff
- group unlock sequence
- control-authority ramps
- policy-influence ramp

These values are not the balance-entry source of truth.

## Required balance-entry data

On balance request acceptance, the runtime must create a dedicated balance-entry record containing:

- attempt-active state
- request-accepted timestamp
- accepted topology snapshot
- freeze state
- quiet-proof state
- late-validation state
- terminal outcome flag
- failure reason if any

## Required convergence snapshot

Prepare and LateValidate gating must use a dedicated authoritative post-update convergence snapshot.

That snapshot must be populated after the runtime work that can change the evaluated state, including target publication and relevant body/control updates.

The snapshot must at minimum be able to report:

- root-side validity
- root tilt from authoritative source
- max sim-body linear speed
- max sim-body angular speed
- worst-bone identifiers
- shell/reference deltas used by entry gating
- target-delta metrics used by entry gating

The snapshot must not be replaced by stale pre-update or loosely related animation/root-bone values.

## Required write-routing contract

During Prepare and LateValidate:

- normal policy writes over the accepted Phase 1 set must be suppressed
- only the explicit allowed hold path may publish to the allowed kinematic bones
- simulated Phase 1 bones must not receive held target writes
- diagnostics must distinguish `normal`, `held`, and `total` writes

## Required freeze contract

The startup bring-up freeze must:

- acquire on transition accept
- remain active for the full Phase 1 attempt
- survive Prepare / LateValidate bouncing inside the same attempt
- release only on terminal outcome

There must be one acquire and one release per attempt.

## Required topology rule

Under the current accepted Phase 1 design, the implementation must treat the intended topology as:

- `root = kinematic`
- `proximal = simulated`
- `distal = simulated`
- `upper = kinematic`

Implementation must not silently change ownership semantics under the guise of runtime tuning or diagnostics work.

Phase 1 topology intent and raw body sim state are not guaranteed to be frame-synchronous inside the same component tick. Telemetry/diagnostics must evaluate ownership violations on the subsequent frame to allow PhysicsControl modifiers to propagate to Chaos.

If implementation changes ownership semantics, the `10-specs` docs must change in the same commit.

## Required pelvis/root rule

Under the current `root=kin` Phase 1 design:

- `pelvisSimulating=false` is not, by itself, a failure
- pelvis/root validity and authoritative uprightness source matter
- pelvis simulation state is diagnostic, not a required admission condition

## Required LateValidate behavior

LateValidate may begin only after Prepare has satisfied the documented admission conditions.

LateValidate must not be entered if a required admission precondition is already known false on that tick.

LateValidate must emit specific failure reasons where possible, especially when the accepted sim set is dynamically unstable.

## Smoke-test evaluation rule

The automation smoke must evaluate the final state using the balance-entry terminal state, not the generic bridge-running state.

Passing outcomes:

- `BalanceActive`
- `SafeDenied`

Failing outcomes:

- `BridgeActive`
- unresolved entry state
- ambiguous failure state

## Documentation alignment rule

No implementation detail is allowed that requires a hidden balance-entry rule absent from `10-specs`.

If implementation adds or removes a real gate, ownership rule, or terminal reason, the `10-specs` balance-entry docs must be updated in the same change.

## Acceptance criteria

This implementation spec is satisfied only when:

- balance entry is implemented as a distinct state path
- a dedicated post-update convergence snapshot exists
- the write-routing contract is explicit
- the freeze contract is explicit
- pelvis simulation is not treated as required under `root=kin`
- smoke evaluation uses terminal balance outcomes
- the runtime cannot silently end the smoke in plain `BridgeActive`
