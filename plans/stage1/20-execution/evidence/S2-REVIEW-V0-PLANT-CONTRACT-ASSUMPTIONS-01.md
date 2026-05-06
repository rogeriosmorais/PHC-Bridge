# S2-REVIEW-V0-PLANT-CONTRACT-ASSUMPTIONS-01 Evidence

Base: `b4716926eda0`

## Result

Acceptance is not standing pass. The review result is:

- A explodes: the 10-body V0 raw-sim plant cannot hold a static authored standing target with PHC disabled.
- Therefore the next blocker is PhysicsControl/raw-body plant setup, target continuity, collision/contact setup, or support-body actuation, before PHC policy tuning.
- Review-only `physanim.EnableInstabilityFailStop=0` was used to keep the evidence harness from aborting on expected instability log errors. It is not a balance assist. CMC remained disabled, raw-sim diagnostic group was Group C, and the V0 named-body sim masks were collected.

## Experiments

### A. Static target, PHC disabled

`V0PlantReview[A_StaticTarget_NoPHC]`: mode=1, runtimeState=11, terminalReason=0, samples=32, duration=0.27, supportHull[min/mean/max]=0.00/1850.62/2253.82, activeSides[min/mean/max]=0.00/1.81/2.00, maxBodyLinear=8779.86(pelvis), maxBodyAngular=48700.75(spine_01), spine01[lin=6827.90 ang=48700.75], spine03[lin=8033.17 ang=11231.87], firstSpineSpike=0.11(spine_01), firstSupportFailure=0.27, policy[inference=0 actionSamples=0 rawAbsMax=0.000 conditionedAbsMax=0.000], targets[samples=0 normal=0 total=0 maxDelta=0.00 maxRawOffset=0.00], bodies[samples=704 simMax=10 excludedSimMax=0 criticalValid=0x3f criticalSim=0x3f supportValid=0xf supportSim=0xf nonzeroVelocitySamples=30], continuityValid=1.

### B. PHC enabled, actions forced zero

`V0PlantReview[B_PHC_ZeroActions]`: mode=2, runtimeState=11, terminalReason=0, samples=422, duration=3.52, supportHull[min/mean/max]=0.00/1896.85/2278.19, activeSides[min/mean/max]=0.00/1.97/2.00, maxBodyLinear=2898.74(spine_03), maxBodyAngular=12499.22(ball_r), spine01[lin=2590.94 ang=5863.63], spine03[lin=2898.74 ang=5156.92], firstSpineSpike=0.14(spine_01), firstSupportFailure=3.52, policy[inference=53 actionSamples=53 rawAbsMax=0.000 conditionedAbsMax=0.000], targets[samples=53 normal=424 total=424 maxDelta=1.10 maxRawOffset=0.00], bodies[samples=9284 simMax=10 excludedSimMax=0 criticalValid=0x3f criticalSim=0x3f supportValid=0xf supportSim=0xf nonzeroVelocitySamples=420], continuityValid=1.

### C. PHC enabled, current actions

`V0PlantReview[C_PHC_CurrentActions]`: mode=0, runtimeState=12, terminalReason=0, samples=465, duration=3.88, supportHull[min/mean/max]=1214.44/1999.96/2259.14, activeSides[min/mean/max]=2.00/2.00/2.00, maxBodyLinear=2338.05(spine_03), maxBodyAngular=11493.81(ball_r), spine01[lin=2215.99 ang=5405.05], spine03[lin=2338.05 ang=4033.24], firstSpineSpike=0.14(spine_01), firstSupportFailure=-1.00, policy[inference=59 actionSamples=59 rawAbsMax=0.090 conditionedAbsMax=0.004], targets[samples=59 normal=472 total=472 maxDelta=3.83 maxRawOffset=0.87], bodies[samples=10230 simMax=10 excludedSimMax=0 criticalValid=0x3f criticalSim=0x3f supportValid=0xf supportSim=0xf nonzeroVelocitySamples=463], continuityValid=1.

### D. Tiny synthetic actions

`V0PlantReview[D_TinySyntheticActions]`: mode=3, runtimeState=11, terminalReason=0, samples=272, duration=2.27, supportHull[min/mean/max]=0.00/1896.46/2245.37, activeSides[min/mean/max]=0.00/1.96/2.00, maxBodyLinear=2703.43(spine_03), maxBodyAngular=11980.19(ball_r), spine01[lin=2106.69 ang=5658.70], spine03[lin=2703.43 ang=3931.49], firstSpineSpike=0.14(spine_01), firstSupportFailure=2.27, policy[inference=34 actionSamples=34 rawAbsMax=0.010 conditionedAbsMax=0.010], targets[samples=34 normal=272 total=272 maxDelta=3.51 maxRawOffset=0.00], bodies[samples=5984 simMax=10 excludedSimMax=0 criticalValid=0x3f criticalSim=0x3f supportValid=0xf supportSim=0xf nonzeroVelocitySamples=270], continuityValid=1.

## Review Answers

1. Can the 10-body V0 raw-sim plant hold a static authored standing target with PHC disabled?

No. Case A fails in 0.27s with support hull dropping to 0.00 cm2, first support failure at 0.27s, pelvis max linear speed 8779.86 cm/s, spine_01 max angular speed 48700.75 deg/s, and first major spine spike at 0.11s.

2. With zero policy action, do `spine_01` and `spine_03` remain bounded?

No. Case B still reaches spine_01 2590.94 cm/s and 5863.63 deg/s, spine_03 2898.74 cm/s and 5156.92 deg/s, with first spine spike at 0.14s and support failure at 3.52s. PHC inference ran 53 times, but raw and conditioned actions were both zero, so the instability remains without policy action.

3. With tiny synthetic actions, does PhysicsControl response scale smoothly, or does it spike?

It spikes. Case D used raw/conditioned action magnitude 0.010 but still produced spine_03 max linear speed 2703.43 cm/s, ball_r max angular speed 11980.19 deg/s, max target delta 3.51 deg, first spine spike at 0.14s, and support failure at 2.27s.

4. Does the PHC observation vector match ProtoMotions/training semantics?

The local source asserts the intended body order and tensor semantics, but this review does not fully prove identity with the training artifact. `PhysAnimBridge.cpp:74` declares the observation body order must match ProtoMotions `smpl.yaml` DFS order, and the SMPL reference maps pelvis, thighs, spine, feet, and balls to the same V0 names. `PhysAnimComponent.Observation.cpp:16` uses `GetSmplObservationBoneNames`; lines 30-52 sample UE physics linear/angular velocities, apply rest correction, and convert positions, rotations, and velocities into Proto runtime coordinates. `PhysAnimBridge.cpp:546` builds self observation with root height, heading-local body positions, tan-normalized rotations, and root-heading-local velocities. `PhysAnimBridge.cpp:735` builds terrain as root height minus sampled ground heights. Risks remain around coordinate handedness/rest correction, exact normalization parity, and the fact that support/contact assumptions are runtime proof surfaces rather than explicit self-observation/action tensors.

5. Does the action vector mean what `ApplyControlTargets` thinks it means?

Internally, yes, with remaining training-distribution risk. `PhysAnimBridge.cpp:817` converts model actions to control rotations as SMPL expmap joint rotations. `PhysAnimComponent.ModifierTracking.cpp:219` feeds conditioned actions through that conversion, and lines 321 and 544 write `SetControlTargetOrientation` into PhysicsControl. The bridge interpretation is coherent, but this task does not prove the exported policy's action scale/distribution matches the runtime gains and clamps.

6. Does support fail before or after the first major spine velocity spike?

After, in the failing cases. A spikes at 0.11s and support fails at 0.27s. B spikes at 0.14s and support fails at 3.52s. D spikes at 0.14s and support fails at 2.27s. C spikes at 0.14s but support remains valid through 3.88s. The first major energy injection precedes support loss.

7. Are COM/proxy and support hull measuring real balance, or merely a convenient proxy?

They are useful truth surfaces, but not sufficient balance proof. Case C keeps support valid while body velocities remain unbounded for standing, and A/B/D show support failure after the spine spike. A real V0 standing proof must require support/proxy validity together with bounded nonzero body motion, named-body raw sim coverage, PHC/control activity, and no CMC/shell/actor-offset assistance.

## Commands

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS.
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions`: PASS after the review harness was made non-red for expected instability evidence.
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.B_PHC_ZeroActions`: PASS.
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.C_PHC_CurrentActions`: PASS.
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.V0PlantContractAssumptions.D_TinySyntheticActions`: PASS.
- `python .\scripts\read_logs.py`: run after automation.

Initial review automation attempts exposed missing sample capture and expected instability log-error aborts. Those were corrected in the harness; no fail-by-design test was left in the main suite.
