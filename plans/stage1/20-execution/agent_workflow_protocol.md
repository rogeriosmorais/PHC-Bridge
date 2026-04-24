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

Agents execute checkpoints by default when a checkpoint is active.

A checkpoint is active when `execution-log.md` has `Checkpoint Status = in-progress`.

For checkpoint execution:

1. Read `AGENTS.md`.
2. Read `execution-log.md`.
3. Read the active checkpoint packet.
4. Resume from `Current Task ID`.
5. Read the current task packet.
6. Execute the current task packet.
7. Run required build/tests.
8. If successful:
   - commit one atomic task commit
   - update `execution-log.md`
   - continue to the next task in the checkpoint
9. If failed before useful edits:
   - leave tree clean
   - update `execution-log.md` as blocked
   - stop
10. If failed after useful edits:
   - create blocker report
   - create blocked-task commit
   - update `execution-log.md` as blocked
   - stop
11. If final checkpoint task passes:
   - generate checkpoint review packet
   - update `execution-log.md` with review packet path
   - stop

No dirty-tree handoffs are allowed.

Review happens at checkpoint boundaries, not after every tiny task.

## Dirty Tree Invariant

Every agent handoff must leave the working tree clean unless the blocker is that git cannot commit.

If useful allowed-file edits exist and the task cannot complete, the agent must create a blocked-task commit.

A blocked-task commit must include:
- useful allowed-file edits
- blocker report
- execution-log blocked update

Blocked-task commit format:

`BLOCKED <TASK-ID>: <short blocker reason>`

A blocked-task commit is not accepted work.
It is a durable recovery point.

## Reviewer Lifecycle

When reviewing a task, the reviewer must:

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/agent_workflow_protocol.md`.
3. Read `plans/stage1/20-execution/execution-log.md`.
4. Read `plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`.
5. Find the review packet path in `execution-log.md`.
6. Read the review packet.
7. Review only:
   - task packet
   - changed files
   - diff
   - build/test output
   - required handoff
8. Return or write a structured review report.

The reviewer must not run scripts by default.
The reviewer must not generate the review packet.
The reviewer must not edit `execution-log.md`.
The reviewer must not implement fixes.

If the review packet path is missing, return:

`Blocked: review packet missing`

If the review packet file is missing, return:

`Blocked: review packet file not found`

## Review Evidence Rule

A reviewer verdict is invalid unless it is backed by a review report.

The review report may be:
- inline in the reviewer response, or
- saved under `plans/stage1/30-evidence/reviews/`

The reviewer must not edit:
- `plans/stage1/20-execution/execution-log.md`
- task packets
- production code
- tests
- planning docs

The reviewer may only:
- return a structured review report, or
- create a review report file if explicitly instructed by the orchestrator.

A verdict of `accept` is valid only when blockers are `none`.

A verdict of `fix required` is valid only when at least one blocker is listed.

A verdict of `reject` is valid only when at least one blocker is listed.

Each blocker must include:
- blocker ID
- violated task-packet rule
- file/path involved
- exact required fix
- whether the fix must stay inside the current task packet

A reviewer may not set task status directly.
Only the orchestrator may update task status after reading valid review evidence.

## Commit Rules

The implementer must not create a successful task commit before required build/tests pass.

If build/tests fail before any useful file edits:
- do not commit
- return a failure handoff
- working tree must be clean

If build/tests fail after useful allowed-file edits:
- create a blocker report under `plans/stage1/30-evidence/blockers/`
- update `execution-log.md` to show the checkpoint/task is blocked
- create a blocked-task commit
- stop

Blocked-task commit format:

`BLOCKED <TASK-ID>: <short blocker reason>`

A blocked-task commit preserves work for future agents.
It does not mark the task complete.
It does not allow the next task to start.

If required build/tests pass:
- create one task commit
- include only allowed files
- include no forbidden files
- include no unrelated edits
- include no next-task work

Reviewer reviews committed code, not an uncommitted working tree.

## Blocked Commit Rules

A blocked-task commit is required when all of these are true:

1. the agent made useful allowed-file changes
2. the required build/test failed or could not run
3. the task cannot continue safely
4. the work would otherwise be left only in the working tree or chat

The blocked-task commit must include:

- current allowed-file edits
- blocker report
- execution-log blocked-state update

The blocker report must include:

- task ID
- checkpoint ID, if any
- task base SHA
- current HEAD before blocked commit
- blocked commit SHA after commit
- changed files
- failed command
- failure category
- exact error summary
- next recommended action
- whether the existing edits should be kept, reverted, or inspected

After a blocked-task commit, the next agent must start from the blocked commit and either:
- fix the blocker and continue the same task
- revert the blocked work
- or escalate if the blocker is outside the task scope

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

Every transition must cite evidence.

Allowed transitions:

1. `runnable` -> `implementation-active`
   Required evidence:
   - user/orchestrator command: `go`

2. `implementation-active` -> `implementation-failed`
   Required evidence:
   - failed build/test command
   - no commit SHA

3. `implementation-active` -> `review-pending`
   Required evidence:
   - task base SHA
   - task head SHA
   - commit SHA
   - build/test result

4. `review-pending` -> `fix-required`
   Required evidence:
   - review report path or inline review report
   - reviewer verdict: `fix required`
   - at least one blocker ID

5. `review-pending` -> `rejected`
   Required evidence:
   - review report path or inline review report
   - reviewer verdict: `reject`
   - at least one blocker ID

6. `review-pending` -> `accepted`
   Required evidence:
   - review report path or inline review report
   - reviewer verdict: `accept`
   - blockers: `none`

7. `fix-required` -> `review-pending`
   Required evidence:
   - original task base SHA
   - new task head SHA
   - fix commit SHA or amended commit SHA
   - build/test result

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
`Review: pending|not started|review report attached`
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
