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

## Mandatory Preflight

Before implementation, run:

`.\scripts\check_workflow_state.ps1 -Mode execute -Checkpoint <CHECKPOINT-ID>`

Use the active checkpoint ID from `execution-log.md` unless the user explicitly provided one.

If preflight fails:
- do not edit files
- do not run build
- do not change execution-log
- report the preflight failure
- stop

## Checkpoint Mode

If executing a checkpoint:

- resume from `Current Task ID` in `execution-log.md`
- skip task packets already listed under completed task commits
- execute one task packet at a time
- create durable build/test/scope logs under `plans/stage1/30-evidence/build/`
- run required build/tests after each task packet
- run scope check before each successful task commit
- commit after each successful task packet
- update `execution-log.md` after each successful task commit
- continue to the next task packet only if the checkpoint packet allows it
- generate one checkpoint review packet only after the final task in the checkpoint passes
- stop after generating the checkpoint review packet

Do not review individual task commits inside a checkpoint unless a task fails.

Required per-task scope check:

`.\scripts\check_task_scope.ps1 -TaskPacket <current-task-packet> -WorkingTree -AllowExecutionLog -AllowEvidence`

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

1. Run required build/tests.
2. Save durable build/test evidence.
3. Run scope check.
4. Confirm working tree contains no forbidden files.
5. Create exactly one task commit.
6. Include only files allowed by the task packet plus allowed execution/evidence updates.
7. Use commit message:

`<TASK-ID>: <short task name>`

Then update `execution-log.md` with:
- completed task commit
- current checkpoint head
- next task ID if checkpoint continues
- build/test/scope result

Do not create a successful task commit if scope check fails.

## Checkpoint Review Packet Rule

Only after the final task in a checkpoint passes:

1. Run checkpoint-wide scope check.
2. Save the scope check output under `plans/stage1/30-evidence/build/<CHECKPOINT-ID>-scope.log`.
3. Generate one checkpoint review packet.
4. Include the checkpoint packet.
5. Include all included task packets.
6. Include changed files from checkpoint base to checkpoint head.
7. Include build/test/scope evidence.
8. Update `execution-log.md` with the review packet path.
9. Ensure working tree is clean.
10. Stop.

Required checkpoint scope check:

`.\scripts\check_task_scope.ps1 -CheckpointPacket <checkpoint-packet> -BaseRef <checkpoint-base> -HeadRef <checkpoint-head> -AllowExecutionLog -AllowEvidence`

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
