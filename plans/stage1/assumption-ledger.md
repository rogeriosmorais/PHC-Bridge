# Stage 1 Assumption Ledger

## Purpose

This is the orchestrator-owned control document for Stage 1. It tracks the assumptions that can kill the project, the signals that would prove those assumptions wrong, and the fallback or stop decision tied to each one.

This is not a passive risk list. The orchestrator uses it to decide whether downstream work is still safe.

## Status Scale

- `green`: assumption currently acceptable for downstream work
- `yellow`: assumption still plausible, but under active watch
- `red`: assumption is likely false or insufficiently supported; downstream work depending on it should stop
- `resolved`: assumption is no longer an open uncertainty because it was confirmed, rejected, or made irrelevant

## How To Use This Ledger

For each major Stage 1 assumption, track:

- what must be true
- why it matters
- how we will test it
- what evidence would falsify it
- what fallback exists
- whether the project should stop if it fails

Only the orchestrator updates status.

## Critical Assumptions

| ID | Assumption | Why It Matters | Current Status | How It Will Be Tested | Falsification Signal | Fallback | Stop If False |
|---|---|---|---|---|---|---|---|
| A-01 | Pretrained policy motion in training looks convincingly alive enough to justify Stage 1 | Stage 1 is pointless if the starting policy already looks robotic | green | G1 training visualization and manual check `MV-G1-01` | motion looks stiff, unstable, or puppet-like in training | narrow to locomotion-only test or stop before deeper integration | yes |
| A-02 | The selected ProtoMotions/PHC config can be mapped cleanly into a UE5 bridge contract | the plugin cannot be built safely without a stable observation/action contract | green | `bridge-spec.md` confirmed in Phase 0 | tensor fields, ordering, or representation stay unclear | narrow the chosen config or pause until a workable config is selected | yes |
| A-03 | SMPL <-> UE5 retargeting can be made stable enough for Manny | wrong transforms or mirroring will invalidate the entire bridge | green | `retargeting-spec.md` plus G1 Manny smoke test | wrong limbs move, mirroring appears, or instability comes from mapping errors | revise mapping and transform layer | yes |
| A-04 | `UPhysicsControlComponent` can express the policy intent well enough for Stage 1 | the entire low-custom-code thesis depends on it | green | control-path prototype and Manny response checks | targets are ignored, unstable, or too limited to drive plausible motion | explicit fallback to raw torque control if the project chooses it | yes |
| A-05 | Chaos substepping at 120-240 Hz is stable enough for the articulated body | unstable simulation breaks any quality comparison | green | Phase 0 substep stability check | persistent instability despite tuning and solver adjustments | try async physics, then reassess Stage 1 viability | yes |
| A-06 | NNE + ONNX Runtime can run the exported policy fast enough and compatibly in UE5 | inference must fit inside Stage 1 without adding a new runtime stack | yellow | dummy-model check, then real ONNX load using `onnx-export-spec.md` | model fails to load, runtime creation fails, or runtime cost is unacceptable | shrink/re-export model, reassess viability | yes |
| A-07 | PoseSearch output is sufficient as the upstream motion-intent source for Stage 1 | the bridge assumes PoseSearch can supply the needed target motion intent | yellow | bridge-spec confirmation and one-character integration | required target features are unavailable or unusable | narrow conditioning assumptions rather than inventing a new architecture | maybe |
| A-07b | The available pretrained policy covers enough broad locomotion to make pretrained-first worthwhile | if not, we should switch to fine-tuning or training earlier | yellow | pretrained evaluation on the locked locomotion set | pretrained result is obviously not useful even for locomotion / general motion | go straight to fine-tuning on the locked locomotion set | no |
| A-07c | The locked Stage 1 locomotion-only motion set is sourceable without major scope creep | undefined or missing motions create silent planning drift | yellow | motion-source review against `motion-set.md` | key locomotion motions are missing or only available through major extra pipeline work | replace or remove missing motions explicitly | maybe |
| A-08 | Physics-driven motion will look noticeably better than kinematic playback | this is the thesis tested by G2 | yellow | G2 side-by-side evaluation | user judges difference as negligible or worse | ship only as experiment documentation, do not continue to Stage 2 | yes |
| A-09 | The final Stage 1 locomotion showcase will be compelling enough to justify Stage 2 | Stage 2 is optional and should not start on weak evidence | yellow | G3 observer evaluation | observers find the result unconvincing | stop at Stage 1 | no |

## Latest Orchestrator Review

- `Review commit`: `eb6944f` plus local uncommitted UE harness debug changes
- `Reviewed artifacts`:
  - `plans/stage1/bridge-spec.md`
  - `plans/stage1/retargeting-spec.md`
  - `plans/stage1/test-strategy.md`
  - `plans/stage1/motion-set.md`
  - `plans/stage1/motion-source-map.md`
  - `plans/stage1/motion-source-lock-table.md`
  - `plans/stage1/phase0-execution-package.md`
  - `PhysAnimUE5/PhysAnimUE5.uproject`
  - `PhysAnimUE5/Config/DefaultEngine.ini`
  - `PhysAnimUE5/Saved/Logs/PhysAnimUE5.log`
  - `PhysAnimUE5/Saved/Logs/PhysAnimUE5_2.log`
- `Conclusion`:
  - the Phase 0 planning contract is now concrete enough to execute on this exact Windows machine without more setup replanning
  - the selected `motion_tracker/smpl` checkpoint is now the preferred Stage 1 runtime target because its inference contract is materially simpler than the deferred MaskedMimic path
  - no G1-critical assumption beyond `A-02` moves out of `yellow` yet because `g1-evidence.md` still has no training-side motion evidence or UE-side manual evidence
  - `A-02` now moves to `green` because the local runtime input set, action shape, representation path, and fixed-gain policy are explicit in `bridge-spec.md`
  - partial setup evidence now confirms the planned UE install root and `UE5_PATH`, but it does not yet reduce any G1-critical risk
  - updated setup evidence now also confirms UE `5.7.3` and the presence of the `F:\NewEngine\PhysAnimUE5` scaffold with Manny content paths available
  - ProtoMotions `v2.3.2`, the selected `motion_tracker/smpl` checkpoint, and a Python `3.11` Windows-native environment are now local and consistent with the Isaac Sim `5.x` requirement
  - Isaac Sim `5.1.0.0` and Isaac Lab `2.3.2.post1` are now installed in the locked env and headless `SimulationApp` startup succeeded locally
  - the current Windows path also required a small local ProtoMotions compatibility patch for Python `3.11` dataclass defaults plus a single-device Fabric override to bypass the default DDP / NCCL path
  - the visual IsaacLab path additionally required local compatibility shims for this machine and package set: preloading `h5py` before `AppLauncher`, excluding broken RTX sensor extensions from Kit launch, constructing `Se2Keyboard` with `Se2KeyboardCfg`, and supporting MoviePy `2.x` without `moviepy.editor`
  - the earlier Vulkan startup crash was consistent with an external graphics hook conflict rather than a PHC or IsaacLab logic failure; future visual checks on this machine should start with overlays and capture hooks disabled
  - Isaac sensor DLL warnings for `generic_mo_io.dll`, lidar, radar, and `isaacsim.sensors.rtx` are still present, but the current evidence suggests they are non-blocking noise for the locomotion-only `MV-G1-01` visual path
  - user evidence on March 10, 2026 now confirms the saved clip `F:\NewEngine\Training\ProtoMotions\output\renderings\phase0_eval_visual-2026-03-10-10-15-07.mp4`, so the `MV-G1-01` recording path itself is no longer in doubt
  - user evidence on March 10, 2026 also judges `MV-G1-01` as `pass`, so `A-01` now moves from `yellow` to `green`
  - the Phase 0 eval command, retargeting validation set, and evidence paths are now frozen for `S1-P0-A2`
  - the current UE scaffold review is stronger now: `PhysAnimUE5.uproject` enables `PoseSearch` and `PhysicsControl`, Manny assets are present under `Content/Characters/Mannequins`, editor logs show `NNERuntimeORT` runtime initialization, and a PIE session launched successfully on March 10, 2026
  - this stronger scaffold evidence reduces setup ambiguity, but it does not by itself satisfy `MV-G1-03` or the substep-stability threshold because those still require user-observed motion behavior
  - user evidence on March 10, 2026 confirms visible left-elbow / left-arm motion after running `PhysAnim.MVG102.Start` in `/Game/ThirdPerson/Lvl_ThirdPerson`
  - the orchestrator narrowed `MV-G1-02` to a stationary proof only, so later movement-induced shoulder artifacts are recorded as out of scope for that checkpoint rather than as a failure of the basic command path
  - `MV-G1-03` now has a frozen UE runtime path on March 10, 2026: `/Game/ThirdPerson/Lvl_ThirdPerson` plus `UPhysAnimMvG103Subsystem` and the `PhysAnim.MVG103.Start` smoke harness for the explicit `isolated left elbow flexion` validation case
  - user evidence on March 10, 2026 now confirms the frozen `120 Hz` synchronous-substep settings (`Tick Physics Async = false`, `Substepping = true`, `Max Substep Delta Time = 0.008333`, `Max Substeps = 4`) remained stable, with no jitter or wobble dominating the run
  - `A-03` now moves from `yellow` to `green` because the Manny smoke-test mapping check completed without leaving an open mapping-failure blocker
  - `A-04` now moves from `yellow` to `green` because the Physics Control command path and Manny response checks both passed
  - `A-05` now moves from `yellow` to `green` because the documented synchronous-substep path stayed controllable on this machine
  - `A-06` remains `yellow`: the local ORT runtime is present and reports GPU interface availability, but no exported Stage 1 model has been loaded in UE yet
- `Phase 0 critical assumptions`:
  - `A-01`
  - `A-02`
  - `A-03`
  - `A-04`
  - `A-05`
  - `A-06`
  - `A-07`
  - `A-07b`
  - `A-07c`

## Reassessment Triggers

The orchestrator must revisit ledger status when:

- a planning spec is locked
- a gate package is prepared
- user evidence arrives
- a worker reports a blocker
- a fallback path becomes more likely than the primary path

## Downstream Safety Rules

- `green`: downstream tasks may proceed normally
- `yellow`: downstream tasks may proceed only if they do not hard-code the uncertain assumption in irreversible ways
- `red`: downstream tasks that depend on the assumption must stop

## Immediate Orchestrator Duties

Before starting the next execution-planning pass, the orchestrator should:

1. record whether the current commit's review of `bridge-spec.md`, `retargeting-spec.md`, and `test-strategy.md` is complete
2. update each assumption above from `yellow` only if evidence justifies it
3. note which assumptions block the Phase 0 execution package

## First Expected Updates

The next meaningful ledger updates should come from:

- confirming the PHC observation/action contract from the frozen local ProtoMotions sources
- capturing Manny bridge smoke-test evidence for `MV-G1-03`
- defining the exact evidence required to call `UPhysicsControlComponent` viable for Stage 1
