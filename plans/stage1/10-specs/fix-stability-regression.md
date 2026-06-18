# PRD: Stage 2 Stability & Regression Prevention

## 1. Vision & Context (5W2H)

* **What:** A comprehensive fix for the stability regression in the Stage 2 Standing Proof system.
* **Why:** Current load tests are failing (falling out of balance) even at 0kg, blocking neural policy performance evaluation.
* **Who:** PhysAnim Engineering team and AI researchers.
* **Where:** Unreal Engine 5 (PhysAnimUE5), specifically within the PhysAnimPlugin automation suite.
* **When:** Immediate unblocker for Stage 2 certification.
* **How:** Simulation Coverage Forensics to identify bone/metric triggering the BridgeActive fallback, adjust PD strengths/tolerances, and implement a Stability Smoke Test.
* **How Much:** Estimated 1-2 day sprint.

## 2. Jobs-to-be-Done (JTBD)

1. **Unload/Load Stability:** When running the 3kg load test, character must stay in `BalanceActive_Standing` for the full duration.
2. **Forensic Debugging:** When a stability failure occurs, get a detailed forensic dump of why `BridgeActive` fallback was triggered.
3. **Regression Testing:** When committing new code, automated smoke test for 0kg stability.

## 3. MoSCoW Prioritization

### Must
* Implement `PHASE1_SIM_COVERAGE_FORENSIC_DUMP` analysis for dumbbell tests.
* Fix state fallback causing 0kg failures.
* Create "Stability Smoke Test" node in functional tests.

### Should
* Automate comparison between unloaded and loaded forensic snapshots.

### Could
* Dynamic PD strength adjustment during the "Settle" phase.

### Won't
* Retrain the neural network.

## 4. Epic & Task Decomposition

### Task 1: Stability Root Cause Investigation (S, P1)
*   **Action:** Decompose current failure using forensic logs to pinpoint failure metric.
*   **AC:** GIVEN a failing 0kg test, WHEN inspecting logs, THEN identify if failure is `shell_divergence`, `support_gap`, or `bone_velocity`.

### Task 2: Implementation of Stability Smoke Test (S, P1)
*   **Action:** Create a standalone functional test for 10s 0kg standing.
*   **AC:** GIVEN functional test suite, WHEN `PhysAnim.StandingProof.StabilitySmoke` is run, THEN character maintains `BalanceActive_Standing` for 10.0s.

### Task 3: State-Machine Resilience Fix (M, P1)
*   **Action:** Apply identified fixes.
*   **AC:** GIVEN fix applied, WHEN 3kg load test is run, THEN reach Hand Sag Verification assertion instead of failing state.

## 5. Risk & Constraint Analysis

* **Risks:**
    * Policy is fundamentally unstable (Low prob, High impact): Mitigation: Revert to Stage 1 PID.
    * Physics jitter at 3kg (High prob, Medium impact): Mitigation: Increase physics substeps.
* **Constraints:**
    * No increase to 3.35s proof duration.
    * Must rely on existing `PHYSANIM_LOG` infrastructure.
