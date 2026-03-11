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

- `Current phase`: Phase 1 / `S1-P1-A1` accepted / `S1-P1-A2` stabilization passes completed for the current smoke target
- `Overall status`: UE install, project scaffold, ProtoMotions checkout, pretrained checkpoint, Python `3.11` environment, and the Isaac Sim / Isaac Lab runtime are confirmed locally; Gate G1 is explicitly `pass`; the selected Phase 1 runtime model is the pretrained `motion_tracker/smpl` checkpoint; the full UE startup path succeeds through `NNERuntimeORTDml`; the one-character bridge now also completes the frozen passive PIE smoke window without catastrophic post-startup instability; a dedicated movement-smoke harness now exists and has already shown that the first real post-policy forward movement is still unstable, so the active bridge is no longer blocked on passive stabilization but is not yet movement-stable
- `Last planning milestone`: the frozen policy-phase stabilization plan converged on March 11, 2026 after continuity fixes, representation-switch cleanup, and the corrected SMPL->UE quaternion basis conversion removed the remaining live-policy blow-up; the next frozen milestone is now the Phase 1 movement-stability plan

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as of commit `0a9bf13` | `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | `plans/stage1/10-specs/environment-spec.md`, `plans/stage1/60-user/user-interventions.md`, `plans/stage1/60-user/user-return-template.md` | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | `plans/stage1/50-content/ue-project-scaffold.md`, `plans/stage1/60-user/user-interventions.md`, `plans/stage1/60-user/user-return-template.md` | UE editor setup | none |
| S1-P0-A1 | AI | completed | `plans/stage1/40-tasks/task-packet-s1-p0-a1.md` plus frozen Phase 0 inputs | `plans/stage1/20-execution/phase0-execution-package.md`, `plans/stage1/10-specs/bridge-spec.md`, `plans/stage1/10-specs/retargeting-spec.md`, `plans/stage1/20-execution/assumption-ledger.md`, `plans/stage1/20-execution/execution-log.md` | none |
| S1-P0-A2 | AI + User | completed | `plans/stage1/20-execution/phase0-execution-package.md`, `plans/stage1/60-user/manual-verification.md`, `plans/stage1/10-specs/acceptance-thresholds.md`, `plans/stage1/30-evidence/g1-evidence.md` | `plans/stage1/30-evidence/g1-evidence.md`, `plans/stage1/20-execution/assumption-ledger.md`, `plans/stage1/20-execution/execution-log.md` | none |
| S1-P1-A1 | AI | completed | `plans/stage1/40-tasks/task-packet-s1-p1-a1.md`, `plans/stage1/20-execution/phase1-implementation-package.md`, `plans/stage1/10-specs/onnx-export-spec.md`, `plans/stage1/10-specs/ue-bridge-implementation-spec.md` | `Training/scripts/export_onnx.py`, `Training/physanim/export_onnx.py`, `Training/tests/test_onnx_export.py`, `plans/stage1/20-execution/phase1-implementation-package.md`, `plans/stage1/10-specs/dependency-lock.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P1-A2 | AI | in_progress | accepted `S1-P1-A1` handoff, `phase1-implementation-package.md`, `manual-verification.md`, `acceptance-thresholds.md`, `phase1-ue-bridge-bringup-runbook.md` | `plans/stage1/40-tasks/task-packet-s1-p1-a2.md`, `plans/stage1/60-user/manual-verification.md`, `plans/stage1/10-specs/acceptance-thresholds.md`, `plans/stage1/30-evidence/g2-evaluation.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |

## Frozen Inputs For Phase 0 Preparation

- `Freeze point`: commit `0a9bf13`
- `Frozen docs`:
  - `AGENTS.md`
  - `ENGINEERING_PLAN.md`
  - `plans/stage1/10-specs/bridge-spec.md`
  - `plans/stage1/10-specs/retargeting-spec.md`
  - `plans/stage1/10-specs/test-strategy.md`
  - `plans/stage1/60-user/manual-verification.md`
  - `plans/stage1/20-execution/assumption-ledger.md`
  - `plans/stage1/10-specs/acceptance-thresholds.md`
  - `plans/stage1/50-content/pretrained-model-selection.md`
  - `plans/stage1/50-content/pretrained-checkpoint-retrieval.md`
  - `plans/stage1/10-specs/environment-spec.md`
  - `plans/stage1/50-content/motion-set.md`
  - `plans/stage1/50-content/motion-source-map.md`
  - `plans/stage1/50-content/motion-source-lock-table.md`
  - `plans/stage1/50-content/ue-project-scaffold.md`
  - `plans/stage1/20-execution/phase0-execution-package.md`
- `Unfreeze rule`: only unfreeze if the user returns setup evidence that changes a planned value or if an assumption moves materially in the ledger

## Next Runnable Tasks

| Priority | Task ID | Why Runnable / Not Runnable Yet |
|---|---|---|
| 1 | S1-P1-A2 | runnable now; startup succeeds through `NNERuntimeORTDml`, so the next safe task is stabilization/tuning planning before G2 |
| 2 | stabilization implementation pass | runnable after the updated `S1-P1-A2` package is accepted |
| 3 | S1-P2-A1 | not runnable until G2 is explicitly passed |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| none | no Phase 0 evidence remains outstanding |

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
- user evidence on March 10, 2026 now confirms the frozen `120 Hz` synchronous-substep configuration (`Tick Physics Async = false`, `Substepping = true`, `Max Substep Delta Time = 0.008333`, `Max Substeps = 4`) stayed controllable without jitter or wobble dominating the run
- Gate G1 now passes; Phase 0 evidence capture is complete and Phase 1 is unblocked
- `Training\scripts\export_onnx.py` now exists and exports the selected pretrained `motion_tracker/smpl` actor path through the locked three-input Stage 1 contract
- `Training\tests\test_onnx_export.py` now validates the real input/output names and numeric parity requirement instead of skipping behind a placeholder contract
- March 10, 2026 worker validation exported `F:\NewEngine\Training\output\phc_policy.onnx` with accepted opset `17` and `onnxruntime 1.24.3` parity max abs diff `1.64e-7`
- the exported ONNX was copied to `F:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx`, so the next runtime step is UE import / NNE model-creation validation rather than more offline export work
- user evidence on March 10, 2026 now confirms the startup-success line:
  - `[PhysAnim] Startup success. Runtime=NNERuntimeORTDml Model=/Game/NNEModels/phc_policy.phc_policy`
- the active Phase 1 blocker has changed:
  - model loading is proven
  - the current failure mode is uncontrolled post-startup flight / spinning
  - the next required planning artifact is a stabilization/tuning package, not a G2 comparison handoff
  - the current implementation now includes objective instability monitoring on the root body, so the next stabilization loop can use fail-stop metrics and runtime diagnostics instead of relying only on visual judgment

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
| S1-P1-A1 | single-character implementation package freeze + ONNX export path | yes | startup now succeeds through `NNERuntimeORTDml`, so export/import discovery is no longer on the critical path |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| G2 | blocked | do not package or judge G2 while the physics-driven runtime is still dominated by immediate post-startup instability |
| S1-P2-A1 | blocked | depends on G2 pass |
| S1-P2-A2 | blocked | depends on Phase 2 result |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks
