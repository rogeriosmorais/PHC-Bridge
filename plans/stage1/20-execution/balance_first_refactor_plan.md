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
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/Support/PhysAnimSupportTruth.h` (and `.cpp`)
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/Validation/PhysAnimValidators.h` (and `.cpp`)

**Test Suites**:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/Tests/PhysAnimSupportTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/Tests/PhysAnimValidatorTests.cpp`

## 4. Refactor Sequence

The refactor MUST proceed in this exact order. Do not skip or reorder these phases.

### Phase 1 — Test Harness First
1.  **Artifact Schema**: Add assertions and builders for the 10-spec suite.
    - **Change**: `PhysAnimComparisonSubsystem.cpp`
    - **Test**: `PhysAnimValidatorTests.cpp`
2.  **Support Harness**: Add pure test harness for support geometry, hulls, and timers.
    - **New**: `PhysAnimSupportTests.cpp`
3.  **Arbitration Harness**: Add pure test harness for terminal-reason arbitration.
    - **New**: `PhysAnimValidatorTests.cpp`

### Phase 2 — Pure Logic Extraction (Slice 1)
4.  **Patch Reduction**: Extract support patch reduction.
    - **Destination**: `PhysAnimSupportTruth::ExtractPatchHull`
    - **Test**: `PhysAnimSupportTests.cpp` (red -> green)
5.  **Classification**: Extract support-mode classification.
    - **Destination**: `PhysAnimSupportTruth::ClassifySupportMode`
    - **Test**: `PhysAnimSupportTests.cpp` (red -> green)
6.  **Proxy Drift**: Extract proxy drift timing.
    - **Destination**: `PhysAnimSupportTruth::AdjudicateProxy`
    - **Test**: `PhysAnimSupportTests.cpp` (red -> green)
7.  **Churn Frequency**: Extract churn rolling-window frequency calculation.
    - **Destination**: `PhysAnimSupportTruth::CalculateChurnHz`
    - **Test**: `PhysAnimSupportTests.cpp` (red -> green)

### Phase 3 — Validator Extraction
8.  **Continuity**: Extract the physical-continuity validator.
    - **Destination**: `PhysAnimValidators::ValidateContinuity`
    - **Test**: `PhysAnimValidatorTests.cpp`
9.  **Capsule**: Extract the capsule contract validator.
    - **Destination**: `PhysAnimValidators::ValidateCapsule`
    - **Test**: `PhysAnimValidatorTests.cpp`
10. **Plant**: Extract the plant contract validator.
    - **Destination**: `PhysAnimValidators::ValidatePlant`
    - **Test**: `PhysAnimValidatorTests.cpp`
11. **Contamination**: Extract the authority-matrix contamination classifier.
    - **Destination**: `PhysAnimValidators::ValidateAuthority`
    - **Test**: `PhysAnimValidatorTests.cpp`

### Phase 4 — Runtime Adapters
12. **Mapping**: Build adapters mapping `UPhysicsControlComponent` data into validators.
    - **Change**: `PhysAnimBridge.cpp` (internal private helpers)
    - **Test**: `PhysAnimValidatorTests.cpp` (integration mocks)
13. **Integration Tests**: Verify validator input mapping from Unreal types.
14. **Freeze**: Freeze `PhysAnimSupportTruth` and `PhysAnimValidators` interfaces.

### Phase 5 — State Machine Rewrite
15. **Ready**: Rewire `PhysAnimBalanceReadyTransition::Tick` with new audits.
16. **BlendIn**: Rewire the rollout and reference-readiness checks in `PhysAnimBridge`.
17. **Validate**: Rewire the hold state in `PhysAnimBridge`.
18. **Standing**: Wire the 3.0s terminal success path.
19. **Terminal Failures**: Wire artifact population in `PhysAnimComparisonSubsystem`.

### Phase 6 — End-to-End Verification
20. **Smoke Harness**: Deterministic runtime smokes for all reasons in `PhysAnim.PIE.BalanceModeSmoke`.
21. **Regression Smokes**: Must-fail smokes for plant/authority breaches.
22. **Product Benchmark**: 3.0s standing pass criteria verification.
23. **Forensic Audit**: Artifact verification for telemetry truth.

## 5. Definition of Done Per Slice

- Failing tests first (demonstrating the need for the change).
- Minimal production change (only what is required for the slice).
- Green unit tests (validating the logic).
- Green integration tests (validating the plumbing).
- No regression in the smoke harness.

## 6. Forbidden Moves

- No broad rewrite before validators exist.
- No direct runtime surgery before pure logic is extracted and test-covered.
- No tuning passes before truth and telemetry are stable.
- No bypassing the authority matrix for "convenience".
