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

- `Status`: pending
- `Checkpoint`: `MV-G1-01`
- `Evidence`:
  - clip:
  - notes:
- `Verdict`: pending
- `Why this verdict was chosen`:

## Criterion 2: PHC Observation/Action Contract Is Locked

- `Status`: complete
- `Evidence`:
  - confirmed observation tensor shape: inference uses a keyed runtime input dictionary, not one flat tensor; required keys are `self_obs=358`, `masked_mimic_target_poses=2024`, `masked_mimic_target_poses_masks=11`, `historical_pose_obs=5385`, `motion_text_embeddings=512`, `motion_text_embeddings_mask=1`, `terrain=256`, and `vae_noise=64`
  - confirmed action tensor shape: `69` floats ordered by `robot.dof_names` (`23` joints x `3` DoFs)
  - representation format: `self_obs` uses heading-aligned max-coords features with tan-norm rotation encoding; actions are exponential-map DoF targets normalized through `tanh`
  - gain-output policy: no gain head; Stage 1 uses fixed external gains and maps actions into built-in PD target ranges
  - source config:
    - `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\config.yaml`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\envs\base_env\components\humanoid_obs.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\envs\mimic\components\masked_mimic_obs.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\envs\base_env\env_utils\humanoid_utils.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\simulator\base_simulator\simulator.py`
    - `F:\NewEngine\Training\ProtoMotions\protomotions\agents\masked_mimic\model.py`
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
  - combat-core source note: every combat-core motion is explicitly marked `combat-finetune` rather than silently assumed to be covered by the pretrained checkpoint
  - missing motions: strafes and short recovery / rebalance remain the main source-risk items and are already called out explicitly with replacement rules
- `Verdict`: pass
- `Why this verdict was chosen`: the motion-source threshold only requires named source families or approved replacements plus explicit risk notes, and the current source map satisfies that bar even though the actual clip list is still deferred

## Assumption Ledger Impact

List every assumption changed by Phase 0 evidence:

- `A-01`:
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

- `MV-G1-01` training-side visual verdict
- `MV-G1-02` Manny control-path evidence
- `MV-G1-03` Manny smoke-test evidence
- UE-side substep-stability evidence

## Final Orchestrator Decision

- `Final verdict`: pending
- `Decision date`:
- `Decision summary`: the bridge contract and motion-source review are now documented strongly enough to score those subchecks, but G1 remains blocked on the training-side and UE-side evidence package
- `Can Phase 1 begin?`: no

## If Verdict Is Not Pass

- `Failure or block reason`:
- `Fallback chosen`:
- `Need user decision?`: yes/no
- `Next task or replan action`:
