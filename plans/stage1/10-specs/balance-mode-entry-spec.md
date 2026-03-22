# Stage 1 Balance-Mode Entry Spec

## Purpose

This document defines the authoritative Phase 1 balance-entry contract for Stage 1.

It exists to remove ambiguity between:

- normal bridge startup
- accepted Phase 1 balance entry
- later balance runtime

It also explicitly separates:

- **contract correctness**
- **physical viability**

A Phase 1 attempt can satisfy the contract and still fail physically.

---

## 1. Scope

This spec defines:

- the accepted Phase 1 topology
- Prepare and LateValidate semantics
- write suppression and hold-path rules
- convergence / admission snapshot rules
- freeze lifetime rules
- what counts as a contract failure
- what counts as a physical viability failure

---

## 2. High-level intent

Balance entry is a transitional contract that converts an already-running bridge into a frozen setup suitable for balance validation.

Phase 1 is not the final architecture.

Long term, balance should become the always-on runtime condition.

For Stage 1, however, Phase 1 must still be a precise and falsifiable contract so the project can determine whether the current accepted setup is viable.

---

## 3. Accepted Phase 1 topology

The current accepted Phase 1 topology is:

- root: kinematic
- proximal set: simulated
- distal set: simulated
- upper body: kinematic

This topology is the authoritative source of truth during Prepare and LateValidate.

### 3.1 Root-side rule

The root side is intentionally kinematic in the current accepted topology.

That means:

- `pelvisSimulating=false` is allowed
- pelvis/root simulation is not a required Phase 1 condition

What is required instead:

- valid root/pelvis-side body source
- valid root-side uprightness source
- correct accepted ownership snapshot

### 3.2 Proximal simulated set

Current intended proximal simulated bones:

- `thigh_l`
- `thigh_r`
- `spine_01`
- `spine_02`
- `spine_03`

### 3.3 Distal simulated set

Current intended distal simulated bones:

- `calf_l`
- `foot_l`
- `ball_l`
- `calf_r`
- `foot_r`
- `ball_r`

### 3.4 Upper-body kinematic set

The upper body remains kinematic in Phase 1.

The upper-body hold-path allowlist is an implementation detail, but it must remain a subset of the intended kinematic side of the accepted topology.

---

## 4. Phase 1 states

The Phase 1 public states are:

- `BalanceEntry_Prepare`
- `BalanceEntry_LateValidate`
- `BalanceEntry_RootOn`
- `BalanceEntry_Settle`

The current stabilization work is centered mainly on:

- `BalanceEntry_Prepare`
- `BalanceEntry_LateValidate`

---

## 5. Freeze rule

Phase 1 owns the startup bring-up freeze for the duration of the Phase 1 attempt.

### 5.1 Acquire

Freeze is acquired on transition accept.

### 5.2 Lifetime

Freeze remains active for the full Phase 1 attempt, including:

- Prepare
- LateValidate
- any Prepare ↔ LateValidate bounce
- any blocked LateValidate admission / reset that still belongs to the same attempt

### 5.3 Release

Freeze is released only on a true terminal outcome of the current attempt:

- successful completion into the next non-Phase-1 success path
- terminal safe deny
- explicit abort / teardown
- true attempt inactivation

One attempt must produce one acquire and one release.

---

## 6. Prepare contract

Prepare is the admission and pre-validation phase for the accepted Phase 1 setup.

Prepare is allowed to:

- preserve the accepted topology
- accumulate a quiet window
- block LateValidate admission when admission prerequisites are not met
- reset its quiet window when the accepted sim set is not dynamically quiet enough

Prepare is **not** allowed to:

- treat pelvis simulation as a required condition under `root=kin`
- keep silently retrying LateValidate when the accepted sim set is already clearly non-viable without resetting admission state appropriately

### 6.1 Prepare admission prerequisites

Prepare may admit LateValidate only when all current required prerequisites are true:

- accepted topology is valid and preserved
- root-side uprightness source is valid
- root tilt is within threshold
- suppression / hold-only contract is active as intended
- cached post-update convergence snapshot is available
- the accepted sim set has sufficient dynamic stability margin to justify LateValidate admission

### 6.2 Prepare blocked-admission rule

If any admission prerequisite fails, Prepare must stay in Prepare and emit a blocked-admission reason.

Blocked-admission reasons must be specific.

Examples include:

- insufficient stability margin
- invalid authoritative root source
- topology mismatch
- tilt too high

`pelvis_not_simulating` is not a valid blocked-admission reason under the current accepted topology.

### 6.3 Prepare terminal rule

Prepare may escalate to terminal safe deny if the current attempt becomes clearly non-viable and is no longer meaningfully retryable.

Terminal reasons must be specific and truthful.

---

## 7. LateValidate contract

LateValidate is the stricter proof phase for the accepted frozen setup.

LateValidate starts only after Prepare admission succeeds.

LateValidate is allowed to:

- verify that the accepted setup remains valid under stricter timing/quiet conditions
- reset back to Prepare if the attempt remains legitimately retryable
- safe-deny if the setup is clearly non-viable

LateValidate is **not** allowed to:

- inherit unresolved startup ownership state
- rely on stale pre-update telemetry when post-update telemetry exists
- hide known physical failure modes behind unhelpful generic reasons when specific reasons are available

### 7.1 Immediate-reset rule

If a condition that should have blocked admission is already false at LateValidate start, the implementation should prefer blocking admission in Prepare rather than entering LateValidate and immediately resetting at `lateValidateSeconds=0.00`.

---

## 8. Control-write contract during Prepare and LateValidate

Prepare and LateValidate are hold-only from the control-write side.

### 8.1 Suppressed behavior

During Prepare and LateValidate:

- normal policy publishing over the accepted Phase 1 set must be suppressed

### 8.2 Allowed behavior

During Prepare and LateValidate:

- explicit held-pose writes may be published only for the approved kinematic side of the accepted Phase 1 topology

### 8.3 Forbidden behavior

During Prepare and LateValidate:

- accepted simulated Phase 1 bones must receive no target writes
- held-pose writes must not be mislabeled as normal policy writes

---

## 9. Convergence telemetry contract

Admission and deny logic must use an authoritative cached post-update convergence snapshot.

That snapshot must be produced after the frame's authoritative runtime work is complete.

The snapshot must include enough information to evaluate both contract and viability conditions, including:

- root linear speed
- root angular speed
- authoritative root tilt
- shell offset / velocity
- max sim-body linear speed
- max sim-body angular speed
- worst linear-motion bone
- worst angular-motion bone
- target delta diagnostics
- snapshot frame index / world time

---

## 10. Root tilt contract

Root tilt is a root-side uprightness check.

It must be derived from the authoritative root-side source.

The source priority should be:

1. pelvis/root body world frame when valid
2. skeletal mesh root/world frame only as fallback
3. actor/capsule frame as final fallback

The chosen source must be observable in diagnostics.

Root tilt is not a proxy for sim-body instability.

---

## 11. Physical viability rules

These rules govern whether the accepted setup is actually viable.

### 11.1 Viability question

The current core question is:

> Is the accepted Phase 1 frozen setup physically viable under current control, tuning, and contact conditions?

### 11.2 What counts as physical failure

Examples of physical viability failure include:

- accepted-topology sim-body dynamic instability
- insufficient stability margin for LateValidate admission
- contact-driven instability that invalidates the frozen setup
- excessive target / tuning pressure that destabilizes the sim set

These are different from contract failures.

### 11.3 Current hypothesis status

At present, the accepted setup is contract-correct much more often than before, but physical viability is still not proven.

That is an allowed and useful outcome.

---

## 12. Contract-invalid conditions

The following are contract-invalid under the current design:

- requiring pelvis/root simulation while the accepted topology is still `root=kin`
- using pre-update telemetry as the authoritative source when post-update telemetry is available
- letting Prepare or LateValidate run normal policy writes over the accepted sim set
- releasing the Phase 1 freeze in the middle of the same Phase 1 attempt
- ending the smoke in plain `BridgeActive`

---

## 13. Logging requirements

A valid Phase 1 implementation must log enough to answer, from one run:

- what topology was accepted
- what root tilt source was used
- what convergence snapshot frame/time was used
- whether admission was blocked or LateValidate started
- whether the failure was contract-level or physical-level
- which bodies were worst when physical failure occurred
- when freeze was acquired and released

---

## 14. Acceptance criteria

This spec is satisfied only when all of the following are true:

- the accepted Phase 1 topology is explicit and preserved
- root/pelvis may remain kinematic under that topology
- Prepare and LateValidate are hold-only from the control-write side
- admission and deny logic use cached post-update telemetry
- root tilt uses the authoritative root-side source
- freeze lifetime covers the full Phase 1 attempt
- blocked-admission reasons and terminal deny reasons are specific and truthful
- the smoke ends only in explicit balance outcomes
