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

## Implementer Startup Rule

If the user says any of the following:

- `execute current task`
- `start current task`
- `run current packet`
- `execute S1-...`
- `start S1-...`

then the agent must use the repository task-packet protocol automatically.

The agent must:

1. Read `AGENTS.md`.
2. Read `plans/stage1/20-execution/execution-log.md`.
3. Identify the current task packet from the `## Current Task Packet` section unless the user explicitly named a task ID.
4. Read only the current task packet.
5. Execute only that task packet.
6. Do not read broader docs unless blocked by missing or contradictory packet instructions.
7. Do not edit files outside the task packet.
8. Do not continue to the next task.
9. Return the required handoff block.

The user should not need to repeat task-specific guardrails that are already encoded in the task packet.

If the current task packet is missing, unclear, or contradictory, stop and report:

`Blocked: current task packet missing or contradictory`

## Current Task Shortcut

When the user says:

`go`

inside an implementation context, interpret it as:

`execute current task`

Do not interpret `go` as permission to perform broad work, skip review gates, advance multiple packets, or modify architecture.

`go` means:
- execute the current task packet only
- obey allowed files
- obey forbidden files
- run required build/tests
- return the required handoff block

## Review Rule

After implementation commits, review must be done from a bounded review packet.

Generate it with:

`.\scripts\make_review_packet.ps1 -TaskPacket plans/stage1/20-execution/task-packets/<TASK-ID>.md -BuildLog <path> -TestLog <path>`

The implementer must not self-approve.

The reviewer must use:

`plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`

The reviewer verdict must be one of:

- `accept`
- `fix required`
- `reject`

The implementer may continue to the next task only after reviewer verdict is `accept`.

## Review Scope Rule

Reviewers must use:

`plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`

Reviewers must review only the generated review packet.

Do not reopen architecture unless the task packet is impossible to execute.
Do not restate unchanged project context.
Do not review unrelated files.

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
  - `Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
  - `Execution log impact: none|updated|blocked`
  - `Tests: <not run|passed|failed + command>`
  - `Build: <not run|passed|failed + command>`
  - `Files changed: <comma-separated paths>`
  - `Forbidden files touched: none|<paths>`
  - `Next task: <task id or none>`
- keep responses short

## What To Read

Use `AGENTS.md` for project rules.

## Constraint To Remember

The bridge is supposed to stay small.

If a proposal replaces an existing UE5 subsystem with a large custom runtime system, it is probably the wrong move.
