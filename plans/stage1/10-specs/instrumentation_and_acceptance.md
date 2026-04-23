# Instrumentation And Acceptance

## Purpose

This document defines the minimum instrumentation, run artifacts, and pass/fail rules for the continuous-balance rewrite.

Build observability before behavior.

## Run Artifact Schema

Every continuous-balance run must emit a structured artifact with the following minimum schema.

### Run Config Fields

| Field | Type | Units / format | Required |
| :--- | :--- | :--- | :--- |
| `commit_sha` | string | git SHA | yes |
| `map_name` | string | UE map name | yes |
| `character_asset` | string | asset path | yes |
| `physics_asset_id` | string | asset path or identifier | yes |
| `constraint_profile_set_id` | string | authored profile identifier | yes |
| `physical_material_set_id` | string | authored profile identifier | yes |
| `collision_disable_table_id` | string | authored table identifier | yes |
| `support_geometry_audit_id` | string | audit artifact identifier | yes |
| `policy_build` | string | build or model identifier | yes |
| `standing_reference_id` | string | versioned reference identifier | yes |
| `standing_reference_asset_path` | string | asset path | yes |
| `standing_reference_authored_space` | string | `authored_parent_local_rotations` | yes |
| `standing_reference_control_space` | string | `gravity_aligned_world_frame` | yes |
| `standing_reference_space` | string | `parent_local_pose_zero_velocity` | yes |
| `standing_reference_rebase_frame` | object | origin/up/yaw capture | yes |
| `dt_seconds` | number | seconds | yes |
| `substeps` | integer | count | yes |
| `solver_settings` | object | named key/value set | yes |
| `mode_name` | string | runtime mode set | yes |
| `shell_mode` | string | `disabled_v0` or later explicit mode | yes |

### Summary Fields

| Field | Type | Units | Required |
| :--- | :--- | :--- | :--- |
| `run_start_time_utc` | string | ISO-8601 | yes |
| `run_duration_seconds` | number | seconds | yes |
| `terminal_state` | string | enum | yes |
| `terminal_reason_family` | string | enum | yes |
| `terminal_reason` | string | enum | yes |
| `co_terminal_reasons` | array of string | enum values | yes |
| `physics_asset_contract_valid` | boolean | boolean | yes |
| `sustained_hold_time_seconds` | number | seconds | yes |
| `contiguous_hold_time_seconds` | number | seconds | yes |
| `mesh_physics_blend_state` | object | named key/value set | yes |
| `mesh_update_when_kinematic_state` | object | named key/value set | yes |
| `root_tilt_envelope_deg` | number | degrees | yes |
| `peak_angular_speed_by_family_deg_per_sec` | object | deg/s | yes |
| `contact_uptime_seconds` | number | seconds | yes |
| `control_effort_proxy` | number | unitless normalized scalar | yes |
| `blend_primitive_bundle` | object | named key/value set | yes |
| `authority_conflict_count` | integer | count | yes |
| `mesh_wide_side_effect_event_count` | integer | count | yes |
| `truth_set_recreation_event_count` | integer | count | yes |
| `continuity_bookkeeping_mismatch_count` | integer | count | yes |
| `topology_change_event_count` | integer | count | yes |
| `shell_helper_used_count` | integer | count | yes |
| `support_loss_gap_max_ms` | number | milliseconds | yes |
| `contact_churn_rate_hz` | number | changes per second | yes |
| `min_support_contact_count_seen` | integer | count | yes |
| `reference_mismatch_max_deg` | number | degrees | yes |
| `reference_mismatch_rms_deg` | number | degrees | yes |

### Time-Series Fields

| Field | Type | Units | Cadence | Required |
| :--- | :--- | :--- | :--- | :--- |
| `samples[].t_seconds` | number | seconds | fixed cadence | yes |
| `samples[].runtime_mode` | string | enum | fixed cadence | yes |
| `samples[].root_tilt_deg` | number | degrees | fixed cadence | yes |
| `samples[].support_contact_active` | boolean | boolean | fixed cadence | yes |
| `samples[].support_contact_count` | integer | count | fixed cadence | yes |
| `samples[].support_contact_source` | string | enum | fixed cadence | yes |
| `samples[].support_substep_contact_count` | integer | count | fixed cadence | yes |
| `samples[].support_hull_point_count` | integer | count | fixed cadence | yes |
| `samples[].support_surface_max_speed_cm_per_sec` | number | cm/s | fixed cadence | yes |
| `samples[].peak_family_angular_deg_per_sec` | object | deg/s | fixed cadence | yes |
| `samples[].control_authority_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].truth_set_raw_simulating` | boolean | boolean | fixed cadence | yes |
| `samples[].pelvis_awake` | boolean | boolean | fixed cadence | yes |
| `samples[].truth_set_recreation_events` | integer | count in sample window | fixed cadence | yes |
| `samples[].continuity_bookkeeping_mismatch` | boolean | boolean | fixed cadence | yes |
| `samples[].target_orientation_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].target_position_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].target_angular_velocity_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].target_linear_velocity_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].spring_strength_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].damping_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].max_torque_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].max_force_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].mesh_physics_blend_weight` | number | `0..1` or sentinel | fixed cadence | yes |
| `samples[].mesh_update_when_kinematic` | boolean | boolean | fixed cadence | yes |
| `samples[].target_discontinuity_deg` | number | degrees | fixed cadence | yes |
| `samples[].reference_mismatch_max_deg` | number | degrees | fixed cadence | yes |
| `samples[].reference_mismatch_rms_deg` | number | degrees | fixed cadence | yes |
| `samples[].reference_projection_valid` | boolean | boolean | fixed cadence | yes |
| `samples[].support_proxy_world_xy_cm` | object | centimeters | fixed cadence | yes |
| `samples[].support_region_valid` | boolean | boolean | fixed cadence | yes |
| `samples[].authority_conflict_events` | integer | count in sample window | fixed cadence | yes |
| `samples[].topology_change_events` | integer | count in sample window | fixed cadence | yes |
| `samples[].terminal_reason_candidate` | string | enum or empty | fixed cadence | yes |
| `samples[].standing_reference_id` | string | identifier | fixed cadence | yes |

### Cadence And Execution Order

All timing, cadence, and execution-order rules are defined in the authoritative:

- [engine_execution_contract.md](engine_execution_contract.md)

### Time Windows and Sampling Rules

- validation hold window: contiguous interval spent in `BalanceActivation_Validate`
- sustained hold window: contiguous interval spent in `BalanceActive_Standing`
- support uptime window: total time with valid support contact during validate and standing windows
- mesh-wide side-effect fields are required because nominally body-level Physics Control and body-modifier writes can still alter whole-skeletal-mesh behavior
- `blend_primitive_bundle` must declare which Physics Control primitives are alpha-ramped in `V0` and which are fixed-per-attempt
- `standing_reference_asset_path`, `standing_reference_authored_space`, and `standing_reference_control_space` are required so every run names the exact authored pose asset, authored-space convention, and runtime control-space projection used for rebasing
- `truth_set_raw_simulating=true` means every active truth-set body reported raw simulation enabled on that sample
- `continuity_bookkeeping_mismatch=true` means raw body continuity and bookkeeping continuity disagreed on that sample; this is diagnostic unless raw continuity was also lost
- `pelvis_awake` exists because `V0` treats persistent sleeping pelvis state after blend start as non-participating physics, not acceptable standing truth
- support contact truth is derived from Chaos contacts at substep resolution, then reduced to the `30 Hz` artifact after a `2`-substep debounce rule
- terminal truth is not decided from the `30 Hz` artifact stream; the artifact reports the result of higher-rate truth evaluation
- the sampled support hull uses reduced accepted manifold points from the frame's final qualifying Chaos substep, not an arbitrary trace fallback
- `reference_mismatch_*` fields are measured against the rebased authored standing reference, not against a live sampled pose or shell snapshot
- `reference_projection_valid=true` means the authored standing reference projected cleanly through the fixed runtime skeleton mapping, gravity-aligned rebase frame, and active fixed Physics Control configuration for that sample

### Terminal Reason Contract

- `terminal_reason_family` may be used for coarse grouping only
- `terminal_reason` must carry the first truthful leaf-level reason that ended the run
- `samples[].terminal_reason_candidate` records the best current leaf-level reason at each sample, or empty when no terminal condition exists yet

Required leaf-level `terminal_reason` values for `V0`:

- `activation_physics_asset_contract_violation`
- `activation_continuous_simulation_lost`
- `activation_topology_change`
- `activation_target_discontinuity`
- `activation_unstable_gain_or_damping`
- `activation_support_failure`
- `activation_proxy_outside_support_region`
- `activation_pose_reference_mismatch`
- `activation_authority_conflict`
- `activation_movement_reclaim`
- `activation_shell_helper_violation`
- `activation_capsule_contract_violation`
- `activation_instability_threshold_breach`
- `activation_standing_validation_timeout`

Contact-measurement rules:

- `support_contact_source` must be `chaos_contacts` for `V0`
- `support_contact_active=true` means the debounced substep-level support truth was active on at least one side at the frame's truth-sensitive sample point
- `support_contact_count` is the count of support sides currently active: `0`, `1`, or `2`
- `support_substep_contact_count` is the number of accepted support-contact observations seen across Chaos substeps within the frame
- `support_hull_point_count` is the number of reduced accepted manifold points used to build the sampled support hull
- `support_surface_max_speed_cm_per_sec` is the maximum sampled world-space speed of any accepted support surface point during the frame; it must stay at or below `5.0`
- contact churn is counted from debounced side-support state transitions at truth cadence, then summarized into the artifact rather than inferred from `30 Hz` transitions alone
- accepted support contacts in `V0` must be against non-character `WorldStatic` geometry whose contact normal is within `5.0 deg` of gravity-up
- contacts against `WorldDynamic`, simulated rigid bodies, moving platforms, other characters, or self-contact are diagnostic-only and may not contribute to support truth
- `support_loss_gap_max_ms` is measured from debounced substep-level intervals where `support_contact_count=0`, then summarized into the artifact
- support-proxy-outside-region time is measured independently from consecutive samples where `support_region_valid=false`
- `standing_reference_rebase_frame` captures the one-time `pelvis` origin, gravity-up axis, and projected `pelvis` yaw used for the attempt
- `reference_mismatch_max_deg` is the maximum shortest-arc quaternion orientation error across the balance-critical chain for the run
- `reference_mismatch_rms_deg` is the unweighted RMS shortest-arc quaternion orientation error across the balance-critical chain for the run

Physics-asset-contract rules:

- `physics_asset_contract_valid=true` is required before `BalanceActivation_Ready` may begin
- the artifact must record the exact physics asset, constraint profile set, physical-material set, collision-disable table, and support-geometry audit used for the run
- if any plant-profile mutation occurs during an active attempt, the run must terminate as `activation_physics_asset_contract_violation`

## Forbidden Metrics

These metrics are not allowed to stand in for success:

- phase-completion counters
- shell-lock or shell-reference status by itself
- “clean transition” counters
- retry counts that do not end in sustained standing

## Acceptance Gates

### Milestone 1

- honest continuous-physics diagnostics exist
- run artifact schema is populated
- failure can be explained without leaning on legacy phase completion

### Milestone 2

- `1.0` second contiguous stable hold exists under the continuous-balance mode

### Milestone 3

- `3.0` second contiguous stable hold exists under the continuous-balance mode

### Milestone 4

- small perturbation recovery is demonstrated after Milestone 3 is real

## Must-Fail Gates

Activation must fail if:

- the physics-asset contract is not satisfied
- the balance-critical chain loses continuous simulation
- any truth-set body is recreated or replaced during the attempt
- shell helper exceeds `V0` policy by being used at all on the balance-critical chain or support set
- the movement component reclaims authority over the balance-critical chain or support set
- a topology change occurs on the balance-critical chain
- standing duration is not contiguous

Support truth must fail if:

- support-loss gap exceeds `100 ms`
- support-contact churn exceeds `12 Hz`
- minimum support-contact count drops below `1`

Standing validation must fail if:

- root tilt envelope exceeds `20.0 deg`
- peak angular speed for the balance-critical chain exceeds `720.0 deg/s`
- peak angular speed for the support set exceeds `720.0 deg/s`
- reference mismatch exceeds `25.0 deg` on any balance-critical-chain body for more than `100 ms`
- reference mismatch RMS across the balance-critical chain exceeds `15.0 deg` for more than `100 ms`

## Threshold Basis

Each numeric threshold in this spec is a `V0` conservative working value, not an empirically validated contract number.
The table below states the derivation class for each value and the kind of evidence required to tighten or relax it.

| Threshold | Value | Derivation class | Evidence needed to revise |
| :--- | :--- | :--- | :--- |
| Root tilt envelope | `20.0 deg` | Conservative design choice based on observed ~45° tilt budget from Phase 1/2 calibration runs; V0 target is intentionally tighter to force honest proximal standing | Artifact histogram of root tilt across multi-second standing attempts on the audited plant |
| Peak angular speed (balance-critical chain) | `720.0 deg/s` | Conservative ceiling derived from Phase 1/2 abort thresholds (~4000 deg/s) divided down to a standing-stability regime; not measured from successful standing runs | Per-body angular speed distribution from artifact time-series on successful V0 hold attempts |
| Peak angular speed (support set) | `720.0 deg/s` | Same derivation class as balance-critical chain; foot/calf speed budget assumed to be in the same regime | Same as above; may be revised independently once foot-contact dynamics are measured |
| Support-loss gap | `100 ms` | Design choice: one-foot-off events shorter than one Chaos substep window (~8 ms) must survive debounce; 100 ms is three full frames at 30 Hz and is wide enough to avoid debounce artifacts without permitting real loss | Measured gap distribution from substep-resolution contact logs on flat-ground standing |
| Contact churn rate | `12 Hz` | Design choice: two side-state transitions per frame at 30 Hz frame rate; intended to allow normal single-foot-off transients while blocking persistent instability | Contact-churn histogram from substep-resolution truth logs on stable standing attempts |
| Reference mismatch max | `25.0 deg` | Conservative ceiling; below the ~45° posture error seen in Phase 1/2; wide enough to survive blend-in transients without permitting gross pose failure | Per-body mismatch time-series from blend-in and validate phases of recorded run artifacts |
| Reference mismatch RMS | `15.0 deg` | Conservative aggregate ceiling; set below the max to catch distributed pose error; not derived from standing run data | RMS mismatch distribution across balance-critical chain from multi-second standing artifacts |
| Support proxy outside region | `100 ms` | Same design class as support-loss gap; chosen to match that timer so neither gate is structurally weaker than the other | Proxy-outside-region duration histogram from artifact time-series on flat-ground standing |

All of these values are open to revision once real V0 run artifacts exist.
Revisions must update this table with the evidence source and must not be made as silent tuning edits.

## Regression Gates

The rewrite branch is acceptable if it delivers:

- more honest failure
- less hidden assistance
- stronger observability

even when early visual behavior looks worse than the legacy path.

## Expected Early Regressions That Are Acceptable

- worse-looking early stance behavior
- earlier failure under continuous physics
- higher visible oscillation because grace logic is no longer hiding it
- clearer controller weakness that used to be misread as a transition problem

These are acceptable during the rewrite if instrumentation quality improves and hidden assistance decreases.
