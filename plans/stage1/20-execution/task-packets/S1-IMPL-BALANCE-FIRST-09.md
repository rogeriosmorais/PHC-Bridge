# S1-IMPL-BALANCE-FIRST-09 — ReduceSupportModeForReportWindow

## Purpose

Implement only `ReduceSupportModeForReportWindow`.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- workflow/process files

## Required Tests

- `LOGIC-14`
- `LOGIC-14A`
- `LOGIC-14B`
- `LOGIC-14C`

## Contract Note

`LOGIC-14B` expects the result to expose `bValidInput = false` when `Modes.Num() != DurationsMs.Num()`.

If `FPhysAnimSupportReportWindowResult` does not yet contain `bool bValidInput`, add that value field in `PhysAnimSupportTruth.h` inside this task and default it to `true`.

This is still a pure value-only API change and is allowed only because the mapped test matrix requires it. Do not edit broad planning documents inside this implementation task.

## Required Work

1. Write failing tests locally for the required LOGIC rows.
2. Implement only `ReduceSupportModeForReportWindow`.
3. If array lengths differ:
   - return `SupportMode = EPhysAnimSupportMode::Airborne`
   - return `TotalWindowDurationMs = 0.0`
   - return `bValidInput = false`
4. If input is empty:
   - return `SupportMode = Airborne`
   - return `TotalWindowDurationMs = 0.0`
   - return `bValidInput = true`
5. Clamp negative durations to `0.0` before accumulation.
6. If total duration is `0.0` after clamping:
   - return `SupportMode = Airborne`
   - return `TotalWindowDurationMs = 0.0`
7. Accumulate duration by mode.
8. Choose the mode with greatest accumulated duration.
9. Break ties by severity in this exact order:
   - `Airborne`
   - `TransientRecovery`
   - `SingleFootSurvival`
   - `TwoFootStable`
10. Do not implement any other Slice 1 function.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.ReduceSupportModeForReportWindow`

## Definition Of Done

- all mapped tests pass
- build passes
- no other function behavior added
- no runtime dependency introduced
- no forbidden files touched
- scope check passes
- one task commit created
- `execution-log.md` updated
- handoff block provided

## Stop Conditions

Stop immediately if:
- report-window reduction requires runtime data
- tests cannot be written from the matrix
- a result field beyond `bValidInput` is needed
- severity order conflicts with another active contract
- behavior for another function is added
- a forbidden file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

`Summary: <one sentence>`
`Task: S1-IMPL-BALANCE-FIRST-09`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <passed|failed> .\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.ReduceSupportModeForReportWindow`
`Build: <passed|failed> .\\scripts\\build.ps1`
`Scope: <passed|failed> .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-09.md -WorkingTree -AllowExecutionLog -AllowEvidence`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Working tree: clean|dirty + reason`
`Next task: S1-IMPL-BALANCE-FIRST-10 or blocked`
