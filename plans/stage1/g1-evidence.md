# Stage 1 Gate G1 Evidence

## Purpose

This document is the evidence bundle for Gate G1.

Gate text from the engineering plan:

> PHC output looks alive in the selected training simulator AND the PHC observation/action contract is locked AND Physics Control Component responds to programmatic targets AND a minimal SMPL/PHC output can drive Manny in Chaos without obvious mapping failure or instability AND substep rate is stable.

## Gate Status

- `Current status`: blocked
- `Decision owner`: orchestrator with required user evidence
- `Final verdict`: pending

Use only:

- `pass`
- `fail`
- `blocked`

## Inputs Used

- `ENGINEERING_PLAN.md`
- `plans/stage1/bridge-spec.md`
- `plans/stage1/retargeting-spec.md`
- `plans/stage1/test-strategy.md`
- `plans/stage1/manual-verification.md`
- `plans/stage1/assumption-ledger.md`
- `plans/stage1/acceptance-thresholds.md`

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

- `Status`: pending
- `Checkpoint`: `MV-G1-02`
- `Evidence`:
  - clip or screenshot:
  - scene or command used:
  - notes:
- `Verdict`: pending
- `Why this verdict was chosen`:

## Criterion 4: Minimal SMPL/PHC Output Drives Manny In Chaos Without Obvious Mapping Failure

- `Status`: pending
- `Checkpoint`: `MV-G1-03`
- `Evidence`:
  - clip:
  - expected pose:
  - observed result:
  - mapping issues found:
- `Verdict`: pending
- `Why this verdict was chosen`:

## Criterion 5: Substep Rate Is Stable

- `Status`: pending
- `Evidence`:
  - settings used:
  - clip or log:
  - notes on stability:
- `Verdict`: pending
- `Why this verdict was chosen`:

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
- `A-02`:
- `A-03`:
- `A-04`:
- `A-05`:
- `A-06`:
- `A-07`:
- `A-08`:
- `A-09`:

## Open Risks

- 

## Missing Evidence

- `MV-G1-02` Manny control-path evidence
- `MV-G1-03` Manny smoke-test evidence
- UE-side substep-stability evidence

## Final Orchestrator Decision

- `Final verdict`: pending
- `Decision date`:
- `Decision summary`: the bridge contract, motion-source review, and training-side `MV-G1-01` checkpoint now all score `pass`, but G1 remains blocked on the UE-side evidence package
- `Can Phase 1 begin?`: no

## If Verdict Is Not Pass

- `Failure or block reason`:
- `Fallback chosen`:
- `Need user decision?`: yes/no
- `Next task or replan action`:
