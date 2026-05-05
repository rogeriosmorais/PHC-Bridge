# S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01 Evidence

Base: `46d881b6b8edc63182ea475f16277114c5ee97d6`

## Result

Incomplete. The diagnostic path now enforces and exposes the V0 raw-sim standing body contract, but Group C is not yet stable enough to satisfy the final standing proof.

## Changes

- Live support evidence now includes `ball_l` and `ball_r` contacts in addition to the configured feet.
- Activated-standing proxy evidence can use the mass-weighted COM of currently raw-simulating V0 bodies instead of the capsule/actor proxy.
- Proxy-outside support failure now uses the drift-duration threshold, not a single outside-hull frame.
- Raw-sim bisect disables CMC during activation and applies test-only conservative PHC/PhysicsControl tuning.
- Stability telemetry now records the fastest body names for max linear/angular speed and tracks `excludedSimMax` for non-V0 required bodies.
- Strict proof-quality assertions now fail if any non-V0 required body simulates during the V0 standing proof path.

## Verification

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.RawSimBisect`: PASS
- `python .\scripts\read_logs.py`: run after automation

## Latest RawSimBisect Evidence

- Group A, pelvis/spine: `simMax=4`, `excludedSimMax=0`, `criticalSim=0xf`, `supportSim=0x0`, support stayed two-sided, max linear `786.49 cm/s (spine_03)`, max angular `2772.24 deg/s (spine_01)`.
- Group B, add thighs: `simMax=6`, `excludedSimMax=0`, `criticalSim=0x3f`, `supportSim=0x0`, lasted `0.20s`, max linear `1295.89 cm/s (spine_03)`, max angular `5657.07 deg/s (spine_01)`.
- Group C, V0 set: `simMax=10`, `excludedSimMax=0`, `criticalSim=0x3f`, `supportSim=0xf`, PHC/control live, nonzero velocities, but only lasted `0.75s`; support dropped to `0.00`, active sides dropped to `0`, max linear `2638.51 cm/s (spine_03)`, max angular `5262.03 deg/s (spine_01)`.
- Group D, full required set: `simMax=22`, `excludedSimMax=12`, support degraded quickly, max linear `2717.43 cm/s (hand_r)`, max angular `9213.17 deg/s (hand_l)`.

## Interpretation

The V0 contract boundary is now explicit: Group C raw-simulates exactly the 10 V0 standing truth bodies and excludes the remaining required body modifiers from simulation. The remaining blocker is bounded physical behavior, not missing PHC/control activity or hidden CMC/capsule assistance.

The strongest current hypothesis is raw-sim energy injection through the spine/support chain. Group D confirms the non-V0 distal/upper bodies remain catastrophic and must not be accepted as the V0 proof target.

## Remaining Gap

The strict V0 standing proof cannot pass yet because Group C still loses support and exceeds bounded-motion expectations. Next work should isolate whether the dominant cause is support-body contact/collision sampling, target discontinuity at raw-sim enable, or PhysicsControl gains for spine/support bodies.
