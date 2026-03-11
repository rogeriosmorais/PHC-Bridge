# Stage 1 Pretrained Checkpoint Retrieval

## Purpose

This document removes guesswork from fetching the Stage 1 pretrained checkpoint.

It defines:

- where the checkpoint comes from
- where it must live locally
- how to fetch it
- how to verify the result before Phase 0 continues

## Official Sources

- local pretrained asset: [motion_tracker/smpl](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl)
- local checkpoint README: [README.md](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/README.md)

## Locked Retrieval Policy

Use the repo-bundled pretrained **motion_tracker SMPL humanoid** asset.

The local target layout is:

- repo root target: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl`
- expected checkpoint filename for Stage 1 eval: `last.ckpt`
- expected full path: `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`

Planning rule:

- adapt the local file layout to match the eval command
- do not rewrite the eval command first

## Preferred Retrieval Method

Use the repo-bundled local asset already present under:

- `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl`

This is the preferred path because it avoids unnecessary retrieval work and keeps the Stage 1 runtime target aligned with the local checked-in asset bundle.

## Manual Fallback Method

If the local asset is missing or invalid:

1. restore the missing files into `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl`
2. place or rename the Stage 1 checkpoint so the final local path is:
   - `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`

## Verification Rule

Do not call retrieval complete until all of these are true:

- the file exists at the exact path expected by the eval command
- the source repo/model page is recorded in [dependency-lock.md](/F:/NewEngine/plans/stage1/10-specs/dependency-lock.md)
- the final local path is recorded in [execution-log.md](/F:/NewEngine/plans/stage1/20-execution/execution-log.md)
- the eval-pretrained workflow can reference the checkpoint without path edits

## If Retrieval Fails

If the checkpoint cannot be retrieved cleanly:

1. mark the checkpoint path row in [dependency-lock.md](/F:/NewEngine/plans/stage1/10-specs/dependency-lock.md) as `blocked`
2. update [execution-log.md](/F:/NewEngine/plans/stage1/20-execution/execution-log.md)
3. do not advance pretrained evaluation until the path is resolved

## Handoff Requirement

The retrieval handoff must include:

- exact source used
- exact command or manual method used
- exact local path
- whether renaming or mirroring was needed
- whether Phase 0 can now run `/eval-pretrained`
