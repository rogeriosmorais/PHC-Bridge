# Authority Matrix

## Purpose

This document is the **sole authoritative owner** of runtime ownership and subsystem authority. It defines who is allowed to write to which body at any given time.

**Failure truth and terminal arbitration are owned exclusively by [continuous_balance_truth_model.md](continuous_balance_truth_model.md).**

## Subsystem Authority Matrix

| Runtime mode | Authoritative owner | Forbidden writes | Capsule state | Mesh-wide authority |
| :--- | :--- | :--- | :--- | :--- |
| `BalanceActivation_Ready` | bridge | locomotion-drive; shell helper; kinematic mode writes | neutralized; frozen at rebase origin | mesh absolute; CharacterMovement off |
| `BalanceActivation_BlendIn` | bridge | abrupt full-authority writes; shell helper; movement-component; plant-profile swap | neutralized; frozen at rebase origin | mesh absolute; CharacterMovement off |
| `BalanceActivation_Validate` | bridge | shell helper; movement-component; topology edits; plant-profile swap | neutralized; frozen at rebase origin | mesh absolute; CharacterMovement off |
| `BalanceActive_Standing` | bridge | legacy activation writes; any external authority reclamation | neutralized; frozen at rebase origin | mesh absolute; CharacterMovement off |

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

## Authority Surfaces

1.  **Truth-Set Bodies**: Owned exclusively by the bridge and physics solver.
2.  **Excluded Bodies**: Managed by the bridge (Rule 4 contamination gate applies).
3.  **Mesh Settings**: `PhysicsBlendWeight` and `bUpdateMeshWhenKinematic` must follow the rules in the [Truth Model](continuous_balance_truth_model.md).
