# Stage 1 Assumption Ledger

## Purpose

This file tracks only high-risk assumptions that can change the plan, tests, contracts, instrumentation, dependency boundaries, or runtime rollout.

This is not:
- a task log
- a daily journal
- a commit history
- a place to record normal implementation progress
- a place to record every failed test

Git history is sufficient for old ledger history.

## Update Rule

Update this file only when an assumption is:

- new
- false
- weaker than expected
- stronger than expected
- blocked
- dangerous
- responsible for changing the plan

Do not update this file for:
- scaffold commits
- passing tests
- normal implementation work
- typo fixes
- expected compile fixes
- ordinary red/green TDD cycles
- implementation of already-planned behavior

## Required Handoff Line

Every repo-work handoff must declare one of:

- `Ledger impact: none`
- `Ledger impact: updated: A-XX`
- `Ledger impact: blocked: assumption decision needed`

## Status Values

Use only these status values:

- `Open`
- `Watching`
- `Resolved`
- `Falsified`
- `Blocked`

## Active Assumptions

| ID | Assumption | Status | Trigger For Update | Current Decision | Next Check |
|---|---|---|---|---|---|
| A-01 | Slice 1 pure support truth can be implemented without runtime object dependencies. | Watching | A pure support function needs `UObject`, `FBodyInstance`, component state, `UWorld`, `AActor`, or Chaos runtime handles. | Keep Slice 1 value-only. Stop if runtime data becomes necessary. | Commits 1-10 of Slice 1. |
| A-02 | The test matrix is sufficient to drive Slice 1 without implementation guessing. | Watching | A mapped test cannot be written cleanly, has missing expected behavior, or lacks required data. | Update the matrix before implementing behavior. | Each Slice 1 behavior commit. |
| A-03 | Runtime adapter capture can be delayed until pure support truth and validators are green. | Watching | Slice 1 or validator code requires live runtime reads before adapter commits. | Do not cross adapter boundary early. Update refactor plan if this fails. | Validator scaffolding and adapter planning. |
| A-04 | Artifact-backed failure classification is sufficient to prevent visual tuning spirals. | Watching | An in-engine failure cannot be explained by canonical terminal reason and required forensic fields. | Stop tuning. Fix instrumentation or contract coverage first. | First shadow-validation and runtime wiring runs. |

## Entry Format

When adding a new assumption, use this format:

| ID | Assumption | Status | Trigger For Update | Current Decision | Next Check |
|---|---|---|---|---|---|
| A-XX | One sentence. | Open | Exact event that requires updating this row. | Current decision or stop rule. | Next test, commit, run, or review point. |

## Minimal Update Rules

When updating an existing row:
- change the status
- change the current decision
- change the next check
- keep the row short

Do not add long narrative history.
Do not paste logs.
Do not duplicate execution-log content.
Do not add run cards here.
