# Stage 1 User Return Template

## Purpose

Use this template when the user returns with setup progress.

It gives the orchestrator the minimum information needed to resume Phase 0 without guessing.

After the user sends this back, transfer the confirmed values into [dependency-lock.md](/F:/NewEngine/plans/stage1/dependency-lock.md).

For the detailed user-side workflow, use [user-runbook.md](/F:/NewEngine/plans/stage1/user-runbook.md).

## How To Fill This In

- if a path exists, paste the exact absolute path
- if a thing is missing, write `missing`
- if a thing is not needed yet, write `not needed yet`
- if a planned value changed, include the corrected value instead of the planned one
- do not leave lines blank if the item matters to setup

## Copyable Template

```md
# Stage 1 Setup Return

## Tool Paths
- UE version:
- UE install root:
- UE5_PATH:
- Python version:
- Python path:
- Conda path:

## External Repos / Data
- ProtoMotions path:
- ProtoMotions version or commit:
- AMASS path:
- Mixamo clip path:
- pretrained checkpoint path:

## UE Project
- UE project created? yes/no
- project path:
- Manny present? yes/no
- required plugins enabled? yes/no
- notes:

## Evidence
- screenshots or clips:
- errors encountered:
- anything that differed from the plan:
```

## Minimum Acceptable Return

At minimum, the user should provide:

- final UE install path
- `UE5_PATH`
- whether ProtoMotions is present
- whether the pretrained checkpoint is present
- whether the UE project scaffold exists
- any blocker that prevented one of those

## Orchestrator Rule

If the return is incomplete, do not pretend setup is done. Update the execution log with the exact missing evidence and keep Phase 0 blocked.
