# Stabilization Stress-Test: Finding the Destabilization Threshold

## Goal

Determine the exact PD gain multiplier at which the physics-driven character can no longer maintain idle balance. This gives a calibrated reference point for setting the G2 perturbation stabilization override.

## Background

The perturbation system needs to temporarily relax PD gains so the articulated body visibly reacts to a shell push. Without knowing where the stability boundary is, the relaxation amount is a guess — too little and the body stays rigid, too much and it collapses.

## Approach: Linear Ramp During Idle

Once the bridge is fully settled (`BridgeActive`, all bring-up groups unlocked, policy influence at 1.0), linearly ramp all stabilization multipliers from `1.0 → 0.0` over a fixed window (30–60 seconds). The character stands idle throughout.

### What to ramp (uniformly)

| Multiplier | Role |
|---|---|
| `AngularStrengthMultiplier` | PD spring stiffness |
| `AngularDampingRatioMultiplier` | PD damping ratio |
| `AngularExtraDampingMultiplier` | Extra angular damping |

All three ramp together from 1.0 → 0.0. This gives the "uniform relaxation" collapse threshold. Individual parameter sweeps can follow later if needed.

### What to log each interval

- Elapsed seconds since ramp start
- Current multiplier value (0.0–1.0)
- Root body height delta (first sign of collapse is vertical sag)
- Max body linear/angular velocity (spikes signal instability onset)
- Instability detector state (`RuntimeInstabilityState`)
- Fail-stop trigger (if reached)
- Local pose drift relative to the actor root for:
  - `spine_01`
  - `head`
  - max of `foot_l` / `foot_r`
- First-event markers:
  - first angular spike bone
  - first linear spike bone
  - first instability sign

### Activation

CVar-driven: `pa.StabilizationStressTest 1` activates the ramp inside the existing tick path. No new subsystem or test harness.

Additional controls:

- `pa.StabilizationStressTestProfile`
  - `0` = ramp down only
  - `1` = ramp down / hold / ramp up
- `pa.StabilizationStressTestTargetMultiplier`
  - target floor multiplier
- `pa.StabilizationStressTestHoldSeconds`
  - hold duration for the recovery profile
- `pa.StabilizationStressTestRecoveryRampSeconds`
  - ramp-up duration for the recovery profile
- `pa.StabilizationStressTestSweepMode`
  - `0` = all three angular multipliers
  - `1` = strength only
  - `2` = damping ratio only
  - `3` = extra damping only
- `pa.StabilizationStressTestAngularSpikeThresholdDegPerSec`
- `pa.StabilizationStressTestLinearSpikeThresholdCmPerSec`

### Implementation location

Inside `UPhysAnimComponent`:
- New CVar to enable stress-test mode
- In `ResolveEffectiveStabilizationSettings()`: if enabled, compute `alpha = elapsed / rampDuration`, multiply all gains by `(1.0 - alpha)`
- Log multiplier + diagnostics at each diagnostics interval
- Stop ramp at 0.0 or on fail-stop, whichever comes first

Estimated size: ~30 lines of new code.

## Interpreting Results

| Collapse Multiplier | Interpretation | Perturbation Strategy |
|---|---|---|
| **0.7–1.0** | Fragile — PD gains are barely holding balance | Small relaxation (~0.9×) already produces visible reaction |
| **0.3–0.6** | Moderate — reasonable stability margin | Set override to ~threshold × 1.3 for visible-but-recoverable reaction |
| **0.0–0.2** | Robust — policy is doing most of the work | Aggressive relaxation needed; may indicate policy is genuinely balancing |

### Setting the perturbation override

Target: `threshold × 1.3` (30% above collapse point).

Example: if collapse occurs at multiplier 0.4, set the perturbation override multiplier to `0.4 × 1.3 = 0.52`. This puts the character close enough to the edge for visible body reaction but leaves enough margin for recovery.

## Status

- [x] Implementation
- [x] First stress-test run and threshold measurement
- [ ] Calibrate `ResolvePresentationStabilizationOverrideSeconds()` relaxation multipliers based on results

## First Run Result

- Date: March 11, 2026
- Command path:
  - `pa.StabilizationStressTest 1`
  - `pa.StabilizationStressTestRampSeconds 45`
  - `Automation RunTests PhysAnim.PIE.Smoke`
- Result:
  - the idle bridge remained stable through the full `1.0 -> 0.0` uniform gain ramp
  - no `Fail-stop` occurred
  - root instability metrics stayed bounded through the smoke window
  - some higher body angular spikes appeared late at `multiplier=0.00`, but they did not trip the instability detector during the idle window

## Current Interpretation

- The uniform idle collapse threshold was **not reached before `0.00`**.
- That puts the current bridge in the `robust` bucket for this specific test.
- Practical implication:
  - the G2 perturbation issue is not primarily that the temporary stabilization override is still too conservative
  - the remaining problem is more likely the perturbation delivery path itself or how much of the shove is coupling into the gameplay shell versus the articulated body

## Measured Answers

### Idle answers

1. `Per-bone stability contribution`
   - the lower body is still the weakest link
   - first meaningful angular spikes appear at `ball_l` / `ball_r`
   - the largest sustained linear offender is usually `calf_l`

2. `PD authority vs. policy authority`
   - `physanim.ForceZeroActions 1` is not a valid measurement path because it keeps the bridge in safe mode
   - the runtime proxy used here is `physanim.ActionScale 0.0`
   - at idle, the proxy-disabled policy path is slightly quieter than the normal policy path
   - interpretation: PD is doing the real idle balancing work; the policy adds looseness rather than extra stability in this regime

3. `Onset-to-collapse duration`
   - no idle run collapsed
   - the bridge stayed stable all the way to `multiplier=0.00`

4. `Recovery test`
   - the down / hold / up profile recovered cleanly
   - there was no fail-stop during re-ramp
   - re-ramp does excite the upper limbs more than the pure down sweep

5. `Asymmetric sensitivity`
   - `AngularDampingRatioMultiplier` is the most stability-critical lever at idle
   - `AngularExtraDampingMultiplier` is the next most important
   - `AngularStrengthMultiplier` matters less than damping in this idle regime

6. `Pose drift under partial relaxation`
   - at about `50%` gains the bridge stays upright and only loosens modestly
   - the most visible drift remains in the feet
   - policy-disabled idle almost eliminates that foot looseness

### Movement answers

Running the same matrix with deterministic locomotion materially changes the picture.

1. `Per-bone stability contribution`
   - movement is still lower-body limited
   - the first angular spike shifts to `ball_r`
   - the first linear spike shifts to `foot_l`

2. `PD authority vs. policy authority`
   - movement is stressful enough that PD alone is not obviously safer
   - the `ActionScale 0.0` proxy still reaches very large body spikes
   - under translation, the useful conclusion is no longer “policy adds looseness only”; both paths are heavily load-bearing

3. `Onset-to-collapse duration`
   - movement introduces a real graceful-degradation zone instead of the idle “never collapses” result
   - in the uniform movement down sweep, the first instability sign appears around `multiplier=0.44`
   - the run still avoids fail-stop, but the body metrics get much worse from that point onward

4. `Recovery test`
   - the movement recovery profile completes without fail-stop
   - but it is much harsher than idle:
     - large spikes persist during and after the hold / re-ramp phases
   - interpretation: recovery remains possible, but the usable perturbation floor under locomotion is much higher than the idle result

5. `Asymmetric sensitivity`
   - movement increases sensitivity across every sweep
   - damping ratio still sits among the most critical levers
   - extra damping is also very important
   - strength-only relaxation remains the least destructive single-parameter sweep

6. `Pose drift under partial relaxation`
   - movement turns the small idle foot looseness into very large lower-body drift
   - local foot drift grows into the `80-90 cm` range by the end of the movement sweeps
   - this is the clearest sign that movement changes the practical relaxation envelope

## Perturbation Implication

- The stress-test result justifies very aggressive temporary relaxation during the G2 perturbation window while the character is standing still.
- That change has now been tested in the presentation harness:
  - the perturbation override drops the angular stabilization multipliers effectively to `0.0` for a short window
  - the shell-level shove has been removed
  - only the body-level contact pusher remains
- Latest result:
  - the bridge no longer slides sideways at the actor-shell level
  - but the physics-driven body response is still modest
  - recent G2 presentation telemetry shows roughly:
    - `actorDelta = 0.0 cm`
    - `localHead ~= 2.3 cm`
    - `localFoot ~= 3.0-4.5 cm`
- Practical conclusion:
  - stabilization strength is no longer the lead constraint
  - the remaining blocker for a convincing G2 perturbation moment is the perturbation path itself, most likely the kinematic root / gameplay-shell anchoring and how contact is coupling into the articulated chain
