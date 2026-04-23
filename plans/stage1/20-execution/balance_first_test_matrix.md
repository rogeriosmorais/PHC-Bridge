# Balance-First Test Matrix

## 1. Pure Logic Unit Tests (Layer 1)

| Test ID | Target | Scenario | Trigger | Expected Output |
|---|---|---|---|---|
| **LOGIC-01** | `ExtractPatchHull` | Valid 2D points | `NumPoints > 2` | `support_hull_area_cm2` > 0 |
| **LOGIC-02** | `ExtractPatchHull` | Points on a line | `Collinear points` | `support_hull_area_cm2` = 0 |
| **LOGIC-03** | `ExtractPatchHull` | Empty points | `NumPoints = 0` | `support_hull_area_cm2` = 0 |
| **LOGIC-04** | `BuildFrameHull` | Two valid body patches | Union vertices from both sides | frame hull area is computed from the union |
| **LOGIC-05** | `ClassifySupportMode` | Both feet down | `Left=true, Right=true` | `support_mode` = `TwoFootStable` |
| **LOGIC-06** | `ClassifySupportMode` | One foot down | `Left=true, Right=false` | `support_mode` = `SingleFootSurvival` |
| **LOGIC-07** | `ClassifySupportMode` | Both feet up, gap <= max | `Both=false, Timer <= limit` | `support_mode` = `TransientRecovery` |
| **LOGIC-08** | `ClassifySupportMode` | Both feet up, gap > max | `Both=false, Timer > limit` | `support_mode` = `Airborne` |
| **LOGIC-09** | `AdjudicateProxy` | Proxy inside hull | `Inside polygon` | `proxy_inside_hull` = `true`, drift timer reset |
| **LOGIC-10** | `AdjudicateProxy` | Proxy outside hull under limit | `Outside polygon, Timer <= limit` | `proxy_inside_hull` = `false`, no terminal reason |
| **LOGIC-11** | `AdjudicateProxy` | Proxy outside hull over limit | `Outside polygon, Timer > limit` | `activation_proxy_outside_support_region` |
| **LOGIC-12** | `AdjudicateProxy` | No support hull | `SideCount = 0` | `proxy_inside_hull` = `null`, proxy test skipped |
| **LOGIC-13** | `CalculateChurnHz` | 5 transitions in 1.0s | `5 events / 1.0s` | `support_churn_hz` = 5.0 |
| **LOGIC-14** | `ReduceSupportModeForReportWindow` | 30 Hz tie-break | Equal durations | severity tie-break: `Airborne` > `TransientRecovery` > `SingleFootSurvival` > `TwoFootStable` |

## 2. Validator Contract Tests (Layer 2)

| Test ID | Target | Scenario | Expected Reason | Required Artifact Truth |
|---|---|---|---|---|
| **VALID-01A** | `ValidateContinuity` | Physics disabled | `activation_continuous_simulation_lost` | `physical_continuity_validator_passed` = `false` |
| **VALID-01B** | `ValidateContinuity` | Pelvis sleep limit exceeded | `activation_continuous_simulation_lost` | sleep duration > 100ms |
| **VALID-01C** | `ValidateContinuity` | Body instance loss | `activation_topology_change` | topology counter increments |
| **VALID-01D** | `ValidateContinuity` | Bookkeeping delta only | `null` | `continuity_bookkeeping_mismatch` = `true` |
| **VALID-02A** | `ValidateCapsule` | Actor moved | `activation_capsule_contract_violation` | `capsule_lock_delta_cm` > 0.01 |
| **VALID-02B** | `ValidateCapsule` | Capsule collision active | `activation_capsule_contract_violation` | `capsule_collision_enabled` != `NoCollision` |
| **VALID-02C** | `ValidateCapsule` | CMC active/ticking | `activation_capsule_contract_violation` | `cmc_is_active` or `cmc_tick_enabled` = `true` |
| **VALID-02D** | `ValidateCapsule` | UpdatedComponent still owned | `activation_capsule_contract_violation` | `cmc_updated_component_is_null` = `false` |
| **VALID-03A** | `ValidatePlant` | Skeleton mismatch | `activation_physics_asset_contract_violation` | `plant_failure_class` = `StaticStructural` |
| **VALID-03B** | `ValidatePlant` | Length or axis drift | `activation_physics_asset_contract_violation` | static audit field is populated |
| **VALID-03C** | `ValidatePlant` | Mass mutation | `activation_physics_asset_contract_violation` | `plant_failure_class` = `Mutation`, `plant_failure_field` = `mass` |
| **VALID-03D** | `ValidatePlant` | Physics asset swap | `activation_physics_asset_contract_violation` | baseline identity mismatch recorded |
| **VALID-04A** | `ValidateAuthority` | Mesh-wide assist | `activation_authority_conflict` | `contamination_class` = `mesh_wide_assist` |
| **VALID-04B** | `ValidateAuthority` | Non-critical body assist | `activation_authority_conflict` | `contamination_class` = `non_critical_body_assist` |
| **VALID-04C** | `ValidateAuthority` | Calf or excluded world brace | `activation_authority_conflict` | `contamination_class` = `excluded_body_world_brace` |
| **VALID-04D** | `ValidateAuthority` | Global blend/kinematic assist | `activation_authority_conflict` | `contamination_class` = `global_blend_or_kinematic_assist` |
| **VALID-05A** | `ValidateControllerStability` | Target jump at blend start | `activation_target_discontinuity` | target delta > 15 deg |
| **VALID-05B** | `ValidateControllerStability` | Unstable gains/damping | `activation_unstable_gain_or_damping` | gain/damping breach field populated |
| **VALID-05C** | `ValidateControllerStability` | Root tilt or angular speed breach | `activation_instability_threshold_breach` | threshold metric exceeds limit |
| **VALID-05D** | `ValidateControllerStability` | Pose mismatch over grace | `activation_pose_reference_mismatch` | mismatch metric exceeds limit for > 0.2s |
| **VALID-05E** | `ValidateControllerStability` | Standing validation timeout | `activation_standing_validation_timeout` | hold never reaches 3.0s before timeout |
| **VALID-06A** | `ValidateMovementReclaim` | CMC correction path runs | `activation_movement_reclaim` | `movement_reclaim_count` increments |
| **VALID-06B** | `ValidateShellHelper` | Shell helper writes during activation | `activation_shell_helper_violation` | `shell_helper_used_count` increments |

## 3. Arbitration Logic Tests (Layer 2.5)

| Test ID | Target | Scenario | Trigger | Expected Outcome |
|---|---|---|---|---|
| **ARBIT-01** | `ArbitrateFailure` | Simultaneous Plant + Support | Same substep | `activation_physics_asset_contract_violation` |
| **ARBIT-02** | `ArbitrateFailure` | Simultaneous Support + Proxy | Same substep | `activation_support_failure` |
| **ARBIT-03** | `ArbitrateFailure` | Earlier lower-rank reason | Authority conflict before plant breach | earliest substep reason wins |
| **ARBIT-04** | `ArbitrateFailure` | Co-terminal failures | Multiple failures in one frame | winning reason plus `co_terminal_reasons[]` |
| **ARBIT-05** | `ArbitrateFailure` | No failures | All green | `null` |

## 4. Runtime Integration Tests (Layer 3)

| Test ID | Target | Scenario | Expected State | Expected Reason | Expected Artifact Truth |
|---|---|---|---|---|---|
| **INTEG-01** | Entry Gate | Invalid plant audit | `Failed` | `activation_physics_asset_contract_violation` | `physics_asset_contract_valid` = `false` |
| **INTEG-02A** | Ready State | Waiting for stance | `BalanceActivation_Ready` | `null` | `hold_duration_sec` = 0.0 |
| **INTEG-02B** | Ready State | Capsule/CMC breach | `Failed` | `activation_capsule_contract_violation` | capsule/CMC fields populated |
| **INTEG-03** | BlendIn State | Continuity breach | `Failed` | `activation_continuous_simulation_lost` | `physical_continuity_validator_passed` = `false`, `control_alpha` < 1.0 |
| **INTEG-04** | Validate State | Airborne breach | `Failed` | `activation_support_failure` | `support_mode` = `Airborne`, `support_gap_timer_ms` > 100 |
| **INTEG-05** | Validate State | Proxy drift breach | `Failed` | `activation_proxy_outside_support_region` | `proxy_inside_hull` = `false` |
| **INTEG-06** | Validate State | Churn Hz breach | `Failed` | `activation_instability_threshold_breach` | `support_churn_hz` > 12.0 |
| **INTEG-07** | Validate State | Authority conflict | `Failed` | `activation_authority_conflict` | contamination fields populated without log correlation |
| **INTEG-08** | Standing Target | Full success | `BalanceActive_Standing` | `null` | `hold_duration_sec` >= 3.0 |

## 5. End-to-End Smoke Tests (Layer 4)

| Test ID | Target | Scenario | Pass Criteria |
|---|---|---|---|
| **SMOKE-01** | `BalanceModeSmoke` | Clean run | `terminal_reason` = `null` AND `BalanceActive_Standing` |
| **SMOKE-02** | Regression | Plant breach | `terminal_reason` = `activation_physics_asset_contract_violation` |
| **SMOKE-03** | Regression | Capsule breach | `terminal_reason` = `activation_capsule_contract_violation` |
| **SMOKE-04** | Regression | Authority breach | `terminal_reason` = `activation_authority_conflict` |
| **SMOKE-05** | Artifact audit | Terminal failure | required forensic fields populated for the winning reason |
