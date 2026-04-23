# Physics Asset Contract (V0)

## Purpose

This document is the **sole authoritative owner** of the physical plant contract. It defines the required authored state of the Manny/Quinn humanoid before balance activation can be considered valid.

Disagreement with this contract triggers a terminal `activation_physics_asset_contract_violation`.

## Physics Asset Identity

- **Authoritative Baseline**: Every attempt must be recorded against a specific `physics_asset_baseline_id`.
- **Identity Check**: If the active `UPhysicsAsset` pointer or asset path does not match the audited baseline for the current skeleton variant, the run is non-admissible.

## Skeleton Audit Contract

The bridge recognizes a skeleton as "Audited Manny/Quinn-Derived" only if it passes this automated audit at activation start:

1.  **Bone Topology Check**: Every bone name and hierarchy relationship defined in the `Source -> Runtime` mapping must exist exactly in the runtime skeletal mesh.
2.  **Bone Axis Audit**: The local rotation axes of the runtime joints must match the source-reference axes within the **Skeleton Axis Alignment** threshold.
3.  **Segment Length Audit**: Parent-to-child joint distances must be within the **Segment Length Tolerance** of the audited baseline segments.
4.  **GUID/Hash Match**: The skeleton's bone hierarchy and names must hash to the declared "Audited Baseline ID" in the plant baseline.

**Failure Surface**: Any breach of this audit emits `activation_physics_asset_contract_violation`. Admissibility is binary; there is no "best-fit" allowance for un-audited skeleton variants in `V0`.

## Mass and Inertia Tolerances

To ensure policy performance is measured against the trained regime, the implementation must audit the live mass distribution:

- **Total Mass**: Must be within the **Mass Tolerance** of the baseline.
- **Truth-Set Mass**: The combined mass of the balance-critical chain must be within the **Mass Tolerance** of the baseline.
- **Principal Inertias**: The diagonalized inertia tensor for the `pelvis` and `thigh` bodies must be within the **Inertia Tolerance** of baseline values.
- **Plant Mutation Rule**: Any runtime modification of mass (e.g., `SetMassOverride`) during an attempt is a terminal violation.

## Constraint Profile Rules

- **Profile Identity**: Only the audited `V0` constraint profile (gains, limits, and damping) may be active on the critical chain.
- **Limit Integrity**: Any runtime "snapping" or "locking" of constraints outside the authored profile is forbidden.
- **Profile Swap Rule**: Swapping constraint profiles during an active attempt is terminal.

## Collision and Filtering Policy

### 1. Self-Collision
- **V0 Rule**: Self-collision between bodies in the truth set (Critical Chain + Support Set) must be **DISABLED** unless explicitly audited for specific interactions (e.g., thigh-to-thigh).
- **Justification**: Uncontrolled self-penetration impulses can inject non-policy forces into the balance truth.

### 2. Support-Body Filtering
- **V0 Rule**: `foot_*` and `ball_*` bodies must be filtered to collide **only** with `WorldStatic` geometry and the character's own bodies (if self-collision is required).
- **Inadmissible Collision**: Contact with `WorldDynamic`, other characters, or moving platforms is terminal contamination.

### 3. Upper-Body Collision
- **V0 Rule**: Collision for all bodies above `spine_01` (head, arms, upper chest) must be **DISABLED** during the attempt.
- **Justification**: This prevents accidental support from the upper body (e.g., "leaning" against a wall) from rescuing an invalid standing state.

## Support-Geometry Audit

- **Audit Requirement**: Every support-set body must use authored collision geometry that is **Bilateral and Non-Degenerate**.
- **Admissibility**: Capsule-tip or needle-contact geometry is not admissible for `V0`. The geometry must be broad enough to produce a stable plantar support footprint.

## Mutation Invalidation

Any of the following detected during the attempt triggers `activation_physics_asset_contract_violation`:
- Runtime asset swap (`SetPhysicsAsset`).
- Material-set mutation.
- Collision-disable table modification.
- Ad hoc mass or inertia edits.
