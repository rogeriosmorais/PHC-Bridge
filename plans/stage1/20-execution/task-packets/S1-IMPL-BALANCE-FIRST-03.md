# S1-IMPL-BALANCE-FIRST-03 — Slice 1 Value Types

## Purpose

Add Slice 1 value types and public function declarations with no behavior.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimTruthTypes.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/balance_first_refactor_plan.md`
- this task packet
- `plans/stage1/20-execution/balance_first_test_matrix.md`

## Required Work

1. Add shared enums to `PhysAnimTruthTypes.h`.
2. Add Slice 1 value structs to `PhysAnimSupportTruth.h`.
3. Add public function declarations only.
4. Do not implement any function.
5. Do not add behavior tests.

## Required Types

Add only the types specified in `balance_first_refactor_plan.md`:

- `EPhysAnimSupportSide`
- `EPhysAnimSupportMode`
- `EPhysAnimTerminalReason`
- `FPhysAnimSupportPoint2D`
- `FPhysAnimSupportPatch`
- `FPhysAnimFrameHull`
- `FPhysAnimProxyAdjudicationInput`
- `FPhysAnimProxyAdjudicationResult`
- `FPhysAnimChurnEvent`
- `FPhysAnimChurnResult`
- `FPhysAnimChurnCalculationInput`
- `FPhysAnimSupportReportWindowInput`
- `FPhysAnimSupportReportWindowResult`

## Required Build

- `.\scripts\build.ps1`

## Definition Of Done

- all Slice 1 value types exist
- public declarations exist
- no function behavior exists
- build passes
- no forbidden files touched
- handoff block provided

## Stop Conditions

Stop immediately if:
- a planned type is insufficient
- a new type is needed that is not in the refactor plan
- a runtime include is needed
- implementation behavior is added
- a forbidden file appears in the diff

## Required Handoff

`Summary: <one sentence>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: not run`
`Build: <passed|failed> .\scripts\build.ps1`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Next task: S1-IMPL-BALANCE-FIRST-04 or blocked`
