# Character Capsule Contract (V0)

## Purpose

This document is the **sole authoritative owner** of the capsule and actor-level architectural contract. It defines the "Architectural Shift" of the `ACharacter` model during balance activation.

Disagreement with this contract triggers a terminal `activation_capsule_contract_violation`.

## The "Mesh-Isolated Character" Model

For `V0`, the bridge implements a **Suspended ACharacter** model where the Actor root is spatially and physically decoupled from the simulated humanoid. 

**Architectural Note**: This is NOT a standard `ACharacter` movement model. The system structurally prevents the `CharacterMovementComponent` from exerting authority to ensure the balance proof is purely physical.

### 1. Capsule and Actor Transform
- **Actor Follow Rule**: The `Actor` transform does **NOT** follow the simulated mesh. It remains frozen at the rebase origin.
- **Mesh Attachment Rule**: The `USkeletalMeshComponent` must be set to **Absolute Transform** mode (`SetUsingAbsoluteLocation(true)`, etc.). It is kinematically detached from the capsule.
- **Lock Rule**: Any delta greater than the **Capsule Lock Delta** between the capsule world-space position and the rebase origin is terminal.

### 2. CharacterMovement (Structural Deactivation)
The `CharacterMovementComponent` (CMC) must be structurally prevented from asserting authority. Passive non-interference is not sufficient for `V0`.
- **Deactivation**: The component must be explicitly deactivated (`Deactivate()`).
- **Tick Disabling**: The `CharacterMovementComponent` tick function must be explicitly **disabled** during the attempt (`SetComponentTickEnabled(false)`). 
  - *Hardening*: Setting `bCanEverTick = false` is a recommended setup-time hardening but is not the primary runtime contract surface.
- **Movement Mode**: The character must be in `MOVE_None` or a dedicated `MOVE_PhysAnimBalance` mode that overrides and skips all internal CMC logic (Floor Finding, Based Movement, etc.).
- **UpdatedComponent**: The `UpdatedComponent` must be set to `nullptr` to ensure no hidden `SafeMoveUpdatedComponent` calls occur during the activation window.

### 3. Collision and Overlaps
- **Collision Mode**: The capsule must be set to `NoCollision`.
- **Overlap Generation**: `SetGenerateOverlapEvents(false)` must be set for the capsule.
- **Justification**: Any capsule collision or overlap resolution can inject kinematic "tugging" into the actor root, which may stabilize or destabilize the mesh simulation through hidden engine paths.

## Non-Terminal Implementation Requirements (Functional)

These requirements are necessary for a correct bridge implementation but are **not** currently part of the physical stability proof. Disagreement with these items does **not** emit `activation_capsule_contract_violation`.

### 1. Camera and Attachment Ownership
- **Camera Anchor**: The bridge **must** re-target the player camera to a simulated bone (e.g., `head`) or the mesh component itself. A camera remaining on the frozen capsule is a functional bug.
- **Attachment Authority**: All gameplay-critical attachments (weapons, props) must be child-attached to **Skeletal Mesh Bones**, not the capsule root.

### 2. Replication and Authority (V0)
- **V0 Scope**: Local-only proof of concept. Networked prediction for the frozen capsule is not required.
- **Authority**: The physical simulation (mesh) is the sole authority for balance; the capsule remains as a "lifecycle anchor" only.

## Terminal Violation Criteria

The run must fail immediately on `activation_capsule_contract_violation` (Rank 2) if:
1. Capsule `CollisionEnabled` != `NoCollision`.
2. Capsule `bGenerateOverlapEvents` is `true`.
3. Capsule world transform deviates from the frozen rebase origin by more than the **Capsule Lock Delta**.
4. `CharacterMovementComponent` is active, ticking, or owning the mesh/actor transform.
5. `SkeletalMeshComponent` is NOT in Absolute Transform mode.
