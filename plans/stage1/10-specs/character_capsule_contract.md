# Character Capsule Contract (V0)

## Purpose

This document is the **sole authoritative owner** of the capsule and actor-level architectural contract. It defines the "Architectural Shift" of the `ACharacter` model during balance activation.

Disagreement with this contract triggers a terminal `activation_capsule_contract_violation`.

## The "Mesh-Isolated Character" Model

For `V0`, the bridge implements a **Suspended ACharacter** model where the Actor root is spatially and physically decoupled from the simulated humanoid.

### 1. Capsule and Actor Transform
- **UpdatedComponent**: The `CharacterMovementComponent` must be deactivated or its `UpdatedComponent` set to `nullptr`. It must NOT own the actor or mesh transform.
- **Actor Follow Rule**: The `Actor` transform does **NOT** follow the simulated mesh. It remains frozen at the rebase origin.
- **Mesh Attachment Rule**: The `USkeletalMeshComponent` must be set to **Absolute Transform** mode (`SetUsingAbsoluteLocation(true)`, etc.). It is kinematically detached from the capsule.
- **Lock Rule**: Any delta > `0.01 cm` between the capsule world-space position and the rebase origin is terminal.

### 2. Collision and Overlaps
- **Collision Mode**: The capsule must be set to `NoCollision`.
- **Overlap Generation**: `SetGenerateOverlapEvents(false)` must be set for the capsule.
- **Justification**: Any capsule collision or overlap resolution can inject kinematic "tugging" into the actor root, which may stabilize or destabilize the mesh simulation through hidden engine paths.

### 3. Camera and Attachment Ownership
- **Camera Anchor**: The bridge **must** re-target the player camera to a simulated bone (e.g., `head`) or the mesh component itself. A camera remaining on the frozen capsule is a functional bug.
- **Attachment Authority**: All gameplay-critical attachments (weapons, props) must be child-attached to **Skeletal Mesh Bones**, not the capsule root.

### 4. Replication and Authority (V0)
- **V0 Scope**: Local-only proof of concept. Networked prediction for the frozen capsule is not required.
- **Authority**: The physical simulation (mesh) is the sole authority for balance; the capsule remains as a "lifecycle anchor" only.

## Terminal Violation Criteria

The run must fail immediately on `activation_capsule_contract_violation` (Rank 2) if:
1. Capsule `CollisionEnabled` != `NoCollision`.
2. Capsule `bGenerateOverlapEvents` is `true`.
3. Capsule world transform deviates from the frozen rebase origin.
4. `CharacterMovementComponent` is active or owning the mesh/actor transform.
5. `SkeletalMeshComponent` is NOT in Absolute Transform mode.
