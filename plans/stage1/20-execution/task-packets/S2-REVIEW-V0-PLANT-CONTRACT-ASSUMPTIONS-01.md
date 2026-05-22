# S2-REVIEW-V0-PLANT-CONTRACT-ASSUMPTIONS-01

## Purpose

Separate V0 raw-sim plant stability from PHC policy correctness before any further Group C tuning.

This is a review/proof-quality task. Acceptance is not "standing passes"; acceptance is a written, evidence-backed answer to the V0 plant/action/observation/support assumptions below.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-REVIEW-V0-PLANT-CONTRACT-ASSUMPTIONS-01.md`
- `plans/stage1/20-execution/evidence/S2-REVIEW-V0-PLANT-CONTRACT-ASSUMPTIONS-01.md`
- `plans/stage1/20-execution/execution-log.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.PhysicsTuning.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponentPrivate.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`

## Forbidden Files

- all model asset files
- all ONNX files
- all PoseSearch tuning files
- all mass tuning files
- runtime adapter files
- runtime orchestrator files
- support truth implementation files
- failure arbitration implementation files
- workflow scripts
- runtime tuning intended to make Group C pass
- staged runtime activation or a new handoff ladder

## Required Review Questions

Answer all of these in the evidence file:

1. Can the 10-body V0 raw-sim plant hold a static authored standing target with PHC disabled?
2. With zero policy action, do `spine_01` and `spine_03` remain bounded?
3. With tiny synthetic actions, does PhysicsControl response scale smoothly, or does it spike?
4. Does the PHC observation vector match ProtoMotions/training semantics for:
   - body order
   - root frame
   - joint rotations
   - velocities
   - units
   - normalization
   - support/contact assumptions
5. Does the action vector mean what `ApplyControlTargets` thinks it means?
6. Does support fail before or after the first major spine velocity spike?
7. Are COM/proxy and support hull measuring real balance, or merely a convenient proxy?

## Required Experiments

Run deterministic V0 Group C tests that separate plant stability from policy correctness:

- A. PHC disabled, static authored target, Group C raw sim.
- B. PHC enabled but actions forced zero, Group C raw sim.
- C. PHC enabled with current actions, Group C raw sim.

Interpretation contract:

- A explodes: PhysicsControl/raw body setup is wrong.
- A stable, B explodes: action-conditioning or target path is still active unexpectedly.
- A/B stable, C explodes: PHC action semantics or observation/action contract is wrong.
- A/B/C stable but support fails: support/contact/proxy truth is wrong.

## Required Evidence

- Base commit.
- Exact commands run.
- Build/test pass/fail status.
- A/B/C metrics:
  - duration
  - runtime state
  - support hull min/mean/max
  - active sides min/mean/max
  - max body linear/angular speed and body names
  - `spine_01` and `spine_03` max linear/angular speed
  - first major spine spike time
  - first support failure time
  - PHC inference count
  - raw/conditioned action magnitude
  - PhysicsControl target writes and max target delta
  - V0 body sim masks and excluded-body sim count
- Observation/action semantic review with source references.
- COM/proxy/support-hull conclusion.

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- deterministic review automation for the A/B/C cases
- `python .\scripts\read_logs.py` after automation
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-REVIEW-V0-PLANT-CONTRACT-ASSUMPTIONS-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`
- `powershell -ExecutionPolicy Bypass -File .\scripts\check_workflow_state.ps1 -Mode status -Strict`

## Stop Conditions

Stop immediately if:

1. Answering the review requires changing model assets, ONNX files, PoseSearch tuning, or mass tuning.
2. The work turns into Group C tuning instead of plant/policy separation.
3. The work requires staged runtime activation.
4. The tests cannot distinguish A/B/C because the harness lacks control over PHC disable, zero action, or tiny synthetic actions.
5. Observation/action semantics cannot be reviewed from local source and committed artifacts.

## Definition Of Done

- The A/B/C separation runs mechanically or the blocker is explicitly classified.
- All seven review questions are answered in evidence.
- No Group C pass is claimed unless the evidence satisfies the actual V0 standing contract.
- Scope check passes.
- One review-task commit is created.
- `execution-log.md` advances back to `S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01` or to the next explicitly created task.
