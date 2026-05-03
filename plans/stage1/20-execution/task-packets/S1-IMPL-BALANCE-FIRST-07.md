# S1-IMPL-BALANCE-FIRST-07 — AdjudicateProxy

## Purpose

Implement only `AdjudicateProxy`.

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

- `LOGIC-09`
- `LOGIC-10`
- `LOGIC-11`
- `LOGIC-12`
- `LOGIC-12A`
- `LOGIC-12B`
- `LOGIC-12C`

## Required Work

1. Write failing tests locally for the required LOGIC rows.
2. Implement only `AdjudicateProxy`.
3. Treat polygon edge and vertex hits as inside the hull.
4. If `ActiveSupportSideCount == 0`:
   - skip polygon test
   - leave `ProxyInsideHull` unset
   - leave `ProxyOutsideHullDurationMs` unset
   - return `TerminalReason = EPhysAnimTerminalReason::None`
5. If `ActiveSupportSideCount > 0` and `HullPointsCm.Num() < 3`:
   - treat the proxy as outside
   - apply the normal outside-duration rule
6. If current proxy is inside:
   - set `ProxyInsideHull = true`
   - set `ProxyOutsideHullDurationMs = 0.0`
   - return `TerminalReason = None`
7. If current proxy is outside and previous duration is unset:
   - set `ProxyOutsideHullDurationMs = DeltaMs`
8. If current proxy is outside and previous duration is set:
   - set `ProxyOutsideHullDurationMs = PreviousProxyOutsideHullDurationMs + DeltaMs`
9. Return `ActivationProxyOutsideSupportRegion` only when the outside duration is greater than `ProxyDriftLimitMs`.
10. Do not implement any other Slice 1 function.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.AdjudicateProxy`

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
- proxy adjudication requires runtime data
- tests cannot be written from the matrix
- polygon edge semantics are ambiguous
- timer semantics require a public API change not listed in this packet
- behavior for another function is added
- a forbidden file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

`Summary: <one sentence>`
`Task: S1-IMPL-BALANCE-FIRST-07`
`Task base: <sha|none>`
`Task head: <sha|none>`
`Commit: <sha|none>`
`Ledger impact: none|updated: A-XX|blocked: assumption decision needed`
`Execution log impact: none|updated|blocked`
`Tests: <passed|failed> .\\scripts\\build.ps1 -Test PhysAnim.SupportTruth.AdjudicateProxy`
`Build: <passed|failed> .\\scripts\\build.ps1`
`Scope: <passed|failed> .\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S1-IMPL-BALANCE-FIRST-07.md -WorkingTree -AllowExecutionLog -AllowEvidence`
`Files changed: <paths>`
`Forbidden files touched: none|<paths>`
`Working tree: clean|dirty + reason`
`Next task: S1-IMPL-BALANCE-FIRST-08 or blocked`
