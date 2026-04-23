# Physics Asset Contract (V0)

## Purpose

This document is the **sole authoritative owner** of the physical plant contract. It defines the required authored state of the Manny/Quinn humanoid before balance activation can be considered valid.

Disagreement with this contract triggers a terminal `activation_physics_asset_contract_violation`.

## Validation Schedule

1.  **Pre-Attempt Audit (Entry Gate)**:
    - Runs **BEFORE** entering `BalanceActivation_Ready`.
    - Passing this audit is a hard prerequisite for balance state machine activation.
    - Performs full skeleton topology, asset identity, and hierarchy hashing.
    - If any audit field fails, the activation is denied.
2.  **Per-Frame Audit (Continuous Monitoring)**:
    - Runs every Chaos substep for mass, inertia, and constraint profile integrity.
    - Monitors for ad hoc mutation calls.
3.  **Mutation-Triggered Revalidation**:
    - Any external call to `SetPhysicsAsset`, `SetPhysicsMaterialOverride`, or `SetConstraintProfile` during the attempt triggers an immediate Rank 1 terminal violation.

## Physics Asset Identity

- **Authoritative Baseline**: Every attempt must be recorded against a specific `physics_asset_baseline_id`.
- **Identity Check**: If the active `UPhysicsAsset` pointer or asset path does not match the audited baseline, the run is non-admissible.

## Skeleton Audit Contract

The bridge recognizes a skeleton as "Audited Manny/Quinn-Derived" only if it passes this automated audit:

| Audit Field | Threshold | Failure Condition |
| :--- | :--- | :--- |
| **Bone Topology** | Exact Match | Any missing or renamed bone in Critical/Support sets |
| **Bone Axis Alignment** | `+/- 0.5 deg` | Local joint axes deviate from source reference |
| **Segment Length** | `+/- 5.0%` | Parent-to-child distance deviates from baseline |
| **Hierarchy Hash** | Exact Match | Bone names and parentage don't hash to Baseline ID |

## Mass and Inertia Tolerances

The implementation must audit the live mass distribution against the baseline:

- **Total Mass**: `+/- 2.5%` tolerance.
- **Truth-Set Mass**: `+/- 2.5%` tolerance for the balance-critical chain.
- **Principal Inertias**: `+/- 5.0%` tolerance for `pelvis` and `thigh` bodies.
- **Plant Mutation**: Any mid-attempt call to `SetMassOverride`, `SetCenterOfMass`, or `SetInertiaTensor` is terminal.

## Constraint Profile Rules

- **Profile Identity**: Only the audited `V0` constraint profile (gains, limits, and damping) may be active.
- **Limit Integrity**: Any runtime "snapping" or "locking" of constraints outside the authored profile is forbidden.
- **Profile Swap Rule**: Swapping constraint profiles (`SetConstraintProfile`) during an active attempt is terminal.

## Collision and Filtering Policy

### 1. Self-Collision
- **V0 Rule**: Self-collision between bodies in the truth set (Critical Chain + Support Set) must be **DISABLED**.

### 2. Support-Body Filtering
- **V0 Rule**: `foot_*` and `ball_*` bodies must be filtered to collide **only** with `WorldStatic` geometry.

### 3. Upper-Body Collision
- **V0 Rule**: Collision for all bodies above `spine_01` (head, arms, upper chest) must be **DISABLED**.

## Support-Geometry Audit

The runtime must audit the `FBodySetup` of support bodies:
- **Geometry Type**: Must be `Convex` or `Sphyl`. `Box` or `Sphere` is permitted only if authored as a flat plantar surface.
- **Surface Area**: The planar bounding box of the support geometry must exceed `50.0 cm^2` per foot.
- **Bilateral Integrity**: The `L` and `R` support geometries must have symmetric volume within `10.0%`.

## Baseline JSON Definition (The "Fingerprint")

The `V0_Plant_Baseline.json` must contain exactly:
- `asset_path`: Full Unreal asset path.
- `baseline_id`: Unique GUID for this plant version.
- `skeleton_hash`: Hash of bone names and parentage hierarchy.
- `mass_table`: Array of `{ name, mass, local_com }`.
- `inertia_table`: Array of `{ name, principal_inertias[] }`.
- `constraint_profile_id`: Identifier for the authored gains/limits.
- `material_set_id`: Identifier for the assigned physics materials.
- `collision_disable_table_id`: Identifier for the self-collision filter set.
- `support_geometry_audit_id`: Identifier for the audited plantar surface data.
