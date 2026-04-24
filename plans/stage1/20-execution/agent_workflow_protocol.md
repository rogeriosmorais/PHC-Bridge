# Agent Workflow Protocol

## Purpose

This protocol keeps Stage 1 automated, small, and implementation-focused.

It intentionally avoids a custom lifecycle engine. The default unit of work is one task packet, one mechanical validation pass, and one commit.

## Core Principle

Spend tokens on technical progress.

Do not spend tokens maintaining review packets, state machines, role ceremonies, or workflow artifacts unless they directly unblock implementation.

## Source Of Truth

The active state is:

`plans/stage1/20-execution/execution-log.md`

The active execution authority is the current task packet in:

`plans/stage1/20-execution/task-packets/`

A task packet controls:
- allowed files
- forbidden files
- required work
- required tests/build
- stop conditions
- definition of done

## Default Command

`go` means:

`execute the current task packet only`

Do not interpret `go` as permission for broad work, workflow rewrites, architecture review, or multi-system debugging.

## Implementation Loop

For each task:

1. Read `AGENTS.md`.
2. Read `execution-log.md`.
3. Read the current task packet.
4. Record base SHA:
   - `git rev-parse HEAD`
5. Edit only allowed files.
6. Run required build/test commands from the task packet.
7. Run scope check:
   - `.\scripts\check_task_scope.ps1 -TaskPacket <task-packet> -WorkingTree -AllowExecutionLog -AllowEvidence`
8. If build/tests/scope pass:
   - create one task commit
   - update `execution-log.md`
   - stop or continue only if the user/checkpoint explicitly asked for a batch
9. If build/tests/scope fail:
   - do not widen scope
   - preserve useful work only if allowed
   - report the failure
   - stop

## Commit Rule

Each task packet produces at most one successful implementation commit.

Commit message format:

`<TASK-ID>: <short task name>`

A successful task commit must not contain:
- forbidden files
- next-task work
- unrelated cleanup
- workflow/process changes
- broad refactors

## Checkpoints

Checkpoints are optional batching documents.

They are not mandatory review gates.

When executing a checkpoint:
- run included task packets in order
- commit after each task passes
- stop on the first failure
- do not combine task commits
- do not generate review packets by default
- do not block later technical tasks on report-format or evidence-range issues

At checkpoint end, run the checkpoint's final build/test command if one exists and record a short summary.

## Mechanical Gates

The only mandatory gates are:

1. required build/test command passes
2. scope check passes
3. forbidden files untouched
4. commit created
5. execution log updated

Durable build/test/scope logs are useful, but missing review reports are not product-code blockers.

## Reviews

Reviews are optional unless the user explicitly requests one.

A review must be small:
- read the task packet
- inspect changed files
- inspect build/test/scope output
- return `accept`, `fix required`, or `reject`

Do not require a durable review report by default.
Do not require an accept step by default.
Do not block product-code progress because a review packet was not generated.

A reviewer may block only for implementation issues:
- forbidden file touched
- required build/test failed
- scope widened
- fake/stub implementation
- wrong task implemented
- pure task gained runtime dependency
- next-task work included

Workflow/report formatting issues are not implementation blockers.

## Workflow / Product Separation

Workflow files must not be edited inside implementation tasks.

Workflow files include:
- `AGENTS.md`
- `plans/stage1/20-execution/agent_workflow_protocol.md`
- `plans/stage1/20-execution/task-packets/README.md`
- `plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`
- `plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`
- `scripts/check_workflow_state.ps1`
- `scripts/make_review_packet.ps1`
- `scripts/check_task_scope.ps1`

If workflow must change, create a separate workflow commit.

## Dirty Tree Rule

Do not leave useful uncommitted work at handoff.

Acceptable handoff states:
- clean after successful commit
- clean after reverting failed edits
- dirty only if git itself is the blocker

If useful allowed-file edits exist but the task cannot finish, either:
- keep them only if the next action is immediate and explicit, or
- commit them as a clearly marked blocked handoff commit

Blocked commit format:

`BLOCKED <TASK-ID>: <short blocker reason>`

Blocked commits are recovery points, not accepted work.

## Anti-Tunnel-Vision Pivot Gate

If the same task fails twice for the same conceptual reason, stop implementation and write:

```text
Pivot memo:
- Task:
- Hypothesis:
- Evidence for:
- Evidence against:
- Failed attempts:
- Cheaper alternative:
- Recommended next experiment:
```

Do not continue stacking fixes in the same direction without this memo.

## Stop Conditions

Stop immediately if:
- a required file is not allowed by the task packet
- the task needs runtime data forbidden by the packet
- a mapped test cannot be written from the matrix
- a shortcut/stub/fake implementation is proposed
- build/test failure requires widening scope
- implementation crosses into the next task
- the same conceptual failure repeats twice
