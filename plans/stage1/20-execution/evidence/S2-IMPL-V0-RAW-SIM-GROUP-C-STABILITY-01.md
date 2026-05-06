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
