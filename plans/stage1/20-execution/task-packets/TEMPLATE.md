# <TASK-ID> — <Task Name>

## Purpose

<One sentence.>

## Allowed Files

- `<path>`

## Forbidden Files

- all files not listed under Allowed Files
- runtime state-machine files unless explicitly allowed
- bridge activation files unless explicitly allowed
- PhysicsControl setup files unless explicitly allowed
- artifact emission files unless explicitly allowed
- workflow/process files

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- directly relevant tests or matrix rows, if applicable

Do not read broad Stage 1 docs by default.

## Required Work

1. <step>
2. <step>

## Required Tests

- `<test command or not applicable>`

## Required Build

- `.\scripts\build.ps1`

## Definition Of Done

- required behavior implemented
- required tests pass
- build passes
- scope check passes
- forbidden files untouched
- one task commit created
- `execution-log.md` updated

## Stop Conditions

Stop immediately if:
- a required edit is outside Allowed Files
- runtime dependency is needed but forbidden
- the build requires widening scope
- the task cannot be completed without changing the packet
- a shortcut, stub, or approximation is proposed
- the same conceptual failure happens twice

## Handoff

```text
Summary:
Task:
Base:
Head:
Commit:
Build:
Tests:
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task:
```
