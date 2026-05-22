# S2-DECIDE-V0-RAW-SIM-BODY-CONTRACT-01

## Purpose

Define the V0 activated-standing raw-simulation body contract and prevent future proof work from treating all `GetRequiredBodyModifierBoneNames()` entries as V0 standing truth bodies.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-DECIDE-V0-RAW-SIM-BODY-CONTRACT-01.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01.md`
- `plans/stage1/20-execution/evidence/S2-DECIDE-V0-RAW-SIM-BODY-CONTRACT-01.md`
- `plans/stage1/20-execution/assumption-ledger.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all runtime code files
- all tests
- all model asset files
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- all workflow scripts

## Required Work

1. Record `git rev-parse HEAD` as the base.
2. Decide that V0 activated-standing proof requires raw simulation for exactly:
   - `pelvis`
   - `spine_01`
   - `spine_02`
   - `spine_03`
   - `thigh_l`
   - `thigh_r`
   - `foot_l`
   - `foot_r`
   - `ball_l`
   - `ball_r`
3. Decide that the remaining bodies in `GetRequiredBodyModifierBoneNames()` are not V0 standing truth bodies.
4. Record that excluded bodies must be classified as excluded/distal/non-V0, isolated from world bracing, and prevented from injecting energy into the V0 critical/support set.
5. Record non-goals:
   - do not add staged runtime activation
   - do not accept `simMax=0`
   - do not force all 22 required body modifiers raw-simulating in V0
6. Create the next implementation task packet aimed at making the 10-body V0 set stable enough to hold.
7. Update the assumption ledger.
8. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-DECIDE-V0-RAW-SIM-BODY-CONTRACT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The decision requires runtime code or test edits.
2. The next implementation task cannot be expressed without staged runtime activation.
3. Scope check fails.
