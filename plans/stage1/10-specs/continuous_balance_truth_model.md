# Continuous Balance Truth Model

## Purpose

This document is the **sole authoritative owner** of failure truth for the continuous-balance bridge. It defines exactly what constitutes a successful run, how failure is detected, and how competing failure reasons are adjudicated.

It owns exclusively:
- **Terminal-reason arbitration** (Master Precedence)
- **Continuity validation** (Body Instance/Simulate Physics truth)
- **Raw-vs-bookkeeping classification**
- **Support-truth precedence** (Plantar Hull vs Contamination)
- **Pose/reference mismatch escalation**

## Primary Physical Signal Families

The bridge monitors three primary signal families to determine balance truth:
1.  **Raw Body Continuity**: Is the body a valid instance, is it simulating, and has its topology remained stable?
2.  **Support Integrity**: Is the COM proxy inside the debounced plantar support hull?
3.  **Control Stability**: Are the control gains stable and is the pose mismatch within the allowed tolerance?

## Arbitration & Precedence

When multiple failure conditions are simultaneously active, the implementation must use this deterministic arbitration logic.

### Master Precedence Table

| Rank | Reason Class | Canonical `terminal_reason` | Authoritative Contract |
| :--- | :--- | :--- | :--- |
| **1** | **Plant Contract** | `activation_physics_asset_contract_violation` | [physics_asset_contract.md](physics_asset_contract.md) |
| **2** | **Capsule Contract** | `activation_capsule_contract_violation` | [character_capsule_contract.md](character_capsule_contract.md) |
| **3** | **Raw Continuity** | `activation_topology_change` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **4** | **Raw Continuity** | `activation_continuous_simulation_lost` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **5** | **Support Truth** | `activation_support_failure` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **6** | **Support Truth** | `activation_proxy_outside_support_region` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **7** | **Controller Stability** | `activation_target_discontinuity` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **8** | **Controller Stability** | `activation_unstable_gain_or_damping` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **9** | **Controller Stability** | `activation_instability_threshold_breach` | [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md) |
| **10** | **Pose/Reference** | `activation_pose_reference_mismatch` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **11** | **Authority/Ownership** | `activation_movement_reclaim` | [authority_matrix.md](authority_matrix.md) |
| **12** | **Authority/Ownership** | `activation_shell_helper_violation` | [authority_matrix.md](authority_matrix.md) |
| **13** | **Authority/Ownership** | `activation_authority_conflict` | [authority_matrix.md](authority_matrix.md) |
| **14** | **Time/Duration** | `activation_standing_validation_timeout` | [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md) |

### General Arbitration Rule

1.  **Temporal Precedence (Substep-Level)**: The reason whose triggering condition was observed at the **earliest substep-level timestamp** wins.
2.  **Rank Precedence (Simultaneous)**: If multiple conditions are observed in the **exact same substep**, the reason with the **highest Rank (lowest number)** in the table above wins.
3.  **Co-Terminal Record**: All other co-occurring terminal conditions detected in the frame must be recorded in the `co_terminal_reasons` array.

## Continuity Validation Algorithm

The implementation must validate the physical continuity of the truth set on every Chaos substep.

### 1. Instance Integrity
- **Rule**: `IsValidBodyInstance()` must be true for all bodies in the balance-critical chain.
- **Fail Condition**: If any body is destroyed, recreated, or replaced during the attempt.
- **Result**: Emit `activation_topology_change`.

### 2. Simulation Continuity
- **Rule**: `IsInstanceSimulatingPhysics()` must be true for all bodies in the Critical Chain and Support Set.
- **V0 Exception**: Pelvis may enter "Sleep" if and only if it persists for less than the **Pelvis Sleep Limit** (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).
- **Fail Condition**: If simulation is disabled mid-attempt or if sleep exceeds the limit.
- **Result**: Emit `activation_continuous_simulation_lost`.

### 3. Raw-vs-Bookkeeping Classification
- **Rule**: If the bridge's internal bookkeeping (modifiers) disagrees with raw body state, the **Raw State** wins. 
- **Reporting**: Disagreement is logged as `continuity_bookkeeping_mismatch` (diagnostic only).

## Support Truth & One-Foot Policy

### Support failure definition
The run fails on `activation_support_failure` if **any** of these support-truth conditions are breached:
1.  **Support Area Breach**: If `active_support_side_count > 0` AND the total unioned **FrameHullArea** is below the **Support Area (Min)** threshold (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).
2.  **Airborne Gap Breach**: Both side-support states are `false` AND the cumulative airborne duration exceeds the **Support Gap (Max)** (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).

When `active_support_side_count == 0`, support failure is governed exclusively by the **Airborne Gap Breach** rule, not the area test.

### One-Foot Support Policy (Honest Balance Survival)
- A run remains **Support Valid** even if only **one side** (e.g., `foot_l` or `ball_l`) is in contact, provided that:
  1.  The active single-foot hull area exceeds the **Support Area (Min)** threshold (breach emits `activation_support_failure`).
  2.  The **Support Proxy** remains within that single-foot hull (breach exceeding **COM Proxy Drift** emits `activation_proxy_outside_support_region`).
- **Justification**: This is an "Honest Balance Survival" benchmark.

### Standing Stability Grades
The support state is classified at the frame level into these four mutually exclusive grades, as defined by the authoritative priority in [engine_execution_contract.md](engine_execution_contract.md):
1.  **TwoFootStable**
2.  **SingleFootSurvival**
3.  **TransientRecovery**
4.  **Airborne**

## Pose Fidelity (Reference Mismatch)

The run fails on `activation_pose_reference_mismatch` if:
1.  Any body in the balance-critical chain exceeds the **Mismatch (Max Body)** threshold for more than the **Mismatch Grace Period** (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).
2.  The RMS mismatch across the entire chain exceeds the **Mismatch (RMS Chain)** threshold for more than the **Mismatch Grace Period** (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).

The run fails on `activation_target_discontinuity` if the delta at blend start exceeds the **Target Discontinuity** threshold (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).

## Unstable Gain / Damping

The run fails on `activation_unstable_gain_or_damping` if:
1.  The `controller_gain_scale` exceeds the **Controller Gain Scale (Max)** threshold (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).
2.  The `controller_damping_ratio` falls below the **Controller Damping Ratio (Min)** threshold (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).

## Instability & Support Churn

The run fails on `activation_instability_threshold_breach` if:
1.  **Support Churn**: The authoritative `support_churn_hz` exceeds the **Support Churn** limit (See [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)).
2.  **Justification**: High-frequency contact "popping" or jitter indicates an unstable simulation state that falsifies the truth-set support proof.

## COM / Support Proxy Definition

### Proxy definition
The `V0` support proxy is the world-space planar centroid of:
- `pelvis`, `thigh_l`, `thigh_r`, `spine_01`

### Support region definition
The authoritative definition of the support region, including per-body patch reduction, hull construction, and the proxy-vs-hull test, is owned exclusively by [engine_execution_contract.md](engine_execution_contract.md).

**Calf Limitation**: Contact on `calf_l` or `calf_r` is strictly defined as contamination in [authority_matrix.md](authority_matrix.md). It is NOT valid support truth and does not contribute to the support region.

## Material Contamination Rules

Contamination logic and authority conflict rules are defined exclusively in [authority_matrix.md](authority_matrix.md). Any breach of these rules triggers an `activation_authority_conflict`.
