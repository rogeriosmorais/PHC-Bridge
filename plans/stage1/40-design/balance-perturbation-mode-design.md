# Balance Perturbation Mode Design

## Objective
The Balance Perturbation Mode is a dedicated diagnostic framework to measure **true PHC-driven balance recovery** of the Stage 1 physics character.

It must isolate neural corrective body dynamics from:
- locomotion assistance
- shell/world translation
- actor/capsule dragging
- startup seeding and handoff assistance
- hidden movement-system correction

This mode is **not** a locomotion test and **not** a shell-motion test.
It is a standing-balance and perturbation-recovery test.

---

## 1. Authoritative Perturbation Method

### Primary Method: Pelvis Rigid-Body Impulse
- **Target**: the `pelvis` rigid body on the skeletal mesh.
- **Type**: a single instantaneous **linear impulse**.
- **Space**: world-space directions only.
- **Directions**: `+X`, `-X`, `+Y`, `-Y`.
- **Application Path**: directly to the pelvis body instance via physics-body impulse APIs.

This is the only authoritative perturbation path for the first-pass framework.

### Why pelvis impulse is the chosen method
- It perturbs the articulated body directly instead of moving the gameplay shell.
- It challenges the main balance loop more honestly than actor displacement.
- It is easier to measure and compare than a continuous force.
- It is less likely than upper-body-only shoves to create a misleading local wobble response.

---

## 2. Rejected Methods and Why

### Shell Offset / `perturbOverride`
**Rejected as a balance test method.**
It moves the reference frame and can effectively “help” the character by shifting the world/shell relationship instead of demanding true body recovery.

### Actor / Capsule Displacement
**Rejected as a balance test method.**
Moving the actor, capsule, or updated component tests shell/world authority and movement logic, not PHC’s physical recovery.

### `MoveComponent`, `SetActorLocation`, shell dragging
**Rejected as a balance test method.**
These are invalid for proving PHC balance because they create or hide recovery through translation rather than articulated control.

### Upper-Body Shove as Primary Test
**Rejected as the primary balance test.**
Upper torso/spine pushes can create visible motion without meaningfully testing the main center-of-mass recovery problem.
They may be useful later as secondary diagnostics, but not as the authoritative Stage 1 balance test.

### Continuous Force as Primary Method
**Rejected for the initial framework.**
Continuous force is harder to compare across runs, more sensitive to timing ambiguity, and makes it harder to separate disturbance from recovery.

### Random body selection / many ad hoc methods
**Rejected.**
The framework must use one canonical method first so results are interpretable.

---

## 3. Runtime Authority Rules

### Balance Perturbation Mode
When this mode is active, the runtime must enforce:

- **Idle reference only**
- **No locomotion entry**
- **No bridge-owned movement drive**
- **No CharacterMovement walking/custom locomotion authority**
- **No trajectory-driven movement assistance**
- **No shell/world translation as a recovery mechanism**
- **No ramp or travel testing**

### Shell / world authority rule
The shell is **not** allowed to act as an active stabilizer in this mode.

However, the rule is **not** “zero actor movement at all costs.”
The correct rule is:

- shell/world translation is disabled as an active recovery mechanism
- minimal collision clamping is allowed only if absolutely unavoidable
- any shell/world correction must be measured
- recovery is considered **contaminated** only if shell/world correction contributes **material corrective displacement**

### CharacterMovement rule
For this mode:
- CharacterMovement walking/custom locomotion authority must be disabled
- actor/capsule translation is not allowed to serve as a recovery mechanism
- any residual movement-system correction must be logged as contamination

---

## 4. Trigger Conditions

A perturbation may only fire when **all** of these are true:

1. Bridge state is `BridgeActive`
2. Startup handoff is complete
3. Startup pose seeding is no longer dominating
4. Policy authority is confirmed active
5. Balance Perturbation Mode is active
6. The system has remained stably idle for a minimum quiet window
7. No locomotion state is active
8. No previous perturbation is still within recovery timeout or cooldown

### Minimum quiet window
Default target: **1.0 second** of stable standing before the perturbation fires.

This should be configurable, but the framework must log the actual quiet-window duration used for each scenario.

---

## 5. Scenario Matrix

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
Do **not** hardcode the design around raw impulse numbers alone.
The tiers must be normalized in a physically meaningful way.

Preferred rule:
- define tiers using **target pelvis Δv** or a mass-normalized impulse
- compute the actual impulse from pelvis body mass
- always log the realized post-impulse pelvis Δv

The design may still expose raw impulse values internally, but the framework should treat **actual measured Δv** as the authoritative comparable quantity.

---

## 6. Perturbation Application Requirements

The perturbation applier must:

1. Resolve the pelvis body instance explicitly
2. Verify the body exists
3. Verify the body is simulating physics
4. Record pre-impulse linear velocity
5. Record pre-impulse angular velocity
6. Apply the world-space impulse once
7. Record immediate post-impulse linear velocity
8. Record immediate post-impulse angular velocity
9. Fail loudly if the pelvis body cannot be found or is not simulating

A perturbation run is **invalid** if the system cannot prove the pelvis body received a measurable impulse response.

---

## 7. Diagnostics and Trace Output

The diagnostics must be rich enough to distinguish:
- real physical response
- real PHC recovery
- fake recovery caused by shell/world assistance
- no-op perturbations

### A. Perturbation proof
For each perturbation event, log:
- scenario name
- fire time
- target body name
- world-space impulse vector
- pelvis body mass, if available
- pre-impulse pelvis linear velocity
- post-impulse pelvis linear velocity
- pre-impulse pelvis angular velocity
- post-impulse pelvis angular velocity
- measured pelvis Δv

### B. Body response over recovery window
Log over the recovery interval:
- pelvis world position delta from pre-impulse pose
- pelvis world linear velocity over time
- pelvis world angular velocity over time
- pelvis/root tilt over time
- pelvis height over time
- peak pelvis displacement
- peak pelvis angular speed

### C. Policy activity during recovery
Log:
- whether PHC stepped during recovery
- control-target update count during recovery
- magnitude summary of target deltas after the perturbation
- whether policy outputs/targets changed materially or were effectively frozen

### D. Contamination / hidden stabilizer checks
Log:
- actor world displacement during recovery
- shell/world displacement during recovery
- whether shell/world clamping or correction occurred
- whether bridge-owned movement drive contributed any displacement
- whether locomotion entry was requested during recovery
- whether startup/settle logic remained active during recovery
- whether CharacterMovement or equivalent movement correction contributed

### E. Contact/support diagnostics
If available, log:
- left/right foot contact state
- approximate COM or equivalent support-related metric

If COM/support-polygon calculation is not feasible in the first pass, state that explicitly and at least log foot contacts and pelvis/root tilt.

---

## 8. Pass / Fail / Contamination Criteria

### Valid perturbation
A perturbation is considered valid only if:
1. the pelvis body was successfully targeted
2. the impulse fired
3. the pelvis body showed a measurable velocity response

### Measurable response
Initial default rule:
- peak pelvis linear velocity after impact must exceed a clearly logged threshold

The threshold must be explicit and configurable.
Do not bury it in code with no documentation.

### Recovered
A run counts as **recovered** only if all of these are true:
1. no fail-stop / instability termination occurred
2. pelvis/root tilt returned below recovery threshold
3. pelvis linear velocity returned below recovery threshold
4. pelvis height returned to an acceptable standing band
5. the stable condition was sustained for a hold duration
6. no locomotion entry occurred during the balance-only test
7. the run was **not contaminated** by shell/world correction

### Failed
A run counts as **failed** if any of these occur:
- instability / fail-stop triggered
- pelvis height drops below fall threshold
- recovery timeout expires without meeting recovery criteria
- perturbation was invalid or produced no measurable response

### Contaminated
A run counts as **contaminated** if shell/world/movement-system correction contributed **material corrective displacement** during the recovery window.

Important:
Contamination is **not** defined as “any actor movement at all.”
It is defined as movement/correction large enough to plausibly explain part of the recovery.

The framework must explicitly log the contamination threshold and whether it was exceeded.

---

## 9. Recovery Threshold Guidance

The implementation must define and log explicit numeric thresholds for:
- measurable response
- recovered linear velocity
- recovered tilt
- recovered pelvis height band
- stable hold duration
- recovery timeout
- contamination displacement threshold

These must not remain vague.

### Recommended structure
Use configuration values such as:
- `ResponseVelocityThresholdCmPerSec`
- `RecoveryVelocityThresholdCmPerSec`
- `RecoveryTiltThresholdDeg`
- `RecoveryHeightToleranceCm`
- `RecoveryStableHoldSeconds`
- `RecoveryTimeoutSeconds`
- `ShellContaminationDisplacementCm`

The exact defaults should be chosen after inspecting the asset/runtime, but they must be documented and emitted in logs.

---

## 10. Trustworthiness Criteria

The framework is considered trustworthy only if it can demonstrate all of the following:

1. pelvis impulse clearly reaches the targeted body
2. body response is measurable and scales with perturbation tier
3. shell/world correction is either absent or explicitly flagged
4. recovery metrics are stable and repeatable across runs
5. larger perturbations generally produce larger recovery times or more failures
6. if PHC/control authority is intentionally weakened or disabled, recovery quality worsens in a visible way

That last negative test is required to prove the framework is measuring policy/body recovery rather than a hidden stabilizer.

---

## 11. Interpretation Rules

### A passing result means
- the articulated body was physically disturbed
- the PHC-controlled body remained standing or returned to standing
- shell/world assistance did not materially explain the recovery

### A passing result does NOT automatically mean
- locomotion is solved
- walking recovery is solved
- ramp handling is solved

This framework only establishes whether **balance-in-place recovery** is real.

---

## 12. Implementation Priorities

The implementation sequence should be:

1. Add Balance Perturbation Mode
2. Disable/contain locomotion and shell assistance in that mode
3. Implement pelvis impulse applier
4. Implement deterministic scenario controller
5. Add structured diagnostics
6. Add explicit recovery/contamination evaluator
7. Add validation helpers/tests
8. Produce a validation report

Do not start by adding more perturbation methods.
Do not start by testing walking.
Do not keep multiple conflicting push systems active.

---

## 13. Final Design Summary

### Primary test method
**Single world-space linear impulse applied to the pelvis rigid body while standing idle in Balance Perturbation Mode.**

### What is disabled
- locomotion entry
- bridge movement drive
- CharacterMovement locomotion authority
- shell/world translation as a recovery mechanism

### What is allowed
- PHC-driven articulated recovery
- minimal unavoidable collision clamping, if any, but only with contamination logging

### What determines trustworthiness
- measurable pelvis-body response
- explicit recovery metrics
- explicit shell contamination metrics
- clear pass/fail thresholds
- negative test showing reduced PHC authority worsens results
