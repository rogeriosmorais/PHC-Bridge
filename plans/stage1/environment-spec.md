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

The Python version must follow the simulator/runtime actually chosen.

For the current Stage 1 primary path:

- **Isaac Sim `5.x` + IsaacLab:** `python=3.11`
  - this is required because Isaac Sim `5.x` is built against Python `3.11`

Secondary references still matter, but they are not the primary path:

- the PyRoki retargeting workflow explicitly uses `python=3.10`
- Genesis notes in the ProtoMotions README also call out Python `3.10`

So the Stage 1 planning default is now:

- **primary Python target for pretrained-first IsaacLab path:** `3.11`
- **secondary Python target for other workflows that still document it:** `3.10`

Do not change the primary target again unless the selected Isaac Sim / IsaacLab version changes.

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

Use this decision rule instead of leaving the platform choice open:

1. try Windows-native setup first for ProtoMotions Stage 1 evaluation
2. switch to WSL only if Windows-native setup fails for a concrete blocker such as:
   - IsaacLab install incompatibility
   - GPU/runtime access failure
   - repeatable package-resolution failure that blocks the documented path
3. record the final choice in [dependency-lock.md](/F:/NewEngine/plans/stage1/dependency-lock.md)

Planning rule:

- do not start with a mixed Windows + WSL setup by default
- earn the WSL switch with an explicit blocker note

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
- exact checkpoint retrieval path for the pretrained model
