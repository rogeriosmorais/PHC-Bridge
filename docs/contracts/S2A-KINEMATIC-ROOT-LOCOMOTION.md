# Stage 2A: Kinematic Root Locomotion Contract

## Overview
This contract defines the technical requirements and success criteria for **Stage 2A Locomotion** within the PHC-Bridge architecture. Stage 2A is a pivot that establishes a stable locomotion baseline using the kinematic root authority established in Stage 1, intentionally deferring the complexities of simulated root handoffs (SimRoot).

## 1. Root Authority & Phase Transitions
- **Mandatory Authority**: The root MUST remain `Stage1_KinematicRoot`. The shell/capsule maintains absolute authority over the root trajectory.
- **Forbidden Transition**: `Phase3_SimRoot` (simulated pelvis settlement/handoff) MUST NOT be attempted. Any transition that attempts to relinquish kinematic shell control to Chaos physics simulation for the root is a contract violation.
- **Perturbation Policy**: Pelvis impulse perturbations are NOT a requirement for Stage 2A success. Since the root is kinematic, impulse-based balance recovery for the root is physically inapplicable.

## 2. Locomotion Mechanics
- **Trajectory Source**: Locomotion is driven by the capsule, shell, or root trajectory motion as defined by the Unreal Engine `CharacterMovementComponent`.
- **Policy Role**: The PHC policy MUST remain active during locomotion. It is responsible for driving pose targets and local limb control targets (PhysicsControl) to match the desired locomotion state (e.g., walking, turning).
- **Control Coupling**: Actuation (PhysicsControl) must remain synchronized with the kinematic root's velocity and orientation.

## 3. Definition of Success
A test or run is considered "Success" only if:
1. **Active Policy Output**: The policy is consistently emitting actions that drive the character's pose.
2. **Kinematic Integrity**: The character moves following the capsule/shell trajectory without jitter or divergence from the root shell.
3. **Locomotion Intent**: Telemetry shows a non-zero locomotion intent being processed by the gate.

## 4. Definition of Failure (Forbidden Modes)
A test or run is considered "Failure" if:
1. **CharacterMovement-Only**: The character moves, but the policy output is zero, frozen, or inactive. This indicates "faking" locomotion without the neural policy.
2. **SimRoot Regression**: The system attempts a Phase 3 SimRoot handoff, leading to shell velocity spikes or physical discontinuity.
3. **Shell Divergence**: The physical bodies diverge from the kinematic shell targets beyond calibrated thresholds (e.g., spine_01/spine_02 divergence).

## 5. Mandatory Telemetry & Observability
Telemetry artifacts for Stage 2A MUST include the following fields:
- `root_mode`: Must report `Stage1_KinematicRoot`.
- `locomotion_intent`: High-level intent (Walk/Turn/Stop).
- `policy_output_active`: Boolean flag indicating non-zero policy activity.
- `capsule_velocity`: Velocity of the driving capsule.
- `shell_divergence_peak`: Maximum observed divergence between shell and simulation.

## 6. Architectural Boundary
Stage 2A is strictly decoupled from **Stage 2B (SimRoot Research)**. No code path in Stage 2A should depend on or enable simulated root handoffs.
