# S2-IMPL-POLICY-INFERENCE-FINITE-BUFFER-CONTRACT-01

## Purpose

Add deterministic finite-value validation for policy inference buffers before running NNE inference.

This fixes the current model-boundary gap where `terrain` is a bound input tensor but is not checked for NaN/Inf before `RunSync`.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.CoreTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Inference.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-POLICY-INFERENCE-FINITE-BUFFER-CONTRACT-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-POLICY-INFERENCE-FINITE-BUFFER-CONTRACT-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all locomotion state-machine files
- all runtime adapter files
- all runtime orchestrator files
- all model asset files
- all termination files
- all validators
- all support truth files
- all failure arbitration files
- all assets
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files
- all workflow scripts

## Required Work

1. Record `git rev-parse HEAD` as the base.
2. Add a pure bridge helper that validates a named float buffer contains only finite values.
3. Add deterministic automation coverage for finite buffers, NaN buffers, Inf buffers, and error naming.
4. Route `UPhysAnimComponent::RunInference` through the helper for `self_obs`, `mimic_target_poses`, `terrain`, and model action output.
5. Preserve existing error semantics where practical, but ensure `terrain` NaN/Inf fails before `RunSync`.
6. Do not edit locomotion, adapters, termination, validators, support truth, arbitration, assets, ONNX, PoseSearch, mass, or PhysicsControl files.
7. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.FiniteFloatBufferContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.InputDescriptorContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionOutputDescriptorContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionConditioningContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-POLICY-INFERENCE-FINITE-BUFFER-CONTRACT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The work requires locomotion state-machine edits.
2. The work requires adapter, termination, validator, support truth, or arbitration edits.
3. The work requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. The work requires running visual tuning or real locomotion movement.
5. Any required test fails after the smallest allowed bridge/inference fix.
6. Scope check fails.
