# Engine Execution Contract

## Purpose

This document defines the authoritative Unreal Engine execution order and data-freshness contract for the continuous-balance bridge.

It is the "hard place" that ensures all subsystems (Animation, Physics, Movement, and Bridge) agree on **WHEN** truth is measured and **WHEN** authority is asserted.

**Failure truth definitions and arbitration logic are owned exclusively by [continuous_balance_truth_model.md](continuous_balance_truth_model.md).**

## Per-Frame Execution Order

Every frame must follow this exact sequence. Any deviation that reorders these steps is a contract violation.

1.  **Animation Evaluation (`USkeletalMeshComponent`)**
    - Authoritative source pose is evaluated.
    - Bone-space transforms for the current frame are materialized.
2.  **Control-Source Capture (`UPhysicsControlComponent`)**
    - The "cached pose" or "target pose" in Physics Control is refreshed from the current-frame animation.
    - **Staleness Rule**: Any read of the source pose before this step is "stale" and must not be used for current-frame control targets.
3.  **Bridge Update (`UPhysAnimComponent` Tick)**
    - **Tick Group**: Must run in `TG_PrePhysics`.
    - Performs observation packing, policy inference, and standing-reference rebasing.
    - Materializes current-frame control targets and body-modifier writes.
4.  **Authority Publication (`UPhysicsControlComponent` / Body Modifiers)**
    - Final publication of targets, gains, and movement-type writes to the physics bodies.
    - **Pre-Physics Rule**: All writes for the current frame must be complete before the first Chaos substep of the frame.

## Reference Rebasing Contract

To isolate the standing proof from pre-activation locomotion drift, the bridge must compute a **one-time rebase frame** at the `BalanceActivation_Ready -> BalanceActivation_BlendIn` boundary:

- **Capture Origin**: Live `pelvis` world-space position at the moment of blend entry.
- **Capture Up**: Authoritative gravity-up axis.
- **Capture Yaw**: Live `pelvis` forward vector projected onto the plane normal to gravity-up.
- **Persistence**: This frame is **frozen** for the entire attempt. No per-body fitting or repeated rebasing is allowed.
- **Target Projection**: Authoritative reference targets are rebased into world placement using this frozen frame and the resolved skeleton hierarchy.

## Alpha-Blend Rollout (V0)

During `BalanceActivation_BlendIn`, the bridge asserts authority via a gradual rollout of a fixed **Physics Control Primitive Bundle**:

- **Rollout Coordinate**: `ControlAuthorityAlpha` (Global scalar, `0.0 -> 1.0`).
- **Primitive Bundle**: Includes Target Orientation, Target Position, Target Velocity (Angular/Linear), Spring Strength, Damping, Max Torque, and Max Force.
- **Blend Rule**: All primitives in the bundle must be scaled or interpolated by the same global alpha.
- **Fixed Parameters**: Control-point offsets, target-space transforms, and parent-dominance settings must remain **fixed** for the whole attempt; they are not alpha-ramped.
- **Discontinuity Rule**: If the delta between the live pose and the initial rebased target exceeds the **Target Discontinuity** threshold, emit `activation_target_discontinuity`.

5.  **Forbidden Latent Path: Movement Integration (Pre-Physics)**
    - Standard `CharacterMovementComponent` tick path.
    - **V0 Contract**: This path must **NOT** execute. It is structurally disabled via `MOVE_None`, `Deactivate()`, and `Tick Disable` (See [character_capsule_contract.md](character_capsule_contract.md)).
6.  **Chaos Simulation (The "Step")**
    - Physics solver runs `N` substeps using the published pre-physics state.
7.  **Substep Truth Accumulation**
    - Occurs *inside* the physics step at substep resolution.
    - Accumulates contact persistence, side-support state, and continuity events.
    - **Resolution Rule**: Truth is accumulated at Chaos substep rate, not at frame rate.
8.  **Truth-Sensitive Sampling Point (Post-Simulation)**
    - The authoritative truth snapshot is taken **immediately after the final Chaos substep**.
    - This is the "Live/Raw" state for the frame.
    - **Ordering Rule**: Must occur before any deferred mesh movement or post-physics movement correction.
9.  **Forbidden Latent Path: Movement Reclaim (Post-Physics)**
    - Post-physics correction, based-movement, and deferred mesh movement paths.
    - **V0 Contract**: This path must **NOT** execute. Any write that displaces the mesh or truth-set bodies here is an `activation_movement_reclaim` violation.

## Data Freshness Contract

| Signal Type | Definition | Authoritative Source |
| :--- | :--- | :--- |
| **Source Pose** | The current-frame animation target | `USkeletalMeshComponent` post-eval |
| **Control Target** | The published desired physical state | Physics Control cached targets |
| **Live/Raw State** | The actual physical state after simulation | Post-Chaos raw body transforms |
| **Stale State** | Data from any previous frame or pre-Chaos cached values | Any capture before Step 8 |

## Cadence and Resolution

- **Reporting Cadence**: `30 Hz` (Downsampled artifact emission).
- **Truth Cadence**: Chaos Substep resolution (e.g., `~120 Hz`).
- **Control Cadence**: Frame rate (Tick-based target publication).

## Support-Truth Reduction Algorithm (V0)

The implementation must use this deterministic algorithm to reduce the high-rate Chaos substep stream into frame-level truth and 30 Hz artifacts.

### 1. Per-Substep Manifold Capture
For every body in the support set (`foot_*`, `ball_*`) on every Chaos substep:
- **Accepted Points**: Capture **All Manifold Points** where `ContactDistance <= 0.0` (active penetration or touch).
- **Calf Exclusion**: Manifold points on `calf_l` or `calf_r` must be **EXCLUDED** from hull construction. They represent terminal contamination, not support.
- **Per-Body Reduction**: For each body, reduce the manifold to a representative **Support Patch** (a set of world-space points) that preserves the planar area of the contact. A single "deepest point" reduction is forbidden as it collapses the plantar footprint into a point.
- **Justification**: Foot-based balance requires a stable area; collapsing this area into a single point causes the Support Proxy to auto-fail during minor angular sway.

### 2. Substep Persistence (Debounce Rule)
- A state change (Contact-Active `true<->false`) for any support body is only accepted if the state persists for **2 consecutive substeps**.
- All timing-sensitive metrics (uptime, gap max) must be calculated using this debounced state.

### 3. Churn and Uptime Counting
- Count each debounced `false->true` or `true->false` transition on either side as **1 churn event**.
- Increment the support uptime timer only on substeps where at least one side-support state is `true` after debounce.

### 4. Final Frame Hull Construction
To produce the single "Frame Truth" reported at the **Truth-Sensitive Sampling Point**:
- **Qualifying Substep**: Select the **final qualifying Chaos substep** of the frame (the last substep before post-simulation sampling).
- **Hull Union**: The authoritative Support Hull for the frame is the **Convex Hull** formed by the union of all **Support Patches** (from Step 1) for all active support bodies in that final qualifying substep.
- **Stability Check**: The support proxy check must be performed against this frame-level convex hull.

### 5. False-Failure Risk
If the implementation uses a "Conservative reduction" (e.g., using only the 4 corners of a box or a single average point), the Support Hull will be smaller than the physical footprint. 
- **Expected Behavior**: This will trigger premature `activation_proxy_outside_support_region` failures during honest standing trials.
- **V0 Requirement**: The reduction must be "Lossless enough" to preserve the authored plantar surface area.

### 6. 30 Hz Artifact Emission
- The `30 Hz` artifact is a downsampled snapshot of the Frame-Level Truth.
- If a terminal failure occurs between 30 Hz samples, the artifact must be emitted immediately to capture the exact terminal state, regardless of the 30 Hz cadence.
