# Determinism and Environment Fingerprint

`environment_fingerprint.py` records the authority needed to interpret repeated Unreal runs. It is automatically invoked by `run_ue_automation_episode.ps1` after a completed fixture.

The fingerprint contains:

- source commit and dirty-tree status;
- protocol identity, version, status, fixed timestep, and determinism fields;
- SHA-256 of the protocol and ONNX model;
- SHA-256 of the UE project descriptor, plugin descriptor, engine/game config, and package lock when present;
- UE engine association and plugin version;
- CPU, GPU, RAM, OS, render mode, and RHI from the Unreal automation report when available;
- Python/runtime architecture used by the evidence tooling;
- one canonical authority digest over the source identity, protocol identity, key settings, and artifact hashes.

## Determinism levels

The project must state which of these it is claiming:

1. **Byte determinism** — selected raw evidence streams and authority artifacts have exact hashes.
2. **Numerical determinism** — values are equal within a preregistered tolerance.
3. **Behavioral determinism** — protocol verdicts match and endpoint variation stays within locked bounds.
4. **Process scope** — same-process, cross-process, or cross-machine.

These claims are not interchangeable. A metadata timestamp can break byte identity without changing behavior; conversely, identical summary verdicts can hide materially different raw trajectories.

## CLI

```powershell
python .\scripts\environment_fingerprint.py `
  --repo-root . `
  --protocol product-gates\scripted-locomotion.v2.json `
  --model PhysAnimUE5\Content\NNEModels\phc_policy.onnx `
  --automation-report test-results\...\automation-report\index.json `
  --output test-results\...\environment-fingerprint.json
```

For authoritative bundles, compare the `authority_digest_sha256` across all repetitions before comparing behavior. A mismatch means the runs are not repetitions of the same authority and the bundle should be considered malformed.
