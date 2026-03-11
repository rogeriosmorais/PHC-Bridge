# Stage 1 Orchestration Model

## Purpose

This document defines the execution protocol for Stage 1. The project uses a **single orchestrator** model rather than a swarm of peer agents.

## Roles

### Orchestrator Agent

The orchestrator owns:

- planning updates
- task sequencing
- dependency control
- the live assumption and failure ledger
- writable-path assignment
- blocker routing
- final integration and acceptance of worker outputs

The orchestrator is the only AI role allowed to make cross-task integration decisions.

### Worker Agent

A worker owns:

- one task ID at a time
- one primary artifact
- one handoff back to the orchestrator

A worker does not own the global plan and does not merge outputs from other workers.

### User

The user owns the intervention points defined in [user-interventions.md](/F:/NewEngine/plans/stage1/60-user/user-interventions.md).

## Core Rules

1. One orchestrator, many workers.
2. Every worker assignment must name a single task ID.
3. Every worker assignment must list frozen inputs.
4. Every worker assignment must list allowed writable paths.
5. Workers may run in parallel only when their writable paths and primary artifacts do not overlap.
6. Only the orchestrator may mark a task accepted, blocked, or superseded.
7. If an upstream assumption changes, the orchestrator must explicitly reissue affected downstream tasks.
8. The orchestrator must keep the assumption ledger current enough that workers are not operating on silently stale assumptions.

## Assignment Protocol

Before delegating a task, the orchestrator must specify:

- `Task ID`
- `Goal`
- `Frozen inputs`
- `Relevant ledger assumptions`
- `Writable paths`
- `Expected artifact`
- `Acceptance criteria`
- `Escalate conditions`

Workers should reject or pause the assignment if any of these are missing.

## Parallelization Rules

### Safe To Run In Parallel

- planning docs with separate output files
- research summaries with separate output files
- test-rubric drafting separate from bridge-spec drafting
- manual-verification drafting separate from user-intervention drafting

### Not Safe To Run In Parallel Without Extra Partitioning

- multiple workers editing the same plan file
- multiple workers editing the same plugin implementation file
- tasks that depend on an unfrozen bridge or retargeting contract
- tasks that consume unresolved user decisions

## Writable Path Policy

The orchestrator must assign disjoint writable paths whenever possible.

Examples:

- Worker A: `plans/stage1/10-specs/bridge-spec.md`
- Worker B: `plans/stage1/10-specs/retargeting-spec.md`
- Worker C: `plans/stage1/10-specs/test-strategy.md`

Do not assign two workers to the same file unless the orchestrator has already partitioned the file into non-overlapping sections and intends to merge manually.

## Blocker Protocol

Workers must stop and hand control back to the orchestrator when:

- a required input is missing
- an upstream artifact is no longer trustworthy
- the task needs a user-owned action
- the task would violate the locked architecture
- the task would require editing outside the assigned writable paths

The worker handoff must state:

- what is blocked
- why it is blocked
- who must unblock it (`orchestrator` or `user`)
- what exact evidence or decision is needed

## Acceptance Protocol

The orchestrator accepts a worker artifact only if:

- the artifact matches the assigned output
- acceptance criteria are met
- blockers are either resolved or explicitly recorded
- the handoff format is complete

If the artifact is not acceptable, the orchestrator either:

- rejects it and reissues the same task
- supersedes it with a revised assignment
- escalates to the user if the blocker is user-owned

## Replan Protocol

The orchestrator must trigger re-planning when:

- the selected ProtoMotions/PHC contract changes
- the retargeting assumptions change materially
- a gate fails
- the user changes scope, timeline, or quality bar
- an assumption in the ledger moves from monitorable to likely false

When replanning, the orchestrator must:

1. identify impacted task IDs
2. mark affected downstream artifacts as stale
3. update the assumption ledger
4. reissue tasks with updated frozen inputs

## Assumption Ledger

The orchestrator must maintain [assumption-ledger.md](/F:/NewEngine/plans/stage1/20-execution/assumption-ledger.md) as a live control document.

The ledger exists to answer:

- what must be true for Stage 1 to succeed
- how we will know each assumption is false
- what the fallback or stop condition is
- whether downstream work is still safe to continue

Workers may reference the ledger, but only the orchestrator may change assumption status.

## Gate Ownership

- G1 package is integrated by the orchestrator and approved with user evidence
- G2 package is integrated by the orchestrator and decided by the user
- G3 package is integrated by the orchestrator and decided by the user with observer feedback

Workers may prepare gate artifacts, but workers do not declare a gate passed on their own.

## Default Operating Pattern

Use this pattern unless there is a better reason not to:

1. Orchestrator picks the next runnable task.
2. Orchestrator freezes inputs and writable paths.
3. Worker produces one artifact.
4. Worker returns a handoff.
5. Orchestrator accepts, rejects, or escalates.
6. Orchestrator launches the next task or next safe parallel batch.

## Immediate Stage 1 Implication

For the current planning bundle, the recommended operating pattern is:

- orchestrator owns `STAGE1_PLAN.md` and integration across `plans/stage1/`
- orchestrator owns `plans/stage1/20-execution/assumption-ledger.md`
- worker tasks produce individual planning specs and execution packages
- no worker edits `STAGE1_PLAN.md` directly unless that is the explicit assignment
