# Stage 1 Phase 0 Execution Package

## Purpose

This package turns the Stage 1 planning documents into an execution-ready Phase 0 sequence. It is the document the orchestrator should use to decide what work is runnable, what the user must do, what evidence must be captured, and how G1 will be judged.

## Scope

Phase 0 exists to answer one question:

Can the planned Stage 1 bridge survive first contact with reality strongly enough to justify implementation work?

The required outputs are:

- confirmed PHC bridge contract
- confirmed retargeting validation plan
- confirmed pretrained-first viability
- confirmed pretrained checkpoint retrieval path
- confirmed test/evidence shape for G1
- evidence that the UE5 control path and Manny/Chaos smoke path are viable

The fastest way to resume this package after user setup work is to gather evidence with [user-return-template.md](/F:/NewEngine/plans/stage1/user-return-template.md) and then issue [task-packet-s1-p0-a1.md](/F:/NewEngine/plans/stage1/task-packet-s1-p0-a1.md).

## Entry Criteria

Do not start Phase 0 until all of these are true:

- [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md) exists
- [retargeting-spec.md](/F:/NewEngine/plans/stage1/retargeting-spec.md) exists
- [test-strategy.md](/F:/NewEngine/plans/stage1/test-strategy.md) exists
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md) has been reviewed by the orchestrator for Phase 0
- [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md) exists
- [pretrained-checkpoint-retrieval.md](/F:/NewEngine/plans/stage1/pretrained-checkpoint-retrieval.md) exists
- the orchestrator has identified which assumptions Phase 0 is intended to confirm or falsify

## Phase 0 Deliverables

1. Updated `bridge-spec.md` with confirmed PHC config details
2. Updated `retargeting-spec.md` with the exact validation cases chosen for G1
3. Motion-source review against [motion-set.md](/F:/NewEngine/plans/stage1/motion-set.md)
4. Checkpoint retrieval recorded in [dependency-lock.md](/F:/NewEngine/plans/stage1/dependency-lock.md)
5. A completed [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md) package
6. A short go / no-go conclusion for Gate G1

## Current Known Local Environment Note

- Confirmed UE version: `5.7.3`
- Confirmed UE install root: `E:\UE_5.7`
- Confirmed `UE5_PATH`: `E:\UE_5.7\Engine`
- Confirmed UE project path: `F:\NewEngine\PhysAnimUE5`
- Confirmed ProtoMotions root: `F:\NewEngine\Training\ProtoMotions`
- Confirmed Python env: `F:\NewEngine\Training\.venv\physanim_proto311`
- Confirmed Isaac Sim launcher: `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaacsim.exe`
- Confirmed Isaac Lab launcher: `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaaclab.exe`
- Confirmed pretrained checkpoint: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`
- Frozen first motion file for Phase 0 training-side eval: `F:\NewEngine\Training\ProtoMotions\data\motions\smpl_humanoid_walk.npy`

## Frozen Phase 0 Command And Evidence Path

Use these exact local paths unless Phase 0 evidence proves they are wrong.

### P0-03: Bridge Contract Confirmation

- Working source of truth for the selected pretrained config:
  - `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\config.yaml`
  - `F:\NewEngine\Training\ProtoMotions\protomotions\eval_agent.py`
  - `F:\NewEngine\Training\ProtoMotions\protomotions\envs\mimic\components\mimic_obs.py`
  - `F:\NewEngine\Training\ProtoMotions\protomotions\agents\mimic\agent.py`
- Record confirmed observation/action facts back into:
  - `plans/stage1/bridge-spec.md`
- Record gate-facing conclusions into:
  - `plans/stage1/g1-evidence.md`

### P0-04: Retargeting Validation Freeze

- Record the final validation set and handedness note in:
  - `plans/stage1/retargeting-spec.md`
- Record runtime pass/fail evidence in:
  - `plans/stage1/g1-evidence.md`

### P0-05: Training-Side Feasibility Check

- Working directory:
  - `F:\NewEngine\Training\ProtoMotions`
- Required environment variable:
  - `OMNI_KIT_ACCEPT_EULA=YES`
- Frozen headless smoke-validation command already exercised successfully on this machine:

```powershell
$env:OMNI_KIT_ACCEPT_EULA='YES'
& 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe' protomotions/eval_agent.py +robot=smpl +simulator=isaaclab +motion_file=F:\NewEngine\Training\ProtoMotions\data\motions\smpl_humanoid_walk.npy +checkpoint=F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt +terrain=flat +headless=True +num_envs=1 +agent.config.max_eval_steps=10 +fabric.strategy=auto +experiment_name=phase0_eval_smoke
```

- Frozen visual-evidence command for `MV-G1-01`:

```powershell
$env:OMNI_KIT_ACCEPT_EULA='YES'
& 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe' protomotions/eval_agent.py +robot=smpl +simulator=isaaclab +motion_file=F:\NewEngine\Training\ProtoMotions\data\motions\smpl_humanoid_walk.npy +checkpoint=F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt +terrain=flat +headless=False +num_envs=1 +agent.config.max_eval_steps=3000 +fabric.strategy=auto +experiment_name=phase0_eval_visual
```

- Viewer recording controls for the visual-evidence run:
  - press `L` to start recording
  - press `L` again to stop and write the mp4
  - wait for the console line `Video saved to ...mp4`
  - press `Q` to close the viewer after the save completes
- Expected output path for the clip:
  - `F:\NewEngine\Training\ProtoMotions\output\renderings\phase0_eval_visual-<timestamp>.mp4`
- Why there are two commands:
  - the headless command is the already-proven runtime sanity check
  - the non-headless command is the one the user must run for the actual `MV-G1-01` visual verdict

- Runtime sanity check already completed for this environment:

```powershell
$env:OMNI_KIT_ACCEPT_EULA='YES'
& 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe' -c "from isaacsim import SimulationApp; app = SimulationApp({'headless': True}); print('runtime_ok'); app.close()"
```

- Record the resulting clip / verdict in:
  - `plans/stage1/g1-evidence.md`
- Local compatibility note:
  - the current Windows `3.11` path required local `default_factory` fixes in:
    - `Training/ProtoMotions/protomotions/simulator/base_simulator/config.py`
    - `Training/ProtoMotions/protomotions/simulator/isaacgym/config.py`
    - `Training/ProtoMotions/protomotions/simulator/isaaclab/config.py`
  - the current pretrained config also needed `+fabric.strategy=auto` to bypass the default DDP / NCCL path on Windows

### P0-06 / P0-07 / P0-08: UE Manual Evidence Path

- Use the manual checkpoints:
  - `MV-G1-02`
  - `MV-G1-03`
- Record all UE-side evidence in:
  - `plans/stage1/g1-evidence.md`
- If any UE-side setup differs from the frozen scaffold, note it in:
  - `plans/stage1/execution-log.md`

## Work Breakdown

### P0-01: Freeze Inputs

- Owner: orchestrator
- Goal: freeze the exact planning inputs for Phase 0 execution
- Inputs:
  - `ENGINEERING_PLAN.md`
  - `plans/stage1/bridge-spec.md`
  - `plans/stage1/retargeting-spec.md`
  - `plans/stage1/test-strategy.md`
  - `plans/stage1/manual-verification.md`
  - `plans/stage1/assumption-ledger.md`
- Output:
  - short frozen-input note added to the Phase 0 handoff or execution log
- Done when:
  - no worker is allowed to reinterpret upstream assumptions during Phase 0

### P0-02: User Prerequisites

- Owner: user
- Goal: satisfy the Phase 0 prerequisites that AI cannot complete alone
- Required user actions:
  - install required tools and record versions / paths
  - obtain external repos and datasets
  - create the UE5 project scaffold
  - prepare editor-only setup for Manny and the physics-control path
- Evidence required back:
  - tool versions and install paths
  - confirmation of dataset / repo locations
  - confirmation that UE `5.7.3` finished installing at `E:\UE_5.7` or a corrected path if it changed
  - project path and brief note that the UE5 scaffold exists
  - confirmation that `PoseSearch`, `PhysicsControl`, and `NNERuntimeORT` are enabled

### P0-03: Confirm PHC Contract

- Owner: AI worker
- Goal: replace planning-level PHC assumptions with config-specific facts
- Inputs:
  - selected ProtoMotions / PHC config
  - `plans/stage1/bridge-spec.md`
- Output:
  - updated `plans/stage1/bridge-spec.md`
  - handoff summarizing confirmed tensor shape, field order, and representation
- Must confirm:
  - observation tensor field order
  - observation tensor dimension
  - action tensor field order
  - action tensor dimension
  - representation format
  - gain-output policy
- Escalate if:
  - the chosen config does not support the intended Stage 1 bridge without scope changes

### P0-03b: Confirm Motion Sources

- Owner: AI worker
- Goal: confirm that the locked Stage 1 motion set is actually sourceable
- Inputs:
  - `plans/stage1/motion-set.md`
  - available pretrained model context
  - planned AMASS / Mixamo sources
- Output:
  - short motion-source note or update to `motion-set.md`
- Must confirm:
  - which locomotion-core motions are covered by the broad pretrained / AMASS path
  - which motions are at risk of being missing
- Escalate if:
  - the locked motion set cannot be sourced without changing scope

### P0-04: Confirm Retargeting Validation Set

- Owner: AI worker
- Goal: finalize which mapping and pose cases must be exercised in Phase 0
- Inputs:
  - `plans/stage1/retargeting-spec.md`
  - `.agents/skills/smpl-skeleton/SKILL.md`
- Output:
  - updated `plans/stage1/retargeting-spec.md`
  - explicit list of static and dynamic validation cases for G1
- Must confirm:
  - mapped joint subset
  - unmapped-bone policy
  - validation poses
  - failure signals to watch for in the Manny smoke test

### P0-05: Training-Side Feasibility Check

- Owner: AI + user
- Goal: confirm that pretrained policy output is worth continuing
- Inputs:
  - training workflow
  - `plans/stage1/motion-set.md`
  - manual check `MV-G1-01`
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - clip or visualization of pretrained broad motion evaluation
  - verdict: `pass`, `fail`, or `unclear`
- Escalate if:
  - the pretrained result is unstable or clearly robotic

### P0-06: UE5 Control-Path Check

- Owner: AI + user
- Goal: confirm that Manny responds to programmatic control updates
- Inputs:
  - UE5 scaffold
  - manual check `MV-G1-02`
- Frozen runtime path:
  - map: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - runtime owner: `UPhysAnimMvG102Subsystem`
  - trigger: PIE console command `PhysAnim.MVG102.Start`
  - expected first-moving region: left arm / left hand on the current mannequin pawn
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - clip or screenshot
  - note on whether `UPhysicsControlComponent` responds as expected
- Escalate if:
  - control updates are ignored or unstable

### P0-07: Substep Stability Check

- Owner: AI + user
- Goal: confirm that the articulated body can remain stable enough for Stage 1
- Inputs:
  - physics-substep plan from `ENGINEERING_PLAN.md`
- Frozen runtime path:
  - map: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - runtime owner: `UPhysAnimMvG102Subsystem`
  - trigger: PIE console command `PhysAnim.MVG102.Start`
  - first configuration:
    - `Tick Physics Async = false`
    - `Substepping = true`
    - `Max Substep Delta Time = 0.008333`
    - `Max Substeps = 4`
  - fallback second configuration only if the first is clearly unstable:
    - `Tick Physics Async = false`
    - `Substepping = true`
    - `Max Substep Delta Time = 0.004167`
    - `Max Substeps = 8`
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - substep settings used
  - clip or note on stability
- Escalate if:
  - stability is not achievable with the documented Stage 1 options

### P0-08: Manny End-To-End Smoke Test

- Owner: AI + user
- Goal: confirm that minimal SMPL/PHC-style outputs can drive Manny without obvious mapping failure
- Inputs:
  - confirmed bridge spec
  - confirmed retargeting validation set
  - manual check `MV-G1-03`
- Frozen runtime path:
  - map: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - runtime owner: `UPhysAnimMvG103Subsystem`
  - trigger: PIE console command `PhysAnim.MVG103.Start`
  - frozen pose case: `isolated left elbow flexion`
  - mapped subset under test: `L_Elbow -> lowerarm_l`, with `upperarm_l` parent context and `hand_l` held near neutral
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - smoke-test clip
  - observed pose behavior
  - failure notes if any limbs map incorrectly
- Escalate if:
  - the mapping is mirrored, unstable, or otherwise obviously wrong

### P0-09: Orchestrator Gate Review

- Owner: orchestrator
- Goal: decide whether G1 is pass, fail, or blocked
- Inputs:
  - completed `g1-evidence.md`
  - current `assumption-ledger.md`
- Output:
  - final G1 decision note in `g1-evidence.md`
  - updated assumption statuses
- Done when:
  - the project has a clear go / no-go answer for Stage 1 Phase 1

## Required Evidence Checklist

The following must be present before G1 can be called `pass`:

- confirmed PHC bridge contract
- confirmed retargeting validation set
- PHC training-side visual evidence
- Manny control-path evidence
- substep-stability evidence
- end-to-end Manny smoke-test evidence
- explicit G1 verdict from the orchestrator
- threshold checks from [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)

## Failure Handling

If any critical Phase 0 check fails:

- update `assumption-ledger.md`
- decide whether the failure is:
  - recoverable within Stage 1 scope
  - recoverable only by invoking an approved fallback
  - a stop condition
- do not begin the Phase 1 package until that decision is written down

## Handback Requirements

The worker or orchestrator closing Phase 0 must produce a handoff that includes:

- `Task ID`: `S1-P0-A1` or `S1-P0-A2`
- decision summary
- produced artifact(s)
- open risks
- blocked by user? yes/no
- verification summary
- next consumer

Use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md) exactly.
