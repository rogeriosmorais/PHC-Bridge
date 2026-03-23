# Balance Mode Phase 1 Stabilization Spec

Status: Authoritative implementation design  
Scope: Stage 1 behavior for `BalanceTransition_Phase1_Prepare` and `BalanceTransition_Phase1_LateValidate`

## 1. Purpose

This document defines the concrete stabilization recipe for Phase 1.

It is authoritative for:

- body sets
- frozen topology capture
- ownership and movement-type intent
- write-routing and suppression
- hold-reference behavior
- quiet proof
- LateValidate sustain
- failure classes
- recovery and retry rules

## 2. Current interpretation

Phase 1 is now best understood as two things at once:

- a contract-validation stage
- a physical-viability test for the accepted pre-root-on setup

Important rule:

- a Phase 1 attempt may be fully contract-correct and still fail because the accepted setup is physically non-viable under current control, tuning, contact, or state-application behavior

## 3. Authoritative body sets

### Root set
- `pelvis`

### Proximal set
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

### Distal set
- `calf_l`
- `calf_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

### Upper-body set
- `clavicle_l`
- `upperarm_l`
- `lowerarm_l`
- `hand_l`
- `clavicle_r`
- `upperarm_r`
- `lowerarm_r`
- `hand_r`
- `neck_01`
- `head`

## 4. Phase 1 Truth Model Alignment

Phase 1 behavior is governed by the authoritative [Phase 1 / LateValidate Truth Model](./phase1-late-validate-truth-model.md).

Important rules:

- `pelvisSimulating=false` is not, by itself, a deny condition under the current topology.
- The frozen Phase 1 topology record is the authoritative contract for Prepare and LateValidate.
- **Accepted Phase 1 Topology**: Root=Kinematic, Proximal=Simulated, Distal=Kinematic, UpperBody=Kinematic.
- **Expected Sim Counts**: `proximalSimCount = 5`, `distalSimCount = 0`, `upperBodySimCount = 0`, `totalSimCount = 5`.
- **Upper-Body Hold**: Must use `LateValidationKinematicHold` frozen from the Phase 1 contract.
- Live readiness reclassification must not silently rewrite the frozen Phase 1 ownership contract while the same attempt is still in Phase 1.
- Ownership evaluation must keep intended/modifier/raw/frozen sources separate as defined in the truth model.

### Timing rule

Phase 1 topology intent and raw body sim state are not guaranteed to be frame-synchronous inside the same component tick.

So:

- same-frame raw state is provisional
- next-frame confirmation is required for ownership-violation diagnostics
- a same-frame mismatch is not, by itself, proof of a persistent ownership failure

## 7. Write-routing contract

During Prepare and LateValidate:

- normal policy writes to the accepted Phase 1 set are suppressed
- only the explicit allowed hold path may write to kinematic Phase 1 bones
- simulated Phase 1 bones must not receive held-path target writes
- diagnostics must distinguish `normal`, `held`, and `total` writes

### Current required kinematic-hold targets

The allowed held path currently exists for the kinematic Phase 1 bones:

- root
- distal set
- upper-body set

The proximal set is the accepted simulated set and must not receive held-path target writes.

## 8. Authoritative Phase 1 movement-type writes

Broad PhysicsControl set writes are not authoritative enough for topology-critical Phase 1 ownership.

Current required implementation rule:

- topology-critical Phase 1 bones must be driven by explicit per-bone authoritative writes
- the runtime must not rely on `SetBodyModifiersInSetMovementType("All", ...)` as the source of truth for distal Phase 1 ownership

## 9. BridgeActive suppression contract

The runtime must not allow `BridgeActive` bring-up logic to poison Phase 1 by re-promoting accepted distal kinematic bones back to simulated before the transition even starts.

Current required behavior:

- `PerBone_BodyModSync` re-promotion of accepted distal kinematic bones must be suppressed while the active ownership rule says they must stay kinematic

This is a contract rule now, not just a debugging convenience.

## 10. Hold-reference contract

Phase 1 posture preservation uses a single authoritative hold reference.

Default rule:

- capture the current skeletal pose once on Phase 1 entry
- do not continuously reseed it
- do not chase live locomotion animation

If a reset path uses cached targets, that behavior must remain consistent with the frozen Phase 1 topology contract.

## 11. Quiet proof

Phase 1 requires an explicit quiet proof object.

Required object:

- `Phase1QuietProof`

The accumulator resets whenever any quiet condition becomes false.

No carryover is allowed.
 
## 11.1 Pending-Reset Handling

Pending cached-target resets represent unapplied discontinuities that can invalidate the quiet proof.

Required behavior:

- admit to LateValidate only when `PendingBodyModifierCachedResetNames` is empty
- during `LateValidationKinematicHold`, any reset name appearing in the pending list for an upper-body bone is a contract terminal violation
- explicitly drain or apply mandatory resets before attempting the Phase 1 quiet proof


## 12. LateValidate

LateValidate exists to prove that the accepted setup remains valid under stricter sustained conditions.

Important rules:

- LateValidate must not start if a required admission precondition is already known false on that tick
- LateValidate upper-body hold must persist for the full LateValidate sustain window if the frozen topology says upper body is under `LateValidationKinematicHold`
- LateValidate must use the frozen Phase 1 topology as the source of truth for ownership expectations

### Current LateValidate contract checks

LateValidate may deny for at least these named reasons:

- `phase1_late_validate_upper_body_instability`
- `phase1_late_validate_sim_coverage_regressed`
- body-motion instability or other equivalent physical-viability reasons

### Required LateValidate Gates

The transition to Phase 2 requires specific proof from these operational gates:

1. **[bringUp]**: Final stabilization group control alpha must be >= 1.0 (settled).
2. **[shellSafety]**: Multi-vector proof of shell stability (offset, velocity, growth) and authority lock/reanchor.
3. **[expectedRelease]**: Satisfactory sustain duration for both LateValidate and ShellHold clocks.
4. **[readyProven]**: Final aggregate signal combining all the above plus `RootCoupledReady` classification.

### Current resolved issue

`phase1_late_validate_upper_body_instability` caused by prematurely freezing `upperBodyOwnership=None` is now treated as a resolved contract bug, not the current leading blocker.

## 13. Convergence and sim-coverage source

Prepare and LateValidate gating must use an authoritative post-update convergence snapshot.

That snapshot is the source of truth for:

- root validity
- authoritative root tilt
- target continuity
- max sim-body linear speed
- max sim-body angular speed
- worst-body identifiers
- shell/reference deltas used by entry gating

But sim-coverage checks must also explicitly compare:

- frozen expected sim coverage
- live observed sim coverage

Those two must not be conflated.

## 13.1 Investigation Surface (Temporary)

This section reflects the current investigation focus. These details are temporary and expected to change as convergence issues are resolved; they do not form part of the permanent design contract.

### Last Confirmed Failure Mode
- `phase1_late_validate_sim_coverage_regressed` (specifically `spine_01` and `thigh` promotion issues)


This means the docs now require explicit visibility into:

- expected proximal sim count
- observed proximal sim count
- expected total sim count
- observed total sim count
- per-bone intended / modifier / raw / counted-as-simulating state for the expected proximal set

## 14. Failure classification

### Contract-level failures

Examples:

- topology mismatch
- wrong frozen ownership capture
- suppression regression
- illegal write leak
- freeze-lifetime violation
- stale or wrong convergence source
- ownership-source mismatch caused by using the wrong source of truth

### Physical-level failures

Examples:

- accepted sim set dynamically unstable
- insufficient stability margin to enter or survive LateValidate
- entry quietness collapses under contact/tuning behavior
- body-motion instability after a contract-correct admission
- sim-coverage regression after a contract-correct topology freeze

These classes must stay distinct in logs and docs.

## 15. Recovery

After Phase 1 failure, recovery must:

- stop Phase 1 timers
- clear transition-local suppression
- restore coherent `BridgeActive` topology
- clear hold-reference state
- clear transition-local ownership latches that belong only to the failed attempt
- return to coherent `BridgeActive`, `BalanceTransitionFailed`, or `SafeDenied`

## 16. Acceptance criteria

This spec is satisfied only when all of the following are true:

- Phase 1 freezes the correct topology and upper-body ownership mode
- Prepare and LateValidate respect that frozen topology for the full attempt
- distal ownership is not re-promoted during BridgeActive or Phase 1 when the accepted rule says distal = kinematic
- topology-critical bones are driven by authoritative per-bone movement-type writes
- ownership telemetry is next-frame-confirmed and keeps intended / modifier / raw state distinct
- the docs explicitly allow the possibility that the accepted setup is still physically non-viable after the contract is correct
