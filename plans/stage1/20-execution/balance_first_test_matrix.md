# Balance-First Test Matrix

## 1. Pure Logic Unit Tests (Layer 1)

| Test ID | Target | Scenario | Trigger | Expected Artifact Fields |
|---|---|---|---|---|
| **LOGIC-01** | `ExtractPatchHull` | Valid 2D points | `NumPoints > 2` | `support_hull_area_cm2` > 0 |
| **LOGIC-02** | `ExtractPatchHull` | Points on a line | `Collinear points` | `support_hull_area_cm2` = 0 |
| **LOGIC-03** | `ExtractPatchHull` | Empty points | `NumPoints = 0` | `support_hull_area_cm2` = 0 |
| **LOGIC-04** | `ClassifySupportMode` | Both feet down | `Left=true, Right=true` | `support_mode` = "TwoFootStable" |
| **LOGIC-05** | `ClassifySupportMode` | One foot down | `Left=true, Right=false` | `support_mode` = "SingleFootSurvival" |
| **LOGIC-06** | `ClassifySupportMode` | Both feet up, gap < max | `Both=false, Timer < limit` | `support_mode` = "TransientRecovery", `support_gap_timer_ms` > 0 |
| **LOGIC-07** | `ClassifySupportMode` | Both feet up, gap > max | `Both=false, Timer > limit` | `support_mode` = "Airborne", `support_gap_timer_ms` > 100 |
| **LOGIC-08** | `AdjudicateProxy` | Proxy inside hull | `Inside polygon` | `proxy_inside_hull` = true |
| **LOGIC-09** | `AdjudicateProxy` | Proxy outside hull | `Outside polygon` | `proxy_inside_hull` = false |
| **LOGIC-10** | `AdjudicateProxy` | No support hull | `SideCount = 0` | `proxy_inside_hull` = null |
| **LOGIC-11** | `CalculateChurnHz` | 5 transitions in 1.0s | `5 events / 1.0s` | `support_churn_hz` = 5.0 |

## 2. Validator Contract Tests (Layer 2)

| Test ID | Target | Scenario | Expected Reason | Expected Fields |
|---|---|---|---|---|
| **VALID-01A** | `ValidateContinuity` | Physics disabled | `activation_continuous_simulation_lost` | `physical_continuity_validator_passed` = false |
| **VALID-01B** | `ValidateContinuity` | Pelvis sleep limit | `activation_continuous_simulation_lost` | `physical_continuity_validator_passed` = false |
| **VALID-01C** | `ValidateContinuity` | Body instance loss | `activation_topology_change` | `physical_continuity_validator_passed` = false |
| **VALID-01D** | `ValidateContinuity` | Bookkeeping delta | `nullptr` | `continuity_bookkeeping_mismatch` = true |
| **VALID-03** | `ValidateCapsule` | Actor moved | `activation_capsule_contract_violation` | `terminal_reason` = reason |
| **VALID-04** | `ValidateCapsule` | CMC active | `activation_capsule_contract_violation` | `cmc_is_active` = true |
| **VALID-05** | `ValidatePlant` | Skeleton mismatch | `activation_physics_asset_contract_violation` | `plant_failure_class` = "StaticStructural" |
| **VALID-06** | `ValidatePlant` | Length drift | `activation_physics_asset_contract_violation` | `plant_failure_class` = "StaticStructural" |
| **VALID-07** | `ValidatePlant` | Mass mutation | `activation_physics_asset_contract_violation` | `plant_failure_class` = "Mutation", `plant_failure_field` = "mass" |
| **VALID-08** | `ValidateAuthority` | External write | `activation_authority_conflict` | `terminal_reason` = reason |

## 3. Arbitration Logic Tests (Layer 2.5)

| Test ID | Target | Scenario | Trigger | Expected Outcome |
|---|---|---|---|---|
| **ARBIT-01** | `ArbitrateFailure` | Multiple failures | `Plant + Support` | `activation_physics_asset_contract_violation` |
| **ARBIT-02** | `ArbitrateFailure` | Multiple failures | `Support + Proxy` | `activation_support_failure` |
| **ARBIT-03** | `ArbitrateFailure` | No failures | `All green` | `nullptr` |

## 4. Runtime Integration Tests (Layer 3)

| Test ID | Target | Scenario | Expected State | Expected Reason | Expected Artifact Truth |
|---|---|---|---|---|---|
| **INTEG-01** | `Entry` Gate | Invalid mass | `No Entry` | `activation_physics_asset_contract_violation` | `physics_asset_contract_valid` = false, `plant_failure_class` = "Mutation" |
| **INTEG-02A** | `Ready` State | Waiting for stance | `BalanceActivation_Ready` | `nullptr` | `hold_duration_sec` = 0.0, `terminal_reason` = null |
| **INTEG-02B** | `Ready` State | CMC Reclaim | `FailStopped` | `activation_capsule_contract_violation` | `cmc_is_active` = true |
| **INTEG-03** | `BlendIn` State | Continuity breach | `FailStopped` | `activation_continuous_simulation_lost` | `physical_continuity_validator_passed` = false, `control_alpha` < 1.0 |
| **INTEG-04** | `Validate` State | Airborne breach | `FailStopped` | `activation_support_failure` | `support_mode` = "Airborne", `support_gap_timer_ms` > 100 |
| **INTEG-05** | `Validate` State | Proxy drift breach | `FailStopped` | `activation_proxy_outside_support_region` | `proxy_inside_hull` = false |
| **INTEG-06** | `Validate` State | Churn Hz breach | `FailStopped` | `activation_instability_threshold_breach` | `support_churn_hz` > 12.0 |
| **INTEG-07** | `Standing` Target | Full success | `BalanceActive_Standing` | `nullptr` | `hold_duration_sec` >= 3.0, `terminal_reason` = null |

## 5. End-to-End Smoke Tests (Layer 4)

| Test ID | Target | Scenario | Pass Criteria |
|---|---|---|---|
| **SMOKE-01** | `BalanceModeSmoke` | Clean run | `terminal_reason` = `nullptr` AND `BalanceActive_Standing` |
| **SMOKE-02** | `Regression` | Plant breach | `terminal_reason` = `activation_physics_asset_contract_violation` |
| **SMOKE-03** | `Regression` | Authority breach | `terminal_reason` = `activation_authority_conflict` |
