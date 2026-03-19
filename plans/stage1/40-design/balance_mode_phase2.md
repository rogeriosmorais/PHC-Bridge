# Balance Mode Phase 2 Root-On Spec

Status: Authoritative implementation spec
Scope: Stage 1 runtime behavior for `BalanceTransition_Phase2_RootOn` and the immediate post-root-on guard window
Audience: runtime, controls, debugging, validation, and transition work for PhysAnim bridge

## 1. Purpose

This document defines the exact root-on choreography for Phase 2 of the Balance Mode entry transition.

It is authoritative for:

- Phase 2 entry preconditions
- safe denial before root-on
- pre-root-on shell-safety proof
- warm-start root-on requirements
- root-on frame order
- guard-window rules
- root-on spike classification
- Phase 2 recovery and retry rules

## 2. Relationship to other docs

This document works with:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`

Interpretation rule:

- the entry spec defines the overall transition contract
- the Phase 1 spec defines the certified handoff payload
- this document defines how Phase 2 consumes that payload and performs root-on

## 3. Core rule

Phase 2 is not “turn pelvis sim on and hope.”

Phase 2 may begin only from a still-valid certified handoff and only after the explicit root-on readiness proof is satisfied.

If the proof is absent, false, or incoherent, Phase 2 must deny safely before root-on.

## 4. Authoritative body sets

### 4.1 Root set

- `pelvis`

### 4.2 Proximal transition-critical set

- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

### 4.3 Distal spike-prone lower-limb set

- `calf_l`
- `calf_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

### 4.4 Upper-body set

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

## 5. Required entry preconditions

Phase 2 may begin only if all are true:

- certified Phase 1 handoff payload exists
- handoff payload is still valid
- late validation completed successfully
- control-authority settled state still matches payload
- topology still matches payload
- target continuity still matches payload
- no reset is pending
- no topology flip is pending
- no fail-stop precursor is active
- shell conditions remain within entry bounds
- root motion remains within entry bounds
- upper-body ownership and stability remain within payload bounds
- the handoff classification is `RootCoupledReadyHandoff`
- `PreRootOnShellSafetyProof` is true

Interpretation rule:

- `UpperOnlySafeDenyHandoff` is a valid Phase 1 success state
- it is not root-on-ready
- if Phase 2 sees that handoff class, it must deny safely

## 6. Safe denial path

If a required Phase 2 entry precondition is false, Phase 2 must deny before any root-on attempt.

Required denial reasons include:

- `phase2_missing_handoff_payload`
- `phase2_handoff_invalidated`
- `phase2_sim_coverage_regressed`
- `phase2_target_discontinuity_too_high`
- `phase2_control_authority_not_settled`
- `phase2_late_validate_not_completed`
- `phase2_upper_body_unstable`
- `phase2_upper_only_handoff_not_root_on_ready`
- `phase2_pre_root_on_shell_correction_safety_not_proven`

Denial is a valid safe outcome.

## 7. `PreRootOnShellSafetyProof`

Phase 2 requires an explicit pre-root-on shell-safety proof.

The proof is named:

- `PreRootOnShellSafetyProof`

Its purpose is to prove that root-on is not expected to immediately fail through shell/capsule planar correction behavior.

### 7.1 Required proof window

The proof must hold continuously for:

- `Phase2PreRootOnShellProofRequiredSeconds`

### 7.2 Required proof inputs

Minimum inputs:

- shell planar offset delta
- shell planar velocity delta
- shell planar offset trend
- shell planar velocity trend
- root planar linear speed
- root angular speed
- certified handoff topology classification
- certified sim counts
- shell authority mode
- whether shell reference was re-anchored
- whether shell reference was reseeded after lock

### 7.3 Pass conditions

The proof is true only if all hold continuously for the full proof window:

- handoff classification = `RootCoupledReadyHandoff`
- `distalSimCount = 0`
- shell planar offset delta below threshold
- shell planar velocity delta below threshold
- shell offset growth below threshold
- shell velocity growth below threshold
- root linear speed below entry threshold
- root angular speed below entry threshold
- locomotion authority inactive
- shell/capsule corrective owner inactive
- no reset pending
- no topology change pending
- shell authority mode = `TransitionOwnedShellLocked`
- shell reference was re-anchored exactly once before the proof window
- shell reference has not been reseeded after lock

### 7.4 Failure result

If the proof is absent, false, or unknown, Phase 2 must deny with:

- `phase2_pre_root_on_shell_correction_safety_not_proven`

## 8. Warm-start contract

Root-on must be executed as a warm start, not a blind flip.

Required behavior:

- do not seed pelvis from animation/root-bone pose alone
- seed from the live physics-consistent neighboring chain whenever available
- validate pelvis-to-thigh and pelvis-to-spine constraint error before enabling root simulation
- abort before root-on if constraint error exceeds threshold
- zero and reseed velocities around the sim flip
- log pre-flip and post-flip constraint error

## 9. Authority matrix during Phase 2

### 9.1 Policy authority

During Phase 2:

- policy writes to `pelvis` are forbidden
- policy writes to the proximal set are forbidden
- policy writes to the distal set are forbidden

Default recommended behavior:

- suppress policy writes globally during the root-on frame and the guard window

### 9.2 Control target authority

Allowed:

- preserve the hold/reference state already established by Phase 1
- keep frozen targets unchanged

Forbidden:

- fresh policy-driven targets on the root-on frame
- new reference reseed on the root-on frame
- same-frame control-family re-enable plus reference change

### 9.3 Body modifier authority

Body modifiers are the authoritative owners of the root-on topology flip.

### 9.4 Cached reset authority

Cached-target resets are forbidden:

- on the root-on frame
- during the guard window

### 9.5 Shell / CharacterMovement authority

During Phase 2:

- CharacterMovement corrective motion must remain suppressed
- capsule/gameplay shell correction must remain suppressed
- bridge-owned shell translation must remain suppressed
- shell correction is measured as contamination or abort condition, not used as assistance

The required shell authority mode is:

- `TransitionOwnedShellLocked`

That shell-lock mode must remain active through the full Phase 2 guard window.

Authoritative shell/capsule ownership chain during Phase 2:

- `GameplayShellAuthority -> TransitionOwnedShellLocked`

## 10. Root-on frame sequence

Phase 2 must execute in this order.

### 10.1 Entry snapshot

Record once:

- root sim state before flip
- root linear velocity before flip
- root angular velocity before flip
- shell offset delta before flip
- shell velocity delta before flip
- sim count before flip
- proximal sim count before flip
- distal sim count before flip
- max body linear speed before flip
- max body angular speed before flip
- max target delta before flip
- mean target delta before flip
- policy suppression state
- reset-pending state

### 10.2 Freeze hazards

Before root-on:

- policy writes suppressed
- cached resets suppressed
- locomotion entry suppressed
- shell assistance suppressed
- CharacterMovement correction suppressed
- `TransitionOwnedShellLocked` already active
- shell reference already re-anchored

### 10.3 Execute root-on

Root-on means:

- pelvis/root body modifier flips to simulated
- pelvis/root collision state becomes the intended simulated state
- pelvis/root sim validity is immediately re-read and confirmed

Hard rule:

- root-on must not be the same frame shell authority transfers from gameplay to transition ownership

### 10.4 Immediate post-flip validation

Immediately after the flip, confirm:

- pelvis/root exists
- pelvis/root is simulating
- no same-frame system turned it back off
- suppression still holds
- no reset was scheduled during the flip

If any check fails, abort Phase 2 immediately.

## 11. Guard window

The guard window begins immediately after technical root-on success.

Required duration:

- `Phase2GuardWindowSeconds`

During the guard window, all of the following remain forbidden:

- policy writes to root/proximal/distal sets
- cached resets
- topology expansion
- locomotion entry
- shell or CharacterMovement correction
- shell reference reseed
- startup/gameplay authority reclaim

## 12. Topology during Phase 2

For the first true success path, the required topology is:

- `pelvis` = simulated
- proximal set = simulated
- distal set = kinematic
- upper-body ownership = unchanged from the certified handoff payload

Hard rules:

- Phase 2 must not combine root-on with proximal topology expansion
- Phase 2 must not combine root-on with distal re-simulation
- upper-body ownership mode must remain unchanged through the guard window

## 13. Root-on spike definition

A `phase2_root_on_spike` occurs if any abort threshold is exceeded during the root-on frame or guard window.

Abort metrics include:

- root linear speed
- root angular speed
- shell offset delta
- shell velocity delta
- max body linear speed
- max body angular speed

Additional hard abort reasons:

- root simulation drops unexpectedly
- cached reset occurs
- policy write leak occurs
- topology expands unexpectedly
- fail-stop precursor becomes active
- material shell correction occurs

## 14. Phase 2 success

Phase 2 succeeds only if all hold for the full guard window:

- pelvis/root remains simulating
- no abort threshold exceeded
- no material shell correction
- no policy write leak
- no reset occurs
- no unexpected topology expansion occurs
- `TransitionOwnedShellLocked` remains coherent

Only then may the runtime advance to Phase 3.

## 15. Forbidden patterns

Phase 2 must not combine any of these in one uncontrolled frame:

- root flip + new policy write
- root flip + cached reset
- root flip + posture reseed
- root flip + distal topology expansion
- root flip + shell correction
- root flip + locomotion entry

## 16. Failure classification

### 16.1 Retryable failures

- `phase2_root_not_confirmed`
- `phase2_topology_not_preserved`
- `phase2_guard_window_interrupted_by_transient_contamination`

### 16.2 Non-retryable failures

- `phase2_policy_write_leak`
- `phase2_reset_violation`
- `phase2_same_frame_conflicting_authority`
- `phase2_no_convergence_path`

### 16.3 Abort-level failures

- `phase2_root_on_spike`
- `phase2_root_simulation_dropped`
- `phase2_shell_correction_material`
- `phase2_fail_stop_precursor`

## 17. Recovery

After Phase 2 failure, recovery must:

- disable pelvis/root simulation if Phase 2 enabled it
- restore intended `BridgeActive` topology
- clear transition-local suppression
- clear guard-window state
- clear spike counters and hazard flags
- clear any stale reset state
- hand shell/capsule authority back coherently

Recovery must not leave the runtime in a half-root-on state.

## 18. Automatic retry

Automatic retry is allowed only if all are true:

- failure class is retryable
- recovery completed
- `BridgeActive` is coherent again
- something material changed
- fresh BridgeActive quiet proof occurred
- cooldown elapsed
- retry budget not exceeded

Repeated unchanged `phase2_root_on_spike` failures must not brute-force through automatic retries.

## 19. Logging contract

Required one-shot logs:

- `PHASE2_ENTRY`
- `PHASE2_DENIED <reason>`
- `PHASE2_ROOT_ON`
- `PHASE2_GUARD_WINDOW_STARTED`
- `PHASE2_GUARD_WINDOW_ABORTED`
- `PHASE2_READY_FOR_PHASE3`
- `PHASE2_ABORT <reason>`
- `PHASE2_RECOVERY_BEGIN`
- `PHASE2_RECOVERY_COMPLETE`

Required summary fields:

- root pre/post linear speed
- root pre/post angular speed
- shell offset delta
- shell velocity delta
- sim counts pre/post
- max target delta
- mean target delta
- policy suppression state
- reset state
- shell authority mode
- shell proof duration
- whether shell reference was re-anchored
- whether shell reference was reseeded

## 20. Required threshold names

Minimum named thresholds:

- `Phase2PreRootOnShellProofRequiredSeconds`
- `Phase2PreRootOnShellProofMaxOffsetDeltaCm`
- `Phase2PreRootOnShellProofMaxVelocityDeltaCmPerSec`
- `Phase2PreRootOnShellProofMaxOffsetGrowthCm`
- `Phase2PreRootOnShellProofMaxVelocityGrowthCmPerSec`
- `Phase2EntryMaxRootLinearSpeedCmPerSec`
- `Phase2EntryMaxRootAngularSpeedDegPerSec`
- `Phase2EntryMaxShellOffsetDeltaCm`
- `Phase2EntryMaxShellVelocityDeltaCmPerSec`
- `Phase2EntryMaxTargetDeltaDeg`
- `Phase2EntryMeanTargetDeltaDeg`
- `Phase2GuardWindowSeconds`
- `Phase2AbortRootLinearSpeedCmPerSec`
- `Phase2AbortRootAngularSpeedDegPerSec`
- `Phase2AbortShellOffsetDeltaCm`
- `Phase2AbortShellVelocityDeltaCmPerSec`
- `Phase2AbortMaxBodyLinearSpeedCmPerSec`
- `Phase2AbortMaxBodyAngularSpeedDegPerSec`
- `Phase2MaxAutomaticRetries`
- `Phase2RetryCooldownSeconds`

## 21. Acceptance criteria

This spec is satisfied only when Phase 2 performs a true warm-start root-on from a still-valid certified handoff, can deny safely before root-on, and does not depend on hidden same-frame assistance or brute-force retry loops to succeed.
