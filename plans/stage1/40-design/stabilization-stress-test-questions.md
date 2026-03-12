# Stabilization Stress-Test: Extended Analysis Questions

Questions to answer from the stabilization ramp test, beyond the basic collapse threshold.

---

## 1. Per-Bone Stability Contribution

**Which bone destabilizes first?**

Track per-bone max angular velocity during the ramp. The first bone to spike reveals the weakest link in the stability chain.

| First failure bone | Interpretation |
|---|---|
| Ankles / feet | Balance is foot-limited; lower-body policy control is the bottleneck |
| Spine | Torso is the weak point; upper-body stabilization doing too much work |
| Arms / head | Cosmetic instability only — not structural, can likely be ignored |

---

## 2. PD Authority vs. Policy Authority

**How much work is the policy actually doing?**

Run the stress-test twice:
1. **Policy active** (normal mode) — ramp gains down
2. **Policy disabled** (force-zero actions) — ramp gains down

- **Same collapse multiplier** → policy isn't contributing to balance, PD controllers do everything
- **Policy-active survives longer** → policy is genuinely helping with balance (PHC bridge is working)

This is a direct measurement of whether the PHC bridge adds real value to balance maintenance.

---

## 3. Onset-to-Collapse Duration

**How fast does failure cascade?**

Measure the time between the first instability sign and full collapse.

| Cascade speed | Interpretation |
|---|---|
| < 0.5s | Brittle — once any gain drops below threshold, everything fails. Recovery from perturbation will be difficult |
| 2–5s | Graceful degradation zone exists. Perturbation can push into this zone and the character has time to recover |

---

## 4. Recovery Test

**Can the character recover if gains are restored?**

Modified ramp: ramp down to a specific multiplier, hold for 2–3 seconds, then ramp back up to 1.0.

- **Character recovers** → that multiplier is a safe perturbation override floor
- **Character doesn't recover** (accumulated error is irreversible) → need a higher floor

This directly validates whether the perturbation strategy is recoverable, not just survivable.

---

## 5. Asymmetric Sensitivity

**Which parameter has the most visual impact per unit of relaxation?**

After the uniform sweep, do individual parameter sweeps:

| Sweep | What it tests |
|---|---|
| Ramp only `AngularStrengthMultiplier` (damping at 1.0) | Is stiffness the dominant factor? |
| Ramp only `AngularDampingRatioMultiplier` (strength at 1.0) | Is damping the dominant factor? |
| Ramp only `AngularExtraDampingMultiplier` (others at 1.0) | Does extra damping matter independently? |

If only one parameter drives collapse, the perturbation override only needs to relax that one — giving maximum visual reaction with minimum instability risk.

---

## 6. Pose Drift Under Partial Relaxation

**At 50% gains, how much does the idle pose drift?**

Even below the collapse threshold, the character may subtly shift posture (lean, tilt, drop arms). Measure bone positions relative to the actor root (local-offset telemetry already exists) at each gain level.

This quantifies how much visual "looseness" each gain level produces — useful for tuning the perturbation to look natural rather than mechanical.

---

## Current Answers

### Idle

1. `Per-bone stability contribution`
   - lower body first
   - first angular spikes: `ball_l` / `ball_r`
   - strongest sustained linear offender: `calf_l`

2. `PD authority vs. policy authority`
   - measured with `physanim.ActionScale 0.0` instead of `physanim.ForceZeroActions 1`, because force-zero keeps the bridge in safe mode
   - result: idle is slightly quieter with policy influence effectively disabled
   - conclusion: PD does the real idle balancing work; policy mainly adds looseness in idle

3. `Onset-to-collapse duration`
   - no collapse under idle
   - no fail-stop even at `multiplier=0.00`

4. `Recovery test`
   - yes, recovery succeeds
   - down / hold / up returns to `1.0` without fail-stop

5. `Asymmetric sensitivity`
   - most sensitive: damping ratio
   - next: extra damping
   - least sensitive: strength

6. `Pose drift under partial relaxation`
   - `50%` gains stay upright
   - visible drift is modest and concentrated in the feet
   - policy-disabled idle greatly reduces that foot drift

### Movement

1. `Per-bone stability contribution`
   - still lower-body first
   - first angular spike shifts to `ball_r`
   - first linear spike shifts to `foot_l`

2. `PD authority vs. policy authority`
   - movement changes this answer materially
   - the `ActionScale 0.0` proxy is no longer clearly safer; PD alone still reaches large spikes
   - conclusion: under translation, both policy and PD are load-bearing enough that the idle answer does not carry over

3. `Onset-to-collapse duration`
   - movement introduces a real degradation zone
   - first instability in the uniform down sweep appears around `multiplier=0.44`

4. `Recovery test`
   - movement recovery still avoids fail-stop
   - but it is much harsher than idle and produces large late spikes

5. `Asymmetric sensitivity`
   - movement raises sensitivity across every sweep
   - damping ratio remains one of the most important levers
   - strength-only is still the least destructive single-parameter relaxation

6. `Pose drift under partial relaxation`
   - movement converts the small idle foot looseness into very large lower-body drift
   - end-of-sweep local foot drift reaches roughly `80-90 cm`
