# Authority Matrix

## Purpose

This document is the **sole authoritative owner** of runtime ownership and subsystem authority. It defines who is allowed to write to which body at any given time.

**Failure truth and terminal arbitration are owned exclusively by [continuous_balance_truth_model.md](continuous_balance_truth_model.md).**

## Subsystem Authority Matrix

| Runtime mode | Authoritative owner | Forbidden writes | Capsule state | Mesh-wide authority |
| :--- | :--- | :--- | :--- | :--- |
| `BalanceActivation_Ready` | bridge | locomotion-drive; shell helper; kinematic mode writes | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Inactive |
| `BalanceActivation_BlendIn` | bridge | abrupt full-authority writes; shell helper; movement-component; plant-profile swap | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Inactive |
| `BalanceActivation_Validate` | bridge | shell helper; movement-component; topology edits; plant-profile swap | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Inactive |
| `BalanceActive_Standing` | bridge | legacy activation writes; any external authority reclamation | NoCollision; Overlaps Off; Actor Frozen | Mesh Absolute; CMC Inactive |

## Movement-Component Do-Not-Own List

During all active modes, the `CharacterMovementComponent` is forbidden from owning or influencing:
- Floor finding / Based movement
- Regular movement integration (Velocity/Acceleration)
- Post-physics correction / Deferred mesh movement
- Mesh smoothing / Client-side interpolation
- Root motion application
- Network correction / Depenetration

Any violation of this list is a terminal `activation_movement_reclaim`.

## Required Runtime Counters

The implementation must track and emit these metrics for each attempt:
- `topology_change_count`
- `authority_conflict_count`
- `shell_helper_used_count`
- `movement_reclaim_event_count`

## Material Contamination (Forbidden Authority)

Contamination is an `activation_authority_conflict` that falsifies the standing proof.

### Rule 1 — Physics Blend Contamination
- Terminal if any body in the truth set has a live `PhysicsBlendWeight` > `0.0`.

### Rule 2 — Kinematic Update Contamination
- Terminal if `bUpdateMeshWhenKinematic` is enabled **AND** the mesh is NOT in **Absolute Transform** mode.
- Terminal if the flag is **mutated mid-attempt** by an external system.

### Rule 3 — Mesh-Wide Side Effects
- Terminal if a non-activation system issues global blend, mobility, or reset writes.
- **Plugin-Mandated Exception**: Events issued by `PhysicsControl` for its internal setup are diagnostic-only.

### Rule 4 — Excluded-Body Contamination
- Terminal if an excluded body is physically connected to the truth set AND receives an authoritative write.
- Terminal if an excluded body bridges the truth set and the walkable world (Dual-Contact).

### Rule 5 — Calf-Contact Contamination
- Terminal if `calf_l` or `calf_r` contacts `WorldStatic` geometry. (Honest Standing must be plantar).

**Result**: Any breach triggers `activation_authority_conflict` as defined in the [Truth Model](continuous_balance_truth_model.md).
