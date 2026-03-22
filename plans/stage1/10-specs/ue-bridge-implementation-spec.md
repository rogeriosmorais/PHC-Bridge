# Stage 1 UE Bridge Implementation Spec

## Purpose

This document freezes the Unreal-side implementation contract for the Stage 1 bridge and for balance entry.

It is implementation-facing and must match the runtime contract in:

- `plans/stage1/10-specs/bridge-spec.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Runtime owner

The live Stage 1 runtime owner is:

- `UPhysAnimComponent`

`UPhysAnimComponent` owns:

- startup validation
- PoseSearch query state
- NNE runtime / model / session lifetime
- observation packing
- action unpacking
- runtime control-target publishing
- runtime state transitions
- balance-entry state transitions
- accepted Phase 1 topology snapshot access
- convergence snapshot production
- runtime diagnostics exposed to smoke tests

The balance-entry controller may live in a helper state-machine object, but `UPhysAnimComponent` remains the authoritative runtime integration owner.

---

## Required public runtime states

The implementation must expose distinct public runtime states for at least:

- `BridgeActive`
- `BalanceEntry_Prepare`
- `BalanceEntry_LateValidate`
- `BalanceEntry_RootOn`
- `BalanceEntry_Settle`
- `BalanceActive`
- `BalanceSafeDeny`

Implementation must not leave balance entry represented only as plain `BridgeActive`.

---

## Normal bring-up implementation

Normal bring-up may continue to use staged non-root groups.

The staged bring-up path may own:

- simulation handoff
- group unlock sequence
- control-authority ramps
- policy-influence ramp

These normal bring-up values are not the accepted Phase 1 topology source of truth.

---

## Required balance-entry record

On balance request acceptance, the runtime must create or refresh a dedicated balance-entry record.

That record must contain enough information to evaluate the full Phase 1 attempt, including:

- accepted request state
- accepted topology snapshot
- quiet-window / quiet-proof state
- blocked-admission state
- terminal outcome state
- terminal denial reason if denied
- freeze-lifetime state for the current attempt

The record may be physically split across the transition object and `UPhysAnimComponent`, but it must behave as a single coherent attempt record.

---

## Required accepted Phase 1 topology snapshot

Before LateValidate admission begins counting quiet-proof time, the implementation must freeze and store:

- root ownership mode
- proximal ownership mode
- distal ownership mode
- upper-body ownership mode
- `simCount`
- `upperBodySimCount`
- policy suppression state for Phase 1
- reset suppression state for Phase 1

This stored snapshot is then used for:

- Phase 1 logs
- Prepare / LateValidate gating
- LateValidate resets
- smoke-visible outcome evaluation

The snapshot must not be recomputed from the live startup bring-up path on every frame.

---

## Required Phase 1 topology rule

The current accepted Phase 1 topology is:

- root: kinematic
- proximal: simulated
- distal: simulated
- upper body: kinematic

Implementation must not silently introduce a different ownership rule without updating `10-specs`.

Under this topology:

- pelvis/root simulation is **not** a required Phase 1 condition
- pelvis/root validity and authoritative uprightness source are required

Any implementation path that blocks or terminates Phase 1 purely because pelvis is not simulating is contract-invalid under the current design.

---

## Required control-write implementation rule

During:

- `BalanceEntry_Prepare`
- `BalanceEntry_LateValidate`

normal policy publishing must be suppressed for the accepted Phase 1 set.

The implementation must enforce:

- held-pose writes only for explicitly approved kinematic Phase 1 bones
- zero target writes for accepted Phase 1 simulated bones
- split diagnostics for normal policy writes vs held writes vs total writes

This write-path separation is required runtime behavior, not optional logging polish.

---

## Required convergence snapshot

The implementation must produce a cached post-update convergence snapshot.

That snapshot must be populated only after the authoritative runtime work for that frame, including at least:

- runtime control tuning
- pending reset application
- control-target publishing
- control update / physics-control update
- instability and body-motion telemetry collection

The snapshot must contain enough data to drive admission / deny logic, including at least:

- root linear speed
- root angular speed
- authoritative root tilt
- shell offset / velocity
- body-motion maxima for the accepted sim set
- worst-body identifiers
- target-delta diagnostics
- snapshot frame index / world time

Prepare / LateValidate admission must use that cached post-update snapshot, not a stale pre-update value.

---

## Required root tilt implementation rule

The implementation must compute Phase 1 root tilt from the authoritative root-side source.

Acceptable priority order:

1. pelvis/root body-instance world transform when valid
2. skeletal mesh root/world transform only as fallback
3. actor/capsule frame only as final fallback

The chosen source must be observable in diagnostics.

---

## Entry-state flow

Required high-level flow:

1. `BridgeActive`
2. balance request accepted
3. Phase 1 freeze acquired
4. accepted Phase 1 topology frozen
5. `BalanceEntry_Prepare`
6. `BalanceEntry_LateValidate` only when admission criteria are satisfied
7. terminal outcome:
   - next balance-success path, or
   - `BalanceSafeDeny`

Disallowed flow:

1. `BridgeActive`
2. balance request accepted
3. repeated unresolved Prepare / LateValidate bouncing
4. smoke exits in plain `BridgeActive`

---

## Prepare admission rule

Prepare may remain active while it accumulates a legitimate quiet window.

However, Prepare must not continue building or retaining that quiet window when the accepted Phase 1 sim set is already dynamically non-quiet.

Implementation must be able to:

- block LateValidate admission on insufficient stability margin
- reset the quiet window on real body-motion instability
- keep blocked-admission reasons specific and truthful

---

## LateValidate timing rule

Quiet-proof timing starts only after:

- the accepted topology is frozen
- the current Phase 1 ownership contract is established
- the runtime has entered `BalanceEntry_LateValidate`
- the admission criteria were satisfied by the authoritative post-update snapshot

Quiet-proof timing must not start while the system is still carrying unresolved startup ownership assumptions.

---

## LateValidate failure rule

When LateValidate fails:

- the failure reason must identify the actual violated contract when possible
- accepted-topology dynamic instability should not collapse into a generic terminal string if a specific deny reason is available
- body-motion-instability failure should identify the key sim-body metrics and worst bones

The implementation may reset back to Prepare only if that reset still belongs to the same Phase 1 attempt and the attempt remains legitimately retryable.

Otherwise it must safe-deny.

---

## Freeze lifetime rule

The startup bring-up freeze is part of the Phase 1 attempt contract.

Implementation must:

- acquire it on transition accept
- keep it active for the whole Phase 1 attempt
- preserve it across any Prepare ↔ LateValidate bounce in the same attempt
- release it only on a real terminal outcome of that attempt

There must be one acquire and one release per attempt.

---

## Smoke-test evaluation rule

The automation smoke must evaluate the final state using the balance-entry terminal state, not the generic bridge-running state.

Required passing outcomes:

- `BalanceActive`
- `BalanceSafeDeny`

Required failing outcomes:

- `BridgeActive`
- unresolved `BalanceEntry_*`
- `Failed`

The smoke may additionally assert contract details such as freeze lifetime and terminal specificity.

---

## Logging requirements

The implementation must emit logs for:

- balance request accepted
- freeze acquire
- accepted topology snapshot
- Prepare admission or blocked-admission reason
- LateValidate start
- LateValidate reset reason if reset occurs
- specific terminal deny reason if denied
- freeze release

Logs around admission / deny must be sufficient to answer:

- which snapshot was used
- which thresholds were evaluated
- which bones were worst
- whether the failure was contract-level or physical-level

---

## Documentation alignment rule

No implementation detail is allowed that requires a hidden balance-entry rule absent from `10-specs`.

If implementation adds or changes:

- a required Phase 1 precondition
- a topology rule
- a suppression rule
- a terminal state or terminal deny class

then the corresponding `10-specs` documents must be updated in the same change.

---

## Acceptance criteria

This implementation spec is satisfied only when:

- balance entry is implemented as a distinct public state path
- the accepted Phase 1 topology snapshot exists and is used
- Prepare / LateValidate hold-only behavior is implemented
- admission / deny logic uses cached post-update convergence telemetry
- root tilt uses the authoritative root-side source
- pelvis simulation is not used as a required gate under the current `root=kin` topology
- freeze lifetime covers the full Phase 1 attempt
- smoke evaluation uses explicit balance terminal outcomes
