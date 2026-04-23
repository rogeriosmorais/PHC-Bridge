# Balance-First Test Matrix

This document defines the deterministic test inventory for the balance-first rewrite.

## Section 1: Pure Logic Unit Tests (Layer 1)

| Test ID | Purpose | Setup | Trigger | Expected Artifact Fields | Expected terminal_reason |
|---|---|---|---|---|---|
| LOGIC-01 | Support patch hull | N manifold points | `ExtractPatchHull()` | `support_hull_points` | `nullptr` |
| LOGIC-02 | Zero-area patch handling | 1 or 2 collinear points | `ExtractPatchHull()` | `support_hull_area_cm2 == 0` | `nullptr` |
| LOGIC-03 | Frame hull union | L + R hulls | `MergeFrameHulls()` | `support_hull_area_cm2` | `nullptr` |
| LOGIC-04 | Support-mode classification | Side states + gap timer | `ClassifySupportMode()` | `support_mode` | `nullptr` |
| LOGIC-05 | Proxy drift timer | Proxy outside hull | `AdjudicateProxy()` | `proxy_inside_hull == false` | `activation_proxy_outside_support_region` (on limit) |
| LOGIC-06 | Churn rolling-window Hz | N transitions over time | `CalculateChurnHz()` | `support_churn_hz` | `activation_instability_threshold_breach` (on limit) |
| LOGIC-07 | Tie-breaking (Artifact) | Equal durations in window | `ReduceSupportMode()` | `support_mode` (Severity rank) | `nullptr` |
| LOGIC-08 | Terminal-reason precedence | Multiple failure triggers | `ArbitrateFailure()` | `terminal_reason` | Highest rank reason |

## Section 2: Validator Tests (Layer 2)

| Test ID | Purpose | Setup | Trigger | Expected Artifact Fields | Expected terminal_reason |
|---|---|---|---|---|---|
| VALID-01 | Continuity valid case | Smooth velocity/pose | `ValidateContinuity()` | `physical_continuity_validator_passed == true` | `nullptr` |
| VALID-02 | Topology replacement failure | Swap skeleton asset | `ValidatePlant()` | `plant_failure_class == topology_mismatch` | `activation_physics_asset_contract_violation` |
| VALID-03 | Simulation disabled failure | `SetSimulatePhysics(false)` | `ValidateSimState()` | `nullptr` | `activation_continuous_simulation_lost` |
| VALID-04 | Pelvis sleep over-limit | Frozen pelvis > 100ms | `ValidateSimState()` | `nullptr` | `activation_continuous_simulation_lost` |
| VALID-05 | Capsule contract failure | Move capsule world-pos | `ValidateCapsule()` | `nullptr` | `activation_capsule_contract_violation` |
| VALID-06 | Plant mass breach | Drift mass > 5% | `ValidatePlant()` | `mass_drift_total_pct` | `activation_physics_asset_contract_violation` |
| VALID-07 | Contamination (Mesh-wide) | External write to mesh | `ValidateAuthority()` | `mesh_wide_assist_detected == true` | `activation_authority_conflict` |

## Section 3: Integration Tests (Layer 3/4)

| Test ID | Purpose | Setup | Trigger | Expected Artifact Fields | Expected terminal_reason |
|---|---|---|---|---|---|
| INTEG-01 | Ready rejects invalid plant | Breach mass in Ready | `EnterBalanceMode()` | `plant_failure_class` | `activation_physics_asset_contract_violation` |
| INTEG-02 | Ready rejects invalid capsule | Shift capsule in Ready | `EnterBalanceMode()` | `nullptr` | `activation_capsule_contract_violation` |
| INTEG-03 | BlendIn refuses invalid reference | High target discontinuity | `StartBlendIn()` | `target_discontinuity_passed == false` | `activation_pose_reference_mismatch` |
| INTEG-04 | Validate fails on support loss | Lose all contact > Max Gap | `HoldValidate()` | `support_mode == Airborne` | `activation_support_failure` |
| INTEG-05 | Validate fails on proxy drift | Proxy outside for 100ms | `HoldValidate()` | `proxy_inside_hull == false` | `activation_proxy_outside_support_region` |
| INTEG-06 | Validate fails on churn breach | High-freq contact chatter | `HoldValidate()` | `support_churn_hz > 12.0` | `activation_instability_threshold_breach` |
| INTEG-07 | Standing success 3.0s | Honest stand on flat ground | `HoldValidate()` | `hold_duration_sec >= 3.0` | `nullptr` |

## Section 4: Must-Fail Regression Tests (Layer 4)

| Test ID | Purpose | Setup | Trigger | Expected Artifact Fields | Expected terminal_reason |
|---|---|---|---|---|---|
| REG-01 | Capsule breach | Move capsule during active | `ManualBreach()` | `nullptr` | `activation_capsule_contract_violation` |
| REG-02 | Plant mutation breach | Change mass during active | `ManualBreach()` | `mass_drift_total_pct` | `activation_physics_asset_contract_violation` |
| REG-03 | Calf contamination breach | Contact on `calf_l` | `ManualBreach()` | `calf_contact_terminal == true` | `activation_authority_conflict` |
| REG-04 | Simulate-physics loss | Disable sim on pelvis | `ManualBreach()` | `nullptr` | `activation_continuous_simulation_lost` |
| REG-05 | Proxy breach | Teleport proxy outside | `ManualBreach()` | `proxy_inside_hull == false` | `activation_proxy_outside_support_region` |
| REG-06 | Movement reclaim breach | Interference from CMC | `ManualBreach()` | `movement_reclaim_count > 0` | `activation_movement_reclaim` |
