# Balance-First TDD Strategy

## 1. Purpose

This document defines the TDD strategy and testing pyramid for the balance-first rewrite. It ensures that every contract requirement is verified by deterministic tests before and during implementation.

## 2. Test Layers

- **Layer 1: Pure Logic Unit Tests**: Stateless verification of geometry, classification, and math (e.g., hull reduction, churn Hz).
- **Layer 2: Validator Contract Tests**: State-aware verification of individual validators (e.g., plant audit, capsule lock).
- **Layer 3: Runtime Integration Tests**: Verification of bridge state transitions and subsystem communication in the UE environment.
- **Layer 4: Deterministic Smoke Tests**: End-to-end PIE tests targeting specific success/failure paths (e.g., `PhysAnim.PIE.BalanceModeSmoke`).
- **Layer 5: Long Soak / Artifact Validation**: Forensic audit of large artifact batches to ensure truth consistency and terminal-reason accuracy.

## 3. TDD Rule

- **Red -> Green -> Refactor**: No production code is written until a test demonstrating its necessity fails.
- **One Contract Surface at a Time**: Complete the test-suite for a single validator or classification logic before moving to the next.
- **No Implementation-First Slices**: Implementation without a corresponding test is a contract violation.

## 4. Primary Test Targets

- **Support Patch Reduction**: Verify area-preserving hull construction from manifold points.
- **Support-Mode Classification**: Verify the four contact-pattern grades against side-support and gap-timer states.
- **Proxy-Outside-Hull Timing**: Verify `activation_proxy_outside_support_region` triggers only after the drift limit.
- **Support-Failure Timing**: Verify `activation_support_failure` for both area and gap breaches.
- **Churn Frequency Calculation**: Verify rolling 1.0s Hz calculation with synthetic transition streams.
- **Continuity Validator**: Verify detection of velocity/pose jumps during activation.
- **Capsule Validator**: Verify detection of unauthorized capsule movement.
- **Plant Validator**: Verify static audit and mutation-triggered mass/alignment checks.
- **Contamination Classification**: Verify the authority matrix rules against simulated external writes.
- **Terminal-Reason Arbitration**: Verify that the rank-based precedence table correctly selects the dominant failure reason.

## 5. Runtime Test Targets

- **Ready -> BlendIn Entry Proof**: Verify successful gate completion before starting the blend.
- **BlendIn Rollout Stability**: Verify continuity and authority during the authority ramp.
- **Validate Hold Behavior**: Verify that the system maintains stability for the full hold duration.
- **Standing 3.0-Second Success**: The primary product benchmark for a passing run.
- **Must-Fail Scenarios**: Verification that the system correctly labels deliberate breaches (Capsule, Plant, Contamination, Simulation Loss).

## 6. Artifact Validation Rule

- Every terminal failure MUST be reconstructible from emitted artifact fields.
- The artifact `support_mode` MUST be derivable from the frame-level classifications and the 30 Hz reduction rule.
- If a terminal reason is emitted without the supporting forensic fields being populated, the test fails.
