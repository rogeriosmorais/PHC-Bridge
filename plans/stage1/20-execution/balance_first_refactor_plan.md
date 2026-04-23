# Balance-First Refactor Plan

## 1. Purpose

This document is the authoritative implementation order for the balance-first rewrite. It defines the step-by-step sequence to move from the legacy multi-phase handoff model to the continuous-physics balance model.

## 2. Refactor Rule

- No production code change without a failing test first.
- No multi-surface refactor in one slice.
- No hidden behavior change outside the current slice.
- Maintain backward compatibility via existing symbols until the final end-to-end cutover.

## 3. Target Code Surfaces

- `PhysAnimBridge`: Primary orchestration and state management.
- `PhysAnimBalanceReadyTransition`: Transition logic and truth adjudication.
- `PhysAnimComparisonSubsystem`: Telemetry collection and artifact generation.
- `PhysAnimPhase1AutoCalibSubsystem`: Auto-calibration and threshold monitoring.
- `PhysAnimComponent`: Physics Control integration and body-set management.

## 4. Refactor Sequence

The refactor MUST proceed in this exact order. Do not skip or reorder these phases.

### Phase 1 — Test Harness First
1.  **Artifact Schema**: Add artifact-schema assertions and builders for the 10-spec suite.
2.  **Support Harness**: Add pure test harness for support geometry, hulls, and timers.
3.  **Arbitration Harness**: Add pure test harness for terminal-reason arbitration and precedence.

### Phase 2 — Pure Logic Extraction
4.  **Patch Reduction**: Extract support patch reduction into a pure function/module.
5.  **Classification**: Extract support-mode classification into a pure function/module.
6.  **Proxy Drift**: Extract proxy drift timing into a pure function/module.
7.  **Churn Frequency**: Extract churn rolling-window frequency calculation.

### Phase 3 — Validator Extraction
8.  **Continuity**: Extract the physical-continuity validator.
9.  **Capsule**: Extract the capsule contract validator.
10. **Plant**: Extract the plant contract validator.
11. **Contamination**: Extract the authority-matrix contamination classifier.

### Phase 4 — Runtime Adapters
12. **Mapping**: Build adapters that map Unreal runtime data into the pure validators.
13. **Integration Tests**: Add integration tests for validator input mapping.
14. **Freeze**: Freeze validator interfaces before touching state-machine behavior.

### Phase 5 — State Machine Rewrite
15. **Ready**: Rewire the `Ready` state with the new plant and capsule audits.
16. **BlendIn**: Rewire the `BlendIn` rollout and reference-readiness checks.
17. **Validate**: Rewire the `Validate` hold state and truth adjudication.
18. **Standing**: Wire the `BalanceActive_Standing` terminal success path.
19. **Terminal Failures**: Rewire terminal failure paths and artifact population.

### Phase 6 — End-to-End Verification
20. **Smoke Harness**: Add deterministic runtime smoke tests for every failure reason.
21. **Regression Smokes**: Add must-fail regression smokes for plant and authority breaches.
22. **Product Benchmark**: Add the 3.0-second standing pass criteria verification.
23. **Forensic Audit**: Add long-soak and artifact verification for telemetry truth.

## 5. Definition of Done Per Slice

- Failing tests first (demonstrating the need for the change).
- Minimal production change (only what is required for the slice).
- Green unit tests (validating the logic).
- Green integration tests (validating the plumbing).
- No regression in the smoke harness (ensuring existing functionality remains stable).

## 6. Forbidden Moves

- No broad rewrite before validators exist.
- No direct runtime surgery before pure logic is extracted and test-covered.
- No tuning passes before truth and telemetry are stable.
- No bypassing the authority matrix for "convenience" during implementation.
