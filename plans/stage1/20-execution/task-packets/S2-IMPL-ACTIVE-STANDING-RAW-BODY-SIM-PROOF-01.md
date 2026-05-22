# S2-IMPL-ACTIVE-STANDING-RAW-BODY-SIM-PROOF-01

## Purpose

Diagnose which raw-sim body group injects catastrophic energy during activated standing, without adding a progressive runtime activation design. The final contract remains all balance-critical and required support bodies raw-simulating together with live PHC and bounded motion.

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
2. Replace the failed always-on active-standing raw-sim enforcement with a disabled-by-default diagnostic body-group switch.
3. Keep the diagnostic switch test-only in purpose: it must not create a runtime handoff ladder or alter default runtime behavior.
4. Add a diagnostic bisect automation path:
   - Experiment A: pelvis plus spine.
   - Experiment B: pelvis plus spine plus thighs.
   - Experiment C: pelvis plus spine plus thighs plus support bodies.
   - Experiment D: full required body set.
5. Each experiment must log runtime state, support validity, named-body masks, max body linear/angular speed, PHC inference/action activity, and control-target writes.
6. Keep physical perturbation as pelvis/body impulse only; do not reintroduce actor offset or CharacterMovement launch fallback.
7. Final acceptance remains strict: all balance-critical plus required support bodies raw-simulating together, live PHC inference, nonzero actions, control targets written, nonzero but bounded body motion, valid support/continuity, and no shell/CMC/actor-offset hidden assistance.
8. Update evidence and `execution-log.md`.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.RawSimBisect`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVE-STANDING-RAW-BODY-SIM-PROOF-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:
1. The work requires model, ONNX, asset, PoseSearch, mass, runtime adapter, runtime orchestrator, support truth, termination, or failure arbitration edits.
2. The proof cannot be made strict without reintroducing actor movement fallback, CharacterMovement authority, capsule collision authority, or global blend assistance.
3. The support bodies cannot raw-simulate and no explicit V0 support-body contract can be defended.
4. The diagnostic bisect cannot identify whether the catastrophic energy begins at pelvis/spine, thighs, support bodies, or the full required body set.
5. Scope check fails.
