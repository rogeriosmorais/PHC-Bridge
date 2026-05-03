# S2-IMPL-SUPPORT-CONTRACT-01 — Pure Support Contract Validator

## Purpose

Implement pure support contract validation and wire support terminal reasons into artifact snapshot assembly.

This task stays value-only. It does not read live Unreal runtime objects and does not enforce runtime behavior.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportContract.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-SUPPORT-CONTRACT-01.md`

## Forbidden Files

- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- `PhysAnimRuntimeAdapter.h`
- `PhysAnimRuntimeAdapter.cpp`
- `PhysAnimFailureArbitration.h`
- `PhysAnimFailureArbitration.cpp`
- `PhysAnimFailureArbitration.Tests.cpp`
- `PhysAnimSupportTruth.h`
- `PhysAnimSupportTruth.cpp`
- artifact emission/runtime logging files
- workflow/process files except `execution-log.md` through the normal task-completion update

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`

Do not read broad Stage 1 docs by default.

## Required Work

1. Add `FPhysAnimSupportContractSnapshot`.
2. Add `FPhysAnimSupportContractValidationResult`.
3. Add `PhysAnimValidators::ValidateSupport`.
4. Validate support area failure:
   - if `ActiveSupportSideCount > 0` and `SupportHullAreaCm2 < SupportAreaMinCm2`, emit `ActivationSupportFailure`.
5. Validate airborne gap failure:
   - if `SupportMode == Airborne` and `SupportGapTimerMs > SupportGapMaxMs`, emit `ActivationSupportFailure`.
6. Validate proxy failure:
   - if no area/gap failure exists and `ProxyTerminalReason == ActivationProxyOutsideSupportRegion`, emit `ActivationProxyOutsideSupportRegion`.
7. Do not fail on support area when `ActiveSupportSideCount == 0`.
8. Add `Support` to `FPhysAnimRunArtifactSnapshotInput`.
9. Copy support fields into `FPhysAnimRunArtifactSnapshot`.
10. Add `Input.Support.TerminalReason` to fallback artifact arbitration candidates.
11. Add `PhysAnimSupportContract.Tests.cpp`.
12. Do not edit support-truth math, runtime adapter files, or runtime state-machine files.

## Required Tests

- `PhysAnim.Validators.Support`

Required scenarios:
- valid one-foot support passes
- active support area below minimum fails with `ActivationSupportFailure`
- airborne gap over limit fails with `ActivationSupportFailure`
- proxy breach fails with `ActivationProxyOutsideSupportRegion`
- zero active support under gap limit does not fail on area
- support terminal reason flows into artifact snapshot fallback arbitration
- support failure outranks proxy failure when both are present in one support snapshot

## Required Commands

- `.\scripts\build.ps1`
- `.\scripts\build.ps1 -Test PhysAnim.Validators.Support`
- `.\scripts\build.ps1 -Test PhysAnim.Validators.ArtifactArbitration`
- `.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-SUPPORT-CONTRACT-01.md -WorkingTree -AllowExecutionLog -AllowEvidence`

## Definition Of Done

- support validator tests pass
- artifact arbitration regression test still passes
- build passes
- scope check passes
- no runtime dependency introduced
- no forbidden files touched
- one task commit created
- `execution-log.md` updated through the normal completion path
- handoff block provided

## Stop Conditions

Stop immediately if:
- support validation requires live runtime data
- a new terminal reason is needed
- support-truth math must change
- runtime adapter capture is needed
- runtime enforcement is attempted
- `PhysAnimFailureArbitration` itself needs modification
- behavior unrelated to support validation or artifact assembly is changed
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-SUPPORT-CONTRACT-01
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
