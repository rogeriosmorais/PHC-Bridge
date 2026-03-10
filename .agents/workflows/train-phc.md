---
description: How to train PHC policy with ProtoMotions
---

# Train Or Fine-Tune PHC Policy

> Status: planned workflow. This may be blocked until ProtoMotions, datasets, configs, and training scripts are in place.

Use `/eval-pretrained` first. This workflow is for the fine-tune or train-from-scratch path after the pretrained shortcut has been evaluated.

## Prerequisites

- conda environment `physanim_proto` activated
- ProtoMotions cloned to `f:\NewEngine\Training\ProtoMotions`
- AMASS data in `f:\NewEngine\Training\data\amass\`
- chosen ProtoMotions/PHC config identified so the observation/action contract is known

## Steps

1. Activate the environment once it exists:
```
conda activate physanim_proto
```

2. Start PHC baseline training on AMASS locomotion once the repo/config path is confirmed:
```
cd f:\NewEngine\Training\ProtoMotions
python phc/run.py --cfg_env phc/data/cfg/phc_kp_mcp_iccv.yaml --cfg_train phc/data/cfg/train/rlg/im_mcp.yaml --motion_file ../data/amass/ --headless
```

Note: This command is the planning default. If the locally pinned ProtoMotions source in `dependency-lock.md` uses a different entry point or config path, record the exact replacement command in the execution log before running it.

3. Monitor training loss. Expect convergence after ~2-4 hours on a 4070 SUPER.

4. Checkpoint location depends on the selected ProtoMotions version/config. Confirm the actual output path before wiring it into export workflows.

5. To fine-tune on fighting mocap, change `--motion_file` to `../data/mixamo_fight/`.
