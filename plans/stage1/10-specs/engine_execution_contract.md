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

The bridge computes a **one-time rebase frame** at the `Ready -> BlendIn` boundary:
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
- **Accepted Points**: Capture **All Manifold Points** where `ContactDistance <= 0.0`.
- **Calf Exclusion**: Manifold points on `calf_l/r` must be **EXCLUDED**.
- **Per-Body Reduction Rule**: 
  - Construct a **2D Convex Hull** of the world-space contact points projected onto the planar support surface.
  - The resulting **Support Patch** for the body is the set of vertices forming this hull.
  - **Minimum Geometry**: A valid patch requires at least **3 non-collinear vertices** to represent a planar area.
  - **Fallback**: If fewer than 3 points exist, the patch collapses to a line or point, triggering a high risk of `activation_proxy_outside_support_region`.

### 2. Final Frame Hull Construction
- **Qualifying Substep**: Sample from the final qualifying Chaos substep of the frame.
- **Hull Union**: The authoritative Frame Support Hull is the **Convex Hull** of the union of all vertices from all active **Support Patches**.
- **Failure Condition**: If the total vertex set across all bodies contains fewer than **3 points**, emit `activation_support_failure`.

### 3. Substep Persistence (Debounce Rule)
- State changes accepted only if they persist for **2 consecutive substeps**.

### 4. Churn and Uptime Counting
- Count each debounced `false<->true` transition as 1 churn event.
- Increment uptime only on side-support `true` substeps.

### 5. False-Failure Risk
Area-preserving reduction is mandatory. Collapsing a plantar contact to its "deepest point" is a contract violation because it eliminates the stability margin required for honest swaying.

### 6. 30 Hz Artifact Emission
Emit immediately if a terminal failure occurs between 30Hz samples.
