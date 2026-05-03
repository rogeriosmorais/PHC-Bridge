# S1-IMPL-BALANCE-FIRST-05 — BuildFrameHull

## Purpose

Implement only `BuildFrameHull`.

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

## Required Tests

- `LOGIC-04`
- `LOGIC-04A`
- `LOGIC-04B`
- `LOGIC-04C`

## Required Work

1. Write failing tests locally for the required LOGIC rows.
2. Implement only `BuildFrameHull`.
3. Ignore invalid patches.
4. Ignore empty patches for active-side counting.
5. Count distinct `EPhysAnimSupportSide` values represented by non-empty valid patches.
6. Compute frame-level convex hull from all valid patch vertices.
7. Compute `SupportHullAreaCm2`.
8. Do not implement any other Slice 1 function.

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.SupportTruth.BuildFrameHull`

## Definition Of Done

- all mapped tests pass
- build passes
- no other function behavior added
- no forbidden files touched
- handoff block provided

## Stop Conditions

Stop immediately if:
- frame hull requires runtime data
- tests cannot be written from the matrix
- active-side counting is ambiguous
- behavior for another function is added
- a forbidden file appears in the diff

## Required Handoff

`Summary: <one sentence>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <passed|failed> .\scripts\build.ps1 -Test PhysAnim.SupportTruth.BuildFrameHull`
`Build: <passed|failed> .\scripts\build.ps1`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Next task: S1-IMPL-BALANCE-FIRST-06 or blocked`
