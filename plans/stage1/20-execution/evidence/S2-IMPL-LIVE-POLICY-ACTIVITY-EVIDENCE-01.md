# S2-IMPL-LIVE-POLICY-ACTIVITY-EVIDENCE-01 Evidence

Base: `dbd682deaa5e5a5365a0a3611fda38aeec1d102f`

## Build

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1`: PASS

## Tests

- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Validators.ArtifactSnapshot.BuildRunArtifactSnapshot`: PASS
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`: PASS
- `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`: PASS

Logs were read with `python .\scripts\read_logs.py` after automation tests.

## Proof-Quality Evidence

Default stability proof now emits live policy/control/body telemetry:

```text
policy[inference=959 actionSamples=959 rawAbsMax=0.087 conditionedAbsMax=0.009 clampedMax=0]
targets[samples=959 normal=7672 total=7672 maxDelta=6.15 meanDeltaMax=0.85 maxRawOffset=4.72 meanRawOffsetMax=2.62]
bodies[samples=163526 simMax=0]
maxBodyLinear=0.00 maxBodyAngular=0.00
```

This proves NNE/action/control-target activity is present, while also exposing that the required runtime bodies are still not raw-simulating during the hold.

The perturbation path no longer falls back to actor offset or CharacterMovement launch. In the current default scene it logs:

```text
Activated standing perturbation requires a raw-simulating pelvis impulse; no actor offset fallback was applied.
```

Strict live-balance proof assertions are present but opt-in with:

```text
p.PhysAnim.StrictLivePolicyProofQuality=1
```
