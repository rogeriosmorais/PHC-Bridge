# AGENTS.md

This is a WINDOWS MACHINE. Do NOT run linux commands like grep, findstr, etc. Use PowerShell syntax instead.

## Project

PHC-Bridge is a UE5 proof-of-concept bridge between an offline-trained PHC-family policy and Unreal Engine runtime systems.

Primary goal:
- drive a physics-based humanoid in UE5 from a neural policy

Secondary goals:
- keep the UE bridge small
- maximize reuse of UE5 built-ins
- preserve a clean separation between offline training and runtime inference

## Architecture Lock

Do not change this architecture unless explicitly asked for an architecture review.

PoseSearch -> PHC Policy (NNE/ONNX) -> Physics Control Component -> Chaos Physics -> Renderer

Interpretation:
- motion selection/search belongs to PoseSearch
- policy inference belongs to UE5 NNE with ONNX Runtime
- low-level actuation belongs to Physics Control
- simulation belongs to Chaos
- training belongs outside UE5

## Hard Rules

1. Prefer UE5 built-ins over custom systems.
2. Keep training and runtime separate.
3. No TensorRT dependency.
4. No custom Python pipeline for UE5 asset authoring.
5. Use TDD by default for all deterministic logic. TDD is optional only for: live runtime/editor/physics behavior, visual/manual quality checks, short exploratory spikes.
6. Any exploratory spike must convert its deterministic logic into tests before the work is considered complete.
7. Do not leave permanent fail-by-design or permanent skip-by-design tests in the main suite without an explicit temporary reason and removal plan.
8. Treat Manny/Quinn as the default runtime skeleton unless changed explicitly.
9. Keep commits small and atomic.
10. Build with .\scripts\build.ps1
11. If you ran any smoke tests, then read the logs with "python .\scripts\read_logs.py". If you didn't, then ignore this step.

## Context Budget Rule

Default working context for repo work is:

1. `AGENTS.md`
2. the current task packet in `plans/stage1/20-execution/task-packets/`
3. directly edited files
4. directly relevant tests
5. directly relevant build/test output

Do not reread or summarize the full Stage 1 document set unless the current task packet is missing, contradictory, or explicitly asks for architecture review.

Do not perform broad review by default.

If broader context is required, stop and report exactly:

`Context expansion needed: <specific file or reason>`

Do not silently expand scope.

## Task Packet Rule

All implementation work must be driven by a task packet.

Task packets live in:

`plans/stage1/20-execution/task-packets/`

An implementation agent may only edit files allowed by the current task packet.

If the task requires editing a file not listed in the packet, stop and report:

`Blocked: task packet does not allow required file <path>`

Do not continue by guessing.
Do not widen scope.
Do not edit runtime files unless the packet explicitly allows them.

## Checkpoint Rule

Tiny tasks do not require individual PRs.

Agents may execute a checkpoint packet when explicitly instructed.

Checkpoint packets live in:

`plans/stage1/20-execution/checkpoints/`

A checkpoint packet may contain multiple task packets.

When executing a checkpoint:

- run task packets strictly in order
- commit after each task packet
- run required build/tests after each task packet
- stop immediately on failure
- do not skip tasks
- do not combine task commits
- generate one checkpoint review packet at the end
- do not continue beyond the checkpoint

Review happens at checkpoint boundaries, not after every tiny task.

## Default Work Command

The normal implementation command is:

`execute checkpoint <CHECKPOINT-ID>`

Do not use `go` as the default project loop.

Allowed commands:

- `execute checkpoint <CHECKPOINT-ID>`
- `review checkpoint <CHECKPOINT-ID>`
- `fix checkpoint <CHECKPOINT-ID>`
- `accept checkpoint <CHECKPOINT-ID> with review report <path>`

Task-level commands are allowed only when the user explicitly names a single task packet.

Current preferred loop:

1. execute checkpoint
2. review checkpoint
3. accept/fix/reject checkpoint
4. execute next checkpoint

Task packets are atomic commit units.
Checkpoint packets are review units.

## Agent Workflow Rule

All implementation, review, commit, fix, reject, and acceptance behavior is governed by:

`plans/stage1/20-execution/agent_workflow_protocol.md`

Use these checkpoint commands:

- `execute checkpoint <CHECKPOINT-ID>` = execute the active checkpoint only
- `review checkpoint <CHECKPOINT-ID>` = review the generated checkpoint review packet only
- `fix checkpoint <CHECKPOINT-ID>` = fix reviewer blockers inside the active checkpoint only
- `accept checkpoint <CHECKPOINT-ID> with review report <path>` = advance execution-log only after reviewer verdict `accept`

Before any command, agents must run:

Implementation/review/fix preflight:

`.\scripts\check_workflow_state.ps1 -Mode <execute|review|fix> -Checkpoint <CHECKPOINT-ID>`

Accept preflight:

`.\scripts\check_workflow_state.ps1 -Mode accept -Checkpoint <CHECKPOINT-ID> -ReviewReport <review-report-path>`

If preflight fails, stop.

Do not improvise workflow behavior.
Do not silently switch command modes.

Implementation agents must use:

`plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`

Reviewer agents must use:

`plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`

If workflow state is unclear, stop and report:

`Blocked: workflow state unclear`

Reviewer verdicts do not directly change task state.

Reviewers must not edit:
- `plans/stage1/20-execution/execution-log.md`
- task packets
- production code
- tests
- planning docs

A reviewer may only return a structured review report, or create a file under:

`plans/stage1/30-evidence/reviews/`

if explicitly instructed.

A verdict of `fix required` or `reject` without at least one blocker is invalid.

Only the orchestrator may update task state after valid review evidence exists.

## Mandatory Script Rule

Only mandatory scripts belong in the workflow.

The user must not be required to run workflow scripts manually.

Before every checkpoint command, the acting agent must run:

- `.\scripts\check_workflow_state.ps1`

Implementation agents must run after each task packet:

- `.\scripts\build.ps1`
- `.\scripts\check_task_scope.ps1`

Implementation agents must create durable build/scope evidence under:

`plans/stage1/30-evidence/build/`

Implementation agents must run only at checkpoint boundaries:

- `.\scripts\make_review_packet.ps1`

Review agents must not run build, scope, or review-packet scripts by default.
Review agents only consume the generated review packet.

Do not create optional workflow helper scripts.

Required ownership:

- Implementer runs workflow preflight before checkpoint execution.
- Implementer runs build after each task packet.
- Implementer writes durable build/test/scope logs.
- Implementer runs scope check before creating a successful task commit.
- Implementer commits after each successful task packet.
- Implementer creates a blocked-task commit after useful edits plus failure.
- Implementer generates review packet only at checkpoint boundary.
- Reviewer runs workflow preflight before review.
- Reviewer consumes the generated review packet.
- Reviewer does not generate the review packet.
- Reviewer does not edit `execution-log.md`.
- Orchestrator runs workflow preflight before acceptance with `-ReviewReport`.
- Orchestrator updates `execution-log.md` only after valid review evidence exists.

## Dirty Tree Rule

Agents must not end a task with useful uncommitted work and no durable trace.

At handoff, the working tree must be one of:

1. clean after a successful task commit
2. clean after a blocked-task commit
3. dirty only if the blocker is specifically that git cannot commit

If build/tests fail after useful allowed-file edits were made, the agent must preserve the work in a blocked-task commit.

Do not leave untracked task files behind.

Blocked-task commit format:

`BLOCKED <TASK-ID>: <short blocker reason>`

A blocked-task commit is not accepted work.
It is a durable handoff point for the next agent.

Blocked-task commits may include:
- allowed task files already edited
- blocker report under `plans/stage1/30-evidence/blockers/`
- `execution-log.md` update to blocked state

Blocked-task commits must not include:
- forbidden files
- next-task work
- unrelated cleanup
- broad refactors



## Failure Classification Rule

When a task fails, classify the failure using exactly one primary category:

- compile failure
- harness registration failure
- mapped test failure
- missing test expectation
- contract gap
- forbidden dependency pressure
- implementation bug
- instrumentation gap
- runtime tuning temptation

After classification, perform only the action allowed by the current task packet or blocker protocol.

Do not make broad fixes.
Do not tune visually.
Do not change multiple subsystems in response to one failure.

## Anti-Spiral Rule

Do not debug balance visually.

A visual improvement is not progress unless it is explained by:
- a mapped test
- a canonical terminal reason
- populated artifact fields
- one explicit hypothesis
- one owning code surface

If an in-engine failure cannot be explained by artifacts, stop implementation and improve instrumentation or contracts before tuning behavior.

## Minimal Assumption Ledger Rule

The assumption ledger is a high-signal risk register, not a task log.

Do not update `plans/stage1/20-execution/assumption-ledger.md` for normal implementation progress, passing tests, scaffold work, typo fixes, or expected compile fixes.

Update the ledger only when work reveals that a project assumption is new, false, weaker, stronger, blocked, or dangerous.

Ledger update triggers:
- a pure function unexpectedly needs runtime data
- a mapped test cannot be written from the matrix
- an artifact field cannot be emitted
- a contract is ambiguous
- a planned API is insufficient
- a forbidden dependency becomes necessary
- a repeated unexplained failure pattern appears
- an in-engine failure has no canonical terminal reason
- a shortcut, stub, or approximation is proposed to make progress look better
- the planned commit order cannot be followed

Every repo-work handoff must include exactly one ledger line:

`Ledger impact: none`

or:

`Ledger impact: updated: A-XX`

or:

`Ledger impact: blocked: assumption decision needed`

No task is complete until ledger impact has been declared.

## Response Style

When working in this repo:
- make file edits directly instead of pasting code into chat
- do not include large code snippets or diffs unless explicitly requested
- after edits, reply with:

- `Summary: <one sentence>`
- `Checkpoint: <checkpoint id|none>`
- `Task: <task id>`
- `Task base: <sha|none>`
- `Task head: <sha|none>`
- `Commit: <sha|none>`
- `Blocked commit: <sha|none>`
- `Review packet: <path|none>`
- `Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
- `Execution log impact: none|updated|blocked`
- `Tests: <not run|passed|failed + command>`
- `Build: <not run|passed|failed + command>`
- `Files changed: <comma-separated paths>`
- `Forbidden files touched: none|<paths>`
- `Working tree: clean|dirty + reason`
- `Next task: <task id|blocked|none>`

- keep responses short

## What To Read

Use `AGENTS.md` for project rules.

## Constraint To Remember

The bridge is supposed to stay small.

If a proposal replaces an existing UE5 subsystem with a large custom runtime system, it is probably the wrong move.
