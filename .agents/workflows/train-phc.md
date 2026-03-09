---
description: How to train PHC policy with ProtoMotions
---

# Train PHC Policy

## Prerequisites
- conda environment `physanim` activated
- ProtoMotions cloned to `f:\NewEngine\Training\ProtoMotions`
- AMASS data in `f:\NewEngine\Training\data\amass\`

## Steps

1. Activate environment:
```
conda activate physanim
```

2. Start PHC baseline training on AMASS locomotion:
```
cd f:\NewEngine\Training\ProtoMotions
python phc/run.py --cfg_env phc/data/cfg/phc_kp_mcp_iccv.yaml --cfg_train phc/data/cfg/train/rlg/im_mcp.yaml --motion_file ../data/amass/ --headless
```
Note: Exact command may vary based on ProtoMotions version. Check ProtoMotions README.

3. Monitor training loss. Expect convergence after ~2-4 hours on a 4070 SUPER.

4. Checkpoint is saved to `f:\NewEngine\Training\ProtoMotions/output/`.

5. To fine-tune on fighting mocap, change `--motion_file` to `../data/mixamo_fight/`.
