# Stage 1 Pretrained Checkpoint Retrieval

## Purpose

This document removes guesswork from fetching the Stage 1 pretrained checkpoint.

It defines:

- where the checkpoint comes from
- where it must live locally
- how to fetch it
- how to verify the result before Phase 0 continues

## Official Sources

- ProtoMotions releases page: https://github.com/NVlabs/ProtoMotions/releases
- MaskedMimic model card: https://huggingface.co/ctessler/MaskedMimic
- Hugging Face CLI guide: https://huggingface.co/docs/huggingface_hub/guides/cli
- Hugging Face model download guide: https://huggingface.co/docs/hub/en/models-downloading

## Locked Retrieval Policy

Use the pretrained **MaskedMimic SMPL humanoid** model from `ctessler/MaskedMimic`.

The local target layout is:

- repo root target: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl`
- expected checkpoint filename for Stage 1 eval: `last.ckpt`
- expected full path: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\last.ckpt`

Planning rule:

- adapt the local file layout to match the eval command
- do not rewrite the eval command first

## Preferred Retrieval Method

Use the Hugging Face CLI to download the model repo into the planned local directory:

```powershell
hf download ctessler/MaskedMimic --local-dir F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl
```

If the CLI is not installed yet:

```powershell
pip install -U huggingface_hub
```

This is the preferred path because it is scriptable and repeatable.

## Manual Fallback Method

If CLI download is blocked, use the model page in the browser:

1. open `https://huggingface.co/ctessler/MaskedMimic`
2. download the checkpoint files manually
3. place or rename the Stage 1 checkpoint so the final local path is:
   - `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\last.ckpt`

## Verification Rule

Do not call retrieval complete until all of these are true:

- the file exists at the exact path expected by the eval command
- the source repo/model page is recorded in [dependency-lock.md](/F:/NewEngine/plans/stage1/dependency-lock.md)
- the final local path is recorded in [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
- the eval-pretrained workflow can reference the checkpoint without path edits

## If Retrieval Fails

If the checkpoint cannot be retrieved cleanly:

1. mark the checkpoint path row in [dependency-lock.md](/F:/NewEngine/plans/stage1/dependency-lock.md) as `blocked`
2. update [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
3. do not advance pretrained evaluation until the path is resolved

## Handoff Requirement

The retrieval handoff must include:

- exact source used
- exact command or manual method used
- exact local path
- whether renaming or mirroring was needed
- whether Phase 0 can now run `/eval-pretrained`
