---
name: Offline Model Lifecycle
description: Evaluate pretrained PHC-family checkpoints, train or fine-tune offline, and export ONNX runtime artifacts
---

# Offline Model Lifecycle

Use this for all offline PHC-family model work.

Covers:
- evaluate pretrained checkpoint
- decide whether to stay pretrained or fine-tune
- train or fine-tune
- export ONNX
- validate the runtime artifact

## Inputs

Required setup:
- training environment available
- ProtoMotions available
- dataset or representative motion file available
- checkpoint path known when evaluating or exporting

Assume repo root is inferred from script location or current working directory.

## Phase 1: Evaluate Pretrained

Purpose:
- decide whether the pretrained route is already good enough
- avoid unnecessary training if the checkpoint already satisfies Stage 1 needs

### Command Template

```powershell
python protomotions/eval_agent.py `
  +robot=smpl `
  +simulator=isaaclab `
  +motion_file=<PATH_TO_REPRESENTATIVE_MOTION> `
  +checkpoint=<PATH_TO_CHECKPOINT> `
  +terrain=flat
```

Optional unconstrained target-pose check:

```powershell
python protomotions/eval_agent.py `
  +robot=smpl `
  +simulator=isaaclab `
  +motion_file=<PATH_TO_REPRESENTATIVE_MOTION> `
  +checkpoint=<PATH_TO_CHECKPOINT> `
  +terrain=flat `
  +opt=masked_mimic/constraints/no_constraint `
  env.config.masked_mimic.masked_mimic_masking.target_pose_visible_prob=1
```

### Required Output

Record:
- checkpoint path
- simulator used
- motion file used
- evidence produced
- verdict:
  - stay pretrained
  - fine-tune
  - stop/replan

## Phase 2: Train Or Fine-Tune

Use only after evaluation.

### Command Template

```powershell
python phc/run.py `
  --cfg_env phc/data/cfg/phc_kp_mcp_iccv.yaml `
  --cfg_train phc/data/cfg/train/rlg/im_mcp.yaml `
  --motion_file <PATH_TO_DATASET> `
  --headless
```

### Required Output

Record:
- exact command used
- configs used
- dataset path used
- checkpoint output path
- verdict:
  - improving
  - stalled
  - misconfigured
  - unusable for current stage

## Phase 3: Export ONNX

Use after selecting a runtime checkpoint.

### Command Template

```powershell
python Training/scripts/export_onnx.py `
  --checkpoint <PATH_TO_CHECKPOINT> `
  --output Training/output/phc_policy.onnx `
  --copy-to-ue
```

### Validation

```powershell
pytest Training/tests/test_onnx_export.py -v
```

Expected UE-side destination:

```text
PhysAnimUE5/Content/NNEModels/phc_policy.onnx
```

## Success Criteria

This skill succeeds when:
- evaluation produces a justified decision
- training produces checkpoints if training was required
- ONNX export completes
- export validation passes
- the UE runtime artifact exists in the intended destination
