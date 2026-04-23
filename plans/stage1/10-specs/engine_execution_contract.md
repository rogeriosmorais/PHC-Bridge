# Engine Execution Contract

## Purpose

This document defines the authoritative Unreal Engine execution order and data-freshness contract for the continuous-balance bridge.

**Failure truth is owned exclusively by [continuous_balance_truth_model.md](continuous_balance_truth_model.md).**

## Per-Frame Execution Order

1.  **Animation Evaluation (`USkeletalMeshComponent`)**
    - Source pose bones are materialized.
2.  **Control-Source Capture (`UPhysicsControlComponent`)**
    - Target pose refreshed from current-frame animation.
3.  **Bridge Update (`UPhysAnimComponent` Tick)**
    - **Tick Group**: `TG_PrePhysics`.
    - Performs observation packing, policy inference, and standing-reference rebasing.
4.  **Authority Publication (`UPhysicsControlComponent`)**
    - Final publication of targets/gains to physics bodies.
5.  **Forbidden Latent Path: Movement Integration (Pre-Physics)**
    - CMC tick path must **NOT** execute in V0.
6.  **Chaos Simulation (The "Step")**
    - Solver runs `N` substeps.
7.  **Substep Truth Accumulation**
    - Occurs *inside* the physics step at substep resolution (~120Hz).
8.  **Truth-Sensitive Sampling Point (Post-Simulation)**
    - Authoritative truth snapshot taken **immediately after the final Chaos substep**.
9.  **Forbidden Latent Path: Movement Reclaim (Post-Physics)**
    - CMC correction paths must **NOT** execute in V0.

## Reference Rebasing Contract

The bridge performs these **Reference Readiness** tasks at the `Ready -> BlendIn` boundary:
- **Existence Check**: Verify that a valid authored `standing_reference_id` exists and is loaded.
- **Rebase Frame Computation**: Compute a one-time world-space rebase frame (frozen for the attempt).
- **Target Materialization**: Project the target reference pose into the rebase frame.
- **Quiet-State Proof**: Confirm the live pose is sufficiently quiet (velocity/stability) to begin the blend.

**Rebase Frame Details**:
- **Origin**: Live `pelvis` world-space position.
- **Up**: Gravity-up.
- **Yaw**: Live `pelvis` forward projected onto horizontal plane.
- **Persistence**: Frozen for the entire attempt.

## Alpha-Blend Rollout (V0)

Authority rollout during `BalanceActivation_BlendIn`:
- **Rollout Coordinate**: `ControlAuthorityAlpha` (`0.0 -> 1.0`).
- **Blend Rule**: All primitives in the bundle scaled by the same global alpha.
- **Discontinuity Check**: Audited against the [Truth Model](continuous_balance_truth_model.md).

## Data Freshness Contract

| Signal Type | Definition | Authoritative Source |
| :--- | :--- | :--- |
| **Source Pose** | Current-frame animation target | `USkeletalMeshComponent` post-eval |
| **Control Target** | Published desired state | Physics Control cached targets |
| **Live/Raw State** | Actual state after simulation | Post-Chaos raw body transforms |
| **Stale State** | Data from previous frames | Any capture before Step 8 |

## Cadence and Resolution

- **Reporting Cadence**: `30 Hz`.
- **Truth Cadence**: Chaos Substep resolution (~120 Hz).
- **Control Cadence**: Frame rate.

## Support-Truth Reduction Algorithm (V0)

The implementation must use this deterministic algorithm to reduce the high-rate Chaos substep stream into frame-level truth.

### 1. Per-Substep Manifold Capture
For every body in the support set (`foot_*`, `ball_*`) on every Chaos substep:
- **Accepted Points**: Capture manifold points ONLY if they satisfy all of the following:
  - **Proximity**: `ContactDistance <= 0.0`.
  - **Support Side**: Contacting body is in the **Support Set** (`foot_*`, `ball_*`).
  - **Admissible Opposing Surface**: Opposing body is **WorldStatic** (Environment).
  - **No Self-Contact**: Opposing body is not part of the character's own skeletal mesh.
  - **No Dynamic Contact**: Opposing body is not a `WorldDynamic`, `PhysicsBody`, or another `SkeletalMesh` (No other characters).
- **Calf Exclusion**: Manifold points on `calf_l/r` must be **EXCLUDED** from patch construction; they are monitored for contamination only.

### 2. Per-Body Support Patch Reduction
For each body with at least one accepted point, build an authoritative **Support Patch**:
- **Projection**: Project all points onto the plane normal to the gravity-up axis.
- **Data Structure**: A `TArray<FVector2D>` representing the vertices of the reduced patch.
- **Reduction Operation**: Compute the **2D Convex Hull** of the projected points.
- **Degeneracy Rules**:
  - **Single Point**: Patch is valid but area is 0.
  - **Collinear Points**: Patch is valid but area is 0.
  - **Area Proof**: A patch provides a stability area only if it contains >= 3 non-collinear vertices.

### 3. Final Frame Hull Construction
- **Qualifying Substep**: Select the **final qualifying Chaos substep** of the frame.
- **Hull Union**: The authoritative Frame Support Hull is the **2D Convex Hull** formed by the union of all vertices from all active **Support Patches**.
- **Data Structure**: A `TArray<FVector2D>` defining the world-space planar support region.
- **Degeneracy Interaction**: Zero-area patches (single-point or collinear) are **valid** for contributing vertices to the union, but a hull formed solely from such patches will fail the **Area-Threshold Test**.

### 4. Support Adjudication Sequence
The implementation must perform truth adjudication in this exact order:
1.  **Manifold Capture**: Collect all valid substep contacts.
2.  **Patch Reduction**: Construct per-body 2D convex hulls.
3.  **Frame Hull Union**: Merge all active patches into the frame-level 2D hull.
4.  **Area-Threshold Test**: 
    - **Condition**: If `active_support_side_count > 0` AND `FrameHullArea < Support Area (Min)`.
    - **Result**: Emit `activation_support_failure` as adjudicated by the [Truth Model](continuous_balance_truth_model.md).
    - **Reasoning**: If at least one foot is down, it MUST provide sufficient area. If both are up, the failure is instead adjudicated by the **Support Gap** in Step 6.
5.  **Proxy Adjudication**: 
    - **Condition**: Only if `active_support_side_count > 0`.
    - **Test**: If `Proxy` is outside the hull, increment drift timer. 
    - **Result**: If `timer > COM Proxy Drift`, emit `activation_proxy_outside_support_region`.
    - **No-Hull Behavior**: If `active_support_side_count == 0`, skip this test entirely. The artifact `proxy_inside_hull` must be emitted as `null` (not evaluated).
6.  **Classification**: Assign `support_mode` using the following priority:
    - **TwoFootStable**: Both sides `true` (debounced).
    - **SingleFootSurvival**: Exactly one side `true` (debounced).
    - **TransientRecovery**: Both sides `false` (debounced) AND `gap_timer <= Support Gap (Max)`.
    - **Airborne**: Both sides `false` (debounced) AND `gap_timer > Support Gap (Max)`. (Triggers `activation_support_failure`).

### 5. Proxy-vs-Hull Test Logic
- **Projection**: Project the **Support Proxy** (defined in [continuous_balance_truth_model.md](continuous_balance_truth_model.md)) onto the same planar support surface.
- **Test**: Perform a **2D Point-in-Polygon** test of the projected proxy against the Frame Support Hull.
- **Result**:
  - `Inside`: Proxy is within the hull or on the edge.
  - `Outside`: Proxy world-space distance to the hull boundary > 0.

### 6. Substep Persistence (Debounce Rule)
- State changes accepted only if they persist for **2 consecutive substeps**.

### 7. Churn and Frequency Measurement
- **Measurement**: Track debounced `false<->true` transitions from **both** support sides combined at substep resolution.
- **Window**: Use a rolling **1.0-second** window.
- **Calculation**: `support_churn_hz = transitions_in_last_1_0_sec / 1.0`.
- **Uptime**: Increment uptime only on side-support `true` substeps.

### 8. False-Failure Risk
Area-preserving reduction is mandatory. Collapsing a plantar contact to its "deepest point" is a contract violation because it eliminates the stability margin required for honest swaying.

### 9. 30 Hz Artifact Emission
Emit immediately if a terminal failure occurs between 30Hz samples.
