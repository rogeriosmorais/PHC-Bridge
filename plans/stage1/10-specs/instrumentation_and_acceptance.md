# Instrumentation and Acceptance

## Purpose

This document is the **sole authoritative owner** of the run artifact schema, emission thresholds, and acceptance gates.

**Physical truth reasoning and subsystem ownership are owned by the [Truth Model](continuous_balance_truth_model.md) and [Authority Matrix](authority_matrix.md).**

## Run Artifact Schema (30 Hz)

The implementation must emit a JSON artifact for every attempt, containing:

### 1. Metadata and Contracts
- `attempt_uuid`: Unique identifier for the run.
- `timestamp`: World time of emission.
- `baseline_id`: GUID of the audited plant fingerprint.
- `standing_reference_id`: Identifier for the authored stance asset.
- `physics_asset_contract_valid`: Boolean flag from the plant audit.
- `skeleton_audit_passed`: Boolean flag from the skeleton alignment check.
- **Plant Audit Telemetry**:
  - `plant_failure_class`: Category of breach (`StaticStructural`, `Mutation`, `Dynamic`).
  - `plant_failure_field`: Specific field triggering the failure.
  - `mass_drift_total_pct`: Observed total mass deviation.
  - `mass_drift_critical_chain_pct`: Observed deviation for critical bodies.
  - `mass_drift_support_set_pct`: Observed deviation for support bodies.
  - `inertia_drift_critical_pct`: Orientation-averaged inertia drift for critical chain.
  - `inertia_drift_support_pct`: Orientation-averaged inertia drift for support set.
  - `constraint_profile_match`: Boolean flag for authored gain/limit integrity.
  - `support_geometry_audit_passed`: Boolean flag for plantar surface validity.
  - `collision_filter_audit_passed`: Boolean flag for self/world filtering integrity.
- **Capsule Contract Truth**:
  - `capsule_collision_enabled`: Actual `ECollisionEnabled` state.
  - `capsule_generate_overlap_events`: Actual boolean state.
  - `capsule_world_pos_cm`: Raw world coordinates of the root.
  - `capsule_lock_delta_cm`: Distance from the rebase origin.
  - `mesh_uses_absolute_location/rotation/scale`: Flags for transform isolation.
  - `cmc_is_active`: Boolean state of the component.
  - `cmc_tick_enabled`: Actual tick-function state.
  - `cmc_movement_mode`: Actual `EMovementMode` value.
  - `cmc_updated_component_is_null`: Confirmation of UpdatedComponent state.

**Rule**: The implementation must emit `activation_capsule_contract_violation` if any of the above fields violate the structural requirements defined in [character_capsule_contract.md](character_capsule_contract.md).

### 2. Primary Metrics
- `hold_duration_sec`: Total contiguous standing time.
- `support_uptime_sec`: Cumulative duration with at least one foot supporting.
- `max_root_tilt_deg`: Peak deviation from gravity-up.
- `peak_angular_speed`: Max body angular velocity recorded.
- `rms_mismatch_deg`: Root-mean-square pose fidelity scalar.
- `max_body_mismatch_deg`: Worst-case orientation error for any body.

### 3. Support Truth
- `support_state_l` / `support_state_r`: Debounced side-contact status.
- `support_mode`: The dominant frame-level stability grade over the 30 Hz sample window.
  - **Tie-Breaking**: If multiple modes occur equally, the artifact MUST report the most severe mode in this priority: `Airborne` > `TransientRecovery` > `SingleFootSurvival` > `TwoFootStable`.
- `support_gap_timer_ms`: Current contiguous contact-loss duration (reset to 0.0 on any contact).
- `proxy_inside_hull`: Boolean result of the point-in-polygon test.
- `active_support_side_count`: Number of sides currently in debounced contact (0, 1, or 2).
- `support_hull_area_cm2`: Total planar area of the frame support hull.
- `support_patch_area_l_cm2` / `support_patch_area_r_cm2`: Individual plantar area per side.
- `support_hull_points`: World-space coordinates of the active footprint.
- `com_proxy_pos`: Projected planar centroid of the critical chain.
- `max_penetration_cm`: Deepest manifold point recorded for the frame.
- `support_churn_count`: Number of debounced state transitions in the current 30 Hz sample window.
- `support_churn_hz`: Rolling frequency of combined transitions (See [engine_execution_contract.md](engine_execution_contract.md)).
- `calf_world_contact_l` / `calf_world_contact_r`: Boolean contamination flags.
- `calf_contact_terminal`: Confirmation that calf contact triggered arbitration.

### 4. Diagnostics and Continuity
- `control_alpha`: Current blend rollout progress (0.0 - 1.0).
- `shell_bookkeeping_state`: Flags for `locked`, `reanchored`, etc.
- `shell_influence_materiality`: Estimated assist force/torque from shell.
- `movement_reclaim_count`: Number of CMC interference events.
- `continuity_bookkeeping_mismatch`: Diagnostic flag for modifier/raw drift.
- `physical_continuity_validator_passed`: Authoritative pass/fail flag from the continuity check.

### 5. Contamination and Authority
- `contamination_class`: Classification of the violation (`mesh_wide_assist`, `non_critical_body_assist`, `excluded_body_world_brace`, `global_blend_or_kinematic_assist`).
- `contamination_source_body`: Name of the body triggering the conflict.
- `contamination_source_subsystem`: Name of the external system issuing the write.
- `mesh_wide_assist_detected`: Boolean flag for global mesh-level interference.
- `non_critical_body_assist_detected`: Boolean flag for excluded body interference.
- `excluded_body_world_contact_source`: Body name for world-bracing events.
- `global_blend_weight`: Live `PhysicsBlendWeight` value (per sample).
- `mesh_update_when_kinematic_enabled`: Actual boolean state of the flag (per sample).

**Requirement**: All fields in the Authority and Plant Audit sections must be populated whenever `activation_physics_asset_contract_violation`, `activation_authority_conflict`, `activation_movement_reclaim`, or `activation_capsule_contract_violation` is the primary `terminal_reason`.

**Auditability Rule**: Terminal contamination must be fully auditable from the artifact schema without requiring external log correlation.

### 6. Termination
- `terminal_reason`: The winning leaf-level reason from the canonical set below.
- `co_terminal_reasons[]`: List of co-occurring terminal conditions.
- `terminal_substep_timestamp`: Exact Chaos substep count of failure.
- `terminal_frame_artifact`: Full raw snapshot of the failure frame.

## Canonical Terminal Reasons

When a run terminates in failure, the primary `terminal_reason` must be exactly one of the values from this canonical set. For successful attempts, `terminal_reason` must be `nullptr`.

1. `activation_physics_asset_contract_violation`
2. `activation_capsule_contract_violation`
3. `activation_topology_change`
4. `activation_continuous_simulation_lost`
5. `activation_support_failure`
6. `activation_proxy_outside_support_region`
7. `activation_target_discontinuity`
8. `activation_unstable_gain_or_damping`
9. `activation_instability_threshold_breach`
10. `activation_pose_reference_mismatch`
11. `activation_movement_reclaim`
12. `activation_shell_helper_violation`
13. `activation_authority_conflict`
14. `activation_standing_validation_timeout`

No other reason labels are permitted for V0 artifacts.

## Threshold Basis Table

These are the authoritative operational thresholds for the `V0` artifact and terminal logic.

| Metric | Threshold | Threshold Source / Logic |
| :--- | :--- | :--- |
| **Max Root Tilt** | `20.0 deg` | Posture envelope for honest standing |
| **Peak Angular Speed** | `720.0 deg/s` | Explosive instability limit |
| **Support Gap (Max)** | `100.0 ms` | Air-time limit for continuous support |
| **COM Proxy Drift** | `100.0 ms` | Duration allowed outside the support hull |
| **Mismatch (Max Body)** | `25.0 deg` | Orientation fidelity limit (for 100ms) |
| **Mismatch (RMS Chain)** | `15.0 deg` | Average pose fidelity limit (for 100ms) |
| **Target Discontinuity** | `15.0 deg` | Max jump at blend start |
| **Mismatch Grace Period** | `0.2 sec` | Settling time for reference match |
| **Support Churn** | `12.0 Hz` | Jitter/Popping limit |
| **Pelvis Sleep Limit** | `100.0 ms` | Max contiguous sleep before simulation loss |
| **Plant Admissibility** | Per Authoritative Plant Tolerances in [physics_asset_contract.md](physics_asset_contract.md) | Mirrored authority for mass/inertia/skeleton. |
| **Skeleton Axis Alignment** | Per [physics_asset_contract.md](physics_asset_contract.md) | Authoritative joint axis audit. |
| **Segment Length Tolerance** | Per [physics_asset_contract.md](physics_asset_contract.md) | Authoritative bone length audit. |
| **Capsule Lock Delta** | `0.01 cm` | Max deviation for frozen root |
| **Support Area (Min)** | `50.0 cm2` | Minimum area required for a valid planar support proof |
| **Hold Duration (Min)** | `3.0 sec` | Acceptance duration for V0 |

## Must-Fail Gates (Regression Criteria)

The implementation is considered broken if it fails to detect and correctly label these "Must-Fail" scenarios:

1.  **Capsule Breach**: Moving the actor or capsule during activation without a Rank 2 failure.
2.  **Plant Breach**: Swapping the physics asset or mass during activation without a Rank 1 failure.
3.  **Contamination Breach**: Contact on the `calf_*` or upper-body without a terminal `activation_authority_conflict`.
4.  **Simulation Loss**: Calling `SetSimulatePhysics(false)` on a truth-set body without `activation_continuous_simulation_lost`.
5.  **Proxy Breach**: Proxy remains outside the hull for > 100ms without `activation_proxy_outside_support_region`.

## Acceptance Gates

A run is **Successful** ONLY if:
1.  The artifact `terminal_reason` is `nullptr`.
2.  The `hold_duration_sec` reaches or exceeds `3.0`.
3.  All primary metrics remained within the thresholds above for the entire duration.
4.  The artifact `physical_continuity_validator_passed` is `true`.
