# Balance-First Test Matrix

## 1. Pure Logic Unit Tests (Layer 1)

| Test ID | Target | Scenario | Expected Outcome | Expected Artifact Fields |
|---|---|---|---|---|
| **LOGIC-01** | `ExtractPatchHull` | Valid 2D points | Convex hull with correct area | `support_hull_area_cm2` > 0 |
| **LOGIC-02** | `ExtractPatchHull` | Points on a line / Zero area | Hull with zero area | `support_hull_area_cm2` = 0 |
| **LOGIC-03** | `ExtractPatchHull` | Empty points | Null/Zero area handling | `support_hull_area_cm2` = 0 |
| **LOGIC-04** | `ClassifySupportMode` | Both feet true | `TwoFootStable` | `support_mode` = "TwoFootStable" |
| **LOGIC-05** | `ClassifySupportMode` | One foot true | `SingleFootSurvival` | `support_mode` = "SingleFootSurvival" |
| **LOGIC-06** | `ClassifySupportMode` | Zero feet, gap < max | `TransientRecovery` | `support_mode` = "TransientRecovery" |
| **LOGIC-07** | `ClassifySupportMode` | Zero feet, gap > max | `Airborne` | `support_mode` = "Airborne" |
| **LOGIC-08** | `AdjudicateProxy` | Proxy inside hull | Drift timer = 0 | `proxy_inside_hull` = true, `proxy_drift_timer_ms` = 0 |
| **LOGIC-09** | `AdjudicateProxy` | Proxy outside hull | Drift timer increments | `proxy_inside_hull` = false, `proxy_drift_timer_ms` > 0 |
| **LOGIC-10** | `AdjudicateProxy` | No support hull | Proxy test skipped | `proxy_inside_hull` = null |
| **LOGIC-11** | `CalculateChurnHz` | 5 transitions in 1.0s | Churn = 5.0 Hz | `support_churn_hz` = 5.0 |

## 2. Validator Contract Tests (Layer 2)

| Test ID | Target | Scenario | Expected Terminal Reason | Associated Fields |
|---|---|---|---|---|
| **VALID-01** | `ValidateContinuity` | Velocity jump over limit | `activation_physical_continuity_breach` | `terminal_failure_reason` |
| **VALID-02** | `ValidateContinuity` | Simulation disabled | `activation_physical_continuity_breach` | `terminal_failure_reason` |
| **VALID-03** | `ValidateCapsule` | Actor transform delta | `activation_capsule_contract_breach` | `terminal_failure_reason` |
| **VALID-04** | `ValidateCapsule` | CMC (Movement) active | `activation_capsule_contract_breach` | `terminal_failure_reason` |
| **VALID-05** | `ValidatePlant` | Skeleton mismatch | `activation_plant_contract_breach` | `plant_failure_class` = "Topology" |
| **VALID-06** | `ValidatePlant` | Bone length drift | `activation_plant_contract_breach` | `plant_failure_class` = "StructuralIntegrity" |
| **VALID-07** | `ValidatePlant` | Mass mutation | `activation_plant_contract_breach` | `plant_failure_class` = "MassDistribution", `plant_failure_field` = "mass" |
| **VALID-08** | `ValidateAuthority` | External write detected | `activation_authority_contamination` | `terminal_failure_reason` |

## 3. Runtime Integration Tests (Layer 3)

| Test ID | Target | Scenario | Expected Outcome |
|---|---|---|---|
| **INTEG-01** | `Ready` Gate | Invalid mass on entry | Stay in `Ready`, do not `BlendIn` |
| **INTEG-02** | `Ready` Gate | Capsule overlapping world | Stay in `Ready`, do not `BlendIn` |
| **INTEG-03** | `BlendIn` Rollout | Continuity breach at 50% | Fail to `FailStopped` |
| **INTEG-04** | `Validate` Hold | Support loss (Airborne) | Fail to `FailStopped` |
| **INTEG-05** | `Validate` Hold | Proxy drift breach | Fail to `FailStopped` |
| **INTEG-06** | `Validate` Hold | Churn Hz breach | Fail to `FailStopped` |
| **INTEG-07** | `Standing` Target | 3.0s sustained balance | Transition to `BalanceActive_Standing` |

## 4. End-to-End Smoke Tests (Layer 4)

| Test ID | Target | Scenario | Pass Criteria |
|---|---|---|---|
| **SMOKE-01** | `BalanceModeSmoke` | Normal activation | Reach `BalanceActive_Standing` |
| **SMOKE-02** | `Regression` | Deliberate plant breach | Fail with `activation_plant_contract_breach` |
| **SMOKE-03** | `Regression` | Deliberate authority breach | Fail with `activation_authority_contamination` |
