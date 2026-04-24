# S1-IMPL-BALANCE-FIRST-04 — ExtractPatchHull

## Purpose

Implement only `ExtractPatchHull`.

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

- `LOGIC-01`
- `LOGIC-02`
- `LOGIC-03`
- `LOGIC-03A`

## Required Work

1. Write failing tests locally for the required LOGIC rows.
2. Implement only `ExtractPatchHull`.
3. Compute deterministic 2D convex hull.
4. Compute `PatchAreaCm2`.
5. Return `PatchAreaCm2 = 0.0` for empty, single-point, or collinear input.
6. Return `bValidInput = false` for mixed `BodyName` or mixed `SupportSide`.
7. Do not implement any other Slice 1 function.

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.SupportTruth.ExtractPatchHull`

## Definition Of Done

- all mapped tests pass
- build passes
- no other function behavior added
- no forbidden files touched
- handoff block provided

## Stop Conditions

Stop immediately if:
- convex hull behavior requires runtime data
- tests cannot be written from the matrix
- additional public API is needed
- behavior for another function is added
- a forbidden file appears in the diff

## Required Handoff

`Summary: <one sentence>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <passed|failed> .\scripts\build.ps1 -Test PhysAnim.SupportTruth.ExtractPatchHull`
`Build: <passed|failed> .\scripts\build.ps1`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Next task: S1-IMPL-BALANCE-FIRST-05 or blocked`
