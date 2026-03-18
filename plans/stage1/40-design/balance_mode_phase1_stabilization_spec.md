# Balance Mode Phase 1 Stabilization Spec

Status: Draft implementation spec  
Scope: Stage 1 runtime stabilization behavior for `BalanceTransition_Phase1_Prepare`  
Audience: runtime, controls, debugging, validation, and transition work for PhysAnim bridge

## 1. Purpose

This document defines the **concrete stabilization recipe** for Phase 1 of the Balance Mode entry transition.

It exists because the entry-transition spec already defines:

- state names
- ownership rules
- queueing behavior
- phase boundaries
- success / failure categories

But it does **not** define, with enough precision, how Phase 1 is supposed to actually converge in code.

This document closes that gap.

Its goal is to remove ambiguity around:

- which bodies are transition-critical
- which bodies are kinematic vs simulated during Phase 1
- which systems are allowed to write authority during Phase 1
- what “preserve posture” means in practice
- which resets are allowed or forbidden
- how the quiet window is measured
- what constitutes a retryable vs non-retryable Phase 1 failure

---

## 2. Relationship to Existing Docs

This document refines and operationalizes:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance-perturbation-mode-design.md`

It does not replace them.

Interpretation rule:

- the entry-transition spec defines **what Phase 1 must achieve**
- this document defines **how Phase 1 is allowed to achieve it**

If this document conflicts with implementation experiments, this document should be treated as the intended contract unless explicitly revised.

---

## 3. Core Design Goal

Phase 1 must transform the runtime from a normal `BridgeActive` standing state into a **quiet, transition-safe pre-root-on state**.

Phase 1 is successful only if it creates a state where enabling pelvis/root simulation in Phase 2 is no longer expected to create an uncontrolled spike.

This means Phase 1 is not merely a waiting period.

It is an **active topology-and-authority shaping phase**.

---

## 4. Non-Goals

Phase 1 does not:

- activate Balance Perturbation Mode
- apply perturbations
- validate recovery performance
- test locomotion
- prove shell/world isolation for the full active mode
- solve ramp / slope / travel behavior

Phase 1 only prepares a safe entry into root-enabled standing-balance diagnostics.

---

## 5. Authoritative Bone Sets

The implementation must stop using vague categories such as “some distal bodies” or “transition-critical-ish set”.

The following sets are authoritative.

## 5.1 Root set

- `pelvis`

This is the root-on target in Phase 2.

## 5.2 Proximal transition-critical set

- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

These bodies are the highest-risk coupling set around the pelvis/root transition.

## 5.3 Distal spike-prone lower-limb set

- `calf_l`
- `calf_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

These bodies are the most likely to produce explosive propagation or target discontinuity during root-on.

## 5.4 Upper-body non-critical set for Phase 1

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

These bodies are not allowed to destabilize Phase 1, but they are not the primary source of root-on safety.

---

## 6. Phase 1 Target Topology

Phase 1 must converge to exactly this topology before Phase 2 may begin.

## 6.1 Required movement types

### Root set
- `pelvis` = **kinematic**

### Proximal transition-critical set
- `spine_01`, `spine_02`, `spine_03`, `thigh_l`, `thigh_r` = **kinematic**

### Distal spike-prone lower-limb set
- `calf_l`, `calf_r`, `foot_l`, `foot_r`, `ball_l`, `ball_r` = **kinematic**

### Upper-body non-critical set
- may remain in their BridgeActive configuration
- but any member of this set that is simulating and causing material instability must be forced kinematic by Phase 1 recovery logic

## 6.2 Rationale

Phase 2 is root-on.

Therefore Phase 1 must remove as many same-frame and near-root coupling surprises as possible.

The default safe shape is:

- pelvis off
- pelvis-adjacent bodies kinematic
- distal lower-limb spike sources kinematic
- no lower-body sim propagation active

This is intentionally conservative.

If later testing proves a less restrictive topology is stable, the design can be revised explicitly. That change must be documented, not improvised.

---

## 7. Authority Matrix for Phase 1

Phase 1 must enforce a strict ownership model.

## 7.1 Policy authority

Policy inference may continue for diagnostics, but policy may **not** drive the transition-critical set.

Required rule:

- policy influence to `pelvis` = 0
- policy influence to proximal transition-critical set = 0
- policy influence to distal spike-prone lower-limb set = 0

Upper-body policy writes may continue only if they do not create measurable root contamination.  
Default implementation should suppress policy target writes globally during Phase 1 unless there is a strong reason not to.

## 7.2 Control target authority

Control targets may be used only as a posture-preservation mechanism.

They may not introduce a new target discontinuity.

Allowed:
- hold current pose
- maintain bounded current-pose-relative target orientation
- freeze target deltas on entry

Not allowed:
- recomputing aggressive fresh targets from a moving reference every frame
- re-enabling target drives on the same frame as a topology flip without a documented reason

## 7.3 Body modifier authority

Body modifiers are the authoritative owners of Phase 1 topology shaping.

Phase 1 must explicitly force the required Phase 1 movement types rather than waiting for incidental bring-up logic to drift there.

## 7.4 Cached reset authority

Phase 1 must treat cached-target resets as hazardous.

Allowed:
- at most one bounded, explicit, transition-owned reset event for non-root bodies **before** the quiet window begins

Forbidden:
- pelvis/root cached reset in Phase 1
- repeated resets every tick
- any reset after the quiet window has started accumulating
- any automatic reset whose owner is unclear

## 7.5 Shell / CharacterMovement authority

During Phase 1:
- CharacterMovement must not provide corrective locomotion assistance
- shell/world translation must not be used to manufacture quietness
- capsule / shell contamination is measured, not used as a stabilizer

If shell motion is unavoidable, it is allowed only as observed contamination and must reset the quiet window.

---

## 8. Phase 1 Entry Actions

On entry into Phase 1, the runtime must perform these actions exactly once.

## 8.1 Freeze further balance-start attempts
- reject or defer any additional promotion attempts
- do not nest transitions

## 8.2 Disable perturbation scheduling
- no scenario trigger logic may run

## 8.3 Snapshot entry baseline
Capture:
- current runtime state
- current body sim topology
- policy influence alpha
- root linear velocity
- root angular velocity
- shell offset delta
- shell velocity delta
- number of simulating bodies
- number of distal sim bodies

## 8.4 Apply transition-owned suppression
- suppress policy writes to the transition-critical set
- suppress any bridge-owned movement drive
- suppress locomotion entry

## 8.5 Force target topology
- force root, proximal, and distal transition sets to the Phase 1 kinematic topology

## 8.6 Seed posture-hold reference
The implementation must capture a **single Phase 1 hold pose** at entry.

This pose is the authoritative reference for posture preservation during Phase 1.

It must be one of:
- the current physical pose if the body is already near quiet
- otherwise the current skeletal pose

The implementation must choose one rule and log which rule it used.

Default recommended rule:
- sample the current skeletal pose once on Phase 1 entry and use that as the hold reference for all kinematic bodies in the transition set

## 8.7 Clear transition-hazard timers
- clear fail-stop precursor accumulation used for the prior attempt
- clear transition-local retry counters
- clear quiet-window accumulator
- clear settle-window accumulator
- clear any “first policy frame” or “just promoted” flags that could create one-frame target discontinuities

---

## 9. Posture Preservation Contract

The phrase “preserve gross posture” must be made operational.

Phase 1 posture preservation means:

- keep the transition-critical set visually close to the entry pose
- do not inject new target deltas larger than necessary
- prefer pose hold over pose correction

## 9.1 Required method

For every body in the transition-critical set, the implementation must use a **hold-to-entry-reference** strategy.

That means:
- capture an entry reference once
- continue writing either zero offset or bounded hold offsets relative to that same reference
- do not chase live animation changes during Phase 1

## 9.2 Forbidden methods

Do not during Phase 1:
- continuously retarget to live locomotion animation
- continuously reseed from a moving physics pose
- combine skeletal retargeting, cached reset, and topology shaping in the same uncontrolled loop

## 9.3 Bounded target continuity rule

On the first frame of Phase 1, target discontinuity for any transition-critical control must be bounded.

Recommended design invariant:
- no target delta above a named `Phase1MaxEntryTargetDeltaDeg` threshold

If exceeded:
- Phase 1 must not start the quiet window
- the transition must move into recovery classification, not pretend to be stable

---

## 10. Quiet-Window Contract

The quiet window is the core proof that Phase 1 succeeded.

It must not be vague.

## 10.1 Metrics used

The Phase 1 quiet window must be measured from:

- root linear speed
- root angular speed
- shell offset delta
- shell velocity delta
- transition-topology correctness
- fail-stop precursor state

Optional additional metrics:
- max body angular speed
- max body linear speed

## 10.2 Frame of measurement

Root and body motion must be measured in the same runtime frame used by instability checks.  
Shell contamination must be measured relative to the actor/capsule reference used by existing shell diagnostics.

The implementation must not mix incompatible frames silently.

## 10.3 Required quiet conditions

Phase 1 is quiet only if all are true continuously:

- root set topology correct
- proximal transition-critical set topology correct
- distal spike-prone set topology correct
- policy suppression to transition-critical set active
- root linear speed <= `Phase1QuietRootLinearSpeedCmPerSec`
- root angular speed <= `Phase1QuietRootAngularSpeedDegPerSec`
- shell offset delta <= `Phase1QuietShellOffsetDeltaCm`
- shell velocity delta <= `Phase1QuietShellVelocityDeltaCmPerSec`
- no fail-stop precursor active
- no pending cached reset
- no topology flip pending
- no quarantine-release event pending

## 10.4 Quiet hold duration

The quiet window must accumulate continuously for at least:

- `Phase1QuietRequiredSeconds`

If any quiet condition becomes false:
- the accumulator resets to zero

No partial carryover is allowed.

---

## 11. Root Reset Rule

This must be explicit because it is a major source of implementation churn.

### Hard rule
Pelvis/root cached reset is **forbidden** in Phase 1.

Reason:
- Phase 1 exists to establish a quiet pre-root-on baseline
- a root reset is itself a discontinuity and invalidates that baseline

If the runtime cannot reach Phase 1 quietness without a pelvis/root reset, the transition design is not yet valid and must fail explicitly.

---

## 12. Quarantine Rule for Hip / Thigh Controls

The implementation may use a temporary quarantine on the thigh controls if needed to avoid same-frame coupling spikes.

If used, the rule must be:

- quarantine may start on Phase 1 entry
- quarantine applies only to `thigh_l` and `thigh_r`
- quarantine duration must be bounded and named
- quiet-window accumulation may not begin until quarantine is fully settled
- quarantine release must itself not occur on the same frame as Phase 2 root-on

Default recommendation:
- if quarantine is needed, release it before the quiet window starts, not during root-on

That makes Phase 1 prove the post-quarantine configuration is stable.

---

## 13. Phase 1 Exit Criteria

Phase 1 may advance to Phase 2 only when all of these are true:

1. required topology is correct
2. policy suppression for transition-critical set is active
3. no cached resets are pending
4. no quarantine release is pending
5. root linear speed is below quiet threshold
6. root angular speed is below quiet threshold
7. shell offset / velocity contamination is below quiet threshold
8. quiet-window hold duration has completed
9. fail-stop precursor is inactive
10. no transition-local hazard flag is active

Not allowed:
- advancing on time alone
- advancing because “things seem calmer”
- advancing while any structural hazard remains unresolved

---

## 14. Phase 1 Failure Classification

Phase 1 failure must be classified, not lumped together.

## 14.1 Retryable failure classes

These may retry automatically **only** if recovery changes the state and the system can prove a convergence path exists:

- `phase1_topology_not_achieved`
- `phase1_pending_reset_not_discharged`
- `phase1_quiet_window_interrupted_by_contamination`
- `phase1_quarantine_not_settled`

## 14.2 Non-retryable failure classes

These must clear the pending request or require explicit user action:

- `phase1_root_reset_requested`
- `phase1_policy_write_leak_to_transition_set`
- `phase1_repeated_target_discontinuity`
- `phase1_no_convergence_path`
- `phase1_missing_required_modifier_or_control`

## 14.3 Abort-level failure classes

These should fail the transition immediately and return to BridgeActive or fail-stop:

- `phase1_baseline_movement_too_high`
- `phase1_fail_stop_precursor`
- `phase1_simulation_explosion`
- `phase1_shell_correction_material`

---

## 15. Recovery Contract After Phase 1 Failure

Recovery must restore a coherent `BridgeActive` state, not a half-transitioned zombie state.

Required recovery actions:

- stop Phase 1 timers
- clear transition-local suppressions
- clear transition-local quarantine state
- restore normal BridgeActive topology
- clear pending transition-local hold pose references
- clear transition-local hazard flags
- restore policy write routing appropriate for BridgeActive
- leave the system in one explicit state only: `BridgeActive` or `BalanceTransitionFailed`

Not allowed:
- remaining partially transitioned while claiming recovery
- retrying from a fail-stop precursor without a fresh quiet proof
- reusing contaminated baseline metrics

---

## 16. Automatic Retry Rule

Automatic retry is permitted only if all are true:

1. previous failure class is retryable
2. recovery made a real topology or authority change
3. runtime returned to coherent `BridgeActive`
4. a fresh quiet-window proof in BridgeActive has occurred
5. retry budget not exceeded

Recommended controls:
- `Phase1MaxAutomaticRetries`
- `Phase1RetryCooldownSeconds`

If these conditions are not met, the request must remain failed or require a fresh manual trigger.

---

## 17. Logging Contract

Phase 1 logs must be compact and decisive.

Required one-shot logs:

- `PHASE1_ENTRY`
- `PHASE1_TOPOLOGY_TARGET`
- `PHASE1_POLICY_SUPPRESSION`
- `PHASE1_HOLD_REFERENCE_CAPTURED`
- `PHASE1_QUIET_WINDOW_STARTED`
- `PHASE1_QUIET_WINDOW_RESET`
- `PHASE1_READY_FOR_ROOT_ON`
- `PHASE1_REJECTED <reason>`
- `PHASE1_RECOVERY_BEGIN`
- `PHASE1_RECOVERY_COMPLETE`

Recommended summary fields:
- simCount
- distalSimCount
- rootLinear
- rootAngular
- shellOffsetDelta
- shellVelocityDelta
- policySuppressed
- pendingResets
- quarantineActive
- topologyValid

Do not spam per-frame logs unless a metric changes materially or a throttle interval elapses.

---

## 18. Required Threshold Names

Do not bury these as unnamed constants.

Minimum named thresholds:

- `Phase1QuietRootLinearSpeedCmPerSec`
- `Phase1QuietRootAngularSpeedDegPerSec`
- `Phase1QuietShellOffsetDeltaCm`
- `Phase1QuietShellVelocityDeltaCmPerSec`
- `Phase1QuietRequiredSeconds`
- `Phase1MaxEntryTargetDeltaDeg`
- `Phase1MaxAutomaticRetries`
- `Phase1RetryCooldownSeconds`
- `Phase1HipQuarantineDurationSeconds` or equivalent tick-based named control

---

## 19. Acceptance Tests

Minimum required tests for this spec:

### Test 1: Normal stable BridgeActive entry
- request arrives
- Phase 1 topology achieved
- quiet window accumulates
- Phase 2 begins

### Test 2: Distal bodies initially simulating
- Phase 1 explicitly forces distal kinematic topology
- quiet window does not start until distal sim count is zero
- transition succeeds

### Test 3: Policy leakage bug
- intentionally allow policy writes to thighs during Phase 1
- verify Phase 1 fails as `phase1_policy_write_leak_to_transition_set`

### Test 4: Root reset bug
- intentionally schedule pelvis reset during Phase 1
- verify hard failure as `phase1_root_reset_requested`

### Test 5: Shell contamination
- inject shell/world corrective motion
- verify quiet window resets and does not falsely succeed

### Test 6: Recovery / retry
- first attempt fails due to retryable topology issue
- recovery restores BridgeActive coherently
- second attempt succeeds after fresh quiet proof

### Test 7: No-convergence structural failure
- remove required modifier/control
- verify no infinite retry loop occurs

---

## 20. Recommended Default Design Decision

The recommended default implementation is:

1. On Phase 1 entry:
   - globally suppress policy target writes
   - freeze locomotion / shell assistance
   - force root, proximal, and distal transition sets kinematic
   - capture one entry hold reference from current skeletal pose

2. During Phase 1:
   - hold the transition set near the entry reference
   - allow no cached resets
   - allow no pelvis/root reset
   - accumulate quiet window only after topology and suppression are already correct

3. On success:
   - advance to Phase 2 root-on

4. On failure:
   - classify failure
   - recover to coherent BridgeActive
   - retry only if convergence is plausible and explicitly budgeted

This is the most conservative and least ambiguous version of Phase 1.

---

## 21. Final Design Summary

Phase 1 is not a vague pre-root-on waiting room.

It is a strict transition-owned stabilization phase with:

- explicit body sets
- explicit topology
- explicit authority suppression
- explicit posture-hold behavior
- explicit quiet-window proof
- explicit failure classes
- explicit recovery and retry rules

The runtime should be considered non-compliant with the Balance Mode entry contract if Phase 1 can only “sometimes” converge through trial-and-error tweaks rather than through this defined mechanism.
