# Task Packets

This folder contains atomic implementation task packets for Stage 1.

Task packets are not review units.

## Operating Model

- task packet = atomic commit unit
- checkpoint packet = review unit
- execution-log = current workflow pointer
- build/test/scope evidence = mechanical proof
- reviewer = consumes generated review packet only
- orchestrator = advances execution-log after valid review evidence

## Task Packet Rule

Agents must not implement from broad plans directly.

Each implementation task must be driven by a task packet in:

`plans/stage1/20-execution/task-packets/`

Agents may edit only files allowed by the active task packet.

If a task requires a file not listed in the packet, stop and report:

`Blocked: task packet does not allow required file <path>`

Do not widen scope.

## Checkpoint Rule

Checkpoint packets live in:

`plans/stage1/20-execution/checkpoints/`

A checkpoint packet may contain multiple task packets.

When executing a checkpoint:

- run task packets strictly in order
- resume from `Current Task ID` in `execution-log.md`
- skip already completed task commits listed in `execution-log.md`
- run build/tests after each task packet
- run scope check before each successful task commit
- commit after each successful task packet
- stop immediately on failure
- create a blocked-task commit after useful edits plus failure
- generate one checkpoint review packet at checkpoint end
- do not continue beyond the checkpoint

Review happens at checkpoint boundaries, not after every task packet.

## Mechanical Gates

Agents must not rely on hand-written claims for scope/build/test status.

Implementation agents must create durable evidence for:

- build
- tests, if applicable
- scope check

Evidence goes under:

`plans/stage1/30-evidence/build/`

Scope check is mandatory before every successful task commit.

Checkpoint-wide scope check is mandatory before checkpoint review.

Reviewer must reject or block any checkpoint review packet missing required evidence.

## Durable Review Reports

Reviewer agents must write their review report to:

`plans/stage1/30-evidence/reviews/<CHECKPOINT-ID>-review-report.md`

The user must not manually copy reviewer output into repo files.

Reviewer agents may update `execution-log.md` only after writing the durable review report, and only with:
- review report path
- review verdict
- checkpoint status
- blocking reason
- next runnable action

Reviewer agents must not edit:
- production code
- tests
- task packets
- checkpoint packets
- workflow scripts
- planning docs
- AGENTS.md

## Workflow Preflight

Before acting, agents must run:

Implementation:

`.\scripts\check_workflow_state.ps1 -Mode execute -Checkpoint <CHECKPOINT-ID>`

Review:

`.\scripts\check_workflow_state.ps1 -Mode review -Checkpoint <CHECKPOINT-ID>`

Fix:

`.\scripts\check_workflow_state.ps1 -Mode fix -Checkpoint <CHECKPOINT-ID>`

Accept:

`.\scripts\check_workflow_state.ps1 -Mode accept -Checkpoint <CHECKPOINT-ID> -ReviewReport <review-report-path>`

If preflight fails, stop without editing files.

## Command Vocabulary

Use checkpoint commands by default:

- `execute checkpoint <CHECKPOINT-ID>`
- `review checkpoint <CHECKPOINT-ID>`
- `fix checkpoint <CHECKPOINT-ID>`
- `accept checkpoint <CHECKPOINT-ID> with review report <path>`

Do not use `go` as the normal project loop.

Task-level commands are allowed only when the user explicitly names a single task packet.

## Clean Workflow Separation

Implementation checkpoints must not include workflow/process changes.

Forbidden inside implementation checkpoints:

- `AGENTS.md`
- `plans/stage1/20-execution/agent_workflow_protocol.md`
- this README
- `plans/stage1/20-execution/task-packets/IMPLEMENTER_PROMPT.md`
- `plans/stage1/20-execution/task-packets/REVIEWER_PROMPT.md`
- checkpoint packet rewrites
- workflow script rewrites

Allowed inside implementation checkpoints:

- files allowed by the active task packet
- `plans/stage1/20-execution/execution-log.md` status updates
- durable build/test/scope evidence under `plans/stage1/30-evidence/build/`
- blocker reports under `plans/stage1/30-evidence/blockers/`
- review packets under `plans/stage1/30-evidence/reviews/`

If an implementation checkpoint requires workflow/process changes:

- stop
- classify as `workflow blocker`
- do not continue implementation
- make workflow changes in a separate workflow checkpoint/commit
