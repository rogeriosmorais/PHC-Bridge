## Context Budget Rule

Default working context is:

1. `AGENTS.md`
2. `plans/stage1/20-execution/execution-log.md`
3. the current task packet
4. directly edited files
5. directly relevant tests
6. directly relevant build/test output

Do not reread or summarize the full Stage 1 document set unless the current task packet is missing, contradictory, or explicitly asks for architecture review.

If broader context is required, stop and report:

`Context expansion needed: <specific file or reason>`

## Task Packet Rule

All implementation work must be driven by one task packet.

Task packets live in:

`plans/stage1/20-execution/task-packets/`

A task packet is the execution authority for:
- purpose
- allowed files
- forbidden files
- required work
- required tests/build
- definition of done
- stop conditions

An implementation agent may edit only files allowed by the active task packet.

If the task requires editing a file not listed in the packet, stop and report:

`Blocked: task packet does not allow required file <path>`

Do not widen scope.
Do not edit runtime files unless the task packet explicitly allows them.
Do not implement from broad plans directly.

## Default Work Loop

`go` means:

`execute the current task packet only`

The current task packet is read from `plans/stage1/20-execution/execution-log.md`.

Implementation loop:

1. Read `AGENTS.md`.
2. Run `.\\scripts\\check_workflow_state.ps1 -Mode execute`.
3. Read `execution-log.md`.
4. Read the current task packet.
5. Record `git rev-parse HEAD`.
6. Edit only allowed files.
7. Run the task's required build/test commands.
8. Run scope check:
   - `.\\scripts\\check_task_scope.ps1 -TaskPacket <task-packet> -WorkingTree -AllowExecutionLog -AllowEvidence`
9. Commit only if build/tests/scope pass.
10. Advance `execution-log.md` using `.\\scripts\\complete_task.ps1` whenever possible.
11. Run `.\\scripts\\check_workflow_state.ps1 -Mode status -Strict`.
12. Stop.

Do not require the user to manually persist output.
Do not create review packets by default.
Do not block task progress on reviewer reports unless the user explicitly asks for a review.

## Execution Log Update Rule

Do not manually duplicate task truth in prose.

After a successful task, update execution state with `scripts/complete_task.ps1` whenever possible. If manual editing is unavoidable, the final step must be:

`.\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

The execution log must satisfy:

- if `Current Task ID = none`, then `Current Task Packet = none`, status is idle, and `Next Action` must not say `go`
- if `Current Task ID != none`, then the packet exists, status is `runnable`, and `Next Action` references that exact task
- a task listed in `Completed Task Commits` must never be the current task
- checkpoint packet references must point to existing files

## Checkpoint Rule

Checkpoints are batching aids, not acceptance gates.

A checkpoint may list several task packets to execute in order, but each task packet remains the atomic commit unit.

Checkpoint rules:
- execute one task at a time
- commit after each task passes
- stop on failure
- do not combine task commits
- do not generate mandatory review packets
- do not treat process/evidence formatting issues as product-code blockers

At checkpoint end, optionally run a suite-level build/test command and record a short summary.

## Mechanical Gates

A task is complete only when:

- required build/test command passed
- scope check passed
- forbidden files untouched
- implementation committed
- `execution-log.md` points to the next task or blocked/idle state
- strict workflow validation passed

The useful proof is mechanical:
- build output
- test output
- scope check output
- commit SHA
- strict workflow-state output

Do not replace mechanical proof with long written reviews.

## Review Rule

Review is optional unless explicitly requested by the user.

Default review behavior:
- inspect only the task packet, changed files, build/test output, and scope output
- return `accept`, `fix required`, or `reject`
- do not edit files unless explicitly asked
- do not create durable review reports unless explicitly asked
- do not block progress because a review packet or report is missing

A reviewer may block only for real implementation issues:
- forbidden file touched
- required test/build failed
- wrong function implemented
- fake/stub implementation
- runtime dependency introduced in a pure task
- scope widened beyond the packet

Process-format issues are not product-code blockers.

## Workflow / Product Separation

Do not mix workflow/process changes with implementation tasks.

Implementation tasks may touch:
- files listed in the task packet
- `execution-log.md`
- build/test/scope evidence under `plans/stage1/30-evidence/`

Workflow/process changes must be separate commits.

If an implementation task reveals that the workflow is wrong, stop implementation and create a separate workflow task. Do not repair workflow inside the implementation task.

## Anti-Tunnel-Vision Rule

If the same task fails twice for the same conceptual reason, stop coding and write a pivot memo of at most 10 lines:

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

Do not keep stacking fixes in the same direction after the pivot memo trigger.

## Failure Classification Rule

When a task fails, classify the primary failure as one of:

- compile failure
- harness registration failure
- mapped test failure
- missing test expectation
- contract gap
- forbidden dependency pressure
- implementation bug
- instrumentation gap
- runtime tuning temptation
- workflow/process blocker

Then take the smallest allowed action.

Do not make broad fixes.
Do not tune visually.
Do not change multiple subsystems in response to one failure.

## Anti-Spiral Rule

Do not debug balance visually as the primary loop.

A visual improvement is not progress unless it is explained by:
- a mapped test
- a canonical terminal reason
- populated artifact fields
- one explicit hypothesis
- one owning code surface

If an in-engine failure cannot be explained by artifacts, improve instrumentation or contracts before tuning behavior.

## Minimal Assumption Ledger Rule

The assumption ledger is a high-signal risk register, not a task log.

Do not update `plans/stage1/20-execution/assumption-ledger.md` for normal implementation progress, passing tests, scaffold work, typo fixes, or expected compile fixes.

Update the ledger only when work reveals that a project assumption is new, false, weaker, stronger, blocked, or dangerous.

Every repo-work handoff must include exactly one ledger line:

`Ledger impact: none`

or:

`Ledger impact: updated: A-XX`

or:

`Ledger impact: blocked: assumption decision needed`

## Required Handoff

Keep handoffs short:

```text
Summary:
Task:
Base:
Head:
Commit:
Build:
Tests:
Scope:
Workflow:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task:
```

## Constraint To Remember

The bridge is supposed to stay small.

If a proposal replaces an existing UE5 subsystem with a large custom runtime system, it is probably the wrong move.