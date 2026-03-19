# Balance Perturbation Mode Design

Status: Revised design contract  
Scope: Stage 1 balance-only diagnostic mode  
Audience: runtime, controls, debugging, validation, and transition work for PhysAnim bridge

## Objective

Balance Perturbation Mode is a dedicated diagnostic framework to measure **true PHC-driven standing balance recovery** of the Stage 1 physics character.

It must isolate articulated-body recovery from:

- locomotion assistance
- shell/world translation assistance
- actor/capsule dragging
- startup seeding and handoff assistance
- hidden movement-system correction
- transition-only entry shaping once active mode begins

This mode is **not** a locomotion test and **not** a shell-motion test.  
It is a standing-balance and perturbation-recovery test entered only through a bounded transition pipeline.

---

## 1. Core validity rule

The mode design is valid only if every condition that is expected to change has a named owner.

Interpretation rule:

- if a condition is expected to stay fixed during a phase, it may be observed-only
- if a condition is expected to become true, false, armed, disarmed, valid, invalid, or settled, one subsystem must own changing it
- if no owner exists, the condition must not appear as a gate, readiness proof, or convergence assumption

This rule applies to:

- entry gating
- quiet windows
- handoff certification
- shell-safety proofs
- perturbation firing eligibility
- recovery and contamination classification
- active-mode exit

Not allowed:

- “wait until it settles” with no owner that can settle it
- “proof becomes true” with no owner that accumulates and invalidates it
- “contamination clears” with no owner that resets the contaminated state
- “mode becomes safe to test” through incidental drift

---

## 2. Relationship to entry / Phase 1 / Phase 2 docs

This document defines what Balance Perturbation Mode is for and what must remain true while it is active.

Companion documents define how the runtime reaches that state:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`
- `plans/stage1/40-design/balance_mode_phase2.md`

Interpretation rule:

- this document does not permit bypassing the entry pipeline
- active mode begins only after the entry transition has succeeded
- any “active mode” assumption that depends on transition-shaped state must name who owns keeping that state or handing it off

---

## 3. Authoritative perturbation method

### Primary method: pelvis rigid-body impulse

- **Target**: the `pelvis` rigid body on the skeletal mesh
- **Type**: a single instantaneous linear impulse
- **Space**: world-space directions only
- **Directions**: `+X`, `-X`, `+Y`, `-Y`
- **Application path**: direct physics-body impulse APIs to the pelvis body instance

This is the only authoritative perturbation path for the first-pass framework.

### Why pelvis impulse is the chosen method

- it perturbs the articulated body directly instead of moving the gameplay shell
- it challenges the main balance loop more honestly than actor displacement
- it is easier to measure and compare than a continuous force
- it is less likely than upper-body-only shoves to create misleading local wobble results

### Ownership rule for perturbation application

The perturbation scheduler owns:

- deciding whether a scenario is eligible to fire
- arming and disarming the pending scenario
- firing the single impulse exactly once
- recording perturbation-proof telemetry
- marking the run invalid if the target body is missing or not simulating

No other system may implicitly “help” a perturbation fire by moving the actor, capsule, or shell.

---

## 4. Rejected methods and why

### Shell offset / `perturbOverride`
Rejected as a balance test method.

It moves the reference frame and can help the character by shifting the shell/world relationship instead of demanding true body recovery.

### Actor / capsule displacement
Rejected as a balance test method.

Moving the actor, capsule, or updated component tests shell/world authority and movement logic, not PHC’s articulated recovery.

### `MoveComponent`, `SetActorLocation`, shell dragging
Rejected as a balance test method.

These are invalid for proving PHC balance because they create or hide recovery through translation rather than articulated control.

### Upper-body shove as primary test
Rejected as the primary balance test.

Upper torso or spine pushes may create visible motion without meaningfully testing the main center-of-mass recovery problem.

### Continuous force as primary method
Rejected for the initial framework.

Continuous force is harder to compare across runs and harder to separate disturbance from recovery.

### Random body selection / many ad hoc methods
Rejected.

The framework must use one canonical method first so results are interpretable.

---

## 5. Runtime authority model while active

When Balance Perturbation Mode is active, the runtime must enforce:

- idle reference only
- no locomotion entry
- no bridge-owned movement drive
- no CharacterMovement walking or custom locomotion authority
- no trajectory-driven movement assistance
- no shell/world translation as a recovery mechanism
- no ramp or travel testing

### 5.1 Shell / world authority rule

The shell is **not** allowed to act as an active stabilizer in this mode.

The correct rule is:

- shell/world translation is disabled as an active recovery mechanism
- minimal collision clamping is allowed only if absolutely unavoidable
- any shell/world correction must be measured
- recovery is contaminated only if shell/world correction contributes material corrective displacement

### 5.2 CharacterMovement rule

For this mode:

- CharacterMovement walking or custom locomotion authority must remain disabled
- actor/capsule translation is not allowed to serve as a recovery mechanism
- any residual movement-system correction must be logged as contamination

### 5.3 Ownership rule for active-mode authority

The active-mode authority owner must be explicit.

Required owner:

- `BalanceModeRuntimeOwner`

This owner is responsible for:

- holding locomotion-disabled state while active
- holding shell-assist-disabled state while active
- holding perturbation scheduler enable state while active
- marking the run contaminated if forbidden owners become active
- handing these responsibilities back only on explicit mode exit

Not allowed:

- relying on startup locks to still be in effect
- relying on Phase 1 shell lock to persist without an explicit active-mode owner
- assuming gameplay owners “probably stay inactive”

---

## 6. Trigger conditions for perturbation firing

A perturbation may fire only when all of these are true:

1. runtime mode state is `BalancePerturbationActive`
2. active-mode authority ownership is held by `BalanceModeRuntimeOwner`
3. pelvis body exists
4. pelvis body is simulating
5. no locomotion state is active
6. no contamination lockout is active
7. no previous perturbation is still within recovery timeout or cooldown
8. active quiet proof is valid
9. scenario scheduler has an armed scenario
10. no explicit abort or fail-stop state is active

### 6.1 Active quiet proof

The mode must not use a vague “stable enough” idea.

Required proof object:

- `ActiveModeQuietProof`

This proof is owned by:

- `BalanceQuietProofAccumulator`

That owner is responsible for:

- starting accumulation when a recovery or entry settle window completes
- resetting accumulation when any quiet condition becomes false
- invalidating the proof when contamination, locomotion entry, or fail-stop occurs
- reporting actual accumulated duration
- exposing a boolean validity state to the scheduler

If the design expects a quiet window to “become true” but no accumulator owns it, the design is invalid.

For the balance-entry transition path, that same ownership rule applies to Phase 1 quiet proof:

- `Phase1QuietProof` is owned by `BalanceQuietProofAccumulator`
- `BalanceModeQuietRequiredSeconds = 1.0`

### 6.2 Minimum quiet window

Default target:

- `BalanceModeQuietRequiredSeconds = 1.0`

The framework must log the actual quiet-window duration used for each scenario.

---

## 7. Scenario matrix

Each scenario is a deterministic trial.

The initial scenario matrix is fixed and must not be expanded in the first implementation.

| Scenario Name | Direction | Magnitude Tier |
| :--- | :--- | :--- |
| `IdleHold_NoPush` | N/A | None |
| `IdleHold_PelvisImpulse_Forward_Small` | +X | Small |
| `IdleHold_PelvisImpulse_Forward_Medium` | +X | Medium |
| `IdleHold_PelvisImpulse_Forward_Large` | +X | Large |
| `IdleHold_PelvisImpulse_Backward_Small` | -X | Small |
| `IdleHold_PelvisImpulse_Backward_Medium` | -X | Medium |
| `IdleHold_PelvisImpulse_Backward_Large` | -X | Large |
| `IdleHold_PelvisImpulse_Left_Small` | -Y | Small |
| `IdleHold_PelvisImpulse_Left_Medium` | -Y | Medium |
| `IdleHold_PelvisImpulse_Left_Large` | -Y | Large |
| `IdleHold_PelvisImpulse_Right_Small` | +Y | Small |
| `IdleHold_PelvisImpulse_Right_Medium` | +Y | Medium |
| `IdleHold_PelvisImpulse_Right_Large` | +Y | Large |

### Magnitude tier rule

Do not hardcode the design around raw impulse numbers alone.

Preferred rule:

- define tiers using target pelvis `Δv` or mass-normalized impulse
- compute actual impulse from pelvis body mass
- always log realized post-impulse pelvis `Δv`

### Ownership rule for scenario progression

A single owner must advance scenario state:

- `BalanceScenarioController`

It owns:

- current scenario selection
- arming
- fire-once behavior
- cooldown start
- recovery timeout start
- scenario completion classification

Not allowed:

- scheduler logic split across unrelated systems with no single state owner
- multiple systems independently deciding a scenario is done

---

## 8. Perturbation application requirements

The perturbation applier must:

1. resolve the pelvis body instance explicitly
2. verify the body exists
3. verify the body is simulating physics
4. record pre-impulse linear velocity
5. record pre-impulse angular velocity
6. apply the world-space impulse once
7. record immediate post-impulse linear velocity
8. record immediate post-impulse angular velocity
9. fail loudly if the pelvis body cannot be found or is not simulating

A perturbation run is invalid if the system cannot prove the pelvis body received a measurable impulse response.

### Ownership rule

Required owner:

- `PelvisImpulseApplier`

This owner must produce:

- target-body resolution result
- pre/post velocity samples
- realized `Δv`
- validity classification

---

## 9. Diagnostics and trace output

Diagnostics must distinguish:

- real physical response
- real PHC recovery
- fake recovery caused by shell/world assistance
- no-op perturbations
- invalid scenario execution
- proof invalidation

### 9.1 Perturbation proof

For each perturbation event, log:

- scenario name
- fire time
- target body name
- world-space impulse vector
- pelvis body mass if available
- pre-impulse pelvis linear velocity
- post-impulse pelvis linear velocity
- pre-impulse pelvis angular velocity
- post-impulse pelvis angular velocity
- measured pelvis `Δv`

### 9.2 Body response over recovery window

Log over the recovery interval:

- pelvis world position delta from pre-impulse pose
- pelvis world linear velocity over time
- pelvis world angular velocity over time
- pelvis/root tilt over time
- pelvis height over time
- peak pelvis displacement
- peak pelvis angular speed

### 9.3 Policy activity during recovery

Log:

- whether PHC stepped during recovery
- control-target update count during recovery
- magnitude summary of target deltas after the perturbation
- whether policy outputs or targets changed materially or were effectively frozen

### 9.4 Contamination / hidden stabilizer checks

Log:

- actor world displacement during recovery
- shell/world displacement during recovery
- whether shell/world clamping or correction occurred
- whether bridge-owned movement drive contributed displacement
- whether locomotion entry was requested
- whether CharacterMovement or equivalent movement correction contributed

### 9.5 Ownership rule for diagnostics

A single owner must aggregate run truth:

- `BalanceRunRecorder`

It owns:

- run start snapshot
- recovery-window metric accumulation
- contamination flags
- pass/fail classification inputs
- final run record emission

Not allowed:

- classifying a run from scattered logs with no canonical record owner

---

## 10. Pass / fail / contamination criteria

### 10.1 Valid perturbation

A perturbation is valid only if:

1. the pelvis body was successfully targeted
2. the impulse fired
3. the pelvis body showed a measurable velocity response

### 10.2 Measurable response

Initial default rule:

- peak pelvis linear velocity after impact must exceed a clearly logged threshold

### 10.3 Recovered

A run counts as recovered only if all are true:

1. no fail-stop or instability termination occurred
2. pelvis/root tilt returned below recovery threshold
3. pelvis linear velocity returned below recovery threshold
4. pelvis height returned to an acceptable standing band
5. stable condition was sustained for a hold duration
6. no locomotion entry occurred during the balance-only test
7. the run was not contaminated by shell/world correction

### 10.4 Failed

A run counts as failed if any of these occur:

- instability or fail-stop triggered
- pelvis height drops below fall threshold
- recovery timeout expires without meeting recovery criteria
- perturbation was invalid or produced no measurable response

### 10.5 Contaminated

A run counts as contaminated if shell/world or movement-system correction contributed material corrective displacement during the recovery window.

### 10.6 Ownership rule for classification

The design must not assume “recovered”, “failed”, or “contaminated” emerge automatically.

Required owners:

- `BalanceRecoveryEvaluator`
- `BalanceContaminationEvaluator`

These owners must explicitly accumulate, invalidate, and finalize their own classifications.

---

## 11. Recovery thresholds

The implementation must define and log explicit numeric thresholds for:

- measurable response
- recovered linear velocity
- recovered tilt
- recovered pelvis height band
- stable hold duration
- recovery timeout
- contamination displacement threshold

### Recommended structure

- `ResponseVelocityThresholdCmPerSec`
- `RecoveryVelocityThresholdCmPerSec`
- `RecoveryTiltThresholdDeg`
- `RecoveryHeightToleranceCm`
- `RecoveryStableHoldSeconds`
- `RecoveryTimeoutSeconds`
- `ShellContaminationDisplacementCm`

Threshold existence alone is not enough.

Required owner:

- `BalanceThresholdConfig`

This owner supplies the authoritative values used by schedulers, evaluators, and logs.

---

## 12. Trustworthiness criteria

The framework is trustworthy only if it demonstrates all of the following:

1. pelvis impulse clearly reaches the targeted body
2. body response is measurable and scales with perturbation tier
3. shell/world correction is either absent or explicitly flagged
4. recovery metrics are stable and repeatable across runs
5. larger perturbations generally produce larger recovery times or more failures
6. if PHC/control authority is intentionally weakened or disabled, recovery quality worsens visibly

That negative test is required to prove the framework is measuring policy/body recovery rather than a hidden stabilizer.

### Ownership rule for negative tests

The design must name the owner that weakens PHC authority for validation experiments.

Required owner:

- `BalanceValidationHarness`

If no harness owns the weakening condition, the negative test requirement is not implementable.

---

## 13. Interpretation rules

### A passing result means

- the articulated body was physically disturbed
- the PHC-controlled body remained standing or returned to standing
- shell/world assistance did not materially explain the recovery

### A passing result does not automatically mean

- locomotion is solved
- walking recovery is solved
- ramp handling is solved

This framework only establishes whether balance-in-place recovery is real.

---

## 14. Implementation priorities

1. implement bounded entry into balance mode
2. hand off active-mode ownership to `BalanceModeRuntimeOwner`
3. disable or contain locomotion and shell assistance in active mode
4. implement pelvis impulse applier
5. implement deterministic scenario controller
6. implement quiet-proof accumulator and recovery evaluator
7. add structured diagnostics
8. add explicit contamination evaluator
9. add validation helpers and negative tests
10. produce a validation report

Do not:

- start by adding more perturbation methods
- start by testing walking
- keep multiple conflicting push systems active
- assume quiet proof or contamination state will maintain itself with no owner

---

## 15. Final design summary

### Primary test method

Single world-space linear impulse applied to the pelvis rigid body while standing idle in Balance Perturbation Mode.

### What is disabled

- locomotion entry
- bridge movement drive
- CharacterMovement locomotion authority
- shell/world translation as a recovery mechanism

### What is allowed

- PHC-driven articulated recovery
- minimal unavoidable collision clamping, if any, with contamination logging

### What determines trustworthiness

- measurable pelvis-body response
- explicit recovery metrics
- explicit shell contamination metrics
- explicit pass/fail thresholds
- a negative test showing reduced PHC authority worsens results

### Final validity rule

The mode design is invalid if any required changing condition lacks a named owner, including:

- active quiet proof
- scenario arm/fire/cooldown progression
- recovery classification
- contamination classification
- active-mode authority hold
- explicit mode exit and ownership handoff
