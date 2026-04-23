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

### 4.1 Pure Logic (PhysAnimSupportTruth)
- **Support Patch Reduction**: Verify area-preserving hull construction from manifold points.
- **Support-Mode Classification**: Verify the four contact-pattern grades (TwoFoot, SingleFoot, Transient, Airborne) against side-support and gap-timer states.
- **Proxy-Outside-Hull Adjudication**: Verify proxy containment and drift-timer logic.
- **Churn Frequency Calculation**: Verify rolling 1.0s Hz calculation with synthetic transition streams.

### 4.2 Adapter-Fed Validators (PhysAnimValidators)
- **Continuity Validator**:
    - **Body Instance**: Detect invalid or missing body instances.
    - **Physics State**: Detect "Simulate Physics" disabling on active bodies.
    - **Sleep Management**: Detect bodies exceeding the sleep/wake limit semantics.
    - **State Jump**: Detect pose/velocity jumps between frames.
- **Capsule Validator**:
    - **Actor Lock**: Detect actor-level transform changes (Freeze check).
    - **Mesh Integrity**: Detect absolute transform deltas on the mesh component.
    - **Component States**: Detect CMC (Character Movement) activity or UpdatedComponent nulling.
    - **External State**: Detect unauthorized collision mode changes or overlap mutations.
- **Plant Validator**:
    - **Topology**: Detect skeleton or skeletal mesh mismatch.
    - **Axis/Length**: Detect bone length or alignment drift.
    - **Mass/Inertia**: Detect mass, center of mass, or inertia tensor mutations against the contract baseline.
    - **Geometry**: Detect collision/filter baseline breaches.
- **Contamination Classifier**:
    - **Authority Matrix**: Detect unauthorized external writes to policy-driven bodies.
    - **Material Breaches**: Detect material-state contamination during activation.

## 5. Validator Scope Freeze

Each validator is strictly allowed to decide ONLY the following:

- **Continuity**: Is the simulation physically contiguous? (Yes/No + Reason)
- **Capsule**: Is the gameplay shell anchored and passive? (Yes/No + Reason)
- **Plant**: Is the physical body-set structurally identical to the training target? (Yes/No + Reason)
- **Contamination**: Is there an authority conflict for simulation ownership? (Yes/No + Reason)

## 6. Runtime Test Targets

- **Ready -> BlendIn Entry Proof**: Verify successful gate completion before starting the blend.
- **BlendIn Rollout Stability**: Verify continuity and authority during the authority ramp.
- **Validate Hold Behavior**: Verify that the system maintains stability for the full hold duration.
- **Standing 3.0-Second Success**: The primary product benchmark for a passing run.
- **Must-Fail Scenarios**: Verification that the system correctly labels deliberate breaches (Capsule, Plant, Contamination, Simulation Loss).

## 7. Artifact Validation Rule

- Every terminal failure MUST be reconstructible from emitted artifact fields.
- The artifact `support_mode` MUST be derivable from the frame-level classifications and the 30 Hz reduction rule.
- If a terminal reason is emitted without the supporting forensic fields being populated, the test fails.
