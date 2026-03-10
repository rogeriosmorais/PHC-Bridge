# Stage 1 Pretrained Model Selection

## Purpose

This document locks the first pretrained model we will try for Stage 1 and defines the exact decision rule for staying on the pretrained path, switching to fine-tuning, or abandoning the shortcut.

## Selected Starting Model

Use the documented pretrained **MaskedMimic SMPL humanoid** model as the first feasibility checkpoint.

What this model is:

- a pretrained **MaskedMimic** agent
- for the **SMPL humanoid (no fingers)**
- trained in **IsaacLab**
- trained using datasets listed on the model card: **AMASS** and **HumanML3D**
- designed to generate motion from **partial constraints**

This is the highest-confidence documented pretrained starting point currently available in the ProtoMotions ecosystem.

## Official Source

- ProtoMotions release note announcing the pretrained SMPL model: [NVlabs/ProtoMotions releases](https://github.com/NVlabs/ProtoMotions/releases)
- Model card: [ctessler/MaskedMimic](https://huggingface.co/ctessler/MaskedMimic)

## Planning Retrieval Target

Until execution proves otherwise, use this target layout locally:

- source repo / model hub: `ctessler/MaskedMimic`
- local checkpoint target: `Training/ProtoMotions/data/pretrained_models/masked_mimic/smpl/last.ckpt`

Inference: this mirrors the checkpoint path used by the official evaluation command. The execution log must record the actual retrieval method used.

Recommended retrieval rule:

1. fetch the model files from the Hugging Face model page
2. place or mirror the checkpoint into the exact local path expected by the eval command
3. do not change the eval command first; change the local file layout first

## First Evaluation Command

The officially documented evaluation command on the model card is:

```bash
PYTHON_PATH protomotions/eval_agent.py +robot=smpl +simulator=isaaclab +motion_file=<path to motion file> +checkpoint=data/pretrained_models/masked_mimic/smpl/last.ckpt
```

Recommended first add-on flag for faster loading:

```bash
+terrain=flat
```

Recommended no-constraint evaluation add-on when checking pure target-pose behavior:

```bash
+opt=masked_mimic/constraints/no_constraint env.config.masked_mimic.masked_mimic_masking.target_pose_visible_prob=1
```

## Why This Model Wins First

This model is the right first choice because:

- it is explicitly documented as pretrained and ProtoMotions-compatible
- it matches the SMPL body family we already planned around
- it gives us the fastest path to a real feasibility result
- it lets us test the training stack without first paying the full cost of training from scratch

## Why It Might Still Fail For Our Project

Do not confuse "general pretrained humanoid motion" with "already good enough for our Stage 1 demo."

Likely limitations:

- it is not combat-specialized
- it was trained in IsaacLab and may transfer worse to IsaacGym or UE/Chaos
- it may cover broad human motion but still fail our locked combat core
- it may be sufficient for G1 but insufficient for G2

## Decision Rule

### Stay On Pretrained Path

Stay on the pretrained path only if:

- the model runs successfully in the selected simulator
- the result looks broadly believable in training
- the locomotion core appears adequately covered
- no early evidence shows that fine-tuning is obviously required for the first quality comparison

### Switch To Fine-Tuning

Switch to fine-tuning if:

- pretrained motion is broadly plausible
- but the combat core is weak or missing
- or the locked motion set is only partially covered
- or the motion style is good enough to justify adaptation rather than restart

### Abandon Pretrained Shortcut

Abandon pretrained-first only if:

- the model cannot be evaluated reliably in the chosen environment
- the result is clearly bad even for broad motion
- or the model/runtime mismatch creates too much execution risk

If this happens, update the assumption ledger before proceeding.

## Required Outputs From Pretrained Evaluation

The first pretrained evaluation must produce:

- exact checkpoint source and local path
- exact simulator used
- exact motion file used
- command run
- clip or visualization result
- pass / fail / unclear verdict
- recommendation:
  - `stay pretrained`
  - `fine-tune`
  - `stop/replan`

## Open Caveat

The model card documents the checkpoint and eval command, but not the final local retrieval command we will use. That exact retrieval method still needs to be recorded during environment setup. The planning target path above remains the default unless execution proves it wrong.
