# Thigh PhysicsControl Mitigation Diagnostics Evidence

**Task:** S2-IMPL-V0-THIGH-PHYSICSCONTROL-MITIGATION-DIAGNOSTICS-01  
**Run:** 2026-05-07T02:14:55Z to 2026-05-07T02:16:08Z  
**Log reference:** `PhysAnimUE5/Saved/Logs/PhysAnimUE5.log`  
**Command:** `.\scripts\build.ps1 -Test PhysAnim.Diagnostics.ThighRestore`  
**Result:** all diagnostic variants completed with `Result={Success}`.

## Diagnostic Coverage

| Variant | Purpose | Result | Log lines |
| --- | --- | --- | --- |
| `Abrupt_0.01` | Abrupt thigh strength sweep | Passed | 1065-2206 |
| `Abrupt_0.02` | Abrupt thigh strength sweep | Passed | 3294-4347 |
| `Abrupt_0.05` | Abrupt thigh strength sweep | Passed | 5387-6440 |
| `Abrupt_0.10` | Abrupt thigh strength sweep | Passed | 7480-8533 |
| `Abrupt_0.15` | Abrupt thigh strength sweep | Passed | 9574-10633 |
| `Abrupt_0.20` | Abrupt thigh strength sweep | Passed | 11673-12732 |
| `KineticGate_ForcedHold_0.20` | Diagnostic-only gate hold using `p.PhysAnim.V0KineticGateThresholdDegPerSec=-1.0` | Passed | 13772-16268 |
| `Ramp_0.20_0.5s` | Ramp 0 to 0.20 over 0.5s | Passed | 18751-19808 |
| `Ramp_0.20_1.0s` | Ramp 0 to 0.20 over 1.0s | Passed | 20847-21906 |
| `PhysAnim.Diagnostics.ThighRestoreContract` | Deterministic variant coverage guard | Passed | 22951 |

## Summary Fields

Each `THIGH_RESTORE_VARIANT_SUMMARY` line now includes:

- terminal reason, duration, first spine spike time/body, first support failure time
- first linear/angular threshold time/body
- angular velocity snapshots at 0.05, 0.10, 0.15, 0.20, 0.30, 0.60, and 1.00 seconds
- thigh angular strength, angular damping, linear strength magnitude, pose-target seeded state
- kinetic gate release count and release snapshot fields
- positive/negative thigh work accumulators

Representative forced-hold summary at line 18744:

```text
THIGH_RESTORE_VARIANT_SUMMARY variant=7 terminalReason=0 duration=6.008 firstSpineSpikeT=-1.000 firstSpineSpikeBody=None firstSupportFailT=-1.000 firstLinearThresholdT=-1.000 firstLinearThresholdBody=None firstAngularThresholdT=-1.000 firstAngularThresholdBody=None maxAngVel005=0.0 maxAngVel010=0.0 maxAngVel015=0.0 maxAngVel020=0.0 maxAngVel030=0.0 maxAngVel060=0.0 maxAngVel100=0.0 thighAngStr=0.0000 thighAngDamp=1.8750 thighLinStr=0.0000 poseSeeded=0 gateReleaseCount=0 pelvisAngVelAtRelease=-1.00 maxSpineAngVelAtRelease=-1.00 thighStrAtRelease=-1.0000 activationTAtRelease=-1.000 positiveWork=0.000000 negativeWork=0.000000 samples=721
```

## Kinetic Gate Evidence

The normal sweep/ramp variants did not exceed the default gate threshold and therefore did not produce hold/release events. The forced-hold diagnostic variant intentionally sets the threshold to `-1.0` for that test only and resets it afterward. It produced repeated hold lines for both thighs, for example:

```text
KINETIC_GATE_HOLD bone=thigh_l P=0.0 S1=0.0 S2=0.0 S3=0.0 thresh=-1.0 activationT=5.475
KINETIC_GATE_HOLD bone=thigh_r P=0.0 S1=0.0 S2=0.0 S3=0.0 thresh=-1.0 activationT=5.475
```

## Interpretation

The tested abrupt envelope `0.01, 0.02, 0.05, 0.10, 0.15, 0.20` and ramp envelope `0->0.20` over `0.5s` and `1.0s` did not produce a spine spike or support failure in this run. The positive/negative work accumulators remained zero because the sampled bodies were kinematic with zero angular velocity in the diagnostic window; the evidence therefore supports "no energy added in the tested static run", not a dynamic energy-removal claim.

No broad substepping, passive damping, PHC tuning, action scale, support threshold, locomotion, CMC, capsule, shell assist, or non-V0 upper body constraint changes were introduced.
