# S2-IMPL-POLICY-ACTION-CONTROL-TARGET-CONTRACT-01

## Purpose

Add deterministic contract coverage for the policy-action to control-target seam before enabling any real locomotion movement.

This task keeps the bridge small by validating existing pure bridge logic instead of adding a new runtime subsystem.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.CoreTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-POLICY-ACTION-CONTROL-TARGET-CONTRACT-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-POLICY-ACTION-CONTROL-TARGET-CONTRACT-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all runtime component files
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
2. Add or extend deterministic bridge tests for policy action conditioning.
3. Add or extend deterministic bridge tests proving `ConvertModelActionsToControlRotations` emits exactly the controlled bone contract and no extra targets.
4. Add or extend deterministic bridge tests proving target rotations are normalized and bounded after conditioning.
5. Add or extend deterministic bridge tests proving `LimitControlRotationStep` respects the configured maximum angular step.
6. Fix only pure bridge logic if a new deterministic test exposes a contract bug.
7. Do not edit runtime component, locomotion, adapter, termination, validator, support, arbitration, asset, ONNX, PoseSearch, mass, or PhysicsControl files.
8. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionConditioningContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ActionToBoneMappingContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.Bridge.ControlTargetStepLimitContract`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.LocomotionActiveShell`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-POLICY-ACTION-CONTROL-TARGET-CONTRACT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The work requires runtime component or locomotion state-machine edits.
2. The work requires adapter, termination, validator, support truth, or arbitration edits.
3. The work requires asset, ONNX, PoseSearch, mass, or PhysicsControl redesign changes.
4. Any new test requires visual tuning or real locomotion movement.
5. Any required test fails after the smallest allowed bridge fix.
6. Scope check fails.
