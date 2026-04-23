# Balance-First Refactor Plan

## 1. Purpose

This document is the authoritative implementation order for the balance-first rewrite. It defines the step-by-step sequence to move from the legacy multi-phase handoff model to the continuous-physics balance model.

## 2. Refactor Rule

- No production code change without a failing test first.
- No multi-surface refactor in one slice.
- No hidden behavior change outside the current slice.
- Maintain backward compatibility via existing symbols until the final end-to-end cutover.

## 3. Target Code Surfaces

**Primary Source Files**:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComparisonSubsystem.cpp`

**New Extraction Modules**:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`

**Test Suites**:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## 4. Refactor Sequence

The refactor MUST proceed in this exact order. Do not skip or reorder these phases.

### Phase 1 — Pure Support Logic Extraction (Slice 1)
1.  **Patch Reduction**: Extract support patch reduction.
    - **Destination**: `PhysAnimSupportTruth::ExtractPatchHull`
    - **Test**: `PhysAnimSupportTruth.Tests.cpp` (red -> green)
2.  **Frame Hull Union**: Extract frame-level hull construction from per-body patches.
    - **Destination**: `PhysAnimSupportTruth::BuildFrameHull`
    - **Test**: `PhysAnimSupportTruth.Tests.cpp` (red -> green)
3.  **Classification**: Extract support-mode classification.
    - **Destination**: `PhysAnimSupportTruth::ClassifySupportMode`
    - **Test**: `PhysAnimSupportTruth.Tests.cpp` (red -> green)
4.  **Proxy Drift**: Extract proxy containment and drift timing.
    - **Destination**: `PhysAnimSupportTruth::AdjudicateProxy`
    - **Test**: `PhysAnimSupportTruth.Tests.cpp` (red -> green)
5.  **Churn Frequency**: Extract churn rolling-window frequency calculation.
    - **Destination**: `PhysAnimSupportTruth::CalculateChurnHz`
    - **Test**: `PhysAnimSupportTruth.Tests.cpp` (red -> green)
6.  **30 Hz Reduction**: Extract report-window support-mode reduction and tie-breaks.
    - **Destination**: `PhysAnimSupportTruth::ReduceSupportModeForReportWindow`
    - **Test**: `PhysAnimSupportTruth.Tests.cpp` (red -> green)

### Phase 2 — Arbitration and Validator Extraction
7.  **Arbitration Harness**: Extract terminal-reason arbitration before runtime rewiring.
    - **Destination**: `PhysAnimValidators::ArbitrateFailure`
    - **Test**: `PhysAnimValidators.Tests.cpp` (red -> green)
8.  **Continuity**: Extract the physical-continuity validator.
    - **Destination**: `PhysAnimValidators::ValidateContinuity`
    - **Test**: `PhysAnimValidators.Tests.cpp`
9.  **Capsule**: Extract the capsule contract validator.
    - **Destination**: `PhysAnimValidators::ValidateCapsule`
    - **Test**: `PhysAnimValidators.Tests.cpp`
10. **Plant**: Extract the plant contract validator.
    - **Destination**: `PhysAnimValidators::ValidatePlant`
    - **Test**: `PhysAnimValidators.Tests.cpp`
11. **Contamination**: Extract the authority-matrix contamination classifier.
    - **Destination**: `PhysAnimValidators::ValidateAuthority`
    - **Test**: `PhysAnimValidators.Tests.cpp`
12. **Controller Stability**: Extract target discontinuity, gain/damping, pose mismatch, and standing-timeout checks.
    - **Destination**: `PhysAnimValidators::ValidateControllerStability`
    - **Test**: `PhysAnimValidators.Tests.cpp`

### Phase 3 — Artifact Schema and Runtime Adapters
13. **Artifact Schema**: Add assertions and builders for the 10-spec suite.
    - **Change**: `PhysAnimComparisonSubsystem.cpp`
    - **Test**: `PhysAnimValidators.Tests.cpp`
14. **Mapping**: Build adapters mapping `UPhysicsControlComponent` data into validators.
    - **Change**: `PhysAnimBridge.cpp` (internal private helpers)
    - **Test**: `PhysAnimValidators.Tests.cpp` (integration mocks)
15. **Integration Tests**: Verify validator input mapping from Unreal types.
16. **Freeze**: Freeze `PhysAnimSupportTruth` and `PhysAnimValidators` interfaces.

### Phase 4 — State Machine Rewrite
17. **Ready**: Rewire `PhysAnimBalanceReadyTransition::Tick` with new audits.
18. **BlendIn**: Rewire the rollout and reference-readiness checks in `PhysAnimBridge`.
19. **Validate**: Rewire the hold state in `PhysAnimBridge`.
20. **Standing**: Wire the 3.0s terminal success path.
21. **Terminal Failures**: Wire artifact population in `PhysAnimComparisonSubsystem`.

### Phase 5 — End-to-End Verification
22. **Smoke Harness**: Deterministic runtime smokes for all reasons in `PhysAnim.PIE.BalanceModeSmoke`.
23. **Regression Smokes**: Must-fail smokes for plant/authority breaches.
24. **Product Benchmark**: 3.0s standing pass criteria verification.
25. **Forensic Audit**: Artifact verification for telemetry truth.

## 5. Definition of Done Per Slice

- Failing tests first (demonstrating the need for the change).
- Minimal production change (only what is required for the slice).
- Pure-logic slices require green unit tests for the extracted logic.
- Adapter and runtime slices require green integration tests for the touched plumbing.
- Smoke regression is required only for runtime/state-machine or end-to-end slices, not for pure extraction slices.

## 6. Forbidden Moves

- No broad rewrite before validators exist.
- No direct runtime surgery before pure logic is extracted and test-covered.
- No tuning passes before truth and telemetry are stable.
- No bypassing the authority matrix for "convenience".
