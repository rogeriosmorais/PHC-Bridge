# S2-IMPL-POLICY-ACTION-OUTPUT-DESCRIPTOR-CONTRACT-01

## Purpose

Add a deterministic contract for the NNE policy action output descriptor before runtime locomotion depends on model output.

This remains a pure bridge/model-boundary task: no real walking, no runtime tuning, and no asset or ONNX edits.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.CoreTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Model.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-POLICY-ACTION-OUTPUT-DESCRIPTOR-CONTRACT-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-POLICY-ACTION-OUTPUT-DESCRIPTOR-CONTRACT-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all locomotion state-machine files
- all runtime adapter files
- all runtime orchestrator files
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
2. Add a pure bridge helper that validates the policy output tensor descriptor contract.
3. The contract must require exactly one output tensor.
4. The output tensor must be `Float`.
5. The output tensor must be rank 2 with batch dimension `1` or dynamic `-1`, and action dimension `PhysAnimBridge::NumActionFloats`.
6. Add deterministic automation coverage for valid and invalid descriptors.
7. Route `UPhysAnimComponent::ValidateModelDescriptorContract` through the helper.
8. Do not edit locomotion, adapters, termination, validators, support truth, arbitration, assets, ONNX, PoseSearch, mass, or PhysicsControl files.
9. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionOutputDescriptorContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.TensorIndexMap`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionConditioningContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionToBoneMappingContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-POLICY-ACTION-OUTPUT-DESCRIPTOR-CONTRACT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The work requires locomotion state-machine edits.
2. The work requires adapter, termination, validator, support truth, or arbitration edits.
3. The work requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. The real model descriptor fails the new contract and fixing it would require asset or ONNX edits.
5. Any required test fails after the smallest allowed bridge/model-boundary fix.
6. Scope check fails.
