# Engine Execution Contract

## Purpose

This document defines the authoritative Unreal Engine execution order and data-freshness contract for the continuous-balance bridge.

It is the "hard place" that ensures all subsystems (Animation, Physics, Movement, and Bridge) agree on when truth is measured and when authority is asserted.

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
5.  **Movement Integration (Pre-Physics CMC)**
    - `CharacterMovementComponent` regular tick runs.
    - **Inertness Rule**: Must not write to the actor transform, capsule, or truth-set bodies.
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
9.  **Movement Reclaim/Correction (Post-Physics CMC)**
    - `CharacterMovementComponent` post-physics correction, based-movement, and deferred mesh movement paths run.
    - **Inertness Rule**: Any write that displaces the mesh or truth-set bodies here is an `activation_movement_reclaim` violation.

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

### 1. Per-Substep Point Selection
For every body in the support set (`foot_*`, `ball_*`) on every Chaos substep:
- If multiple contact manifold points exist for a single body, the implementation must select the **Greatest-Penetration Manifold Point** as the representative contact for that body in that substep.
- **Justification**: This is the most conservative choice for stability; if the deepest point is outside the hull, the body is materially unstable.

### 2. Substep Persistence (Debounce Rule)
- A state change (Contact-Active `true<->false`) is only accepted if it persists for **2 consecutive substeps**.
- This filters single-substep jitter ("popping") from being recorded as churn or support loss.
- All timing-sensitive metrics (uptime, gap max) must be calculated using this debounced state.

### 3. Churn and Uptime Counting
- Count each debounced `false->true` or `true->false` transition on either side as **1 churn event**.
- Increment the support uptime timer only on substeps where at least one side-support state is `true` after debounce.
- Track the longest contiguous duration (gap) where both side-support states are `false` after debounce.

### 4. Frame-Level Reduction
To produce the single "Frame Truth" reported at the **Truth-Sensitive Sampling Point**:
- **Final Qualifying Substep Rule**: The frame state must be sampled from the **final qualifying Chaos substep** of that frame. A "qualifying" substep is one where the solver reached completion without a NaN or total explosion.
- **Frame Support Hull**: The reported support hull for the frame is the **Convex Hull** of all representative points selected in Step 1 from that final qualifying substep.
- **Proxy Check**: The COM Proxy check for the frame must be performed against this reduced convex hull.

### 5. 30 Hz Artifact Emission
- The `30 Hz` artifact is a downsampled snapshot of the Frame-Level Truth.
- If a terminal failure occurs between 30 Hz samples, the artifact must be emitted immediately to capture the exact terminal state, regardless of the 30 Hz cadence.

## Terminal Reason Arbitration

When multiple failure conditions are simultaneously active in the same frame or truth window, the implementation must use this rank-ordered precedence table to decide the "winning" `terminal_reason`.

### Master Precedence Table

| Rank | Reason Class | Leaf-level `terminal_reason` | Arbitration Logic |
| :--- | :--- | :--- | :--- |
| **1** | **Plant Contract** | `activation_physics_asset_contract_violation` | Always wins; pre-empts all other classes. |
| **2** | **Plant Contract** | `activation_capsule_contract_violation` | Wins over all physical/stability failures. |
| **3** | **Raw Continuity** | `activation_topology_change` | Wins over `simulation_lost` if both occur. |
| **4** | **Raw Continuity** | `activation_continuous_simulation_lost` | Wins over support/controller classes. |
| **5** | **Support Truth** | `activation_support_failure` | Wins over `proxy_outside_region`. |
| **6** | **Support Truth** | `activation_proxy_outside_support_region` | Wins over controller stability classes. |
| **7** | **Controller Stability** | `activation_target_discontinuity` | Wins over gain/damping instability. |
| **8** | **Controller Stability** | `activation_unstable_gain_or_damping` | Wins over threshold breaches. |
| **9** | **Controller Stability** | `activation_instability_threshold_breach` | Wins over pose mismatch. |
| **10** | **Pose/Reference** | `activation_pose_reference_mismatch` | Wins over authority conflict. |
| **11** | **Authority/Ownership** | `activation_movement_reclaim` | Wins over generic authority conflict. |
| **12** | **Authority/Ownership** | `activation_shell_helper_violation` | Wins over generic authority conflict. |
| **13** | **Authority/Ownership** | `activation_authority_conflict` | Wins only over timeout. |
| **14** | **Time/Duration** | `activation_standing_validation_timeout` | Only emitted if no physical failure occurred. |

### General Arbitration Rule

When multiple terminal conditions are observed in the same frame:

1.  **Temporal Precedence (Substep-Level)**: The reason whose triggering condition was observed at the **earliest substep-level timestamp** wins.
2.  **Rank Precedence (Simultaneous)**: If multiple conditions are observed in the **exact same substep**, the reason with the **highest Rank (lowest number)** in the Master Precedence Table wins.
3.  **Co-Terminal Record**: All other co-occurring terminal conditions detected in the frame must be recorded in the `co_terminal_reasons` array.

### Clarification on Authority vs. Support

The **Temporal Precedence** rule naturally handles authority-driven failures:
- If a `movement_reclaim` event occurs in substep `N` and a `support_failure` follows in substep `N+2`, the `activation_movement_reclaim` is emitted.
- If a `support_failure` occurs in substep `N` and a `movement_reclaim` follows in substep `N+2` (e.g., as part of an automatic recovery attempt), the `activation_support_failure` is emitted.
- If both occur in the same substep `N`, the `activation_support_failure` wins per **Rank Precedence**.

This provides a deterministic, implementation-ready logic for all co-occurrence scenarios.

## Material Contamination Rules

The implementation must distinguish between **diagnostic** and **terminal** mesh-wide side effects. For `V0`, the following rules are deterministic.

### Rule 1 — Physics Blend Contamination
If the mesh-wide `PhysicsBlendWeight` (or equivalent `samples[].mesh_physics_blend_weight`) exceeds `0.0` for any frame during the `BalanceActivation` or `BalanceActive` modes, the run must fail on `activation_authority_conflict`.
- **Justification**: A non-zero blend weight allows the engine to kinematic-drag simulated bodies, which falsifies the balance proof.

### Rule 2 — Kinematic Update Contamination
If `bUpdateMeshWhenKinematic` (or `samples[].mesh_update_when_kinematic`) is enabled during the attempt, the run must fail on `activation_authority_conflict`.
- **Justification**: This setting allows the skeletal mesh to reposition bodies based on the animation pose even while simulating, which can "re-anchor" a falling character.

### Rule 3 — Authoritative Side-Effect Events
Any mesh-wide event recorded in `samples[].mesh_wide_side_effect_events` is **terminal** if it belongs to any of these authoritative classes:
- **Global Blend Writes**: `SetAllBodiesBelowPhysicsBlendWeight`, `SetPhysicsBlendWeight`.
- **Global Mobility Writes**: `SetSimulatePhysics` (called on the mesh or whole body set).
- **Global Reset/Teleport**: `ResetAllBodiesSimulatePhysics`, `SetAllBodiesBelowSimulatePhysics`.

### Rule 4 — Excluded-Body Contamination (Materiality Criterion)
A body outside the truth set (e.g., head, arms, or props) causes **material contamination** and a terminal `activation_authority_conflict` if it satisfies either of these criteria:

1.  **Adjacency Constraint Rule**: The excluded body is physically connected via a `PhysicsConstraint` to any body in the truth set (Critical Chain or Support Set), AND the excluded body receives a non-zero `PhysicsControl` target or `BodyModifier` write during the attempt.
    - **Justification**: An authoritative write to a connected body can "pull" or "stabilize" the truth set through the constraint hierarchy.
2.  **Dual-Contact Rule**: The excluded body is in simultaneous contact with **both** a truth-set body AND the walkable world (e.g., a hand touching a wall while the feet are simulate-standing).
    - **Justification**: This creates a "character-world-bridge" loop that can inject external support forces into the truth set that are not captured by the primary support-set logic.

### Rule 5 — Diagnostic Side-Effect Events
Side-effect events are **diagnostic only** (non-terminal) if they are limited to:
- **Read-only queries**: Component-wide bounds updates, overlap queries (without auto-resolution), or visibility updates.
- **Disconnected writes**: Authoritative writes to bodies that have no physical constraints or active contact paths to the truth set.

## Continuity Validation Algorithm

The implementation must use this deterministic logic to validate continuous simulation. Disagreement with this logic is a contract violation.

### Validator Logic (Per-Frame)

A frame is **Continuity Clean** only if all of these conditions are met:

1.  **Simulate-Physics Rule**: `BodyInstance->IsInstanceSimulatingPhysics()` is `true` for all bodies in the active truth sets (Critical Chain + Support Set).
2.  **Body Validity Rule**: `BodyInstance->IsValid()` is `true` for all bodies in the active truth sets.
3.  **Topology Rule**: The `FPhysicsBodyInstance` pointers for the truth set have not changed since the start of the attempt (no recreation or replacement).
4.  **Pelvis Wake Rule**: The pelvis body must remain **AWAKE**.
    - If the pelvis sleeps during `BalanceActivation_Validate` or `BalanceActive_Standing`, record `activation_continuous_simulation_lost`.
    - **Exception**: Sleep is allowed during `BalanceActivation_Ready`.

### Admissible States (Non-Terminal)

The following conditions are **admissible** and do not falsify continuity:

1.  **Support-Body Sleep**: Bodies in the support set (`foot_*`, `ball_*`) are allowed to sleep. 
    - **Justification**: A firmly planted foot may sleep while the upper body remains simulated and awake.
2.  **Wake/Sleep Oscillation**: Frequent wake/sleep transitions on support bodies are **diagnostic only**.
3.  **Bookkeeping Disagreement**: If the bridge's internal modifier record disagrees with the raw body state, but the **Raw Simulate-Physics Rule** is still satisfied, the run is **Continuity Clean**.
    - The disagreement must be logged as `continuity_bookkeeping_mismatch` but it is not a terminal failure.

### Terminal Classification Rule

- If Rules 1, 2, or 3 fail: Emit `activation_continuous_simulation_lost`.
- If Rule 4 fails (Pelvis sleep): Emit `activation_continuous_simulation_lost`.
- If Rule 3 fails specifically because of a pointer change: Emit `activation_topology_change`.

## Arbitration of Diagnostic Events

If a diagnostic event (e.g., `continuity_bookkeeping_mismatch`) is detected:

- It must be recorded against the current frame.
- It is a `terminal_reason` candidate ONLY if it rank-precedes the observable physical failures for that frame (per the Master Precedence Table).
- If it rank-follows the physical failure, it must be logged as secondary context only.
- Continuous simulation continuity (the Validator Logic above) always outranks authority bookkeeping (modifiers) as a source of truth for the terminal state.
