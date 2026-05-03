# Balance Mode Phase 1 Stabilization Spec

Status: Authoritative implementation design  
Scope: Stage 1 physical-readiness requirements before controller blend-in

## Legacy Filename Note

This filename is retained for compatibility.

The target design is no longer a `Phase1_Prepare` / `Phase1_LateValidate` ritual. This document now defines physical ownership and initial quiet-state requirements for balance-first activation.

## 1. Purpose

This document defines the concrete requirements for entering `BridgeActive_Physical`.

It is authoritative for:

- body sets
- continuous ownership requirements
- initial quiet-state requirements
- shell bookkeeping versus shell influence separation
- failure classes before controller blend-in

## 2. Authoritative Body Sets

### Balance-Critical Chain

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

### Distal Support Set

- `calf_l`
- `calf_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

### Upper-Body Set

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

## 3. Continuous Ownership Rule

The balance-critical chain must be continuously simulated before controller blend-in begins.

Required interpretation:

- `pelvisSimulating=false` is not an acceptable target steady state for balance activation
- temporary kinematic re-ownership of the balance-critical chain is not the intended activation mechanism
- intended ownership, modifier-record ownership, and raw body state must remain distinct observables

## 4. Initial Quiet-State Rule

The runtime must establish a quiet enough physical state before entering controller blend-in.

Required quiet-state evidence:

- balance-critical chain remains simulated
- worst-body linear and angular motion remain inside configured readiness bounds
- shell influence is not materially active
- no reset or conflicting locomotion authority is pending

## 5. Shell Rule

The runtime must keep separate:

- shell bookkeeping state
- shell influence materiality

Shell bookkeeping may exist without being a failure.

Material shell influence on the balance-critical chain before blend-in is a failure.

## 6. Diagnostics Rule

Physical-readiness diagnostics may:

- report ownership continuity
- report quiet-state metrics
- report shell bookkeeping and shell influence separately

They may not:

- redefine instability as success
- treat a grace window as proof that the state is physically ready

## 7. Failure Classification

### Contract-level failures

Examples:

- balance-critical chain not continuously simulated
- wrong truth source used for ownership continuity
- shell bookkeeping and shell influence conflated

### Physical-level failures

Examples:

- live physical state too unstable to begin blend-in
- shell influence materially active on the balance-critical chain
- persistent body-motion instability

These classes must stay distinct in logs and docs.

## 8. Acceptance Criteria

This spec is satisfied only when:

- the balance-critical chain is explicit
- continuous simulated ownership is required before blend-in
- initial quiet-state requirements are explicit
- shell bookkeeping and shell influence are explicitly separated
- no authoritative text presents a kinematic root plus later flip as the target design
