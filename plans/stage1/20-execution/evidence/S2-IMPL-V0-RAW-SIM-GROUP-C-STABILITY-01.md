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
