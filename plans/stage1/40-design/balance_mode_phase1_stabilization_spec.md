# Balance Mode Phase 1 Stabilization Spec

Status: Authoritative implementation design  
Scope: Stage 1 behavior for `BalanceTransition_Phase1_Prepare` and `BalanceTransition_Phase1_LateValidate`

## 1. Purpose

This document defines the concrete stabilization recipe for Phase 1.

It is authoritative for:

- body sets
- target topology
- policy suppression
- hold-reference behavior
- quiet proof
- late-validation sustain
- failure classes
- recovery and retry rules

## 2. Current interpretation

Phase 1 is no longer mainly a “can the runtime represent the state machine?” problem.

The current leading question is:

> is the accepted Phase 1 frozen setup physically viable under current control, tuning, and contact conditions?

This document therefore distinguishes:

- Phase 1 contract success
- Phase 1 physical viability

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

## 4. Accepted Phase 1 topology

Under the current design:

- `pelvis = kinematic`
- proximal set = simulated
- distal set = simulated
- upper body = kinematic

Important rule:

- `pelvisSimulating=false` is not, by itself, a Phase 1 deny condition under this topology
- Phase 1 topology intent and raw body sim state are not guaranteed to be frame-synchronous inside the same component tick. Telemetry must evaluate ownership violations on the subsequent frame (post-physics step) to allow PhysicsControl modifiers to propagate to Chaos.

## 5. Write-routing contract

During Prepare and LateValidate:

- normal policy writes to the accepted Phase 1 set are suppressed
- only the explicit allowed hold path may write to the allowed kinematic bones
- simulated Phase 1 bones must receive no held writes
- diagnostics must distinguish normal / held / total writes

## 6. Hold-reference contract

Phase 1 posture preservation uses a single authoritative hold reference.

Default rule:

- capture the current skeletal pose once on Phase 1 entry
- do not continuously reseed it
- do not chase live locomotion animation

## 7. Quiet proof

Phase 1 requires an explicit quiet proof.

Required object:

- `Phase1QuietProof`

The accumulator resets whenever any quiet condition becomes false.

No carryover is allowed.

## 8. LateValidate

LateValidate exists to prove the accepted setup remains valid under stricter sustained conditions.

Important rule:
LateValidate should not start if a required admission precondition is already known false on that tick.

## 9. Convergence source

Prepare and LateValidate gating must use the authoritative post-update convergence snapshot.

That snapshot is the source of truth for:

- authoritative root tilt
- root validity
- target continuity
- max sim-body linear speed
- max sim-body angular speed
- worst-body identifiers
- shell/reference deltas used by entry gating

## 10. Failure classification

### Contract-level failures
Examples:
- topology mismatch
- suppression regression
- illegal write leak
- freeze-lifetime violation
- stale or wrong convergence source

### Physical-level failures
Examples:
- accepted sim set dynamically unstable
- insufficient stability margin to enter LateValidate
- entry quietness collapses under contact/tuning behavior
- body-motion instability after a contract-correct admission

These classes must stay distinct.

## 11. Recovery

After Phase 1 failure, recovery must:

- stop Phase 1 timers
- clear transition-local suppression
- restore normal `BridgeActive` topology
- clear hold-reference state
- return to coherent `BridgeActive`, `BalanceTransitionFailed`, or `SafeDenied`

## 12. Acceptance criteria

This spec is satisfied only when Phase 1 reaches readiness through the documented topology and write-routing contract, and the docs explicitly allow the possibility that the accepted setup is still physically non-viable.
