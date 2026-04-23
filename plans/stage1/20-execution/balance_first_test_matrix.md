# Balance-First Test Matrix

## 1. Pure Logic Unit Tests (Layer 1)

| Test ID | Target | Scenario | Trigger | Expected Artifact Fields |
|---|---|---|---|---|
| **LOGIC-01** | `ExtractPatchHull` | Valid 2D points | `NumPoints > 2` | `support_hull_area_cm2` > 0 |
| **LOGIC-02** | `ExtractPatchHull` | Points on a line | `Collinear points` | `support_hull_area_cm2` = 0 |
| **LOGIC-03** | `ExtractPatchHull` | Empty points | `NumPoints = 0` | `support_hull_area_cm2` = 0 |
| **LOGIC-04** | `ClassifySupportMode` | Both feet down | `Left=true, Right=true` | `support_mode` = "TwoFootStable" |
| **LOGIC-05** | `ClassifySupportMode` | One foot down | `Left=true, Right=false` | `support_mode` = "SingleFootSurvival" |
| **LOGIC-06** | `ClassifySupportMode` | Both feet up, gap < max | `Both=false, Timer < limit` | `support_mode` = "TransientRecovery" |
| **LOGIC-07** | `ClassifySupportMode` | Both feet up, gap > max | `Both=false, Timer > limit` | `support_mode` = "Airborne" |
| **LOGIC-08** | `AdjudicateProxy` | Proxy inside hull | `Inside polygon` | `proxy_inside_hull` = true, `proxy_drift_timer_ms` = 0 |
| **LOGIC-09** | `AdjudicateProxy` | Proxy outside hull | `Outside polygon` | `proxy_inside_hull` = false, `proxy_drift_timer_ms` > 0 |
| **LOGIC-10** | `AdjudicateProxy` | No support hull | `SideCount = 0` | `proxy_inside_hull` = null |
| **LOGIC-11** | `CalculateChurnHz` | 5 transitions in 1.0s | `5 events / 1.0s` | `support_churn_hz` = 5.0 |

## 2. Validator Contract Tests (Layer 2)

| Test ID | Target | Scenario | Expected Reason | Expected Fields |
|---|---|---|---|---|
| **VALID-01** | `ValidateContinuity` | Velocity jump | `activation_continuous_simulation_lost` | `terminal_reason` = reason |
| **VALID-02** | `ValidateContinuity` | Physics disabled | `activation_continuous_simulation_lost` | `terminal_reason` = reason |
| **VALID-03** | `ValidateCapsule` | Actor moved | `activation_capsule_contract_violation` | `terminal_reason` = reason |
| **VALID-04** | `ValidateCapsule` | CMC active | `activation_capsule_contract_violation` | `terminal_reason` = reason |
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

| Test ID | Target | Scenario | Trigger | Expected State |
|---|---|---|---|---|
| **INTEG-01** | `Ready` State | Invalid mass | `Mass delta > limit` | `Ready` (No transition) |
| **INTEG-02** | `Ready` State | Overlapping capsule | `Overlap > 0` | `Ready` (No transition) |
| **INTEG-03** | `BlendIn` State | Continuity breach | `Jump at alpha=0.5` | `FailStopped` |
| **INTEG-04** | `Validate` State | Airborne breach | `Gap > max` | `FailStopped` |
| **INTEG-05** | `Validate` State | Proxy drift breach | `Drift > max` | `FailStopped` |
| **INTEG-06** | `Validate` State | Churn Hz breach | `Hz > limit` | `FailStopped` |
| **INTEG-07** | `Standing` Target | Full success | `Hold time > 3.0s` | `BalanceActive_Standing` |

## 5. End-to-End Smoke Tests (Layer 4)

| Test ID | Target | Scenario | Pass Criteria |
|---|---|---|---|
| **SMOKE-01** | `BalanceModeSmoke` | Clean run | `terminal_reason` = `nullptr` AND `BalanceActive_Standing` |
| **SMOKE-02** | `Regression` | Plant breach | `terminal_reason` = `activation_physics_asset_contract_violation` |
| **SMOKE-03** | `Regression` | Authority breach | `terminal_reason` = `activation_authority_conflict` |
