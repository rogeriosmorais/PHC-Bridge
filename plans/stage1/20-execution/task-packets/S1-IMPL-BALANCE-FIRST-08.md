# S1-IMPL-BALANCE-FIRST-08 — CalculateChurnHz

## Purpose

Implement only `CalculateChurnHz`.

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

- `LOGIC-13`

## Required Work

1. Write the failing test locally for `LOGIC-13`.
2. Implement only `CalculateChurnHz`.
3. Filter `HistoricalEvents` using this exact inclusion boundary:
   - `TimestampSec > CurrentTimestampSec - WindowSeconds`
   - `TimestampSec <= CurrentTimestampSec`
4. Set `SupportChurnCount` to the count of filtered events.
5. Set `SupportChurnHz = SupportChurnCount / WindowSeconds`.
6. Do not implement any other Slice 1 function.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.CalculateChurnHz`

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
- churn calculation requires runtime data
- tests cannot be written from the matrix
- non-positive `WindowSeconds` behavior is required but not specified by the matrix
- behavior for another function is added
- a forbidden file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

`Summary: <one sentence>`
`Task: S1-IMPL-BALANCE-FIRST-08`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <passed|failed> .\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.CalculateChurnHz`
`Build: <passed|failed> .\\scripts\\build.ps1`
`Scope: <passed|failed> .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-08.md -WorkingTree -AllowExecutionLog -AllowEvidence`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Working tree: clean|dirty + reason`
`Next task: S1-IMPL-BALANCE-FIRST-09 or blocked`
