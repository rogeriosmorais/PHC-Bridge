# S1-IMPL-BALANCE-FIRST-06 — ClassifySupportMode

## Purpose

Implement only `ClassifySupportMode`.

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

- `LOGIC-05`
- `LOGIC-06`
- `LOGIC-07`
- `LOGIC-08`

## Required Work

1. Write failing tests locally for the required LOGIC rows.
2. Implement only `ClassifySupportMode`.
3. Return `TwoFootStable` when both support sides are active.
4. Return `SingleFootSurvival` when exactly one support side is active.
5. Return `TransientRecovery` when neither side is active and `SupportGapTimerMs <= SupportGapMaxMs`.
6. Return `Airborne` when neither side is active and `SupportGapTimerMs > SupportGapMaxMs`.
7. Do not implement any other Slice 1 function.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.ClassifySupportMode`

## Definition Of Done

- all mapped tests pass
- build passes
- no other function behavior added
- no forbidden files touched
- scope check passes
- one task commit created
- `execution-log.md` updated
- handoff block provided

## Stop Conditions

Stop immediately if:
- support classification requires runtime data
- tests cannot be written from the matrix
- enum values are insufficient
- behavior for another function is added
- a forbidden file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

`Summary: <one sentence>`
`Task: S1-IMPL-BALANCE-FIRST-06`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <passed|failed> .\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.ClassifySupportMode`
`Build: <passed|failed> .\\scripts\\build.ps1`
`Scope: <passed|failed> .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-06.md -WorkingTree -AllowExecutionLog -AllowEvidence`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Working tree: clean|dirty + reason`
`Next task: S1-IMPL-BALANCE-FIRST-07 or blocked`
