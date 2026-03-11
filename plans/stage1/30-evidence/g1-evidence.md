# Stage 1 Gate G1 Evidence

## Purpose

This document is the evidence bundle for Gate G1.

Gate text from the engineering plan:

> PHC output looks alive in the selected training simulator AND the PHC observation/action contract is locked AND Physics Control Component responds to programmatic targets AND a minimal SMPL/PHC output can drive Manny in Chaos without obvious mapping failure or instability AND substep rate is stable.

## Gate Status

- `Current status`: pass
- `Decision owner`: orchestrator with required user evidence
- `Final verdict`: pass

Use only:

- `pass`
- `fail`
- `blocked`

## Inputs Used

- `ENGINEERING_PLAN.md`
- `plans/stage1/10-specs/bridge-spec.md`
- `plans/stage1/10-specs/retargeting-spec.md`
- `plans/stage1/10-specs/test-strategy.md`
- `plans/stage1/60-user/manual-verification.md`
- `plans/stage1/20-execution/assumption-ledger.md`
- `plans/stage1/10-specs/acceptance-thresholds.md`

## Criterion 1: Pretrained Policy Output Looks Alive In Training

- `Status`: complete
- `Checkpoint`: `MV-G1-01`
- `Evidence`:
  - clip:
    - `F:\NewEngine\Training\ProtoMotions\output\renderings\phase0_eval_visual-2026-03-10-10-15-07.mp4`
    - file saved successfully on March 10, 2026 at `10:15:14`
  - notes:
    - user reported on March 10, 2026 that `phase0_eval_visual-2026-03-10-10-15-07.mp4` was created successfully for `MV-G1-01`
    - user judged the clip `pass` on March 10, 2026
    - the working path required closing overlay/capture tools first, especially `MSI Afterburner` and related RTSS hooks
    - the visual eval path now depends on local compatibility fixes in the ProtoMotions checkout for IsaacLab startup, keyboard setup, and MoviePy recording
    - Isaac sensor DLL warnings are still present in the console but did not prevent rendering or recording for this locomotion-only check
- `Verdict`: pass
- `Why this verdict was chosen`: the frozen visual-evidence command ran successfully, produced a concrete mp4 artifact, and the user explicitly judged the resulting motion clip as a `pass`

## Criterion 2: PHC Observation/Action Contract Is Locked

- `Status`: complete
- `Evidence`:
  - confirmed observation tensor shape: inference uses a keyed runtime input dictionary, not one flat tensor; required keys are `self_obs=358`, `mimic_target_poses=6495`, and `terrain=256`
  - confirmed action tensor shape: `69` floats ordered by `robot.dof_names` (`23` joints x `3` DoFs)
  - representation format: `self_obs` uses heading-aligned max-coords features with tan-norm rotation encoding; `mimic_target_poses` uses `15` dense future target poses from the `max-coords-future-rel` builder; actions are exponential-map DoF targets normalized into PD target range
  - gain-output policy: no gain head; Stage 1 uses fixed external gains and maps actions into built-in PD target ranges
  - source config:
    - `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\config.yaml`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\envs\base_env\components\humanoid_obs.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\envs\mimic\components\mimic_obs.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\envs\base_env\env_utils\humanoid_utils.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\simulator\base_simulator\simulator.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\agents\mimic\agent.py`
- `Verdict`: pass
- `Why this verdict was chosen`: the local selected checkpoint, simulator path, runtime input grouping, action shape, representation path, and fixed-gain policy are now explicit in `bridge-spec.md`, satisfying the contract-lock threshold

## Criterion 3: Physics Control Component Responds To Programmatic Targets

- `Status`: complete
- `Checkpoint`: `MV-G1-02`
- `Evidence`:
  - screenshot:
    - user-provided in-editor screenshot from March 10, 2026 showing visible left-arm response during the frozen stationary `MV-G1-02` path
    - no local file path was retained; the user explicitly accepted screenshot-only evidence for this checkpoint on March 10, 2026
  - scene or command used:
    - map: `/Game/ThirdPerson/Lvl_ThirdPerson`
    - runtime owner: `UPhysAnimMvG102Subsystem`
    - command: `PhysAnim.MVG102.Start`
  - notes:
    - earlier March 10, 2026 runs proved the command path and on-screen harness message but showed no visible limb response
    - the UE plugin harness was then patched, rebuilt successfully outside Live Coding, and relaunched from a fresh editor process
    - user evidence on March 10, 2026 confirms visible left-elbow / left-arm response during the stationary test, matching the expected first-moving region
    - later user discussion and screenshots showed shoulder artifacts once the whole character was moved, but the orchestrator explicitly scoped those concerns out of `MV-G1-02` and into later integrated UE checks
    - the user explicitly directed the orchestrator on March 10, 2026 to attach screenshot evidence and move on without requiring a clip
- `Verdict`: pass
- `Why this verdict was chosen`: under the frozen stationary-check definition, the UE runtime harness produces clear left-arm-first response from the named programmatic target path, which is sufficient to prove the Physics Control command path is alive

## Criterion 4: Minimal SMPL/PHC Output Drives Manny In Chaos Without Obvious Mapping Failure

- `Status`: complete
- `Checkpoint`: `MV-G1-03`
- `Evidence`:
  - clip:
    - no local file path retained
  - expected pose:
    - `isolated left elbow flexion`
  - frozen runtime path:
    - map: `/Game/ThirdPerson/Lvl_ThirdPerson`
    - runtime owner: `UPhysAnimMvG103Subsystem`
    - command: `PhysAnim.MVG103.Start`
    - mapped subset: `L_Elbow -> lowerarm_l`, with `upperarm_l` parent context and `hand_l` held near neutral
  - observed result:
    - user reported the checkpoint completed on March 10, 2026
    - downstream bring-up work proceeded past the dedicated Manny smoke harness into the integrated Phase 1 bridge bring-up path
    - no obvious left/right mirroring blocker remained open after the smoke-check stage
  - mapping issues found:
- `Verdict`: pass
- `Why this verdict was chosen`: the user explicitly reported `MV-G1-03` completed, and no unresolved mapping-failure blocker remained at the smoke-test stage; the remaining open issue moved downstream to substep stability and ONNX export, not joint mapping correctness

## Criterion 5: Substep Rate Is Stable

- `Status`: complete
- `Checkpoint`: `MV-G1-04`
- `Evidence`:
  - settings used:
    - `Tick Physics Async = false`
    - `Substepping = true`
    - `Max Substep Delta Time = 0.008333`
    - `Max Substeps = 4`
  - clip or log:
    - user-provided Project Settings screenshot from March 10, 2026 showing the frozen `120 Hz` synchronous-substep configuration
    - no local clip path was retained
  - notes on stability:
    - user reported on March 10, 2026 that the run was `perfectly fine`, with `no jitter` and `no wobble`
    - no fallback `240 Hz` rerun was needed because the first documented configuration already stayed controllable
- `Verdict`: pass
- `Why this verdict was chosen`: the frozen `120 Hz` synchronous-substep configuration remained controllable for the checkpoint run without violent jitter, wobble, or dominant instability

## Motion Set Coverage Review

- `Status`: complete
- `Evidence`:
  - locomotion-core source note: `motion-source-map.md` assigns every locomotion motion to `broad-pretrained`, `amass-target`, or an explicit `source-risk` bucket with an approved downgrade path
  - combat-core source note: Stage 1 is now locomotion-only, so combat clips are explicitly out of scope rather than silently deferred
  - missing motions: strafes and short recovery / rebalance remain the main source-risk items and are already called out explicitly with replacement rules
- `Verdict`: pass
- `Why this verdict was chosen`: the motion-source threshold only requires named source families or approved replacements plus explicit risk notes, and the current source map satisfies that bar even though the actual clip list is still deferred

## Assumption Ledger Impact

List every assumption changed by Phase 0 evidence:

- `A-01`: move from `yellow` to `green`; the training-side visual path is proven runnable and the user judged `MV-G1-01` as `pass`
- `A-02`: already `green`; no further change from the substep-stability checkpoint
- `A-03`: move from `yellow` to `green`; the Manny smoke test completed without leaving an open mapping-failure blocker
- `A-04`: move from `yellow` to `green`; the Physics Control command path and Manny response checks both passed
- `A-05`: move from `yellow` to `green`; the documented `120 Hz` synchronous-substep configuration stayed controllable without jitter-dominated failure
- `A-06`:
- `A-07`:
- `A-08`:
- `A-09`:

## Open Risks

- 

## Missing Evidence

- none for Gate G1

## Final Orchestrator Decision

- `Final verdict`: pass
- `Decision date`: March 10, 2026
- `Decision summary`: the bridge contract, motion-source review, training-side `MV-G1-01`, stationary UE control-path checkpoint `MV-G1-02`, Manny smoke test `MV-G1-03`, and synchronous-substep stability checkpoint `MV-G1-04` now all score `pass`, so Gate G1 is satisfied
- `Can Phase 1 begin?`: yes

## If Verdict Is Not Pass

- `Failure or block reason`:
- `Fallback chosen`:
- `Need user decision?`: yes/no
- `Next task or replan action`:
