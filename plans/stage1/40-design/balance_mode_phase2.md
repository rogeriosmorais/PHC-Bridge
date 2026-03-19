# Balance Mode Phase 2 Root-On Spec

Status: Draft implementation spec  
Scope: Stage 1 runtime behavior for `BalanceTransition_Phase2_RootOn` and the immediate post-root-on guard window  
Audience: runtime, controls, debugging, validation, and transition work for PhysAnim bridge

## 1. Purpose

This document defines the **exact root-on choreography** for Phase 2 of the Balance Mode entry transition.

It exists because the entry-transition spec already defines:

- that pelvis/root simulation must become true
- that same-frame policy drive is dangerous
- that root-on spikes must abort the transition
- that recovery must return to coherent `BridgeActive`

But it does **not** define, with enough precision, how the root-on flip should be executed frame-by-frame.

This document closes that gap.

Its goal is to remove ambiguity around:

- the exact order of operations for root-on
- which body sets may change on the root-on frame
- whether policy writes are allowed on the root-on frame
- whether cached-target resets are allowed on the root-on frame
- how long the post-root-on guard window lasts
- what counts as a root-on spike
- what conditions are strong enough to deny Phase 2 before root-on
- what recovery must do after `phase2_root_on_spike`
- when retry is allowed and when it is prohibited

---

## 2. Relationship to Existing Docs

This document refines and operationalizes:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`
- `plans/stage1/40-design/balance-perturbation-mode-design.md`

Interpretation rule:

- the entry-transition spec defines the state machine and high-level transition contract
- the Phase 1 stabilization spec defines how the pre-root-on state becomes quiet and safe
- this document defines how the runtime executes the **actual root-on flip** and the immediate post-flip guard window

This document does not replace those docs.

---

## 3. Core Design Goal

Phase 2 must convert the runtime from a **quiet, transition-safe pre-root-on state** into a **real pelvis/root-simulating state** without introducing an uncontrolled spike.

Phase 2 is successful only if:

- pelvis/root simulation becomes true
- no conflicting authority interferes on the root-on frame
- no same-frame discontinuity is injected by target writes, resets, or shell correction
- the immediate post-root-on window remains within named spike thresholds

Phase 2 is not a broad settle phase.  
It is a short, tightly controlled **state-flip and guard-window phase**.

---

## 4. Non-Goals

Phase 2 does not:

- establish full Balance Perturbation Mode behavior
- schedule perturbations
- validate recovery from pushes
- test locomotion
- replace Phase 3 bounded settle logic

Phase 2 only governs:

- the root-on flip
- the immediate authority constraints around that flip
- the short guard window after the flip
- abort classification and recovery if the flip is unstable

---

## 5. Authoritative Bone Sets

The same authoritative sets from the Phase 1 stabilization spec apply.

## 5.1 Root set
- `pelvis`

## 5.2 Proximal transition-critical set
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

## 5.3 Distal spike-prone lower-limb set
- `calf_l`
- `calf_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

## 5.4 Upper-body non-critical set
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

---

## 6. Required Phase 2 Entry Preconditions

Phase 2 may begin only if all of these are already true:

1. Phase 1 succeeded under the Phase 1 stabilization spec
2. transition-critical topology matches the Phase 1 target topology
3. policy suppression for the transition-critical set is active
4. no cached-target reset is pending
5. no quarantine release is pending
6. no fail-stop precursor is active
7. root linear speed is below `Phase2EntryMaxRootLinearSpeedCmPerSec`
8. root angular speed is below `Phase2EntryMaxRootAngularSpeedDegPerSec`
9. shell offset delta is below `Phase2EntryMaxShellOffsetDeltaCm`
10. shell velocity delta is below `Phase2EntryMaxShellVelocityDeltaCmPerSec`
11. certified Phase 1 handoff payload is present and still valid
12. `simCount`, `proximalSimCount`, and `distalSimCount` match the intended handoff topology, including the conservative upper-only late-validation topology if that is the documented Phase 1 handoff mode
13. control-authority settled state matches the certified handoff payload
14. max target delta is below `Phase2EntryMaxTargetDeltaDeg`
15. mean target delta is below `Phase2EntryMeanTargetDeltaDeg`
16. late-validation sustain duration from Phase 1 completed successfully
17. upper-body ownership mode matches the certified handoff payload
18. upper-body linear/angular stability remains within the late-validation envelope at entry

Interpretation rule:

- successful late validation is not, by itself, permission to root-on
- successful late validation minimum is not, by itself, permission to root-on
- `UpperOnlySafeDenyHandoff` is a valid safe-denial-capable Phase 1 success state, not a root-on-ready state
- `RootCoupledReadyHandoff` is the first documented topology class that may permit Phase 2 root-on
- Phase 2 entry must also require an explicit pre-root-on shell-correction safety proof
- a readiness proof that cannot rule out immediate `phase2_shell_correction_material` is incomplete and must not permit `BRT_Phase2_RootOn`

Phase 2 must not be entered “optimistically.”

If these conditions are not true, the runtime must remain in Phase 1 or fail.

### 6.1 Entry denial path

Phase 2 must have an explicit denial path before any root-on attempt occurs.

If a required Phase 2 entry precondition is false, the runtime must emit:

- `PHASE2_DENIED <reason>`

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

Interpretation rule:

- `phase2_sim_coverage_regressed` means the runtime regressed away from the documented certified handoff topology
- it must not be emitted merely because proximal/distal simulation counts are zero in the conservative upper-only late-validation topology

Denial is a safe no-root-on outcome.  
It is not a root-on failure and must not be logged as one.

### 6.2 Pre-root-on shell-correction safety proof

Before Phase 2 may begin, the runtime must have an explicit proof that the root-on attempt is not expected to immediately fail on shell correction.

Minimum contract:

- the proof must be evaluated before `SetPhase(BRT_Phase2_RootOn)`
- the proof must be part of Phase 2 entry validation, not a later Phase 2 guard-window discovery
- if the proof is absent, false, or unknown, Phase 2 must deny safely before root-on

Not allowed:

- permitting `BRT_Phase2_RootOn` based only on shell-hold duration, policy settle, and bring-up control settle
- discovering that root-on was unsafe only after root-on has already begun when that risk could have been classified as missing proof at entry

Interpretation rule:

- `phase2_shell_correction_material` remains a valid post-entry abort reason if a root-on attempt still fails despite the best available proof
- but if the current design cannot produce a truthful pre-root-on shell-correction safety proof, the runtime must deny with `phase2_pre_root_on_shell_correction_safety_not_proven` instead of attempting root-on

### 6.2.1 `PreRootOnShellSafetyProof`

The required proof is named `PreRootOnShellSafetyProof`.

Its purpose is narrow:

- prove that root-on is not expected to create an immediate pelvis-to-capsule planar separation large enough to trigger deterministic shell correction failure during the Phase 2 guard window

This proof is required for any handoff topology that intends to permit actual root-on.

#### Inputs

The proof must be evaluated from the same actor/capsule and pelvis-root reference frame used by the existing shell diagnostics.

Minimum proof inputs:

- current shell planar offset delta
- current shell planar velocity delta
- shell planar offset delta trend over the proof window
- shell planar velocity delta trend over the proof window
- root planar linear speed
- root angular speed
- certified handoff topology class
- certified proximal/distal/upper-body sim counts

#### Required proof window

The proof must hold continuously for a named duration before Phase 2 begins:

- `Phase2PreRootOnShellProofRequiredSeconds`

No partial carryover is allowed.

#### Pass conditions

`PreRootOnShellSafetyProof` is true only if all of the following remain true continuously for the full proof window:

1. certified handoff topology = `RootCoupledReadyHandoff`
2. `distalSimCount = 0`
3. shell planar offset delta <= `Phase2PreRootOnShellProofMaxOffsetDeltaCm`
4. shell planar velocity delta <= `Phase2PreRootOnShellProofMaxVelocityDeltaCmPerSec`
5. shell planar offset delta trend is non-growing within a named tolerance:
   `Phase2PreRootOnShellProofMaxOffsetGrowthCm`
6. shell planar velocity delta trend is non-growing within a named tolerance:
   `Phase2PreRootOnShellProofMaxVelocityGrowthCmPerSec`
7. root linear speed <= `Phase2EntryMaxRootLinearSpeedCmPerSec`
8. root angular speed <= `Phase2EntryMaxRootAngularSpeedDegPerSec`
9. no locomotion authority activation occurs
10. no shell/capsule corrective movement owner is active
11. no posture reseed, cached reset, or topology expansion becomes pending

Interpretation rule:

- “safe enough” does not mean shell offset must be exactly zero
- it means shell offset is already small, not growing, and not coupled to an active corrective owner immediately before root-on

#### Fail conditions

The proof is false if any of the following are observed:

- shell planar offset delta above `Phase2PreRootOnShellProofMaxOffsetDeltaCm`
- shell planar velocity delta above `Phase2PreRootOnShellProofMaxVelocityDeltaCmPerSec`
- positive shell offset growth above `Phase2PreRootOnShellProofMaxOffsetGrowthCm`
- positive shell velocity growth above `Phase2PreRootOnShellProofMaxVelocityGrowthCmPerSec`
- locomotion authority not idle
- shell/capsule corrective owner active or unknown
- handoff topology, sim coverage, or authority changes during the proof window

#### Logging contract

When the proof is evaluated, Phase 2 entry logging must make it possible to answer:

- whether the proof was evaluated
- how long the proof window actually held
- current shell offset and velocity deltas
- whether shell trends were growing, flat, or shrinking
- whether a shell/capsule corrective owner was active

Recommended fields:

- `shellProofReady`
- `shellProofDuration`
- `shellProofRequiredSeconds`
- `shellOffsetDelta`
- `shellVelocityDelta`
- `shellOffsetGrowth`
- `shellVelocityGrowth`
- `shellCorrectionOwnerActive`

#### Denial rule

If `RootCoupledReadyHandoff` is present but `PreRootOnShellSafetyProof` is false or unknown, Phase 2 must deny with:

- `phase2_pre_root_on_shell_correction_safety_not_proven`

### 6.2.2 Shell authority contract required to make the proof pass

The current runtime evidence shows that `PreRootOnShellSafetyProof` will remain false if normal gameplay shell ownership is left active up to the root-on boundary.

Therefore the design must define an explicit shell-authority mode for the proof window.

This document defines two modes:

- `GameplayShellObservedOnly`
- `TransitionOwnedShellLocked`

Interpretation rule:

- `GameplayShellObservedOnly` is compatible with safe denial
- `TransitionOwnedShellLocked` is the required mode for the first permitted true root-on-success path

#### `GameplayShellObservedOnly`

Behavior:

- existing capsule / CharacterMovement / gameplay shell ownership may remain active
- shell metrics are observed only
- any corrective owner activity forces `PreRootOnShellSafetyProof = false`

This mode is valid for:

- `UpperOnlySafeDenyHandoff`
- `RootCoupledReadyHandoff` safe denial

This mode is not sufficient for:

- true Phase 2 root-on success

#### `TransitionOwnedShellLocked`

Behavior:

- before the shell proof window begins, transition logic takes ownership of the actor/capsule shell state required for root-on
- CharacterMovement corrective motion is disabled
- capsule-driven planar correction is disabled
- bridge/gameplay movement drive is disabled
- the shell reference used by the proof window is re-anchored once to the current pelvis/root state
- after that re-anchor, the shell reference must remain unchanged through the proof window, root-on frame, and Phase 2 guard window

Required invariants while `TransitionOwnedShellLocked` is active:

1. no CharacterMovement movement mode transition that can inject planar correction
2. no capsule relocation or sweep-based correction owned by gameplay movement
3. no bridge-owned shell translation assist
4. no shell-reference reseed after the proof window starts
5. no root-on-frame shell snap

Required logging fields:

- `shellAuthorityMode`
- `shellReferenceReanchored`
- `characterMovementSuppressed`
- `capsuleCorrectionSuppressed`
- `bridgeShellDriveSuppressed`

If any invariant is violated while this mode is active:

- `PreRootOnShellSafetyProof` becomes false
- Phase 2 must deny or abort by class rather than continue optimistically

### 6.3 Root-on-readiness proof for `RootCoupledReadyHandoff`

`RootCoupledReadyHandoff` is root-on-ready only if all of the following are true continuously for a named proof window before `SetPhase(BRT_Phase2_RootOn)`:

1. handoff topology classification = `RootCoupledReadyHandoff`
2. `pelvis=kinematic`, proximal set = simulated, distal set = kinematic
3. `proximalSimCount=5`
4. `distalSimCount=0`
5. `upperBodyOwnership` matches the documented late-validation ownership mode and does not change during the proof window
6. policy suppression for `pelvis`, proximal, and distal sets remains active
7. no cached reset, hold-reference reseed, or quarantine release is pending
8. root linear and angular speeds remain below the named Phase 2 entry thresholds
9. shell offset delta and shell velocity delta remain below the named Phase 2 entry thresholds
10. max and mean target deltas remain below the named Phase 2 entry thresholds
11. the proximal set remains bounded under initial policy influence with no same-window flare large enough to predict immediate root-on contamination
12. no body in the proximal or upper-body sets exceeds the named pre-root-on maximum linear or angular speed thresholds
13. `PreRootOnShellSafetyProof` is true for the same handoff window and remains true at the moment Phase 2 begins
14. `shellAuthorityMode = TransitionOwnedShellLocked`
15. the shell reference was re-anchored exactly once before the proof window and not reseeded afterward

The proof must emit, at minimum:

- `rootOnReady=1`
- `rootOnReadinessClassification=root_coupled_ready`
- `rootOnReadinessGateReason=none`
- the certified topology classification
- `simCount`
- `proximalSimCount`
- `distalSimCount`
- `upperBodySimCount`
- proof-window duration actually achieved
- shell-safety proof duration actually achieved
- shell offset and velocity deltas at proof completion
- shell authority mode at proof completion

If any required field is absent or unstable, Phase 2 must deny safely.

---

## 7. Phase 2 Authority Matrix

Phase 2 must use a stricter authority model than BridgeActive.

## 7.1 Policy authority

Policy inference may continue for diagnostics, but policy must not drive the transition-critical set on the root-on frame.

Required rule:

- policy writes to `pelvis` = forbidden
- policy writes to proximal transition-critical set = forbidden
- policy writes to distal spike-prone lower-limb set = forbidden

Default recommended rule:
- suppress all policy target writes during the root-on frame and through the post-root-on guard window

Policy influence may have been partially restored during Phase 1 late validation, but Phase 2 must treat that as a proof input, not as permission to continue uncontrolled writes through root-on.

## 7.2 Control target authority

Control targets may exist, but they may not inject a new discontinuity on the root-on frame.

Allowed:
- preserve the hold/reference state already established by Phase 1
- hold previously frozen targets unchanged

Forbidden:
- recomputing fresh policy-driven targets on the root-on frame
- reseeding control targets from a different reference on the root-on frame
- enabling a control family and changing its reference in the same uncontrolled step

## 7.3 Body modifier authority

Body modifiers are the authoritative owners of the root-on topology flip.

The runtime must not rely on incidental simulation propagation.  
The pelvis/root body must be explicitly transitioned by the transition logic.

## 7.4 Cached reset authority

Cached-target resets are forbidden on the root-on frame and during the post-root-on guard window.

This includes:
- root reset
- proximal reset
- distal reset
- automatic deferred reset discharge

If a reset is still needed, Phase 2 must not begin.

## 7.5 Shell / CharacterMovement authority

During Phase 2:
- CharacterMovement must not inject corrective motion
- shell/world correction must not inject corrective displacement or velocity
- capsule/shell movement is observed as contamination, not used as a stabilizer

If shell correction occurs materially during Phase 2, the transition must abort.

For the first permitted true root-on-success path:

- the runtime must enter `TransitionOwnedShellLocked` before the shell proof window begins
- that mode must remain active through the full Phase 2 guard window
- returning shell authority to gameplay systems is deferred to Phase 3 or later

---

## 8. Phase 2 Frame Sequence

The runtime must execute Phase 2 in this order.

## 8.1 Phase 2 entry snapshot

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

## 8.2 Freeze transition hazards

Before root-on occurs, the runtime must ensure all are true on the entry frame:

- policy writes suppressed
- cached resets suppressed
- locomotion entry suppressed
- shell assistance suppressed
- CharacterMovement correction suppressed
- `TransitionOwnedShellLocked` already active
- shell reference already re-anchored to current root

## 8.3 Execute root-on

Root-on means:

- pelvis/root body modifier is switched to simulated
- pelvis/root collision state is switched to the intended simulated state
- pelvis/root simulation validity is re-read and confirmed

This must be an explicit transition-owned action.

Hard rule:

- root-on must not be the same frame that shell authority transfers from gameplay to transition ownership

## 8.4 Validate immediate root-on result

Immediately after the flip, confirm:

- pelvis/root body exists
- pelvis/root reports simulating
- no same-frame system turned it back off
- transition-critical suppression is still active
- no reset was scheduled during the flip

If any check fails, abort Phase 2 immediately.

## 8.5 Start post-root-on guard window

Once root-on succeeds technically, Phase 2 enters a bounded guard window.

During this guard window:
- no policy write may be re-enabled for the transition-critical set
- no cached reset may be discharged
- no topology expansion may occur
- no new posture reseed may occur
- no shell correction may materially assist the body

---

## 9. Topology Rules During Phase 2

Phase 2 must not change everything at once.

The topology described below is valid only if it matches the certified Phase 1 handoff contract.

If implementation instead depends on preserved non-root simulation coverage, this section must be revised explicitly rather than bypassed in code.

Current runtime evidence indicates that preserved non-root coverage and upper-body ownership are part of the real handoff contract.
Therefore Phase 2 must not assume that a quiet pre-root-on state is sufficient unless the late-validation sustain already proved those conditions.

## 9.1 Root set
- `pelvis` = simulated

## 9.2 Proximal transition-critical set
Required state during the root-on frame and guard window depends on the certified handoff topology:

- if the certified handoff is `UpperOnlySafeDenyHandoff`, Phase 2 must deny before root-on
- if the certified handoff is `RootCoupledReadyHandoff`, `spine_01`, `spine_02`, `spine_03`, `thigh_l`, `thigh_r` remain simulated through the root-on frame and guard window

Hard rule:

- Phase 2 must not combine root-on with a proximal topology flip
- the proximal state on the root-on frame must match the certified handoff payload consumed at Phase 2 entry

## 9.3 Distal spike-prone lower-limb set
During the root-on frame and guard window:
- `calf_l`, `calf_r`, `foot_l`, `foot_r`, `ball_l`, `ball_r` = kinematic

Phase 2 must not allow distal re-simulation during the guard window.

## 9.4 Upper-body non-critical set
Upper-body topology may remain unchanged from the Phase 1 exit state, provided it does not introduce material root contamination.

If upper-body simulation materially contributes to root-on spike behavior, the design must be revised explicitly.

## 9.5 Shell topology / authority state

The first permitted true root-on-success path requires:

- handoff topology = `RootCoupledReadyHandoff`
- shell authority mode = `TransitionOwnedShellLocked`
- gameplay shell authority = suppressed
- shell reference = re-anchored once before proof, then held fixed through guard window

This is the first non-upper-only handoff topology plus shell-ownership topology that may truthfully permit Phase 2.

---

## 10. Post-Root-On Guard Window

The post-root-on guard window exists to catch deterministic spikes that appear immediately after the flip.

## 10.1 Duration

The runtime must define a named duration:

- `Phase2GuardWindowSeconds`

This window begins immediately after technical root-on success.

## 10.2 Rules during the guard window

The following remain forbidden during the guard window:

- policy target writes to the transition-critical set
- cached-target resets
- transition-topology expansion
- locomotion entry
- shell/world corrective assistance
- CharacterMovement corrective assistance
- root reseed from a new pose reference

## 10.3 Metrics tracked during the guard window

At minimum:

- peak root linear speed
- peak root angular speed
- peak shell offset delta
- peak shell velocity delta
- peak max body linear speed
- peak max body angular speed
- sim count
- distal sim count
- whether root simulation stayed true

Optional:
- proximal-set max velocity summary
- distal-set max velocity summary

---

## 11. Root-On Spike Definition

A Phase 2 spike is not a vague visual impression.

It must be defined using named thresholds.

A `phase2_root_on_spike` occurs if any of these exceed their abort threshold during the root-on frame or guard window:

- root linear speed > `Phase2AbortRootLinearSpeedCmPerSec`
- root angular speed > `Phase2AbortRootAngularSpeedDegPerSec`
- shell offset delta > `Phase2AbortShellOffsetDeltaCm`
- shell velocity delta > `Phase2AbortShellVelocityDeltaCmPerSec`
- max body linear speed > `Phase2AbortMaxBodyLinearSpeedCmPerSec`
- max body angular speed > `Phase2AbortMaxBodyAngularSpeedDegPerSec`

Additional hard abort reasons:

- root simulation drops unexpectedly
- cached reset occurs
- policy write leak to transition-critical set occurs
- topology expands unexpectedly
- fail-stop precursor becomes active

---

## 12. Root-On Success Criteria

Phase 2 succeeds only if all are true for the full guard window:

1. pelvis/root remains simulating
2. no abort threshold is exceeded
3. no shell/material contamination occurs
4. no policy write leak occurs
5. no reset occurs
6. no unexpected topology expansion occurs

Only then may the runtime advance to Phase 3.

---

## 13. Forbidden Root-On Patterns

The runtime must not do any of the following in one uncontrolled frame:

- root flip + new policy write
- root flip + cached reset
- root flip + posture reseed
- root flip + topology expansion of distal bodies
- root flip + shell correction
- root flip + locomotion entry

If any implementation depends on one of these combined patterns to succeed, the design is not yet valid and must be rewritten explicitly.

---

## 14. Failure Classification

Phase 2 failure must be classified.

## 14.1 Retryable failure classes

These may retry automatically only if recovery changes something material and the retry rules are satisfied:

- `phase2_root_not_confirmed`
- `phase2_topology_not_preserved`
- `phase2_guard_window_interrupted_by_transient_contamination`

## 14.2 Non-retryable failure classes

These should block automatic retry until code/design changes or explicit user action:

- `phase2_policy_write_leak`
- `phase2_reset_violation`
- `phase2_same_frame_conflicting_authority`
- `phase2_no_convergence_path`

## 14.3 Abort-level failure classes

These are immediate transition failures:

- `phase2_root_on_spike`
- `phase2_root_simulation_dropped`
- `phase2_shell_correction_material`
- `phase2_fail_stop_precursor`

---

## 15. Recovery Contract After Phase 2 Failure

Recovery must return the runtime to a coherent `BridgeActive` state.

Required recovery actions:

- disable pelvis/root simulation if Phase 2 enabled it
- restore the intended BridgeActive topology
- clear transition-local suppressions
- clear transition-local guard window state
- clear transition-local snapshots and spike counters
- clear transition-local hazard flags
- ensure no cached reset remains armed from the failed attempt
- ensure the runtime is not left in a half-root-on state

Not allowed:
- remaining in a partially simulated post-root-on topology while claiming recovery
- leaving pelvis sim on and simply looping back into Phase 1
- preserving a pending request when no new convergence evidence exists

---

## 16. Automatic Retry Rule

A failed Phase 2 attempt must not immediately recycle into another attempt merely because a request is still pending.

Automatic retry is permitted only if all are true:

1. previous failure class is marked retryable
2. recovery completed and restored coherent `BridgeActive`
3. recovery changed something material about the state or the convergence path
4. a fresh BridgeActive quiet proof occurred after recovery
5. retry cooldown elapsed
6. retry budget not exceeded

Recommended controls:

- `Phase2MaxAutomaticRetries`
- `Phase2RetryCooldownSeconds`

Repeated `phase2_root_on_spike` failures with unchanged setup must not be brute-forced through retries.  
That pattern is evidence of a root-on design defect.

---

## 17. Logging Contract

Phase 2 logs must be sparse, one-shot, and phase-authoritative.

Required logs:

- `PHASE2_ENTRY`
- `PHASE2_ROOT_ON`
- `PHASE2_GUARD_WINDOW_STARTED`
- `PHASE2_GUARD_WINDOW_ABORTED`
- `PHASE2_READY_FOR_PHASE3`
- `PHASE2_ABORT <reason>`
- `PHASE2_RECOVERY_BEGIN`
- `PHASE2_RECOVERY_COMPLETE`

Recommended root-on summary fields:

- rootPreLin
- rootPreAng
- rootPostLin
- rootPostAng
- shellOffsetDelta
- shellVelocityDelta
- simCountPre
- proximalSimCountPre
- simCountPost
- distalSimPre
- distalSimPost
- maxTargetDeltaPre
- meanTargetDeltaPre
- policySuppressed
- resetScheduled

Required retry-loop logs:
- why retry is allowed or denied
- what changed since the prior failure
- whether fresh BridgeActive quiet proof was re-established
- remaining retry budget

---

## 18. Required Threshold Names

Do not bury these as unnamed constants.

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

---

## 19. Acceptance Tests

Minimum required tests:

### Test 1: Clean root-on
- Phase 1 succeeds
- `PreRootOnShellSafetyProof` succeeds
- root-on occurs
- guard window completes
- Phase 3 begins

### Test 1b: Denied root-on due to invalidated handoff
- Phase 1 succeeds
- certified handoff payload regresses before Phase 2
- verify `PHASE2_DENIED phase2_handoff_invalidated`
- verify no root-on attempt occurs

### Test 1c: Denied root-on due to missing late-validation sustain
- Phase 1 quiet window succeeds
- late-validation sustain does not complete or regresses
- verify `PHASE2_DENIED phase2_late_validate_not_completed`
- verify no root-on attempt occurs

### Test 2: Same-frame policy leak
- intentionally enable policy write on root-on frame
- verify `phase2_policy_write_leak`

### Test 3: Reset violation
- intentionally discharge a cached reset on root-on frame
- verify `phase2_reset_violation`

### Test 4: Distal re-sim propagation
- allow distal bodies to re-enter simulation during guard window
- verify `phase2_topology_not_preserved` or spike abort

### Test 5: Root-on spike
- reproduce deterministic post-root-on velocity spike
- verify `phase2_root_on_spike`
- verify coherent recovery to BridgeActive

### Test 5b: Target discontinuity denial
- hold root motion quiet but inject excessive target delta before root-on
- verify `PHASE2_DENIED phase2_target_discontinuity_too_high`
- verify no root-on attempt occurs

### Test 5c: Shell-safety proof denial
- reach `RootCoupledReadyHandoff`
- leave shell offset or shell growth above the proof threshold
- verify `PHASE2_DENIED phase2_pre_root_on_shell_correction_safety_not_proven`
- verify no root-on attempt occurs

### Test 6: Bad retry loop prevention
- fail Phase 2 with unchanged setup
- verify immediate automatic retry is denied

### Test 7: Legitimate retry
- fail Phase 2 for a retryable reason
- recovery changes material state
- fresh BridgeActive quiet proof occurs
- retry allowed within budget

---

## 20. Recommended Default Design Decision

The recommended default implementation is:

1. Enter Phase 2 only after full Phase 1 quiet proof
2. Require a valid certified handoff payload at the moment Phase 2 begins
3. Require completed Phase 1 late-validation sustain, not just pre-policy quietness
4. Require `TransitionOwnedShellLocked` and a passing `PreRootOnShellSafetyProof`
5. Deny Phase 2 safely if sim coverage, suppression state, target continuity, upper-body stability, or shell authority proof regressed
6. Freeze policy writes, resets, locomotion, and shell assistance
7. Flip only `pelvis` to simulated
8. Keep proximal set simulated and distal set kinematic through the guard window
9. Forbid resets, shell reseeds, and policy writes for the entire guard window
10. Abort immediately on threshold breach
11. Recover fully to BridgeActive
12. Deny automatic retry after repeated unchanged root-on spikes

This is intentionally conservative.

It is better to prove a narrow, stable root-on design first than to combine multiple topology and authority changes in one fragile step.

---

## 21. Final Design Summary

Phase 2 is not just “turn pelvis sim on.”

It is a tightly controlled root-on flip with:

- explicit preconditions
- explicit authority suppression
- explicit topology restrictions
- explicit guard-window rules
- explicit spike thresholds
- explicit recovery rules
- explicit retry prohibitions

The runtime should be considered non-compliant with the Balance Mode entry contract if Phase 2 can only succeed through repeated retries, hidden same-frame writes, or uncontrolled post-root-on propagation.
