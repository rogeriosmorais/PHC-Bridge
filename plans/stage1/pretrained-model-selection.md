# Stage 1 Pretrained Model Selection

## Purpose

This document locks the first pretrained model we will try for Stage 1 and defines the exact decision rule for staying on the pretrained path, switching to fine-tuning, or abandoning the shortcut.

## Selected Starting Model

Use the documented pretrained **motion_tracker SMPL humanoid** model as the Stage 1 checkpoint.

What this model is:

- a pretrained **motion tracker** agent
- for the **SMPL humanoid (no fingers)**
- trained in **IsaacLab**
- designed to reproduce kinematic recordings inside simulation
- built around the simpler tracking-style runtime contract:
  - current body state
  - future target poses
  - terrain height samples

This is the lowest-resistance Stage 1 runtime candidate currently available in the local ProtoMotions assets.

## Official Source

- Local pretrained asset path: [motion_tracker/smpl](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl)
- Local README: [README.md](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/README.md)

## Planning Retrieval Target

Until execution proves otherwise, use this target layout locally:

- local checkpoint target: `Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/last.ckpt`
- local config target: `Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml`

Inference: this mirrors the checkpoint path used by the local pretrained asset's eval command. The execution log must record the actual retrieval method used.

Recommended retrieval rule:

1. use the repo-bundled pretrained asset already present locally
2. keep the checkpoint in the exact path expected by the eval command
3. do not fork the file layout unless execution proves the local asset is invalid

## First Evaluation Command

The documented local evaluation command is:

```bash
PYTHON_PATH protomotions/eval_agent.py +robot=smpl +simulator=isaaclab +motion_file=<path to motion file> +checkpoint=data/pretrained_models/motion_tracker/smpl/last.ckpt
```

Recommended first add-on flag for faster loading:

```bash
+terrain=flat
```

## Why This Model Wins First

This model is the right first choice because:

- it is already present in the local ProtoMotions checkout
- it matches the SMPL body family we already planned around
- it keeps the runtime bridge closer to the original Stage 1 mental model
- it removes the extra MaskedMimic runtime inputs that were increasing UE-side complexity
- it is a better fit for a locomotion-only Stage 1

## Why It Might Still Fail For Our Project

Do not confuse "pretrained motion tracker" with "already good enough for our Stage 1 demo."

Likely limitations:

- it was trained in IsaacLab and may transfer worse to IsaacGym or UE/Chaos
- it may track some locomotion classes better than others
- it may still be awkward once driven from UE target poses instead of the native training pipeline
- it may be sufficient for G1 but insufficient for G2

## Decision Rule

### Stay On Pretrained Path

Stay on the pretrained path only if:

- the model runs successfully in the selected simulator
- the result looks broadly believable in training
- the locomotion core appears adequately covered
- no early evidence shows that fine-tuning is obviously required even for the locomotion-only comparison

### Switch To Fine-Tuning

Switch to fine-tuning if:

- pretrained motion is broadly plausible
- but the locomotion-only Stage 1 set is only partially covered
- or the motion style is good enough to justify adaptation rather than restart

### Abandon Pretrained Shortcut

Abandon pretrained-first only if:

- the model cannot be evaluated reliably in the chosen environment
- the result is clearly bad even for broad motion
- or the model/runtime mismatch creates too much execution risk

If this happens, update the assumption ledger before proceeding.

## Deferred Alternative

The local repo also contains a richer pretrained `masked_mimic/smpl` checkpoint.

That checkpoint is no longer the default Stage 1 runtime target because:

- its inference contract is materially more complex
- Stage 1 is now locomotion-only
- its extra sparse-conditioning flexibility is not needed for the narrowed Stage 1 scope

Keep it as a fallback or later-stage research path, not the first UE runtime target.

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
