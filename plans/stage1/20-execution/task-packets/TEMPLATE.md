# <TASK-ID> — <Task Name>

## Purpose

<One sentence only.>

## Allowed Files

- `<path>`

## Forbidden Files

- all files not listed under Allowed Files
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files

## Required Inputs

- `AGENTS.md`
- this task packet
- directly relevant test rows, if any

## Required Work

1. <step>
2. <step>
3. <step>

## Forbidden Work

- <forbidden item>

## Required Tests

- <test or not applicable>

## Required Build

- `.\scripts\build.ps1`

## Definition Of Done

- <condition>
- build result recorded
- forbidden files untouched
- handoff block provided

## Stop Conditions

Stop immediately if:
- a required edit is outside Allowed Files
- a runtime dependency is needed
- the build requires widening scope
- the task cannot be completed without changing the packet
- a shortcut, stub, or approximation is proposed

## Required Handoff

`Summary: <one sentence>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <not run|passed|failed + command>`
`Build: <not run|passed|failed + command>`
`Files changed: <comma-separated paths>`
`Forbidden files touched: none|<paths>`
`Next task: <task id or none>`
