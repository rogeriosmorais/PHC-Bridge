# S1-IMPL-BALANCE-FIRST-01 — Pure Support Module Scaffold

## Purpose

Create the empty pure support module scaffold with no behavior.

## Startup Shortcut

If the user says `go` or `execute current task`, execute this packet only.

Do not require the user to repeat the task instructions.

Do not add enums, structs, tests, or behavior.

This packet is scaffold-only.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimTruthTypes.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.Readiness.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.LateValidation.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.Certification.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.PolicyAndShell.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComparisonSubsystem.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimPhase1AutoCalibSubsystem.cpp`
- any runtime state-machine file
- any PhysicsControl setup file
- any artifact emission file

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet

Do not read `balance_first_refactor_plan.md` unless this packet is missing, incomplete, or contradictory.

## Required Work

1. Create `PhysAnimTruthTypes.h`.
2. Create `PhysAnimSupportTruth.h`.
3. Create `PhysAnimSupportTruth.cpp`.
4. Add only the minimum declarations/includes required for the files to compile.
5. Do not add production behavior.
6. Do not add tests yet.
7. Do not touch runtime files.

## Required Contents

`PhysAnimTruthTypes.h` may contain only:
- `#pragma once`
- minimal Core include if required

Do not add enums, structs, function declarations, or behavior in this packet.
Enums are introduced only in S1-IMPL-BALANCE-FIRST-03.

`PhysAnimSupportTruth.h` may contain only:
- `#pragma once`
- include of `PhysAnimTruthTypes.h`
- empty namespace or forward declarations required for scaffold compile

`PhysAnimSupportTruth.cpp` may contain only:
- include of `PhysAnimSupportTruth.h`
- empty namespace block if needed

## Forbidden Work

- no `ExtractPatchHull`
- no `BuildFrameHull`
- no `ClassifySupportMode`
- no `AdjudicateProxy`
- no `CalculateChurnHz`
- no `ReduceSupportModeForReportWindow`
- no tests
- no stubs pretending to implement behavior
- no runtime includes
- no `UObject`
- no `FBodyInstance`
- no `UWorld`
- no `AActor`
- no Chaos runtime handles

## Required Tests

- not applicable for this packet

## Required Build

- `.\scripts\build.ps1`

## Definition Of Done

- the three scaffold files exist
- build passes
- no behavior exists
- no tests were added
- no forbidden files were touched
- handoff block is provided

## Stop Conditions

Stop immediately if:
- scaffold compile requires editing `.Build.cs`
- scaffold compile requires runtime includes
- scaffold compile requires adding behavior
- an enum/type decision is unclear
- any forbidden file appears in the diff

## Required Handoff

`Summary: <one sentence>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: not run`
`Build: <passed|failed> .\scripts\build.ps1`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Next task: S1-IMPL-BALANCE-FIRST-02 or blocked`
