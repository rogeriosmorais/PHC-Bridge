---
description: How to evaluate the Stage 1 pretrained ProtoMotions policy
---

# Evaluate Pretrained Policy

> Status: planned workflow. This may be blocked until ProtoMotions, the pretrained checkpoint, and the selected simulator path exist.

## Purpose

This is the first Stage 1 training-side workflow.

Use it before `/train-phc` so Phase 0 can answer whether the documented pretrained shortcut is already good enough, needs fine-tuning, or should be abandoned.

## Prerequisites

- conda environment `physanim_proto` activated
- ProtoMotions cloned to `f:\NewEngine\Training\ProtoMotions`
- pretrained checkpoint placed at the planned local path or equivalent recorded path
- selected simulator path confirmed, defaulting to `isaaclab`
- one representative motion file chosen for the first evaluation

## Steps

1. Activate the environment once it exists:
```bash
conda activate physanim_proto
```

2. Change into the ProtoMotions repo:
```bash
cd f:\NewEngine\Training\ProtoMotions
```

3. Run the documented Stage 1 evaluation command once the checkpoint and motion file exist:
```bash
PYTHON_PATH protomotions/eval_agent.py +robot=smpl +simulator=isaaclab +motion_file=<path to motion file> +checkpoint=data/pretrained_models/masked_mimic/smpl/last.ckpt +terrain=flat
```

4. If you are explicitly checking unconstrained target-pose behavior, add:
```bash
+opt=masked_mimic/constraints/no_constraint env.config.masked_mimic.masked_mimic_masking.target_pose_visible_prob=1
```

5. Record the exact evidence for Phase 0:
- checkpoint source and local path
- simulator used
- motion file used
- clip or visualization output
- verdict: `stay pretrained`, `fine-tune`, or `stop/replan`

## Notes

- Treat the checkpoint path in the command above as the planning target layout.
- If the real local path differs, record it in `plans/stage1/execution-log.md` and `plans/stage1/g1-evidence.md`.
- If `isaaclab` is not practical locally, document why before switching to `isaacgym`.
