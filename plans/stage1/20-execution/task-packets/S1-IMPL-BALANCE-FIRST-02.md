# S1-IMPL-BALANCE-FIRST-02 — Automation Harness Registration

## Purpose

Register the first Unreal Automation Test for the pure support module.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- `PhysAnimSupportTruth.cpp` behavior implementation

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- active checkpoint packet, if executing inside a checkpoint
- this task packet
- existing scaffold files from `S1-IMPL-BALANCE-FIRST-01`

Do not read `balance_first_refactor_plan.md` unless this packet is missing, incomplete, or contradictory.

## Required Work

1. Add `PhysAnimSupportTruth.Tests.cpp`.
2. Register exactly one test:
   - `PhysAnim.SupportTruth.Harness.CompilesAndRuns`
3. The test must only prove the harness compiles and runs.
4. Do not add behavior tests.
5. Do not add production behavior.

## Required Test

- `PhysAnim.SupportTruth.Harness.CompilesAndRuns`

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.SupportTruth.Harness.CompilesAndRuns`

## Definition Of Done

- harness test appears in Automation
- harness test runs
- build passes
- no production behavior added
- no forbidden files touched
- handoff block provided

## Stop Conditions

Stop immediately if:
- the test cannot register without changing module dependencies
- the test requires PIE, a map, a skeletal mesh, `UWorld`, `UObject`, `FBodyInstance`, or PhysicsControl
- production behavior becomes necessary
- a forbidden file appears in the diff

## Required Handoff

`Summary: <one sentence>`
`Checkpoint: <checkpoint id|none>`
`Task: S1-IMPL-BALANCE-FIRST-02`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Blocked commit: <sha|none>`
`Review packet: <path|none>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <not run|passed|failed + command>`
`Build: <not run|passed|failed + command>`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Working tree: clean|dirty + reason`
`Next task: S1-IMPL-BALANCE-FIRST-03|blocked|none`
