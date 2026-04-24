# Implementer Prompt

You are implementing exactly one task packet.

Use the repository workflow protocol:

`plans/stage1/20-execution/agent_workflow_protocol.md`

## Startup

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/agent_workflow_protocol.md`.
3. Read `plans/stage1/20-execution/execution-log.md`.
4. Find the current task packet from the `## Current Task Packet` section unless a task ID was explicitly provided.
5. Read the current task packet.
6. Execute only that task packet.

## Scope

Do not read broad Stage 1 docs unless the task packet is missing, contradictory, or explicitly asks for them.

Do not edit files outside the task packet.

Do not continue to the next task.

Do not self-review.

Do not approve your own work.

## Task Base

Before editing, record:

`git rev-parse HEAD`

This is `Task base`.

## Build/Test Rule

Run only the build/tests required by the task packet.

If build/tests fail:
- do not commit
- stop
- return the required handoff block
- set `Commit: none`
- set `Review: not started`

## Commit Rule

Commit only after required build/tests pass.

Create exactly one task implementation commit.

Commit message format:

`<TASK-ID>: <short task name>`

The commit may include only files allowed by the current task packet.

After committing, record:

`git rev-parse HEAD`

This is `Task head`.

## Review Packet Rule

After committing, the implementer must generate a review packet.

Run:

`.\scripts\make_review_packet.ps1 -TaskPacket <task-packet> -BaseRef <task-base> -HeadRef <task-head> -BuildLog <path-if-known> -TestLog <path-if-known> -OutputPath plans/stage1/30-evidence/reviews/<TASK-ID>-review-packet.md`

The implementer must then update `execution-log.md` only enough to record:
- lifecycle status: `review-pending`
- task base SHA
- task head SHA
- commit SHA
- build/test result
- review packet path

The implementer must not mark the task accepted.

The implementer must not continue to the next task.

## If Blocked

If the packet cannot be executed, stop and classify the blocker as one of:

- compile failure
- harness registration failure
- mapped test failure
- missing test expectation
- contract gap
- forbidden dependency pressure
- implementation bug
- instrumentation gap
- runtime tuning temptation

Then return the required handoff block.

## Required Handoff

`Summary: <one sentence>`
`Task: <task id>`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Review: pending|not started|review report attached`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <not run|passed|failed + command>`
`Build: <not run|passed|failed + command>`
`Files changed: <comma-separated paths>`
`Forbidden files touched: none|<paths>`
`Next task: <task id|blocked|none>`
