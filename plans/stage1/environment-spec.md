# Stage 1 Environment Spec

## Purpose

This document locks the expected environment shape for Stage 1 so setup work does not drift.

When real machine values are known, copy them into [dependency-lock.md](/F:/NewEngine/plans/stage1/dependency-lock.md).

## Environment Strategy

Use separate environment concerns instead of trying to force everything into one mixed setup:

- **UE environment**
  - local Unreal install for Manny / plugin / runtime validation
- **ProtoMotions environment**
  - Python environment for training and evaluation
- **Optional PyRoki environment**
  - separate Python environment only if retargeting workflow requires it

## Current Local UE Plan

- UE version being installed: `5.7.3`
- install root: `E:\UE_5.7`
- `UE5_PATH` should point to: `E:\UE_5.7\Engine`

## ProtoMotions Environment Decision

For Stage 1, the primary pretrained-first path should prefer **IsaacLab** first, because:

- the documented pretrained SMPL MaskedMimic model was trained in IsaacLab
- the model card explicitly says it may perform worse in IsaacGym/Genesis

IsaacGym remains relevant as a fallback or comparison path, but not as the first pretrained evaluation target.

## Python Version Expectations

The official ProtoMotions docs do not present one single pinned Python version for every simulator path in the snippets we found, but:

- the PyRoki retargeting workflow explicitly uses `python=3.10`
- Genesis installation notes in the ProtoMotions README snippet also call out Python 3.10

So the Stage 1 planning default is:

- **primary Python target:** `3.10`

Do not upgrade this casually unless an official dependency requirement forces it.

## ProtoMotions Install Contract

Planning default source pin:

- **ProtoMotions target release:** `v2.3.2`
- reason:
  - latest stable release we have verified in the current planning pass
  - includes the pretrained MaskedMimic SMPL release lineage from `v2.3`
  - avoids planning against an unpinned moving target

If execution must deviate from `v2.3.2`, record the exact replacement tag or commit in the execution log before continuing.

For IsaacLab, the README path we verified expects:

```bash
PYTHON_PATH -m pip install -e .
PYTHON_PATH -m pip install -r requirements_isaaclab.txt
PYTHON_PATH -m pip install -e isaac_utils
PYTHON_PATH -m pip install -e poselib
```

The verified `requirements_isaaclab.txt` includes packages such as:

- `tensordict==0.9.0`
- `lightning`
- `hydra-core==1.3.2`
- `pydantic==1.10.9`
- `wandb==0.15.12`
- `moviepy`
- `dm_control`
- `pandas`

For IsaacGym, the verified `requirements_isaacgym.txt` includes packages such as:

- `torch>=2.2`
- `lightning>=2.3`
- `tensordict>=0.5.0`
- `omegaconf==2.3.0`
- `transformers>=4.40`
- `dm_control>=1.0`
- `pandas`

## Named Environments

Use these environment names unless execution forces a change:

- `physanim_proto` for ProtoMotions
- `physanim_pyroki` for PyRoki if needed

## Simulator Preference Order

For Stage 1:

1. `isaaclab` for pretrained evaluation
2. `isaacgym` only if:
   - pretrained evaluation is not practical in IsaacLab, or
   - we specifically need comparison behavior, or
   - downstream tooling forces it

## Windows / Linux / WSL Planning Constraint

This is still a partially open execution detail.

What is already safe to say:

- UE work is local Windows work
- ProtoMotions Python setup may or may not be easiest in Windows-native Python depending on IsaacLab installation behavior

What is not yet fully locked:

- whether the training/eval stack should run Windows-native or through WSL/Linux tooling

So for now:

- treat this as a setup confirmation item
- do not assume a mixed Windows + WSL path unless the install actually forces it

## GPU / Runtime Expectations

- local GPU target: `RTX 4070 SUPER`
- NNE runtime on UE side: `NNERuntimeORT`
- on UE 5.7 docs, `NNERuntimeORT` is described as ONNX Runtime backed, accelerated by CPU and DirectML execution providers

Planning implication:

- on Windows, the expected GPU-oriented path is **DirectML-backed ONNX Runtime**, not CUDA-backed TensorRT

## Required Setup Evidence

Before the orchestrator treats the environment as ready, capture:

- Python version used for `physanim_proto`
- exact ProtoMotions source version or commit
- simulator selected for pretrained eval
- exact `PYTHON_PATH` binding
- exact `UE5_PATH`
- confirmation of whether PyRoki is needed yet

## Still Open

These details still need to be recorded during execution:

- exact IsaacLab install path
- whether WSL is needed
- exact checkpoint retrieval path for the pretrained model
