# Stage 1 UE Bridge Implementation Spec

## Purpose

This document freezes the Unreal-side implementation contract for the Stage 1 bridge and for balance-mode entry.

It is implementation-facing and must match the runtime contract in the balance-mode design docs.

## Runtime owner

The live Stage 1 runtime owner is:

- `UPhysAnimComponent`

`UPhysAnimComponent` owns:

- startup validation
- PoseSearch query state
- NNE runtime/model/session lifetime
- observation packing
- action unpacking
- PhysicsControl writes
- bridge runtime state
- balance-entry state
- frozen topology capture
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

## Required balance-entry data

On balance request acceptance, the runtime must create a dedicated balance-entry record containing:

- attempt-active state
- request-accepted timestamp
- accepted topology snapshot
- frozen upper-body ownership mode
- freeze state
- quiet-proof state
- LateValidate state
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
- live sim-coverage counts used by Phase 1 validation

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
- `distal = kinematic`
- `upper = kinematic`

The implementation must not silently change ownership semantics under the guise of runtime tuning or diagnostics work.

If ownership semantics change, the implementation spec and balance-mode design docs must change in the same commit.

## Required frozen-capture rule

The accepted Phase 1 topology record is authoritative once captured.

The implementation must not derive ongoing Phase 1 ownership from live readiness classification when that would conflict with the frozen accepted topology.

Current specific requirement:

- if Phase 1 LateValidate requires upper-body hold, the frozen record must store `LateValidationKinematicHold`
- it must not silently freeze `None` just because the live classification happened to look ready on the entry tick

## Required ownership-separation rule

The implementation must keep these sources separate:

- intended ownership
- PhysicsControl modifier-record ownership
- raw body sim state

These are different signals.

### Timing rule

Phase 1 topology intent and raw body sim state are not guaranteed to be frame-synchronous inside the same component tick.

So:

- same-frame ownership mismatch is provisional
- ownership-violation telemetry must use next-frame confirmation
- the telemetry path must be read-only, not self-healing

## Required topology-critical write rule

For topology-critical Phase 1 bones, the implementation must not depend solely on broad-set PhysicsControl movement-type writes such as `"All"`.

Current requirement:

- topology-critical Phase 1 bones must be enforced by explicit per-bone authoritative writes

## Required BridgeActive suppression rule

The runtime must not allow BridgeActive per-bone sync to re-promote accepted distal kinematic bones back to simulated before or during a balance-entry attempt.

## Required LateValidate behavior

LateValidate may begin only after Prepare has satisfied the documented admission conditions.

LateValidate must not be entered if a required admission precondition is already known false on that tick.

LateValidate must emit specific failure reasons where possible, including current contract/viability reasons such as:

- `phase1_late_validate_upper_body_instability`
- `phase1_late_validate_sim_coverage_regressed`

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

No implementation detail is allowed that requires a hidden balance-entry rule absent from the current design/spec docs.

If implementation adds or removes a real gate, ownership rule, frozen-topology rule, or terminal reason, the design/spec docs must be updated in the same change.

## Acceptance criteria

This implementation spec is satisfied only when:

- balance entry is implemented as a distinct state path
- a dedicated frozen Phase 1 topology record exists
- a dedicated post-update convergence snapshot exists
- the write-routing contract is explicit
- the freeze contract is explicit
- ownership sources remain distinct
- topology-critical writes are authoritative per-bone where required
- smoke evaluation uses terminal balance outcomes
- the runtime cannot silently end the smoke in plain `BridgeActive`
