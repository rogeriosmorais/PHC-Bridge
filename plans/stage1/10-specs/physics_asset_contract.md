# Physics Asset Contract (V0)

## Purpose

This document is the **sole authoritative owner** of the physical plant contract. It defines the required authored state of the Manny/Quinn humanoid before balance activation can be considered valid.

Disagreement with this contract triggers a terminal `activation_physics_asset_contract_violation`.

## Validation Cadence

1.  **Pre-Activation Audit**: The full skeleton and asset identity audit must pass before `BalanceActivation_Ready` can transition to `BalanceActivation_BlendIn`.
2.  **Continuous Monitoring**: Mass, inertia, and asset-path integrity are monitored every frame during the attempt. Any mid-run mutation is a Rank 1 terminal failure.

## Physics Asset Identity

- **Authoritative Baseline**: Every attempt must be recorded against a specific `physics_asset_baseline_id`.
- **Identity Check**: If the active `UPhysicsAsset` pointer or asset path does not match the audited baseline for the current skeleton variant, the run is non-admissible.

## Skeleton Audit Contract

The bridge recognizes a skeleton as "Audited Manny/Quinn-Derived" only if it passes this automated audit at activation start:

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
- **Justification**: Uncontrolled self-penetration impulses can inject non-policy forces into the balance truth.

### 2. Support-Body Filtering
- **V0 Rule**: `foot_*` and `ball_*` bodies must be filtered to collide **only** with `WorldStatic` geometry.
- **Inadmissible Collision**: Contact with `WorldDynamic`, other characters, or moving platforms is terminal contamination.

### 3. Upper-Body Collision
- **V0 Rule**: Collision for all bodies above `spine_01` (head, arms, upper chest) must be **DISABLED**.

## Support-Geometry Audit

The runtime must audit the `FBodySetup` of support bodies to ensure physical stability:
- **Geometry Type**: Must be `Convex` or `Sphyl`. `Box` or `Sphere` is permitted only if authored as a flat plantar surface.
- **Surface Area**: The planar bounding box of the support geometry must exceed `50.0 cm^2` per foot.
- **Bilateral Integrity**: The `L` and `R` support geometries must have symmetric volume within `10.0%`.

## Baseline JSON Fields (The "Fingerprint")

The `V0_Plant_Baseline.json` must contain:
- `baseline_id`: GUID
- `target_skeleton_hash`: String
- `body_baselines`: Array of `{ name, mass, inertia_tensor, local_com }`
- `joint_baselines`: Array of `{ name, parent_offset, local_axes }`
- `authored_constraint_profile`: `{ linear_gains, angular_gains, limits }`
