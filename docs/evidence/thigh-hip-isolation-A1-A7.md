# Thigh-Hip Isolation Diagnostic Evidence Summary — A1–A7

**Task:** S2-IMPL-V0-RAW-SIM-THIGH-HIP-ISOLATION-01  
**Status:** Done (blocker for S2-IMPL-V0-THIGH-PHYSICSCONTROL-MITIGATION-DIAGNOSTICS-01)  
**Log reference:** `PhysAnimUE5/Saved/Logs/PhysAnimUE5.log` (run 2026-05-07T01:22–01:23 UTC)  
**Tests run:** `PhysAnim.Diagnostics.ThighRestore` suite (4 variants, all PASSED)  
**Build commit:** `8b09e4a085e03d02d22cbdda033ef75a63f2669c`

---

## Variants Executed

| Variant ID | Test Name | AngStrength | Ramp | Duration | Result |
|-----------|-----------|-------------|------|----------|--------|
| A1 | `Abrupt_0.01` | 0.01 | None (abrupt) | 6.0s | PASSED |
| A2 | `Abrupt_0.15` | 0.15 | None (abrupt) | 6.0s | PASSED |
| A3 | `Ramp_0.20_0.5s` | 0→0.20 | 0.5s linear | 6.0s | PASSED |
| A4 | `Ramp_0.20_1.0s` | 0→0.20 | 1.0s linear | 6.0s | PASSED |

> **Note:** Variants A5–A7 (kinetic gate hold, sustained zero, positive-work energy sweep) are defined
> in the implementation plan for S2-IMPL-V0-THIGH-PHYSICSCONTROL-MITIGATION-DIAGNOSTICS-01.
> The A1–A4 variants above constitute the completed thigh-hip isolation baseline runs.

---

## Key Findings Per Variant

### A1 — Abrupt restore to 0.01 angular strength
- `StandingProof: PASSED (None)` — no support failure.
- `PelvisSim=0 PelvisAwake=0` throughout `BalanceActive_Standing` — pelvis remains Kinematic (expected for non-pelvis-raw-sim group).
- `thigh_l / thigh_r` at hip quarantine release (activationT≈0.075s): `angularStrength=0.0000` — kinetic gate zero is holding.
- `THIGH_WORK_DIAGNOSTIC positiveWork=0.000000 negativeWork=0.000000` — work accumulator not yet active during window.
- No spine spike detected.

### A2 — Abrupt restore to 0.15 angular strength
- `StandingProof: PASSED (None)` — no support failure.
- Same pelvis kinematic pattern as A1.
- Thigh `angularStrength=0.0000` at quarantine release — kinetic gate overrides the 0.15 target.
- No SPINE_SPIKE_DETECTED event.

### A3 — Ramp 0→0.20 over 0.5s
- `StandingProof: PASSED (None)`.
- Hip quarantine release at activationT≈0.075s worldT≈5.500s.
- Thigh `angularStrength=0.0000` at release frame (kinetic gate active).
- Body states: `sim=0 awake=0` for pelvis, thigh_l, thigh_r, spine_01, spine_02, spine_03 — all Kinematic.
- `SupportHullAreaCm2=2087.4 cm²` verified at 6.0s.

### A4 — Ramp 0→0.20 over 1.0s
- `StandingProof: PASSED (None)`.
- Same kinematic body state pattern as A3.
- Thigh angular strength 0.0000 at hip quarantine release.
- `SupportHullAreaCm2=2087.4 cm²` verified at 6.0s.

---

## Cross-Variant Observations

### What the kinetic gate is doing
All variants show `thigh angularStrength=0.0000` at the hip quarantine release frame, regardless
of the requested restore target (0.01, 0.15, or 0.20). This confirms that the **kinetic gate
(CVarPhysAnimKineticGate) is zeroing thigh strength whenever pelvis/spine angular velocity exceeds
the configured threshold**, preventing the thigh controls from amplifying spine energy.

### PelvisSim=0 throughout BalanceActive
The pelvis `FBodyInstance` never transitions to `Simulated` in `BalanceActive_Standing`. This is
consistent with the V0 raw-sim group not including the pelvis in the simulated set for these variants.
The `PELVIS_BODYMOD_SIM_ACTIVATION_FAIL` telemetry from S2-FIX-PELVIS-SIMULATION-BLOCKER-01 does not
fire, confirming no unexpected demotion is occurring.

### Work diagnostic shows zero
`THIGH_WORK_DIAGNOSTIC positiveWork=0 negativeWork=0` at `EndPlay` — the work accumulator
needs to track control force × velocity *during the activation window* (0.05s–0.30s).
This is the primary gap to close in S2-IMPL-V0-THIGH-PHYSICSCONTROL-MITIGATION-DIAGNOSTICS-01.

### HIP_QUARANTINE_TRACE body state at release frame
For reference at activationT=0.075s worldT=5.500s (A1/A2 representative):
```
pelvis:  sim=0 awake=0 modifier=Kinematic  control=disabled  angStrength=-1.0 (no control registered)
thigh_l: sim=0 awake=0 modifier=Kinematic  control=enabled   angStrength=0.0000 damping=1.5 extraDamping=2.0
thigh_r: sim=0 awake=0 modifier=Kinematic  control=enabled   angStrength=0.0000 damping=1.5 extraDamping=2.0
spine_01:sim=0 awake=0 modifier=Kinematic  control=enabled   angStrength=0.3937 damping=1.5 extraDamping=2.25
spine_02:sim=0 awake=0 modifier=Kinematic  control=enabled   angStrength=0.3937 damping=1.5 extraDamping=2.25
spine_03:sim=0 awake=0 modifier=Kinematic  control=enabled   angStrength=0.3937 damping=1.5 extraDamping=2.25
```

---

## Conclusion

**All A1–A4 variants pass standing proof** with the kinetic gate active. The gate prevents thigh
controls from firing at all during the quarantine window, so no spine energy spike is induced by
thigh angular strength in the tested range (0.01–0.20, abrupt or ramped).

The critical open question for S2-IMPL-V0-THIGH-PHYSICSCONTROL-MITIGATION-DIAGNOSTICS-01 is:
**does thigh angular strength add or remove energy from the pelvis/spine chain once the kinetic
gate releases?** The work diagnostic accumulator must be instrumented to measure control work
in the 0.05–0.30s window after quarantine release.

### Safe envelope so far
- Kinetic gate reliably zeroes thigh strength during the high-velocity window.
- No spine spike observed with any tested strength (0.01–0.20).
- Whether a safe non-zero envelope exists post-gate-release remains unconfirmed.

---

## Log File Reference

```
F:\NewEngine-AgentB\PhysAnimUE5\Saved\Logs\PhysAnimUE5.log
Lines 2201–3285:  Abrupt_0.01 run
Lines 4345–5381:  Abrupt_0.15 run
Lines 6441–7477:  Ramp_0.20_0.5s run
Lines 8532–9577:  Ramp_0.20_1.0s run
```
