# Truth Model For Continuous Balance

## Purpose

This document is the **sole authoritative owner** of failure truth for the continuous-balance bridge. It defines exactly what constitutes a successful run, how failure is detected, and how competing failure reasons are adjudicated.

It owns exclusively:
- **Terminal-reason arbitration** (Master Precedence)
- **Continuity validation** (Body Instance/Simulate Physics truth)
- **Raw-vs-bookkeeping classification**
- **Support-truth precedence** (Plantar Hull vs Contamination)
- **Pose/reference mismatch escalation**

## Primary Physical Signal Families

1. raw body continuity
2. contact persistence and support truth
3. root pose and root tilt
4. COM behavior or support proxy
5. body angular and linear stability
6. sustained hold duration
7. controller pose fidelity (reference mismatch)

## Source-Of-Truth Precedence

Use this precedence order whenever observables disagree:

1. **Raw body continuity and raw contact state** (Chaos Ground Truth)
2. **Derived physical metrics** (Tilt, Proxy, Stability, Mismatch)
3. **Bookkeeping state** (Modifier records)
4. **Declared intent** (Bridge state)
5. **Shell state** (Diagnostic helpers)

---

## Terminal Reason Arbitration

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
| **13** | **Authority/Ownership** | `activation_authority_conflict` | [continuous_balance_truth_model.md](continuous_balance_truth_model.md) |
| **14** | **Time/Duration** | `activation_standing_validation_timeout` | [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md) |

### General Arbitration Rule

1.  **Temporal Precedence (Substep-Level)**: The reason whose triggering condition was observed at the **earliest substep-level timestamp** wins.
2.  **Rank Precedence (Simultaneous)**: If multiple conditions are observed in the **exact same substep**, the reason with the **highest Rank (lowest number)** in the table above wins.
3.  **Co-Terminal Record**: All other co-occurring terminal conditions detected in the frame must be recorded in the `co_terminal_reasons` array.

---

## Continuity Validation Algorithm

The implementation must use this deterministic logic to validate continuous simulation.

### Validator Logic (Per-Frame)
A frame is **Continuity Clean** only if:
1.  **Simulate-Physics Rule**: `IsInstanceSimulatingPhysics()` is `true` for all bodies in the Critical Chain and Support Set.
2.  **Body Validity Rule**: `BodyInstance->IsValid()` is `true` for all bodies in those sets.
3.  **Topology Rule**: No `FPhysicsBodyInstance` pointer changes (recreation/replacement) since activation start.
4.  **Pelvis Wake Rule**: The pelvis must not be in a **SLEEP** state for more than the **Pelvis Sleep Limit** contiguously during active modes.

### Classification Rules
- **Pelvis Sleep Violation**: If Rule 4 is breached (sleep > Pelvis Sleep Limit), emit `activation_continuous_simulation_lost`.
- **Support Sleep Churn**: If support-body sleep/wake transitions exceed the **Support Churn** limit, escalate to `activation_unstable_gain_or_damping`.
- **Support Sleep Dominance**: If total cumulative sleep time for any support-set body exceeds the **Support Sleep Dominance** threshold, emit `activation_continuous_simulation_lost`.
- **Raw Simulation Loss**: If Rule 1 or 2 fail: Emit `activation_continuous_simulation_lost`.
- **Topology Change**: If Rule 3 fails: Emit `activation_topology_change`.
- **Raw-vs-Bookkeeping**: If the bridge's internal bookkeeping (modifiers) disagrees with raw body state, the **Raw State** wins. Disagreement is logged as `continuity_bookkeeping_mismatch` (diagnostic only) provided the raw rules are satisfied.

---

## Support Truth & One-Foot Policy

### One-Foot Support Policy (Honest Balance Survival)
- A run remains **Support Valid** even if only **one side** (e.g., `foot_l` or `ball_l`) is in contact, provided the **Support Proxy** remains within that single-foot hull.
- **Justification**: This is an "Honest Balance Survival" benchmark. It proves the policy can physically equilibrate on a reduced support region without help from non-simulated forces.

### Standing Stability Grades
To prevent "One-Foot Survival" from being indistinguishable from "Neutral Standing", the implementation must classify the support state into these grades:

1.  **Two-Foot Stable**: Both sides (`foot_l/ball_l` AND `foot_r/ball_r`) maintain persistent contact (> 90% of frame substeps).
2.  **Single-Foot Survival**: Only one side maintains persistent contact, but the **Support Proxy** remains within that single-foot hull.
3.  **Transient Recovery**: A side loses contact but regains it within the **Support Gap (Max)** limit.
4.  **Airborne (Terminal)**: Both sides lose contact for more than the gap limit.

**Reporting**: Every 30Hz artifact must record the dominant `support_mode` for the sample window.

---

## Material Contamination Rules

Contamination logic and authority conflict rules are defined exclusively in [authority_matrix.md](authority_matrix.md). Any breach of these rules triggers an `activation_authority_conflict`.

---

## Controller Stability Rules

Stability failures in the control layer trigger terminal termination before physical collapse occurs.

### 1. Target Discontinuity
- The run must fail on `activation_target_discontinuity` if the rebase delta at blend-start exceeds the **Target Discontinuity** threshold.

### 2. Unstable Gain or Damping
- The run must fail on `activation_unstable_gain_or_damping` if the implementation detects:
  - Any `NaN` or `Inf` in the published control targets, gains, or forces.
  - Explosive feedback that materially exceeds the audited plant baseline.
  - High-frequency oscillation in control effort that materiality destabilizes the truth set.

---

## Pose Fidelity (Reference Mismatch)

### Mismatch Terminalization
The run fails on `activation_pose_reference_mismatch` if:
1.  Any body in the balance-critical chain exceeds the **Mismatch (Max Body)** threshold for more than the 100ms grace period.
2.  The RMS mismatch across the entire chain exceeds the **Mismatch (RMS Chain)** threshold for more than the 100ms grace period.
3.  Target discontinuity at blend start exceeds the **Target Discontinuity** threshold.

---

## COM / Support Proxy Definition

### Proxy definition
The `V0` support proxy is the world-space planar centroid of:
- `pelvis`, `thigh_l`, `thigh_r`, `spine_01`

### Support region definition
The `V0` support region is the convex hull of all qualifying `foot_*` and `ball_*` manifold points from the final qualifying Chaos substep.

**Calf Limitation**: Contact on `calf_l` or `calf_r` is strictly defined as contamination in [authority_matrix.md](authority_matrix.md). It is NOT valid support truth and cannot be downgraded to diagnostic-only.
