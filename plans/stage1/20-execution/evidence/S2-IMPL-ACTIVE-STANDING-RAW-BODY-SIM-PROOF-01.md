# S2-IMPL-ACTIVE-STANDING-RAW-BODY-SIM-PROOF-01 Evidence

Base: `129ffcd761ef873494c7075a4c3baa5169db54a0`

## Build

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS

## Tests

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.RawSimBisect`: PASS
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`: PASS

Logs were read with `python .\scripts\read_logs.py` after automation tests.

## Diagnostic Body-Group Bisect

The diagnostic switch is `p.PhysAnim.RawSimDiagnosticGroup` and defaults to `0` (off). It is used only by the bisect automation path; default standing behavior is unchanged.

```text
RawSimBisect[A_PelvisSpine]: group=1 runtimeState=11 samples=6 duration=0.05 maxBodyLinear=336.75 maxBodyAngular=1129.93 bodies[simMax=4 criticalSim=0xf supportSim=0x0]
RawSimBisect[B_AddThighs]: group=2 runtimeState=11 samples=6 duration=0.05 maxBodyLinear=435.01 maxBodyAngular=2480.54 bodies[simMax=6 criticalSim=0x3f supportSim=0x0]
RawSimBisect[C_AddSupport]: group=3 runtimeState=11 samples=6 duration=0.05 maxBodyLinear=433.64 maxBodyAngular=2905.69 bodies[simMax=10 criticalSim=0x3f supportSim=0xf]
RawSimBisect[D_FullRequired]: group=4 runtimeState=11 samples=6 duration=0.05 maxBodyLinear=10529.90 maxBodyAngular=21147.84 bodies[simMax=22 criticalSim=0x3f supportSim=0xf]
```

All four groups transitioned to `BalanceSafeDeny` through `PHASE3_ACTIVE_SUPPORT_FAILURE`. The catastrophic energy spike is specific to the full required body set: the first three groups are hundreds of cm/s and low thousands of deg/s, while the full required set jumps to `10529.90 cm/s` and `21147.84 deg/s`.

The default stability proof remains unchanged with the diagnostic switch off:

```text
StandingProof: StabilityMetrics ... policy[inference=959 actionSamples=959 rawAbsMax=0.087 conditionedAbsMax=0.009] targets[samples=959 normal=7672 total=7672 maxDelta=6.15] bodies[samples=163526 simMax=0 criticalValid=0x3f criticalSim=0x0 supportValid=0xf supportSim=0x0 nonzeroVelocitySamples=0]
```

## Final Contract

No progressive runtime activation design was added. The final pass still must require all balance-critical and required support bodies raw-simulating together, live PHC inference, nonzero conditioned actions, PhysicsControl target writes, nonzero but bounded body motion, valid support/continuity, and no shell/CMC/actor-offset hidden assistance.
