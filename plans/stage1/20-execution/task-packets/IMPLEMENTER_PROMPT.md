# Implementer Prompt

You are implementing either:

1. one task packet, if explicitly assigned a task packet, or
2. one checkpoint packet, if explicitly assigned a checkpoint.

Use repository protocol.

## Startup

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/execution-log.md`.
3. If a checkpoint is active, read the active checkpoint packet.
4. Read the current task packet from `execution-log.md`.
5. Execute only the active task or checkpoint.

## Checkpoint Mode

If executing a checkpoint:

- resume from `Current Task ID` in `execution-log.md`
- skip task packets already listed under completed task commits
- execute one task packet at a time
- run required build/tests after each task packet
- commit after each successful task packet
- update `execution-log.md` after each successful task commit
- continue to the next task packet only if the checkpoint packet allows it
- generate one checkpoint review packet only after the final task in the checkpoint passes
- stop after generating the checkpoint review packet

Do not review individual task commits inside a checkpoint unless a task fails.

## Task Mode

If executing one task packet only:

- execute only that packet
- run required build/tests
- commit if successful
- stop

## Dirty Tree Rule

Never end with useful uncommitted work.

At handoff, the working tree must be clean unless git itself is the blocker.

If build/tests fail before useful edits:
- leave the working tree clean
- do not commit
- report failure

If build/tests fail after useful allowed-file edits:
- create a blocker report under `plans/stage1/30-evidence/blockers/`
- update `execution-log.md` to blocked
- create a blocked-task commit
- stop

Blocked commit format:

`BLOCKED <TASK-ID>: <short blocker reason>`

## Successful Task Commit Rule

After each successful task packet:

- create exactly one task commit
- include only files allowed by the task packet
- use commit message:

`<TASK-ID>: <short task name>`

Then update `execution-log.md` with:
- current completed task commit
- current task head
- next task ID if checkpoint continues
- build/test result

## Checkpoint Review Packet Rule

Only after the final task in a checkpoint passes:

- generate one checkpoint review packet
- include the checkpoint packet
- include all included task packets
- include changed files from checkpoint base to checkpoint head
- include build/test evidence
- update `execution-log.md` with the review packet path
- stop

## Required Handoff

`Summary: <one sentence>`
`Checkpoint: <checkpoint id|none>`
`Task: <task id>`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Blocked commit: <sha|none>`
`Review packet: <path|none>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <not run|passed|failed + command>`
`Build: <not run|passed|failed + command>`
`Files changed: <comma-separated paths>`
`Forbidden files touched: none|<paths>`
`Working tree: clean|dirty + reason`
`Next task: <task id|blocked|none>`
