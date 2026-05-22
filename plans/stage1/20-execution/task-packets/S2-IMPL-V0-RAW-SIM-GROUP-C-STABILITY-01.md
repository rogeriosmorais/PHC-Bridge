# S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01

## Purpose

Make the V0 10-body activated-standing raw-sim set stable enough to hold without introducing staged runtime activation or hidden assistance.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.PhysicsTuning.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01.md`
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
- staged runtime activation or a new handoff ladder

## Required Work

1. Record `git rev-parse HEAD` as the base.
2. Implement or expose the V0 standing contract as the 10 named raw-sim bodies:
   - `pelvis`, `spine_01`, `spine_02`, `spine_03`, `thigh_l`, `thigh_r`, `foot_l`, `foot_r`, `ball_l`, `ball_r`
3. Exclude the remaining `GetRequiredBodyModifierBoneNames()` bodies from V0 standing truth while preventing them from world bracing or injecting energy into the V0 critical/support set.
4. Investigate and address the immediate support failure in the 10-body set. Focus on:
   - foot/ball collision setup
   - initial contact penetration
   - target discontinuity at raw-sim enable
   - PhysicsControl gains for feet/thighs
   - constraint coupling from excluded bodies
5. The final proof path must require:
   - all 10 V0 bodies raw-simulating together
   - live PHC inference
   - nonzero conditioned actions
   - PhysicsControl target writes
   - nonzero but bounded body motion
   - valid support and physical continuity
   - no actor offset fallback
   - no CMC, shell, global blend, or hidden assist

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.RawSimBisect`
- strict proof-quality run for the V0 10-body standing proof
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The work requires staged runtime activation.
2. The work reintroduces actor offset fallback, CharacterMovement authority, shell/global assist, or all-22 raw-sim forcing as V0 acceptance.
3. The failure cannot be explained by contact, target discontinuity, PhysicsControl gains, or excluded-body constraint coupling evidence.
4. Scope check fails.
