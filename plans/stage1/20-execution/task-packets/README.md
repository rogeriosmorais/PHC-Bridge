# Task Packets

This folder contains the implementation task packets for Stage 1.

Agents must not implement from broad plans directly.

## Implementer Prompt

Implementation agents should be launched with:

`plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`

The user may simply say:

`execute current task`

or:

`go`

The agent must then read `execution-log.md`, find the current task packet, and execute only that packet.

For implementation work, read only:

1. `AGENTS.md`
2. the current task packet
3. directly edited files
4. directly relevant tests
5. build/test output

The task packet is the working context.
The refactor plan is the authority.
The execution log is the state board.

## Rules

- Do not edit files outside the current task packet.
- Do not advance to the next packet until the current packet is complete.
- Do not combine packets.
- Do not add behavior in scaffold-only packets.
- Do not add tests in packets that forbid tests.
- Do not touch runtime state-machine files unless the packet explicitly allows them.
- Do not reopen architecture unless the packet is impossible.

## Required Handoff

Every task handoff must end with:

`Summary: <one sentence>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <not run|passed|failed + command>`
`Build: <not run|passed|failed + command>`
`Files changed: <comma-separated paths>`
`Forbidden files touched: none|<paths>`
`Next task: <task id or none>`

## Complete Task Lifecycle

Use this lifecycle for every implementation task:

1. User says `go`.
2. Implementer reads:
   - `AGENTS.md`
   - `agent_workflow_protocol.md`
   - `execution-log.md`
   - `IMPLEMENTER_PROMPT.md`
   - current task packet
3. Implementer records task base with `git rev-parse HEAD`.
4. Implementer edits only allowed files.
5. Implementer runs required build/tests.
6. If build/tests fail:
   - no commit
   - handoff reports failure
   - task remains current
7. If build/tests pass:
   - one task commit is created
   - handoff includes task base, task head, and commit SHA
8. Review packet is generated from task base to task head.
9. Reviewer uses only:
   - `REVIEWER_PROMPT.md`
   - generated review packet
10. If reviewer verdict is `accept`:
   - orchestrator may advance execution-log to the next task packet
11. If reviewer verdict is `fix required`:
   - same task remains current
   - fixes must stay inside the same task packet
12. If reviewer verdict is `reject`:
   - task commit is reverted
   - same task remains current

Do not start the next task until reviewer verdict is `accept`.

## Review Packet Command

After a task commit, generate a bounded review packet with:

`.\scripts\make_review_packet.ps1 -TaskPacket plans/stage1/20-execution/task-packets/<TASK-ID>.md -BuildLog <path> -TestLog <path>`

Use `-FullDiff` only when the reviewer explicitly asks for the full diff.

Paste only that review packet into the reviewer.

Do not ask for broad repo review after implementation commits.

## Reviewer Trigger

The implementer must not self-approve.

After a task packet is completed, the implementer stops.

The user/orchestrator then triggers a reviewer pass using:

`.\scripts\make_review_packet.ps1 -TaskPacket plans/stage1/20-execution/task-packets/<TASK-ID>.md`

The next task may start only after reviewer verdict:

`accept`

If verdict is `reject` or `fix required`, the next task remains blocked.

## Reviewer Prompt

Reviewer agents must be launched with:

`plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`

The reviewer receives only:

1. `REVIEWER_PROMPT.md`
2. the generated review packet

The reviewer must not receive:
- broad repo context
- full conversation history
- implementer reasoning
- architecture summaries
- unrelated docs

## Command Cheatsheet

Use:

`go`

to execute the current implementation task.

Use:

`review current task`

to review the current implementation commit against the current task packet.

Use:

`fix current task`

to fix reviewer blockers inside the current task packet.

Use:

`accept current task`

to advance the execution log after reviewer verdict `accept`.

Do not use `go` for review.
Do not use `review current task` for implementation.
Do not use `accept current task` without reviewer verdict `accept`.


