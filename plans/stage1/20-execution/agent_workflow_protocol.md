# Agent Workflow Protocol

## Purpose

This document defines the complete implementation/review/acceptance lifecycle for Stage 1 agent work.

It exists to prevent improvisation.

## Commands

Use these commands:

- `go`
  - execute the current implementation task packet only

- `review current task`
  - review the current task implementation against the current task packet only

- `fix current task`
  - fix reviewer blockers inside the same task packet only

- `accept current task`
  - advance the execution log to the next task packet after reviewer verdict `accept`

Do not use `go` for review.
Do not use `review current task` for implementation.
Do not advance to the next task without reviewer verdict `accept`.

## Roles

### Implementer

The implementer:
- executes exactly one task packet
- edits only allowed files
- runs required build/tests
- commits only after required build/tests pass
- triggers or prepares review
- does not approve its own work
- does not advance to the next task

### Reviewer

The reviewer:
- reviews only the review packet
- uses `REVIEWER_PROMPT.md`
- does not edit files
- does not fix code
- does not reopen architecture unless the task packet is impossible
- returns only `accept`, `fix required`, or `reject`

### Orchestrator

The orchestrator:
- decides when to start implementation
- decides when to start review if automated review is unavailable
- accepts/rejects the reviewer verdict
- advances `execution-log.md` after acceptance

## Task Lifecycle

Each task moves through this lifecycle:

1. `runnable`
   - current task packet is ready

2. `implementation-active`
   - implementer is working on the packet

3. `implementation-failed`
   - build/tests failed
   - no commit is created
   - same task remains current

4. `review-pending`
   - build/tests passed
   - implementation commit exists
   - review has not accepted it yet

5. `fix-required`
   - reviewer found bounded issues
   - same task remains current
   - fixes must stay inside the same task packet

6. `rejected`
   - reviewer found forbidden scope, wrong task, failed required tests/build, unrelated files, or fake implementation
   - task commit must be reverted
   - same task remains current

7. `accepted`
   - reviewer verdict is `accept`
   - execution log may advance to the next task packet

## Implementer Lifecycle

When executing a task, the implementer must:

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/execution-log.md`.
3. Read `plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`.
4. Read the current task packet.
5. Record the task base ref before edits:

   `git rev-parse HEAD`

6. Edit only files allowed by the packet.
7. Run required build/tests.
8. If build/tests fail:
   - do not commit
   - stop
   - return handoff with `Commit: none`
   - return `Review: not started`

9. If build/tests pass:
   - create exactly one task implementation commit
   - use commit message format:

     `<TASK-ID>: <short task name>`

   - record task head ref:

     `git rev-parse HEAD`

10. Generate a review packet using:

    `.\scripts\make_review_packet.ps1 -TaskPacket <packet> -BaseRef <task-base-ref> -HeadRef <task-head-ref> -BuildLog <path-if-known> -TestLog <path-if-known>`

11. If context-isolated reviewer sub-agent is available:
    - give it only:
      - `REVIEWER_PROMPT.md`
      - generated review packet
    - report reviewer verdict in the handoff

12. If context-isolated reviewer sub-agent is not available:
    - stop
    - report `Review: pending`
    - include exact review command

The implementer must not continue to the next task.

## Reviewer Lifecycle

When reviewing a task, the reviewer must:

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/execution-log.md`.
3. Read `plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`.
4. Identify the current task packet unless a task ID was explicitly provided.
5. Use the generated review packet, or generate it if enough refs/log paths are available.
6. Review only:
   - task packet
   - changed files
   - diff
   - build/test output
   - required handoff
7. Return:
   - blockers
   - non-blocking nits
   - verdict
   - next action

The reviewer must not implement fixes.

## Review Evidence Rule

A reviewer verdict is not valid unless it is backed by a review report.

A review report may be:
- returned in the reviewer response, or
- written to `plans/stage1/30-evidence/reviews/`

The reviewer must not update:
- `execution-log.md`
- task packets
- production code
- tests
- planning docs

The reviewer may only:
- return a structured review response, or
- create a review report file if explicitly instructed by the orchestrator.

A verdict of `fix required` is invalid unless it includes at least one blocker.

A verdict of `reject` is invalid unless it includes at least one blocker.

Each blocker must include:
- blocker ID
- violated task-packet rule
- file/path involved
- exact required fix
- whether the fix must stay inside the current task packet

A reviewer may not set task status directly.
Only the orchestrator may update task status after reading the review report.

## Commit Rules

The implementer must not commit before required build/tests pass.

If required build/tests fail:
- no commit
- same task remains current

If required build/tests pass:
- create one task commit
- include only allowed files
- include no forbidden files
- include no unrelated edits
- include no next-task work

Reviewer reviews committed code, not an uncommitted working tree.

## Fix Rules

If reviewer verdict is `fix required`:

1. Same task remains current.
2. Implementer runs `fix current task`.
3. Fixes must stay inside the same task packet.
4. Build/tests must pass again.
5. Implementer may either:
   - amend the task commit, or
   - create a small fixup commit
6. Review must compare from the original task base ref to the new task head ref.
7. The next task remains blocked until reviewer verdict is `accept`.

## Reject Rules

If reviewer verdict is `reject`:

1. Do not continue.
2. Revert the task implementation commit.
3. Keep the current task packet unchanged.
4. Record the rejection reason in the handoff.
5. Update the assumption ledger only if the rejection reveals a plan/contract/dependency assumption problem.

## Acceptance Rules

If reviewer verdict is `accept`:

1. The current task may be marked accepted.
2. `execution-log.md` may advance to the next task packet.
3. The next task becomes runnable only after the execution-log pointer is updated.
4. Runtime rewrite remains blocked until Slice 1 pure support logic is green.

## State Transition Evidence Rule

No task status may change based only on a bare verdict.

Every transition must cite the evidence that caused it.

Allowed transitions:

1. `runnable` -> `implementation-active`
   Evidence required:
   - user/orchestrator command `go`

2. `implementation-active` -> `implementation-failed`
   Evidence required:
   - failed build/test command
   - no commit SHA

3. `implementation-active` -> `review-pending`
   Evidence required:
   - task base SHA
   - task head SHA
   - commit SHA
   - build/test result

4. `review-pending` -> `fix-required`
   Evidence required:
   - review report path or pasted review report
   - reviewer verdict `fix required`
   - at least one blocker ID

5. `review-pending` -> `rejected`
   Evidence required:
   - review report path or pasted review report
   - reviewer verdict `reject`
   - at least one blocker ID

6. `review-pending` -> `accepted`
   Evidence required:
   - review report path or pasted review report
   - reviewer verdict `accept`
   - no blockers

7. `fix-required` -> `review-pending`
   Evidence required:
   - fix commit SHA or amended task head SHA
   - build/test result
   - same original task base SHA

A state transition without required evidence is invalid.

## Execution Log Advance

Advancing the execution log is a separate orchestration step.

It may change only:
- current task status
- current task packet pointer
- next runnable task
- latest accepted commit SHA
- notes required to preserve task state

It must not change implementation code.

## Handoff Fields

Every implementer handoff must include:

`Summary: <one sentence>`
`Task: <task id>`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Review: pending|accept|fix required|reject|not started`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <not run|passed|failed + command>`
`Build: <not run|passed|failed + command>`
`Files changed: <comma-separated paths>`
`Forbidden files touched: none|<paths>`
`Next task: <task id|blocked|none>`

Every reviewer handoff must use `REVIEWER_PROMPT.md`.

## Non-Negotiable Stop Rules

Stop immediately if:
- a task needs a file not allowed by the packet
- a task needs runtime data forbidden by the packet
- a test cannot be written from the matrix
- a shortcut/stub/fake implementation is proposed
- build/test failure requires widening scope
- implementation crosses into the next packet
- reviewer verdict is not `accept`
