# Physics Asset Contract (V0)

## Purpose

This document is the **sole authoritative owner** of the physical plant contract. It defines the required authored state of the Manny/Quinn humanoid before balance activation can be considered valid.

Disagreement with this contract triggers a terminal `activation_physics_asset_contract_violation`.

## Validation Schedule

1.  **Static Structural Audit (Entry Gate)**:
    - Runs **BEFORE** entering `BalanceActivation_Ready`.
    - **Scope**: Skeleton topology, asset identity, hierarchy hashing, and bone length integrity.
    - **Enforcement**: Passing this audit is a hard prerequisite for entry.

2.  **Mutation-Triggered Audit (Integrity Monitoring)**:
    - Runs immediately if any mutation event is detected (e.g., API calls to `SetMass`, `SetInertia`, `SetPhysicsAsset`, `SetConstraintProfile`).
    - **Scope**: Re-validates the Mass Table, Inertia Table, and Constraint Profiles against the audited baseline.
    - **Result**: Immediate `activation_physics_asset_contract_violation` (Rank 1) if the mutation creates drift.

3.  **Live Dynamic Monitoring**:
    - Runs every Chaos substep (~120Hz).
    - **Scope**: Material property integrity and constraint limit state.
    - **Reasoning**: Monitors for properties that could materially affect solver stability but are not captured by the structural or mutation audits.

## Physical Plant Requirements

### 1. Mass Distribution Audit
- **Total Mass**: Must match baseline within **Total Mass Tolerance**.
- **Critical Chain Mass**: Aggregate mass of `pelvis` through `thigh_*` must match baseline.
- **Support Set Mass**: Aggregate mass of `foot_*` and `ball_*` must match baseline.
- **Justification**: Support-body mass drift materially alters the dynamics of the plantar hull.

### 2. Inertia Tensor Audit
- **Critical Bodies**: `pelvis` and `thigh_*` principal moments must match baseline.
- **Support Bodies**: `foot_*` and `ball_*` moments must match baseline within **Inertia Tolerance**.
- **Justification**: Support-body inertia affects the fidelity of the "Honest Balance" proof during rapid support transitions.

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
