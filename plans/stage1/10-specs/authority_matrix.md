# Authority Matrix

## Purpose

This document is the **sole authoritative owner** of runtime ownership and subsystem authority. It defines who is allowed to write to which body at any given time and establishes the non-negotiable boundaries of material contamination.

**Failure truth and terminal arbitration are owned exclusively by [continuous_balance_truth_model.md](continuous_balance_truth_model.md).**

## Subsystem Authority Matrix

| Runtime mode | Authoritative owner | Forbidden writes | Capsule state | Mesh-wide authority |
| :--- | :--- | :--- | :--- | :--- |
| `BalanceActivation_Ready` | bridge | locomotion-drive; shell helper; kinematic mode writes | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Deactivated; Tick Disabled; UpdatedComp=null |
| `BalanceActivation_BlendIn` | bridge | abrupt full-authority writes; shell helper; movement-component; plant-profile swap | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Deactivated; Tick Disabled; UpdatedComp=null |
| `BalanceActivation_Validate` | bridge | shell helper; movement-component; topology edits; plant-profile swap | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Deactivated; Tick Disabled; UpdatedComp=null |
| `BalanceActive_Standing` | bridge | legacy activation writes; any external authority reclamation | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Deactivated; Tick Disabled; UpdatedComp=null |

## ACharacter Movement Model (V0)

**CRITICAL**: During all active balance modes, the system is NOT running a standard `ACharacter` movement model. The structural deactivation of the `CharacterMovementComponent` and the isolation of the skeletal mesh from the capsule root represent a complete departure from the default Unreal Engine character authority. Any implementation that allows standard CMC logic to execute is a contract violation.

## Movement-Component Do-Not-Own List

During all active modes, the `CharacterMovementComponent` is forbidden from owning or influencing:
- Floor finding / Based movement
- Regular movement integration (Velocity/Acceleration)
- Post-physics correction / Deferred mesh movement
- Mesh smoothing / Client-side interpolation
- Root motion application
- Network correction / Depenetration

Any violation of this list is a terminal `activation_movement_reclaim`.

## Contamination Policy (Material Assist)

Contamination is an `activation_authority_conflict` that falsifies the standing proof by injecting non-policy forces or constraints into the truth set.

### 1. Mesh-Wide Assist
- **Rule**: Any global mesh write (Alpha-blend, Mobility reset, bUpdateMeshWhenKinematic) that originates outside the bridge is terminal.
- **Materiality**: These writes materially assist the proof by damping simulation transients or forcing bone placement independent of the policy.

### 2. Non-Critical-Body Assist
- **Rule**: Any authoritative write (Kinematic position, Physics Control target) to a body in the **Excluded Set** that is physically connected to the **Critical Chain** is terminal.
- **Materiality**: Energy injected into connected bodies (e.g., arms, head) can act as a counterbalance or inertial damper, falsifying the policy's balance performance.

### 3. Excluded-Body World Bracing (Calf Promote)
- **Rule**: Terminal if any body in the **Excluded Set** or **Monitor Set** (calves) contacts `WorldStatic` geometry.
- **Emitted Reason**: `activation_authority_conflict`.
- **Materiality**: This "Third-Point Support" (e.g., leaning against a wall with an arm, or floor contact with a calf) provides external bracing that rescues an otherwise failing balance state.

### 4. Global Blend/Kinematic Assist
- **Rule**: Terminal if `PhysicsBlendWeight` > 0.0 for any truth-set body, or if `bUpdateMeshWhenKinematic` is enabled without **Absolute Transform** isolation.
- **Materiality**: This allows the engine's animation-matching or root-relative placement logic to "carry" the simulated bodies, masking true physical instability.

## Implementation Note: PhysicsControl Side Effects

**Non-Admissibility Warning**: If the current `UPhysicsControlComponent` implementation or its parent runtime path sets mesh-wide states (e.g., global blend weights or kinematic updates) that auto-trigger Rule 1 or Rule 4, the `V0` attempt is **non-admissible**. The runtime path must be modified to ensure isolation before the bridge can consider the balance proof valid.

## Required Runtime Counters

The implementation must track and emit these metrics for each attempt:
- `topology_change_count`
- `authority_conflict_count`
- `shell_helper_used_count`
- `movement_reclaim_event_count`
