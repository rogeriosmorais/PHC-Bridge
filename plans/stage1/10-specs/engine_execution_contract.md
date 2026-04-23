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

- **Reporting Cadence**: 30 Hz (Downsampled artifact emission).
- **Truth Cadence**: Chaos Substep resolution (e.g., ~120 Hz).
- **Control Cadence**: Frame rate (Tick-based target publication).

## Arbitration

If a diagnostic event (e.g., `authority_conflict`) is detected at any point in this sequence, it must be recorded against the current frame's artifact using the rules in `continuous_balance_truth_model.md`.
