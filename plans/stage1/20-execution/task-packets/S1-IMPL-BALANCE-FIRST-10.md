# S1-IMPL-BALANCE-FIRST-10 — Slice 1 Aggregation / No-Runtime-Dependency Proof

## Purpose

Add the final Slice 1 aggregation test proving the pure support-truth surface can produce all Slice 1 outputs without runtime dependencies.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- workflow/process files

## Required Tests

- `PhysAnim.SupportTruth.Aggregation.NoRuntimeDependencyProof`
- full suite: `PhysAnim.SupportTruth`

## Required Work

1. Add exactly one aggregation automation test:
   - `PhysAnim.SupportTruth.Aggregation.NoRuntimeDependencyProof`
2. The test must construct value-only inputs and call:
   - `ExtractPatchHull`
   - `BuildFrameHull`
   - `ClassifySupportMode`
   - `AdjudicateProxy`
   - `CalculateChurnHz`
   - `ReduceSupportModeForReportWindow`
3. The test must verify representative non-default outputs from each function.
4. The test file must include only:
   - `PhysAnimSupportTruth.h`
   - `Misc/AutomationTest.h`
   - minimal Core headers if required
5. Do not add production behavior.
6. Do not edit public types.
7. Do not touch runtime files.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.Aggregation.NoRuntimeDependencyProof`
- `.\\scripts\\build.ps1 -Test PhysAnim.SupportTruth`

## Definition Of Done

- aggregation test passes
- full `PhysAnim.SupportTruth` suite passes
- build passes
- no production files changed
- no runtime dependency introduced
- no forbidden files touched
- scope check passes
- one task commit created
- `execution-log.md` updated
- handoff block provided

## Stop Conditions

Stop immediately if:
- the aggregation proof requires editing production code
- the aggregation proof requires runtime data
- a missing behavior is discovered in an earlier function
- the test requires PIE, a map, a skeletal mesh, `UWorld`, `UObject`, `FBodyInstance`, PhysicsControl, or Chaos runtime handles
- a forbidden include becomes necessary
- a forbidden file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

`Summary: <one sentence>`
`Task: S1-IMPL-BALANCE-FIRST-10`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <passed|failed> .\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.Aggregation.NoRuntimeDependencyProof; <passed|failed> .\\scripts\\build.ps1 -Test PhysAnim.SupportTruth`
`Build: <passed|failed> .\\scripts\\build.ps1`
`Scope: <passed|failed> .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-10.md -WorkingTree -AllowExecutionLog -AllowEvidence`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Working tree: clean|dirty + reason`
`Next task: none or next Slice 2 task packet`
