# Agent Workflow Protocol

## Purpose

This protocol keeps Stage 1 automated, small, and implementation-focused.

The live workflow engine is now mcp-graph. The default unit of work is one mcp-graph task, one bounded validation pass, and one handoff.

Legacy task packets and `execution-log.md` are historical reference material. They are not live task authority unless a current mcp-graph node explicitly names them as evidence.

## Core Principle

Spend tokens on technical progress.

Do not spend tokens maintaining review packets, state machines, role ceremonies, or workflow artifacts unless they directly unblock implementation.

## Source Of Truth

The active state is mcp-graph.

mcp-graph owns:

- task status
- WIP
- next action
- dependencies
- blockers
- acceptance criteria
- estimates
- completion rationale
- evidence/test file references

Docs own durable context:

- contracts in `plans/stage1/10-specs`
- evidence summaries in `plans/stage1/30-evidence` and `docs/evidence`
- risk notes in `assumption-ledger.md`
- weak-agent route and branch rules in `weak-agent-balanceactive-protocol.md`
- test and stabilization strategy documents

Legacy mirror workflow files are not live state:

- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/*`
- `plans/stage1/20-execution/checkpoints/*`
- `plans/stage1/40-tasks/*`

If any legacy mirror doc conflicts with mcp-graph, mcp-graph wins.

## Workflow Invariants

Required invariants:

- there is at most one implementation task in progress per agent
- every implementation edit has a corresponding mcp-graph node before code changes start
- the node has acceptance criteria, stop conditions, and a validation route
- task completion records exact commands, evidence paths, and rationale in mcp-graph
- docs may supplement but must not override graph task state

If graph state and docs disagree, stop and update/demote the stale doc instead of inferring intent from prose.

## Default Command

`go` means:

`pull and execute the current mcp-graph task only`

Do not interpret `go` as permission for broad work, workflow rewrites, architecture review, or multi-system debugging.

`go` is valid only after the mcp-graph route identifies the next executable node.

## Implementation Loop

For each task:

1. Read `AGENTS.md`.
2. Pull the next mcp-graph task with the granular workflow required by `AGENTS.md`.
3. Read compact graph context and relevant RAG/context.
4. Read only the docs named by the graph task or its authoritative contracts.
5. Move the graph task to `in_progress`.
6. Record base SHA:
   - `git rev-parse HEAD`
7. Edit only files needed for the current graph task.
8. Run required build/test commands from the graph task AC or authoritative test strategy.
9. If smoke tests run, read logs with `python .\\scripts\\read_logs.py`.
10. Validate AC through mcp-graph.
11. If build/tests/AC pass:
   - update graph status to `done` with rationale and test/evidence paths
   - stop or pull the next task only if explicitly requested
12. If build/tests/AC fail:
   - do not widen scope
   - preserve useful work only if allowed
   - report the failure
   - stop

## Commit Rule

Each mcp-graph implementation task should produce at most one successful implementation commit when commits are requested.

Commit message format:

`<TASK-ID>: <short task name>`

A successful task commit must not contain:
- forbidden files
- next-task work
- unrelated cleanup
- workflow/process changes
- broad refactors

## Completion Transition Rule

Do not manually duplicate live task state in docs.

Use mcp-graph status updates for task transitions:

- `in_progress` before work starts
- `blocked` when branch/stop conditions fire
- `done` only after AC validation and required evidence

If a doc needs updating, it must be because the graph task asked for a durable contract, protocol, risk, or evidence update. Do not recreate `execution-log.md` style state.

## Checkpoints

Checkpoints are optional batching documents.

They are legacy reference material unless a current mcp-graph task explicitly reactivates one.

Do not execute a checkpoint as a live plan without first importing or mapping it to mcp-graph nodes.

## Mechanical Gates

The only mandatory gates are:

1. graph node exists
2. graph status is set to `in_progress` before work
3. required build/test command passes
4. AC validation passes
5. forbidden architecture/workflow shortcuts are absent
6. smoke logs are read when smoke tests run
7. graph status and rationale are updated

Durable build/test/scope logs are useful, but missing review reports are not product-code blockers.

## Reviews

Reviews are optional unless the user explicitly requests one.

A review must be small:
- read the graph task and AC
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
- `plans/stage1/20-execution/weak-agent-balanceactive-protocol.md`
- `plans/stage1/20-execution/task-packets/README.md`
- `plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`
- `plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`
- `scripts/check_workflow_state.ps1`
- `scripts/complete_task.ps1`
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
- graph workflow invariants fail
- a required file is outside the graph task scope
- the task needs runtime data forbidden by the graph task or active contract
- a mapped test cannot be written from the matrix
- a shortcut/stub/fake implementation is proposed
- build/test failure requires widening scope
- implementation crosses into the next task
- the same conceptual failure repeats twice
