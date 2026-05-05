# S2-IMPL-LIVE-POLICY-ACTIVITY-EVIDENCE-01

## Purpose

Add live proof evidence that policy inference, conditioned actions, and PhysicsControl target writes are active during the activated-standing hold, remove the non-physical actor-offset perturbation fallback, and make the stricter live-balance proof checks opt-in until the raw-simulation contract is implemented.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Inference.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.ModifierTracking.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimProofArtifactEmitter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-LIVE-POLICY-ACTIVITY-EVIDENCE-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-LIVE-POLICY-ACTIVITY-EVIDENCE-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all model asset files
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files
- all runtime adapter files
- all runtime orchestrator files
- all termination files
- all support truth files
- all failure arbitration files
- all workflow scripts

## Required Work

1. Record `git rev-parse HEAD` as the base.
2. Add artifact snapshot fields for live policy inference counts, action magnitude, control-target write counts/deltas, and sampled body simulation counts/speeds.
3. Populate those fields from the existing component diagnostics during live runtime evidence capture.
4. Extend JSON artifact emission to include the new policy/action/control/body evidence fields.
5. Extend activated-standing stability metrics and proof logs to report the same evidence over the hold window.
6. Remove the actor-offset and character-launch perturbation fallback from `ApplyActivatedStandingPerturbation`; the proof perturbation must be a pelvis impulse on a raw-simulating body or fail.
7. Add deterministic artifact snapshot coverage for copying the new fields.
8. Add opt-in strict activated-standing stability and perturbation proof checks so `p.PhysAnim.StrictLivePolicyProofQuality=1` requires live policy/control samples, simulating body telemetry, and a measured physical perturbation response. The default required suite must continue to pass while logging these fields.
9. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Validators.ArtifactSnapshot.BuildRunArtifactSnapshot`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-LIVE-POLICY-ACTIVITY-EVIDENCE-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The work requires model, ONNX, asset, PoseSearch, mass, or PhysicsControl redesign changes.
2. The work requires runtime adapter, orchestrator, termination, support truth, or arbitration edits.
3. A physical perturbation cannot be represented without changing architecture.
4. The activated-standing proof cannot pass without reintroducing actor movement fallback.
5. Any required test fails after the smallest allowed instrumentation or perturbation fix.
6. Scope check fails.
