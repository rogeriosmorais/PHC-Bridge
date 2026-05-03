# Blocker Reports

This folder stores durable blocker reports for agent tasks.

A blocker report is required when an agent cannot complete a task after making useful allowed-file edits.

Blocked work must not exist only in chat or only in an uncommitted working tree.

## Required Rule

If useful allowed-file edits exist and the task cannot complete:

1. write a blocker report
2. update `execution-log.md`
3. create a blocked-task commit

## Blocked Commit Format

`BLOCKED <TASK-ID>: <short blocker reason>`

## Required Report Fields

Each blocker report must include:

- task ID
- checkpoint ID, if any
- task base SHA
- head before blocked commit
- blocked commit SHA after commit
- changed files
- failed command
- failure category
- exact error summary
- next recommended action
- whether edits should be kept, reverted, or inspected
