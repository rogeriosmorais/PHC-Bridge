# S2-FIX-LIVE-SUPPORT-HULL-AREA-01 - Fix Live Support Hull Area Evidence

## Purpose

Fix live support evidence gathering so that `StandingProof.Live` no longer reaches the Settle phase with `SupportHullAreaCm2 = 0.0` when the character has real ground support.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-FIX-LIVE-SUPPORT-HULL-AREA-01.md`
- `plans/stage1/20-execution/evidence/S2-FIX-LIVE-SUPPORT-HULL-AREA-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- runtime state-machine logic outside of debugging logs
- bridge activation logic outside of debugging logs
- Physics Control setup logic
- capsule/CMC runtime behavior logic

## Required Work

1.  **Diagnose**: Log per-foot sample counts and mapped hit positions (before/after adapter conversion) to determine why `SupportHullAreaCm2` is 0.0.
2.  **Instrumentation**: Add temporary diagnostic logs to `CaptureLiveRuntimeEvidenceHitResultForBody`.
3.  **Geometry Correction**: Ensure each supported foot produces a valid patch. If radial sphere sweeps are too small or collapsing to a single hit, replace them with a robust sampling pattern (e.g., 4-point downward line traces around the foot).
4.  **Strictness**: Keep all contacts runtime-derived; do NOT change validator thresholds or bypass Settle denial.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\run-pie-smoke.ps1 -TestName PhysAnim.StandingProof.Live`

## Definition Of Done

- `PhysAnim.StandingProof.Live` no longer fails with `SETTLE_DENIED hull_area=0.0`.
- The character successfully reaches a state with nonzero `SupportHullAreaCm2` during the Settle phase.
- All diagnostic logs are clean or properly categorized as `Warning` for PIE monitoring.
- Scope check passes.
- `execution-log.md` is updated.

## Required Handoff

```text
Summary:
Task: S2-FIX-LIVE-SUPPORT-HULL-AREA-01
Base:
Head:
Commit:
Build:
Tests:
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task:
```
