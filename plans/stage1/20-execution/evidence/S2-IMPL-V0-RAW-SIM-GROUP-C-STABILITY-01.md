# S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01 Evidence

Base: `00d6c77c7c25`

## Result

Partial diagnostic progress. This pass does not claim a 3s standing proof and does not tune PHC, policy action scale, observation/action semantics, support thresholds, progressive activation, CMC, capsule, or shell assistance.

The Case A failure is now tied to the raw-sim enable transition rather than to policy action or target writes.

## Instrumentation Added

- Logs the exact activation-time transition when each V0 Group C body first becomes raw-simulating.
- Captures per-body before/after body state: world transform, BodyInstance transform, linear/angular velocity, collision, movement type, physics blend, update-kinematic-from-simulation, mass, inertia, awake state, and overlap/penetration summaries.
- Logs world, capsule, and skeletal-body overlap counts and names at raw-sim enable.
- Logs the first major spine velocity spike with support hull and active support side state at the spike.
- Logs Group C completion with `simBodies=10` and `excludedSimMax`.

## Verification

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.A_StaticTarget_NoPHC`: PASS
- `python .\scripts\read_logs.py`: run after automation

## Case A Evidence

Mode: V0 Group C raw sim, PHC disabled, static authored target, no action samples, no control target writes.

Automation summary:

```text
V0PlantReview[A_StaticTarget_NoPHC]: mode=1 runtimeState=11 terminalReason=0 samples=34 duration=0.29 supportHull[min/mean/max]=0.00/1482.32/2178.47 activeSides[min/mean/max]=0.00/1.65/2.00 maxBodyLinear=13916.87(pelvis) maxBodyAngular=64783.49(spine_01) spine01[lin=11300.64 ang=64783.49] spine03[lin=7517.74 ang=21419.10] firstSpineSpike=0.08(spine_01) firstSupportFailure=0.29 policy[inference=0 actionSamples=0 rawAbsMax=0.000 conditionedAbsMax=0.000] targets[samples=0 normal=0 total=0 maxDelta=0.00 maxRawOffset=0.00] bodies[samples=748 simMax=10 excludedSimMax=0 criticalValid=0x3f criticalSim=0x3f supportValid=0xf supportSim=0xf nonzeroVelocitySamples=32] continuityValid=1
```

Raw-sim Group C completion:

```text
[PhysAnimV0] RAW_SIM_GROUP_C_COMPLETE activationT=0.000 runtimeState=BalanceActive_Standing simBodies=10 excludedSimMax=0 bodies=pelvis,spine_01,spine_02,spine_03,thigh_l,thigh_r,foot_l,foot_r,ball_l,ball_r
```

First major spine spike:

```text
[PhysAnimV0] FIRST_MAJOR_SPINE_SPIKE bone=spine_01 activationT=0.078 runtimeState=BalanceActive_Standing lin=445.33 angDeg=5966.20 sim=1 awake=1 collision=3 supportHull=2178.47 activeSides=2 xfLoc=(-201.82,-168.72,307.19) xfRot=(38.82,22.41,83.62)
```

Raw-sim enable highlights:

- All 10 V0 bodies become raw-simulating at `activationT=0.000`.
- All 10 V0 bodies report zero linear and angular velocity before and immediately after raw-sim enable.
- `spine_01` reports `mass=0.005 kg`, inertia approximately `(0.00,0.00,0.00)`, and skeletal overlaps with `pelvis` and `spine_02`.
- `spine_02` overlaps `pelvis`, `spine_01`, and `spine_03`.
- `spine_03` overlaps `pelvis` and `spine_02`.
- `pelvis` overlaps `thigh_l`, `thigh_r`, `spine_01`, `spine_02`, and `spine_03`.
- `foot_l`, `foot_r`, `ball_l`, and `ball_r` each overlap the world floor at activation and overlap adjacent skeletal bodies.
- Capsule overlap count is zero for the logged V0 bodies.

## Interpretation

Case A excludes PHC and target writes as the first energy source: `policy[inference=0 actionSamples=0]` and `targets[samples=0 normal=0 total=0]`.

The first major spine spike occurs at `0.078s`, after all 10 V0 bodies are raw-simulating together at `0.000s` and before the first support failure at `0.29s`. At the spike, support is still valid enough to report `supportHull=2178.47` and `activeSides=2`.

The strongest current explanation is raw-sim enable into an already interpenetrating articulated body graph, amplified by the pathological `spine_01` physical body properties (`0.005 kg` with near-zero inertia). The failure is tied to the raw-sim state transition and initial skeletal-body overlaps/constraint solve, not to policy inference, action conditioning, control target writes, support thresholding, CMC, capsule, or shell assistance.

## Remaining Gap

This pass reduces the uncertainty around the Case A failure but does not yet reduce the physical spike itself. The next smallest technical work should stay in the plant/collision/physical-asset path and explain whether `spine_01` mass/inertia, initial torso interpenetration, or constraint impulses are authoritative for the first spike.

## Hip Quarantine Release Diagnostic

Context: editor-side physics asset changes were present in `PA_Mannequin.uasset` and were intentionally left unstaged. Those changes corrected `spine_01` to sane mass/inertia and disabled relevant V0 spine projection/collision. This code pass only adds release-sequence instrumentation.

Additional instrumentation:

- `HIP_QUARANTINE_RELEASED` now logs frame, world time, activation time, ticks before/after decrement, and whether the next-tick trace is armed.
- `HIP_QUARANTINE_TRACE` logs release-frame pre-tuning, release-frame post-tuning/pre-release, release-frame post-decrement, next-tick pre-tuning, and next-tick post-tuning body snapshots.
- The trace covers pelvis, thighs, V0 spine bodies, feet, and balls with body velocities, transforms, collision, modifier movement/collision/blend/update-kinematic state, support/world/skeletal penetrations, and intended PhysicsControl multiplier state.

Verification:

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.A_StaticTarget_NoPHC`: PASS
- `python .\scripts\read_logs.py`: run after automation

Case A summary with the editor-side physics asset changes:

```text
V0PlantReview[A_StaticTarget_NoPHC]: mode=1 runtimeState=11 terminalReason=0 samples=34 duration=0.28 supportHull[min/mean/max]=0.00/1681.17/2214.42 activeSides[min/mean/max]=0.00/1.76/2.00 maxBodyLinear=7164.29(spine_02) maxBodyAngular=47599.99(spine_01) spine01[lin=7102.64 ang=47599.99] spine03[lin=6175.54 ang=22440.08] firstSpineSpike=0.18(spine_01) firstSupportFailure=0.28 policy[inference=0 actionSamples=0 rawAbsMax=0.000 conditionedAbsMax=0.000] targets[samples=0 normal=0 total=0 maxDelta=0.00 maxRawOffset=0.00] bodies[samples=748 simMax=10 excludedSimMax=0 criticalValid=0x3f criticalSim=0x3f supportValid=0xf supportSim=0xf nonzeroVelocitySamples=32] continuityValid=1
```

Sequence:

- Group C still completes at `activationT=0.000` with `simBodies=10` and `excludedSimMax=0`.
- `HIP_QUARANTINE_RELEASED` occurs at frame `1222`, `activationT=0.150`, `ticksBefore=1`, `ticksAfter=0`.
- On the release frame, thighs are still in the active quarantine tuning path and their intended angular strength is held at `0.0000`.
- On the next tuning tick, frame `1223`, `activationT=0.167`, quarantine is inactive and thigh angular strength returns to `0.2000`.
- No traced body changes movement type, collision, physics blend, or update-kinematic-from-simulation across release or the next tuning tick.
- `FIRST_MAJOR_SPINE_SPIKE` follows at `activationT=0.175`, one tick after thigh control resumes, with `spine_01 lin=3221.39 cm/s`, `ang=7135.92 deg/s`, `supportHull=2145.12`, and `activeSides=2`.
- Support sampling fails later and reaches `PHASE3_ACTIVE_SUPPORT_FAILURE` with `hull_area=0.0`.

Key release-frame / next-tick body telemetry:

- Release frame pre-tuning: `spine_01 linSpeed=330.96`, `angDeg=2429.58`; `spine_03 linSpeed=577.60`, `angDeg=696.79`; `foot_l angDeg=1288.31`; `ball_l linSpeed=100.50`; thighs are below `110 cm/s` and `280 deg/s`.
- Next tick post-tuning: `spine_01 linSpeed=231.28`, `angDeg=2436.87`; `spine_03 linSpeed=460.10`, `angDeg=623.69`; `foot_l angDeg=1177.51`; `ball_l angDeg=812.44`; thighs remain below `75 cm/s` and `200 deg/s`.

Interpretation:

Hip quarantine release is causal in the narrow control-state sense: it is the transition that re-enables thigh angular strength from `0.0000` to `0.2000` on the next tick. It is not causal through hidden simulation authority changes: movement type, collision, blend, and update-kinematic flags remain unchanged for all traced V0 bodies.

The spine is already carrying large angular velocity before the release (`spine_01` around `2430 deg/s`). The release appears to be an amplifier/correlation point rather than the first energy source. The next technical blocker is why the pelvis/thigh/spine/support coupled plant has already accumulated high V0 body motion by `activationT=0.150` and why resumed thigh control lets that existing motion turn into a larger `spine_01` spike and support-body sample loss.

## Early Control Isolation Subvariants

Context: these are Case A subvariants only. PHC remains disabled, action samples remain zero, control target writes remain zero, Group C remains `simMax=10`, and excluded bodies remain `excludedSimMax=0`.

Additional instrumentation:

- `p.PhysAnim.V0PlantEarlyControlZeroGroup` selects a diagnostic-only early zero-strength group for the first `0.30s`: `1=all_v0`, `2=torso`, `3=thighs`, `4=support`.
- `EARLY_CONTROL_SAMPLE` logs pelvis, thighs, feet, balls, and spine bodies at `0.05`, `0.10`, `0.15`, and `0.20s`.
- Each sample includes body speed, applied PhysicsControl multipliers, modifier movement/collision/blend/update-kinematic state, target cache state, support hull/active sides, and penetration summary.
- `EARLY_CONTROL_THRESHOLD` records the first body to exceed `1200 cm/s` linear speed and `3600 deg/s` angular speed for each variant.

Verification:

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.A1_StaticTarget_NoPHC_AllV0ControlsZero`: PASS
- `python .\scripts\read_logs.py`: run after A1
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.A2_StaticTarget_NoPHC_TorsoControlsZero`: PASS
- `python .\scripts\read_logs.py`: run after A2
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.A3_StaticTarget_NoPHC_ThighControlsZero`: PASS
- `python .\scripts\read_logs.py`: run after A3
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.A4_StaticTarget_NoPHC_SupportControlsZero`: PASS
- `python .\scripts\read_logs.py`: run after A4

Summary:

```text
A1 all V0 controls zero:
  duration=0.32, firstSpineSpike=0.31(spine_01), firstSupportFailure=0.32
  maxBodyLinear=3664.39(pelvis), maxBodyAngular=23296.11(spine_01)
  0.15 sample: spine_01 lin=82.78 ang=780.23, spine_02 lin=164.32 ang=2105.99, spine_03 lin=417.61 ang=1866.87
  0.20 sample: spine_01 lin=185.49 ang=802.16, spine_02 lin=275.08 ang=1314.63, spine_03 lin=439.49 ang=1555.92

A2 torso controls zero:
  duration=0.23, firstSpineSpike=0.18(spine_01), firstSupportFailure=0.23
  maxBodyLinear=7418.15(pelvis), maxBodyAngular=17712.39(spine_01)
  first thresholds at 0.183: pelvis lin=3565.19 ang=6193.46, supportHull=1570.54, activeSides=2
  0.15 sample: spine_01 lin=86.81 ang=785.65, spine_02 lin=169.73 ang=2113.19, spine_03 lin=423.31 ang=1881.71
  0.20 sample: spine_01 lin=5090.33 ang=17712.39, spine_02 lin=4403.26 ang=17280.83, spine_03 lin=2640.51 ang=8003.11

A3 thigh controls zero:
  duration=0.43, firstSpineSpike=0.31(spine_01), firstSupportFailure=0.43
  maxBodyLinear=7350.99(pelvis), maxBodyAngular=47395.49(spine_01)
  first angular threshold at 0.100: ball_r lin=312.53 ang=4617.55, supportHull=2211.29, activeSides=2
  first linear threshold at 0.317: pelvis lin=3752.13 ang=8463.45, supportHull=1764.98, activeSides=2
  0.15 sample: spine_01 lin=331.11 ang=2429.87, spine_02 lin=473.07 ang=562.38, spine_03 lin=577.71 ang=696.89
  0.20 sample: spine_01 lin=119.61 ang=2301.79, spine_02 lin=228.87 ang=757.54, spine_03 lin=317.92 ang=486.87

A4 support controls zero:
  duration=0.28, firstSpineSpike=0.18(spine_01), firstSupportFailure=0.28
  maxBodyLinear=7284.75(spine_02), maxBodyAngular=47645.73(spine_01)
  first thresholds at 0.183: pelvis lin=3825.89 ang=8595.21, supportHull=1783.85, activeSides=2
  0.15 sample: spine_01 lin=333.29 ang=2435.43, spine_02 lin=476.20 ang=571.64, spine_03 lin=583.71 ang=709.20
  0.20 sample: spine_01 lin=1889.03 ang=19799.32, spine_02 lin=2332.76 ang=17561.26, spine_03 lin=3441.22 ang=7370.00
```

Target/cache state:

- All subvariants report `currentPoseTargetsSeeded=1`, `previousTargets=21`, `blendStartTargets=21`, and `pendingCachedResets=0`.
- The Case A summaries keep `policy[inference=0 actionSamples=0 rawAbsMax=0 conditionedAbsMax=0]`.
- The Case A summaries keep `targets[samples=0 normal=0 total=0 maxDelta=0 maxRawOffset=0]`.

Interpretation:

Early motion is not PHC/policy driven and is not caused by control target writes. It is present with PHC disabled and no action samples.

Passive raw physics/contact/constraint settling contributes immediately: A1, with all traced V0 control strengths zero for the first `0.30s`, still shows nonzero support/body motion and support-body angular speeds by `0.05-0.15s`. That motion remains much smaller through `0.20s` and the major spine spike is delayed until after the zero window ends.

Torso controls are not sufficient to explain the early catastrophic spike. In A2, torso angular strengths are zero, but active thigh/support control still produces the same early failure shape: first major spine spike at `0.175s`, first pelvis threshold at `0.183s`, and support failure at `0.23s`.

Support controls are not sufficient to explain the early catastrophic spike. In A4, foot/ball angular strengths are zero, but active thigh/torso control still produces first major spine spike at `0.175s` and pelvis thresholds at `0.183s`.

Thigh/hip coupling is the strongest current causal lever. A3, with thigh controls zero, delays the major spine spike to `0.308s` and support failure to `0.43s`, similar to A1. This matches the earlier hip-quarantine finding: re-enabling thigh angular strength amplifies already accumulated pelvis/spine/support motion into the spine spike.

The first energy source before `0.150s` is best described as passive raw-sim contact/constraint settling in an already interpenetrating V0 plant, with active thigh/hip PhysicsControl coupling acting as the early amplifier. Support-body drive contributes local foot/ball angular churn, but zeroing it does not prevent the pelvis/spine threshold. Torso drive contributes once active, but zeroing torso alone does not prevent the early pelvis-driven failure.
