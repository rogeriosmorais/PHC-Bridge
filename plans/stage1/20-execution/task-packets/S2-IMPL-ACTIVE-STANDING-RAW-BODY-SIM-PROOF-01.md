# S2-IMPL-ACTIVE-STANDING-RAW-BODY-SIM-PROOF-01

## Purpose

Prove balance-critical bodies are raw-simulating during activated standing, and move the enforcement to the authoritative PhysicsControl/body tuning path.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.PhysicsTuning.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVE-STANDING-RAW-BODY-SIM-PROOF-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-ACTIVE-STANDING-RAW-BODY-SIM-PROOF-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all model asset files
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- all runtime adapter files
- all runtime orchestrator files
- all termination files
- all support truth files
- all failure arbitration files
- all workflow scripts
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.ModifierTracking.cpp`

## Required Work

1. Record `git rev-parse HEAD` as the base.
2. Enforce activated-standing raw simulation in `PhysAnimComponent.PhysicsTuning.cpp`, where bridge authority establishes PhysicsControl/body modifier state.
3. During `BalanceActive_Standing`, require valid `FBodyInstance` coverage for pelvis, `spine_01`, `spine_02`, `spine_03`, `thigh_l`, and `thigh_r`.
4. During `BalanceActive_Standing`, require those critical bodies to report `IsInstanceSimulatingPhysics() == true`.
5. Track support body coverage for `foot_l`, `foot_r`, `ball_l`, and `ball_r`; they must be simulating unless the code emits an explicit V0 support-body justification.
6. Keep physical perturbation as pelvis/body impulse only; do not reintroduce actor offset or CharacterMovement launch fallback.
7. After perturbation, require nonzero body velocity telemetry.
8. Make the strict standing proof pass only when raw body telemetry is nonzero and physical continuity remains valid.
9. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.Perturbation`
- strict proof-quality run for `PhysAnim.ActivatedStanding.StabilityMetrics` with `p.PhysAnim.StrictLivePolicyProofQuality=1`
- strict proof-quality run for `PhysAnim.ActivatedStanding.Perturbation` with `p.PhysAnim.StrictLivePolicyProofQuality=1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVE-STANDING-RAW-BODY-SIM-PROOF-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The work requires model, ONNX, asset, PoseSearch, mass, runtime adapter, runtime orchestrator, support truth, termination, or failure arbitration edits.
2. The proof cannot be made strict without reintroducing actor movement fallback, CharacterMovement authority, capsule collision authority, or global blend assistance.
3. The support bodies cannot raw-simulate and no explicit V0 support-body contract can be defended.
4. Any required test fails after one narrow PhysicsControl/body-tuning fix.
5. Scope check fails.
