# Stage 1 Execution Log

## Purpose

This file is the orchestrator-owned live task-state board for Stage 1.

Use it to track:

- what is active
- what is blocked
- what is waiting on the user
- what frozen inputs are in effect
- what handoffs were accepted

## Current State

- `Current phase`: Phase 0 / `S1-P0-A1` complete / `S1-P0-A2` in progress
- `Overall status`: UE install, project scaffold, ProtoMotions checkout, pretrained checkpoint, Python `3.11` environment, and the Isaac Sim / Isaac Lab runtime are confirmed locally; the UE scaffold is now also verified on disk with Manny assets present, `PoseSearch` and `PhysicsControl` enabled in the project file, `NNERuntimeORT` mounting in editor logs, and a successful PIE launch recorded on March 10, 2026; Stage 1 remains narrowed to locomotion-only, the selected runtime checkpoint is `motion_tracker/smpl`, the Windows command path for Phase 0 is frozen, `MV-G1-01` has a saved clip artifact and a `pass` verdict, and `MV-G1-02` is now accepted as a screenshot-backed stationary control-path `pass`
- `Last planning milestone`: orchestrator reviewed the current UE scaffold artifacts, confirmed Manny content and plugin state from local project files and logs, and re-synced the Phase 0 blocker list to the remaining manual G1 checkpoints

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as of commit `0a9bf13` | `plans/stage1/execution-log.md`, `plans/stage1/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | `plans/stage1/environment-spec.md`, `plans/stage1/user-interventions.md`, `plans/stage1/user-return-template.md` | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | `plans/stage1/ue-project-scaffold.md`, `plans/stage1/user-interventions.md`, `plans/stage1/user-return-template.md` | UE editor setup | none |
| S1-P0-A1 | AI | completed | `plans/stage1/task-packet-s1-p0-a1.md` plus frozen Phase 0 inputs | `plans/stage1/phase0-execution-package.md`, `plans/stage1/bridge-spec.md`, `plans/stage1/retargeting-spec.md`, `plans/stage1/assumption-ledger.md`, `plans/stage1/execution-log.md` | none |
| S1-P0-A2 | AI + User | in_progress | `plans/stage1/phase0-execution-package.md`, `plans/stage1/manual-verification.md`, `plans/stage1/acceptance-thresholds.md`, `plans/stage1/g1-evidence.md` | `plans/stage1/g1-evidence.md`, `plans/stage1/assumption-ledger.md`, `plans/stage1/execution-log.md` | G1 evidence capture, including user-observed manual checks |

## Frozen Inputs For Phase 0 Preparation

- `Freeze point`: commit `0a9bf13`
- `Frozen docs`:
  - `AGENTS.md`
  - `ENGINEERING_PLAN.md`
  - `plans/stage1/bridge-spec.md`
  - `plans/stage1/retargeting-spec.md`
  - `plans/stage1/test-strategy.md`
  - `plans/stage1/manual-verification.md`
  - `plans/stage1/assumption-ledger.md`
  - `plans/stage1/acceptance-thresholds.md`
  - `plans/stage1/pretrained-model-selection.md`
  - `plans/stage1/pretrained-checkpoint-retrieval.md`
  - `plans/stage1/environment-spec.md`
  - `plans/stage1/motion-set.md`
  - `plans/stage1/motion-source-map.md`
  - `plans/stage1/motion-source-lock-table.md`
  - `plans/stage1/ue-project-scaffold.md`
  - `plans/stage1/phase0-execution-package.md`
- `Unfreeze rule`: only unfreeze if the user returns setup evidence that changes a planned value or if an assumption moves materially in the ledger

## Next Runnable Tasks

| Priority | Task ID | Why Runnable / Not Runnable Yet |
|---|---|---|
| 1 | S1-P0-A2 | runnable now; the simulator/runtime path is concrete, the Windows eval command is frozen, and the remaining work is evidence collection plus manual judgment |
| 2 | S1-P1-A1 | not runnable until G1 is explicitly passed |
| 3 | S1-P1-A2 | not runnable until Phase 1 starts |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| substep-stability evidence | substep settings used plus a short clip or note saying whether Manny stayed controllable for roughly `30` seconds |

## Latest Phase 0 Evidence Progress

- the selected local `motion_tracker/smpl` checkpoint contract is now written down in `bridge-spec.md`
- G1 Criterion 2 now has concrete evidence in `g1-evidence.md` and scores `pass`
- the motion-source review in `g1-evidence.md` now scores `pass`
- the UE scaffold is now verified more concretely: `PhysAnimUE5.uproject` lists `PoseSearch` and `PhysicsControl`, Manny content exists under `Content/Characters/Mannequins`, editor logs show `NNERuntimeORT` runtime availability, and PIE launched successfully
- March 10, 2026 IsaacLab debug work identified one real launch hazard and several compatibility mismatches in the local ProtoMotions path: overlay/capture hooks could crash Vulkan startup, `h5py` had to be imported before Isaac Lab app launch on this setup, the local Isaac Lab package requires `Se2KeyboardCfg`, and the installed MoviePy package uses the new root-level `ImageSequenceClip` export instead of `moviepy.editor`
- the current visual eval path now renders and records locally with those compatibility fixes in place; noisy RTX sensor DLL errors still appear in the console, but they are not currently blocking `MV-G1-01`
- user evidence now confirms `F:\NewEngine\Training\ProtoMotions\output\renderings\phase0_eval_visual-2026-03-10-10-15-07.mp4` was saved successfully for `MV-G1-01`
- user evidence now also confirms a `pass` verdict for `MV-G1-01`
- `MV-G1-02` is now frozen to `/Game/ThirdPerson/Lvl_ThirdPerson` plus the plugin runtime harness command `PhysAnim.MVG102.Start`
- Visual Studio Build Tools 2022 with MSVC v143 and Windows SDK `22621` were installed on March 10, 2026, and Unreal built `PhysAnimPlugin` successfully on this machine
- the UE `MV-G1-02` harness was debugged through missing visible motion, a Live Coding linker failure, and one Live Coding class-reload crash; a normal closed-editor rebuild then succeeded and user evidence on March 10, 2026 confirmed visible left-elbow movement in the frozen test path
- later March 10, 2026 discussion narrowed `MV-G1-02` to an explicitly stationary proof; movement-induced shoulder artifacts were judged out of scope for this checkpoint and deferred to later integrated UE checks
- the user explicitly accepted screenshot-only evidence for `MV-G1-02`, so that checkpoint now scores `pass`
- `MV-G1-03` is now frozen to `/Game/ThirdPerson/Lvl_ThirdPerson` plus the dedicated mapped-joint smoke harness command `PhysAnim.MVG103.Start`, using the explicit validation case `isolated left elbow flexion`
- the user later confirmed `MV-G1-03` completed; the Manny smoke-test mapping check is now accepted as `pass`, with no remaining open blocker at the isolated left-elbow smoke-harness stage
- later March 10, 2026 Phase 1 bring-up work reached the UE model-loading gate and confirmed the next blocker is not Unreal wiring but missing export output: `F:\NewEngine\Training\output\phc_policy.onnx` does not exist, `Training\scripts\export_onnx.py` does not exist, the ONNX validation test is still a skip-if-placeholder, and no accepted ONNX export handoff exists yet
- the selected pretrained checkpoint still exists at `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`, so the missing ONNX file is an orchestration/process gap rather than a missing model-source artifact
- G1 remains blocked overall because the UE substep-stability check is still missing

## Accepted Handoffs

| Task ID | Artifact | Accepted? | Notes |
|---|---|---|---|
| S1-PLAN-01 | Stage 1 planning bundle | yes | foundational planning artifacts in place |
| S1-PLAN-02 | `bridge-spec.md` | yes | planning-level contract defined and now updated with the selected local runtime contract |
| S1-PLAN-03 | `retargeting-spec.md` | yes | planning-level mapping defined, awaits runtime validation |
| S1-PLAN-04 | `test-strategy.md` | yes | verification split defined |
| S1-PLAN-05 | environment / pretrained / scaffold / threshold bundle | yes | execution-planning gaps materially reduced |
| S1-PLAN-06 | task packets / lock sheets / user return path | yes | re-entry into Phase 0 is now operationally defined |
| S1-PLAN-07 | retrieval / export / comparison lock bundle | yes | remaining Phase 0-1 planning gaps materially reduced |
| S1-P0-A1 | Phase 0 machine-specific execution package | yes | Windows-native Isaac Sim / Isaac Lab path is frozen and Phase 0 can advance without more setup replanning |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| S1-P1-A1 | blocked | depends on G1 pass |
| S1-P1-A2 | blocked | depends on Phase 1 result |
| S1-P2-A1 | blocked | depends on G2 pass |
| S1-P2-A2 | blocked | depends on Phase 2 result |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks
