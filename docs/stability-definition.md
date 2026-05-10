# Stability Definition Lock - Stage 1

This document defines the formal criteria for "True Stability" in the PHC-Bridge project, specifically for Stage 1 (Kinematic Root). It serves as the authoritative reference for distinguishing between intermediate diagnostic progress and final product success.

## 1. Product Success Benchmark (Minimum)
A task or scenario can only claim **Product Success** if it meets ALL of the following criteria:

- **State Maintenance**: `BalanceActive_Standing` must be held continuously for at least **3.0 seconds** in the runtime environment.
- **Terminal Integrity**: No terminal failures (Transition rejections, watchdog timeouts, or instability gates) occurred during the window.
- **Physical Truth**: Live support and contact truth must be maintained; the character must not be "floating" or supported by non-physics constraints (e.g., world-anchor components).
- **Authority Discipline**: No "hidden" authority assistance (e.g., manual state overrides, frame-by-frame teleportation, or disabled physics controls) was active to sustain the state.

## 2. Progress Definitions

### Diagnostic Progress
- **Definition**: Verification of a specific subsystem, gate, or telemetry point without proving full state maintenance.
- **Example**: "Gate X now accepts LocomotionActiveShell" or "Watchdog correctly identifies policy timeout."
- **Scope**: Internal logic validation, TDD unit tests, and headless component tests.

### Stability Progress
- **Definition**: Significant improvement in the *duration* or *quality* of state maintenance, but falling short of the Product Success Benchmark.
- **Example**: "Standing time increased from 0.5s to 1.5s" or "Energy injection spikes reduced by 50%."

### Contract Failure
- **Definition**: Any violation of the architectural lock or runtime safety gates.
- **Example**: "Phase 3 authority active while in Stage 2A" or "Policy watchdog failed to trigger on inference hang."

### Product Success
- **Definition**: Full satisfaction of the Benchmark (Section 1) as verified by a formal smoke proof.

## 3. Task Mapping

### Tasks that can claim Product Success:
- `node_02b012838316`: S1-EXECUTE-STANDING-STABILITY-PROOF-01 (Kinematic Root Standing Stability)
- `node_64520f7aa607`: Stage 1 Standing Stability Proof (Kinematic Root)
- `node_8d94b0d7e2a3`: S1-STANDING-STABILITY-REGRESSION-SOAK-01 (Repeatability Evidence)

### Tasks that can only claim Diagnostic Progress:
- `node_cfe476217e0c`: S2-MEASURE-ACTIVATED-STANDING-STABILITY-01
- `node_16ec46a18b4f`: S2-TUNE-ACTIVATED-STANDING-STABILITY-01
- All tasks prefixed with `S2A-IMPL-` or `S2A-HARVEST-`.

## 4. Acceptance Source
The authoritative source for these criteria is the **Stage 1 Stability Proof Contract** defined in `AGENTS.md` and the `FPhysAnimStandingProof` test harness.

> [!IMPORTANT]
> If any implementation task attempts to claim "Stability Reached" without citing a formal 3.0s proof, it must be downgraded to "Diagnostic Progress" during review.
