# Balance-First Test Matrix

## 1. Pure Logic Unit Tests (Layer 1)

| Test ID | Target | Scenario | Trigger | Expected Output |
|---|---|---|---|---|
| **LOGIC-01** | `ExtractPatchHull` | Valid 2D points | `NumPoints > 2` | `patch_area_cm2` > 0 |
| **LOGIC-02** | `ExtractPatchHull` | Points on a line | `Collinear points` | `patch_area_cm2` = 0 |
| **LOGIC-03** | `ExtractPatchHull` | Empty points | `NumPoints = 0` | `patch_area_cm2` = 0 |
| **LOGIC-04** | `BuildFrameHull` | Offset Unit Squares | L:[0,0]-[1,1]; R:[2,0]-[3,1] | `support_hull_area_cm2` = 3.0 |
| **LOGIC-05** | `ClassifySupportMode` | Both feet down | `Left=true, Right=true` | `support_mode` = `TwoFootStable` |
| **LOGIC-06** | `ClassifySupportMode` | One foot down | `Left=true, Right=false` | `support_mode` = `SingleFootSurvival` |
| **LOGIC-07** | `ClassifySupportMode` | Both feet up, gap <= max | `Both=false, Timer <= limit` | `support_mode` = `TransientRecovery` |
| **LOGIC-08** | `ClassifySupportMode` | Both feet up, gap > max | `Both=false, Timer > limit` | `support_mode` = `Airborne` |
| **LOGIC-09** | `AdjudicateProxy` | Proxy inside hull | `Inside polygon` | `proxy_inside_hull` = `true` |
| **LOGIC-10** | `AdjudicateProxy` | Proxy outside hull under limit | `Outside polygon, Timer <= limit` | `proxy_inside_hull` = `false`, `proxy_outside_hull_duration_ms` <= 100.0, `terminal_reason` = `nullptr` |
| **LOGIC-11** | `AdjudicateProxy` | Proxy outside hull over limit | `Outside polygon, Timer > limit` | `proxy_inside_hull` = `false`, `proxy_outside_hull_duration_ms` > 100.0, `terminal_reason` = `activation_proxy_outside_support_region` |
| **LOGIC-12** | `AdjudicateProxy` | No support hull | `SideCount = 0` | `proxy_inside_hull` = `nullptr`, `proxy_outside_hull_duration_ms` = `nullptr`, proxy test skipped |
| **LOGIC-13** | `CalculateChurnHz` | 5 transitions in 1.0s | `5 events / 1.0s` | `support_churn_hz` = 5.0 |
| **LOGIC-14** | `ReduceSupportModeForReportWindow` | 30 Hz tie-break | Equal durations | severity tie-break: `Airborne` > `TransientRecovery` > `SingleFootSurvival` > `TwoFootStable` |

## 2. Validator Contract Tests (Layer 2)

| Test ID | Target | Scenario | Expected Reason | Required Artifact Truth |
|---|---|---|---|---|
| **VALID-01A** | `ValidateContinuity` | Physics disabled | `activation_continuous_simulation_lost` | `physical_continuity_validator_passed` = `false`, `terminal_reason` = `activation_continuous_simulation_lost` |
| **VALID-01B** | `ValidateContinuity` | Pelvis sleep limit exceeded | `activation_continuous_simulation_lost` | `pelvis_sleep_duration_ms` > 100.0, `physical_continuity_validator_passed` = `false`, `terminal_reason` = `activation_continuous_simulation_lost` |
| **VALID-01C** | `ValidateContinuity` | Body instance loss | `activation_topology_change` | `terminal_reason` = `activation_topology_change`, `physical_continuity_validator_passed` = `false` |
| **VALID-01D** | `ValidateContinuity` | Bookkeeping delta only | `terminal_reason` = `nullptr` | `continuity_bookkeeping_mismatch` = `true` |
| **VALID-02A** | `ValidateCapsule` | Actor moved | `activation_capsule_contract_violation` | `capsule_lock_delta_cm` > 0.01, `terminal_reason` = `activation_capsule_contract_violation` |
| **VALID-02B** | `ValidateCapsule` | Capsule collision active | `activation_capsule_contract_violation` | `capsule_collision_enabled` != `NoCollision`, `terminal_reason` = `activation_capsule_contract_violation` |
| **VALID-02C** | `ValidateCapsule` | CMC active/ticking | `activation_capsule_contract_violation` | `cmc_is_active` or `cmc_tick_enabled` = `true`, `terminal_reason` = `activation_capsule_contract_violation` |
| **VALID-02D** | `ValidateCapsule` | UpdatedComponent still owned | `activation_capsule_contract_violation` | `cmc_updated_component_is_null` = `false`, `terminal_reason` = `activation_capsule_contract_violation` |
| **VALID-03A** | `ValidatePlant` | Skeleton mismatch | `activation_physics_asset_contract_violation` | `plant_failure_class` = `StaticStructural`, `terminal_reason` = `activation_physics_asset_contract_violation` |
| **VALID-03B** | `ValidatePlant` | Length or axis drift | `activation_physics_asset_contract_violation` | `physics_asset_contract_valid` = `false`, `plant_failure_class` = `StaticStructural`, `plant_failure_field` = `segment_length` or `axis_alignment`, `terminal_reason` = `activation_physics_asset_contract_violation` |
| **VALID-03C** | `ValidatePlant` | Mass mutation | `activation_physics_asset_contract_violation` | `plant_failure_class` = `Mutation`, `plant_failure_field` = `mass`, `terminal_reason` = `activation_physics_asset_contract_violation` |
| **VALID-03D** | `ValidatePlant` | Physics asset swap | `activation_physics_asset_contract_violation` | `physics_asset_contract_valid` = `false`, `plant_failure_class` = `Mutation`, `plant_failure_field` = `physics_asset_identity`, `terminal_reason` = `activation_physics_asset_contract_violation` |
| **VALID-04A** | `ValidateAuthority` | Mesh-wide assist | `activation_authority_conflict` | `contamination_class` = `mesh_wide_assist`, `terminal_reason` = `activation_authority_conflict` |
| **VALID-04B** | `ValidateAuthority` | Non-critical body assist | `activation_authority_conflict` | `contamination_class` = `non_critical_body_assist`, `terminal_reason` = `activation_authority_conflict` |
| **VALID-04C** | `ValidateAuthority` | Calf or excluded world brace | `activation_authority_conflict` | `contamination_class` = `excluded_body_world_brace`, `terminal_reason` = `activation_authority_conflict` |
| **VALID-04D** | `ValidateAuthority` | Global blend/kinematic assist | `activation_authority_conflict` | `contamination_class` = `global_blend_or_kinematic_assist`, `terminal_reason` = `activation_authority_conflict` |
| **VALID-05A** | `ValidateControllerStability` | Target jump at blend start | `activation_target_discontinuity` | `target_discontinuity_deg` > 15.0, `target_discontinuity_phase` = `BlendStart`, `terminal_reason` = `activation_target_discontinuity` |
| **VALID-05B** | `ValidateControllerStability` | Controller gain breach | `activation_unstable_gain_or_damping` | `controller_gain_damping_valid` = `false`, `controller_gain_scale` > Controller Gain Scale (Max), `controller_stability_failure_field` = `controller_gain_scale`, `terminal_reason` = `activation_unstable_gain_or_damping` |
| **VALID-05C** | `ValidateControllerStability` | Controller damping breach | `activation_unstable_gain_or_damping` | `controller_gain_damping_valid` = `false`, `controller_damping_ratio` < Controller Damping Ratio (Min), `controller_stability_failure_field` = `controller_damping_ratio`, `terminal_reason` = `activation_unstable_gain_or_damping` |
| **VALID-05D** | `ValidateControllerStability` | Root tilt breach | `activation_instability_threshold_breach` | `max_root_tilt_deg` > 20.0, `controller_stability_failure_field` = `max_root_tilt_deg`, `terminal_reason` = `activation_instability_threshold_breach` |
| **VALID-05E** | `ValidateControllerStability` | Angular speed breach | `activation_instability_threshold_breach` | `peak_angular_speed` > 720.0, `controller_stability_failure_field` = `peak_angular_speed`, `terminal_reason` = `activation_instability_threshold_breach` |
| **VALID-05F** | `ValidateControllerStability` | Max-body mismatch over grace | `activation_pose_reference_mismatch` | `max_body_mismatch_deg` > 25.0, `mismatch_duration_ms` > 200.0, `controller_stability_failure_field` = `max_body_mismatch_deg`, `terminal_reason` = `activation_pose_reference_mismatch` |
| **VALID-05G** | `ValidateControllerStability` | RMS-chain mismatch over grace | `activation_pose_reference_mismatch` | `rms_mismatch_deg` > 15.0, `mismatch_duration_ms` > 200.0, `controller_stability_failure_field` = `rms_mismatch_deg`, `terminal_reason` = `activation_pose_reference_mismatch` |
| **VALID-05H** | `ValidateControllerStability` | Standing validation timeout | `activation_standing_validation_timeout` | `hold_duration_sec` < 3.0, `standing_validation_timed_out` = `true`, `terminal_reason` = `activation_standing_validation_timeout` |
| **VALID-06A** | `ValidateMovementReclaim` | CMC correction path runs | `activation_movement_reclaim` | `movement_reclaim_count` > 0, `terminal_reason` = `activation_movement_reclaim` |
| **VALID-06B** | `ValidateShellHelper` | Shell helper writes during activation | `activation_shell_helper_violation` | `shell_helper_used_count` > 0, `terminal_reason` = `activation_shell_helper_violation` |

## 3. Arbitration Logic Tests (Layer 2.5)

| Test ID | Target | Scenario | Trigger | Expected Outcome |
|---|---|---|---|---|
| **ARBIT-01** | `ArbitrateFailure` | Simultaneous Plant + Support | Same substep | `activation_physics_asset_contract_violation` |
| **ARBIT-02** | `ArbitrateFailure` | Simultaneous Support + Proxy | Same substep | `activation_support_failure` |
| **ARBIT-03** | `ArbitrateFailure` | Earlier lower-rank reason | Authority conflict before plant breach | earliest substep reason wins |
| **ARBIT-04** | `ArbitrateFailure` | Co-terminal failures | Multiple failures in one frame | winning reason plus `co_terminal_reasons[]` |
| **ARBIT-05** | `ArbitrateFailure` | No failures | All green | `terminal_reason` = `nullptr` |

## 4. Runtime Integration Tests (Layer 3)

| Test ID | Target | Scenario | Expected State | Expected Reason | Expected Artifact Truth |
|---|---|---|---|---|---|
| **INTEG-01** | Entry Gate | Invalid plant audit | `Failed` | `activation_physics_asset_contract_violation` | `physics_asset_contract_valid` = `false` |
| **INTEG-02A** | Ready State | Waiting for stance | `BalanceActivation_Ready` | `terminal_reason` = `nullptr` | `hold_duration_sec` = 0.0 |
| **INTEG-02B1** | Ready State | CMC active during Ready | `Failed` | `activation_capsule_contract_violation` | `cmc_is_active` = `true` |
| **INTEG-02B2** | Ready State | CMC tick enabled during Ready | `Failed` | `activation_capsule_contract_violation` | `cmc_tick_enabled` = `true` |
| **INTEG-02B3** | Ready State | Capsule collision not disabled | `Failed` | `activation_capsule_contract_violation` | `capsule_collision_enabled` != `NoCollision` |
| **INTEG-02B4** | Ready State | UpdatedComponent still owned | `Failed` | `activation_capsule_contract_violation` | `cmc_updated_component_is_null` = `false` |
| **INTEG-03** | BlendIn State | Continuity breach | `Failed` | `activation_continuous_simulation_lost` | `physical_continuity_validator_passed` = `false`, `control_alpha` < 1.0 |
| **INTEG-04** | Validate State | Airborne breach | `Failed` | `activation_support_failure` | `support_mode` = `Airborne`, `support_gap_timer_ms` > 100 |
| **INTEG-05** | Validate State | Proxy drift breach | `Failed` | `activation_proxy_outside_support_region` | `proxy_inside_hull` = `false`, `proxy_outside_hull_duration_ms` > 100.0, `terminal_reason` = `activation_proxy_outside_support_region` |
| **INTEG-06** | Validate State | Churn Hz breach | `Failed` | `activation_instability_threshold_breach` | `support_churn_hz` > 12.0 |
| **INTEG-07** | Validate State | Authority conflict | `Failed` | `activation_authority_conflict` | `contamination_class` in {canonical_set}, `contamination_source_body` != "", `contamination_source_subsystem` != "", `terminal_reason` = `activation_authority_conflict` |
| **INTEG-08** | Standing Target | Full success | `BalanceActive_Standing` | `terminal_reason` = `nullptr` | `hold_duration_sec` >= 3.0 |

## 5. End-to-End Smoke Tests (Layer 4)

| Test ID | Target | Scenario | Pass Criteria |
|---|---|---|---|
| **SMOKE-01** | `BalanceModeSmoke` | Clean run | `terminal_reason` = `nullptr` AND `BalanceActive_Standing` |
| **SMOKE-02** | Regression | Plant breach | `terminal_reason` = `activation_physics_asset_contract_violation` |
| **SMOKE-03** | Regression | Capsule breach | `terminal_reason` = `activation_capsule_contract_violation` |
| **SMOKE-04** | Regression | Authority breach | `terminal_reason` = `activation_authority_conflict` |
| **SMOKE-05** | Artifact audit | Terminal failure | `terminal_reason` is canonical, `terminal_substep_timestamp` emitted, `terminal_frame_artifact` emitted, all reason-specific forensic fields populated |
