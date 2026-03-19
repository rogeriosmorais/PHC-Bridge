# Balance Mode Phase 1 Stabilization Spec

Status: Authoritative implementation spec
Scope: Stage 1 runtime stabilization behavior for `BalanceTransition_Phase1_Prepare` and `BalanceTransition_Phase1_LateValidate`
Audience: runtime, controls, debugging, validation, and transition work for PhysAnim bridge

## 1. Purpose

This document defines the concrete stabilization recipe for Phase 1 of the Balance Mode entry transition.

It is authoritative for:

- body sets
- target topology
- policy suppression
- hold-reference behavior
- quiet proof
- late-validation sustain
- shell-lock requirements for the true success path
- failure classes
- recovery and retry rules

## 2. Relationship to other docs

This document works with:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase2.md`

Interpretation rule:

- the entry spec defines the state machine and overall transition contract
- this document defines exactly how Phase 1 is allowed to converge
- the Phase 2 spec defines root-on and the guard-window contract

## 3. Core goal

Phase 1 must transform the runtime from normal `BridgeActive` into a quiet, transition-safe pre-root-on state.

Phase 1 is successful only if it creates a state from which Phase 2 can either:

- deny safely
- or truthfully attempt root-on

Phase 1 is not a passive wait state.

It is an active topology-and-authority shaping phase.

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

## 5. Valid handoff topologies

Phase 1 must converge to one explicitly named handoff topology.

### 5.1 `UpperOnlySafeDenyHandoff`

Required movement types:

- `pelvis` = kinematic
- proximal set = kinematic
- distal set = kinematic
- upper body = one explicit documented ownership mode

This topology is valid for safe denial.
It is not root-on-ready.

### 5.2 `RootCoupledReadyHandoff`

Required movement types:

- `pelvis` = kinematic
- proximal set = simulated
- distal set = kinematic
- upper body = one explicit documented ownership mode that remains unchanged through root-on

Recommended initial ownership numbers for the first success path:

- `simCount = 9`
- `proximalSimCount = 5`
- `distalSimCount = 0`
- `upperBodySimCount = 4`

This is the first topology class that may truthfully permit Phase 2 root-on.

## 6. Topology consistency rule

Phase 1 must not certify readiness under one topology description while implementation depends on another.

Not allowed:

- claiming lower-body kinematic quiet while depending on preserved lower-body sim coverage
- emitting “ready” with no coherent topology counts
- relying on incidental runtime drift

## 7. Authority matrix

### 7.1 Policy authority

During Phase 1:

- policy inference may continue for diagnostics
- policy writes to `pelvis` are suppressed
- policy writes to the proximal set are suppressed
- policy writes to the distal set are suppressed

Default recommended behavior:

- suppress policy writes globally during Phase 1 unless a narrower routing contract is already proven

### 7.2 Control target authority

Control targets may be used only to preserve posture without creating a new discontinuity.

Allowed:

- hold current pose
- maintain bounded hold offsets relative to the entry reference

Not allowed:

- retargeting to live locomotion animation
- aggressive fresh targets from a moving reference
- same-frame re-enable plus reference change

### 7.3 Body modifier authority

Body modifiers are the authoritative owners of Phase 1 topology shaping.

Phase 1 must explicitly force the documented topology.
It must not passively hope that startup bring-up drifts there.

### 7.4 Cached reset authority

Phase 1 treats cached-target resets as hazardous.

Allowed:

- at most one bounded explicit non-root reset before quiet accumulation begins, if documented and transition-owned

Forbidden:

- pelvis/root cached reset
- repeated resets
- any reset after quiet accumulation starts
- undocumented reset discharge

### 7.5 Shell / CharacterMovement authority

Phase 1 must not use shell or movement correction as a hidden stabilizer.

Allowed shell authority modes:

- `GameplayShellObservedOnly`
- `TransitionOwnedShellLocked`

Interpretation rule:

- `GameplayShellObservedOnly` is compatible with safe denial
- `TransitionOwnedShellLocked` is required for the first true root-on success path

## 8. Phase 1 entry actions

On Phase 1 entry, perform exactly once:

1. freeze further balance-start attempts
2. disable perturbation scheduling
3. snapshot baseline metrics
4. apply transition-owned suppression
5. force the target topology
6. capture a single hold reference
7. clear transition-local timers and hazard state

## 9. Hold-reference contract

Phase 1 posture preservation uses a single authoritative hold reference.

Default rule:

- capture the current skeletal pose once on Phase 1 entry
- use that as the hold reference for kinematic bodies in the transition set

Not allowed:

- continuously reseeding the hold pose
- chasing live animation
- moving-reference hold logic during Phase 1

## 10. Quiet proof

Phase 1 requires an explicit quiet proof.

Required proof object:

- `Phase1QuietProof`

Required owner:

- `BalanceQuietProofAccumulator`

### 10.1 Quiet conditions

Phase 1 is quiet only if all remain true continuously:

- target topology correct
- policy suppression active
- control-authority settled
- root linear speed below threshold
- root angular speed below threshold
- shell offset delta below threshold
- shell velocity delta below threshold
- max target delta below threshold
- mean target delta below threshold
- no fail-stop precursor
- no pending cached reset
- no topology flip pending
- no quarantine release pending

### 10.2 Quiet duration

Required minimum:

- `BalanceModeQuietRequiredSeconds = 1.0`

The accumulator resets to zero whenever any quiet condition becomes false.

No carryover is allowed.

## 11. Late-validation sustain

Quiet proof alone is not enough.

After quiet proof succeeds, Phase 1 must complete late validation.

Required proof object:

- `Phase1LateValidateProof`

Late validation succeeds only if all remain true continuously for the named sustain duration:

- documented handoff topology remains intact
- upper-body ownership mode remains unchanged
- sim coverage remains within the documented envelope
- target continuity remains within late-validation bounds
- no cached reset becomes pending
- no topology flip becomes pending
- no hold-reference reseed occurs
- if on the true success path, transition-owned shell lock remains coherent
- startup/gameplay ownership does not reclaim shell/capsule authority

## 12. Shell-lock requirement for the true success path

If Phase 1 intends to produce `RootCoupledReadyHandoff`, it must:

- transfer shell authority into `TransitionOwnedShellLocked`
- suppress CharacterMovement corrective motion before the proof window
- suppress capsule/gameplay shell correction before the proof window
- re-anchor the shell reference exactly once before the proof window
- hold that shell reference unchanged through late validation

Phase 1 must emit whether:

- shell lock was transferred
- shell reference was re-anchored
- shell reference was reseeded after lock
- startup/gameplay ownership tried to reclaim shell authority

## 13. Phase 1 success payload

Phase 1 success requires emission of the certified handoff payload containing at minimum:

- handoff topology classification
- `simCount`
- `proximalSimCount`
- `distalSimCount`
- policy suppression state
- control-authority settled state
- max target delta
- mean target delta
- quiet proof duration
- late-validation sustain duration
- upper-body ownership mode
- upper-body stability summary
- shell authority mode
- whether shell lock was coherent
- whether shell reference was re-anchored
- whether shell reference was reseeded
- whether startup/gameplay ownership remained suppressed

## 14. Exit criteria

Phase 1 may exit successfully only if all are true:

- documented target topology achieved
- policy suppression holds
- control-authority settled
- target continuity within named bounds
- no reset pending
- no topology flip pending
- no quarantine release pending
- quiet proof completed
- late validation completed
- no fail-stop precursor active
- no transition-local hazard active

Interpretation rule:

- `UpperOnlySafeDenyHandoff` may exit as a valid Phase 1 success state for safe denial
- `RootCoupledReadyHandoff` may exit as a valid Phase 1 success state for Phase 2 consideration

## 15. Handoff invalidation

If the certified handoff regresses after Phase 1 success but before or during Phase 2 entry, the runtime must invalidate readiness.

Regression examples:

- topology mismatch
- sim coverage mismatch
- policy suppression regression
- target continuity regression
- reset pending
- topology flip pending
- shell lock released
- shell reference reseeded
- startup/gameplay ownership returns

## 16. Failure classification

### 16.1 Retryable failures

- `phase1_topology_not_achieved`
- `phase1_pending_reset_not_discharged`
- `phase1_quiet_window_interrupted_by_contamination`
- `phase1_quarantine_not_settled`
- `phase1_late_validate_sim_coverage_regressed`
- `phase1_late_validate_upper_body_unstable`

### 16.2 Non-retryable failures

- `phase1_root_reset_requested`
- `phase1_policy_write_leak_to_transition_set`
- `phase1_repeated_target_discontinuity`
- `phase1_late_validate_hidden_reset_or_relock`
- `phase1_no_convergence_path`
- `phase1_missing_required_modifier_or_control`

### 16.3 Abort-level failures

- `phase1_baseline_movement_too_high`
- `phase1_fail_stop_precursor`
- `phase1_simulation_explosion`
- `phase1_shell_correction_material`

## 17. Recovery

After Phase 1 failure, recovery must:

- stop Phase 1 timers
- clear transition-local suppression
- clear quarantine state
- restore normal `BridgeActive` topology
- clear hold-reference state
- clear hazard flags
- restore BridgeActive policy routing
- return to one coherent state only:
  - `BridgeActive`
  - or `BalanceTransitionFailed`

## 18. Automatic retry

Automatic retry is allowed only if all are true:

- failure class is retryable
- recovery made a real topology or authority change
- runtime returned to coherent `BridgeActive`
- fresh BridgeActive quiet proof occurred
- retry budget not exceeded

## 19. Logging contract

Required one-shot logs:

- `PHASE1_ENTRY`
- `PHASE1_TOPOLOGY_TARGET`
- `PHASE1_POLICY_SUPPRESSION`
- `PHASE1_HOLD_REFERENCE_CAPTURED`
- `PHASE1_SHELL_LOCK_TRANSFERRED`
- `PHASE1_QUIET_WINDOW_STARTED`
- `PHASE1_QUIET_WINDOW_RESET`
- `PHASE1_LATE_VALIDATE_STARTED`
- `PHASE1_LATE_VALIDATE_RESET`
- `PHASE1_READY_FOR_PHASE2`
- `PHASE1_REJECTED <reason>`
- `PHASE1_RECOVERY_BEGIN`
- `PHASE1_RECOVERY_COMPLETE`

## 20. Required threshold names

Minimum named thresholds:

- `BalanceModeQuietRequiredSeconds`
- `Phase1QuietRootLinearSpeedCmPerSec`
- `Phase1QuietRootAngularSpeedDegPerSec`
- `Phase1QuietShellOffsetDeltaCm`
- `Phase1QuietShellVelocityDeltaCmPerSec`
- `Phase1MaxEntryTargetDeltaDeg`
- `Phase1QuietMaxTargetDeltaDeg`
- `Phase1QuietMeanTargetDeltaDeg`
- `Phase1LateValidateRequiredSeconds`
- `Phase1LateValidateMaxTargetDeltaDeg`
- `Phase1LateValidateMeanTargetDeltaDeg`
- `Phase1LateValidateMaxUpperBodyAngularSpeedDegPerSec`
- `Phase1LateValidateMaxUpperBodyLinearSpeedCmPerSec`
- `Phase1MaxAutomaticRetries`
- `Phase1RetryCooldownSeconds`

## 21. Acceptance criteria

This spec is satisfied only when Phase 1 reaches readiness through the documented topology-and-authority shaping path, not through incidental drift or brute-force retry loops.