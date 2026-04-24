# Authority Matrix

## Purpose

This document is the **sole authoritative owner** of runtime ownership and subsystem authority. It defines who is allowed to write to which body at any given time and establishes the non-negotiable boundaries of material contamination.

**Failure truth is owned exclusively by [continuous_balance_truth_model.md](continuous_balance_truth_model.md).**

## Subsystem Authority Matrix

| Runtime mode | Authoritative owner | Forbidden writes | Capsule state | Mesh-wide authority |
| :--- | :--- | :--- | :--- | :--- |
| `BalanceActivation_Ready` | bridge | locomotion-drive; shell helper | NoCollision;<br>Overlaps off;<br>Actor frozen at rebase origin | Mesh absolute transform;<br>CMC deactivated;<br>Tick function disabled;<br>UpdatedComponent=nullptr;<br>MOVE_None or MOVE_PhysAnimBalance |
| `BalanceActivation_BlendIn` | bridge | movement-component; plant-profile swap | NoCollision;<br>Overlaps off;<br>Actor frozen at rebase origin | Mesh absolute transform;<br>CMC deactivated;<br>Tick function disabled;<br>UpdatedComponent=nullptr;<br>MOVE_None or MOVE_PhysAnimBalance |
| `BalanceActivation_Validate` | bridge | shell helper; movement-component | NoCollision;<br>Overlaps off;<br>Actor frozen at rebase origin | Mesh absolute transform;<br>CMC deactivated;<br>Tick function disabled;<br>UpdatedComponent=nullptr;<br>MOVE_None or MOVE_PhysAnimBalance |
| `BalanceActive_Standing` | bridge | external authority reclamation | NoCollision;<br>Overlaps off;<br>Actor frozen at rebase origin | Mesh absolute transform;<br>CMC deactivated;<br>Tick function disabled;<br>UpdatedComponent=nullptr;<br>MOVE_None or MOVE_PhysAnimBalance |
| `BalanceActive_Recovery` | bridge (recovery) | locomotion-drive; shell helper | NoCollision;<br>Overlaps off;<br>Actor frozen at rebase origin | Mesh absolute transform;<br>CMC deactivated;<br>Tick function disabled;<br>UpdatedComponent=nullptr;<br>MOVE_None or MOVE_PhysAnimBalance |
| `SafeDenied` | locomotion / engine | active balance policy writes | Collision-Active (Reverting) | Root-Relative (Reverting); CMC Active |
| `Failed` | system (ragdoll) | all active bridge writes | Collision-Active (Reverting) | Physics-Simulated (Ragdoll); CMC Disabled |

**Operational Pre-condition**: Entering `BalanceActivation_Ready` requires a **passed** Static Structural Audit (Entry Gate) as defined in [physics_asset_contract.md](physics_asset_contract.md).

## ACharacter Movement Model (V0)

**CRITICAL**: V0 is NOT running a standard `ACharacter` movement model. Structural deactivation of the `CharacterMovementComponent` (CMC) and isolation of the skeletal mesh from the capsule root are mandatory.

**Structural Deactivation Rule**: Passive non-interference is insufficient for `V0`. The implementation must structurally prevent CMC from asserting authority via:
- `Deactivate()` + Tick function disabled
- `MOVE_None` or dedicated `MOVE_PhysAnimBalance` (skips internal CMC logic)
- `UpdatedComponent = nullptr`

## Movement-Component Do-Not-Own List

During activation, CMC is forbidden from owning:
- Floor finding / Based movement
- Regular movement integration
- Post-physics correction / Deferred mesh movement
- Mesh smoothing / Root motion / Network correction

Violation emits `activation_movement_reclaim` as adjudicated by the [Truth Model](continuous_balance_truth_model.md).

## Contamination Policy (Material Assist)

Contamination is an `activation_authority_conflict` that falsifies the standing proof.

### 1. Mesh-Wide Assist
- Global mesh writes (Alpha-blend, Mobility reset, Kinematic updates) originating outside the bridge are terminal.

### 2. Non-Critical-Body Assist
- Authoritative writes to an **Excluded Body** physically connected to the **Critical Chain** are terminal.

### 3. Excluded-Body World Bracing (Calf Promote)
- World contact on any **Excluded** or **Monitor** (calf) body is terminal.
- **Emitted Reason**: `activation_authority_conflict`.

### 4. Global Blend/Kinematic Assist
- Terminal if `PhysicsBlendWeight` > 0.0 for any truth-set body, or if `bUpdateMeshWhenKinematic` is enabled without absolute isolation.

## Implementation Note: PhysicsControl Side Effects

If `UPhysicsControlComponent` sets mesh-wide states that auto-trigger Rule 1 or 4, the V0 attempt is **non-admissible**. The runtime path must be modified to ensure isolation.

## Required Runtime Counters

Implementation must track: `topology_change_count`, `authority_conflict_count`, `shell_helper_used_count`, `movement_reclaim_count`.
