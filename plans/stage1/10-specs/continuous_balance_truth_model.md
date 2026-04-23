# Truth Model For Continuous Balance

## Purpose

This document is the **sole authoritative owner** of failure truth for the continuous-balance bridge. It defines exactly what constitutes a successful run, how failure is detected, and how competing failure reasons are adjudicated.

Core rule:
- success is sustained physical stability under continuous simulation
- no phase completion, shell status, or compatibility label can substitute for that

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
4.  **Pelvis Wake Rule**: The pelvis body must remain **AWAKE** during `Validate` and `Standing` modes.

### Classification Rules
- If Rule 1, 2, or 4 fail: Emit `activation_continuous_simulation_lost`.
- If Rule 3 fails specifically because of a pointer change: Emit `activation_topology_change`.
- **Raw-vs-Bookkeeping**: If the bridge's internal bookkeeping (modifiers) disagrees with raw body state, the **Raw State** wins. Disagreement is logged as `continuity_bookkeeping_mismatch` (diagnostic only) provided the raw rules are satisfied.

---

## Support Truth & One-Foot Policy

### One-Foot Support Policy (Honest Balance Survival)
- A run remains **Support Valid** even if only **one side** (e.g., `foot_l` or `ball_l`) is in contact, provided the **Support Proxy** remains within that single-foot hull.
- **Justification**: This is an "Honest Balance Survival" benchmark. It proves the policy can physically equilibrate on a reduced support region without help from non-simulated forces.

### Support Failure Criteria
1.  **Support-Loss Gap**: Terminal if both sides lose contact for more than `100 ms` (debounced).
2.  **Proxy Outside Region**: Terminal if the COM proxy stays outside the active support hull for more than `100 ms`.
3.  **Churn Rate**: Terminal if side-state transitions exceed `12 Hz`.

---

## Material Contamination Rules

Contamination is an `activation_authority_conflict` that falsifies the standing proof.

### Rule 1 — Physics Blend Contamination
- Terminal if any body in the truth set has a live `PhysicsBlendWeight` > `0.0`.

### Rule 2 — Kinematic Update Contamination
- Terminal if `bUpdateMeshWhenKinematic` is enabled **AND** the mesh is NOT in **Absolute Transform** mode.
- Terminal if the flag is **mutated mid-attempt** by an external system.

### Rule 3 — Mesh-Wide Side Effects
- Terminal if a non-activation system issues global blend, mobility, or reset writes.
- **Plugin-Mandated Exception**: Events issued by `PhysicsControl` for its internal setup are diagnostic-only.

### Rule 4 — Excluded-Body Contamination
- Terminal if an excluded body is physically connected to the truth set AND receives an authoritative write.
- Terminal if an excluded body bridges the truth set and the walkable world (Dual-Contact).

### Rule 5 — Calf-Contact Contamination
- Terminal if `calf_l` or `calf_r` contacts `WorldStatic` geometry. (Honest Standing must be plantar).

---

## Controller Stability Rules

Stability failures in the control layer trigger terminal termination before physical collapse occurs.

### 1. Target Discontinuity
- The run must fail on `activation_target_discontinuity` if the rebase delta at blend-start exceeds **15.0 deg** on any balance-critical body.
- **Justification**: Large initial snaps can cause unrecoverable impulse transients that falsify the balance proof.

### 2. Unstable Gain or Damping
- The run must fail on `activation_unstable_gain_or_damping` if the implementation detects:
  - Any `NaN` or `Inf` in the published control targets, gains, or forces.
  - Explosive feedback (> 5000 N/m or equivalent) that is not part of the audited plant baseline.
  - High-frequency oscillation in control effort (> 30Hz) that materially destabilizes the truth set.

---

## Pose Fidelity (Reference Mismatch)

### Mismatch Terminalization
The run fails on `activation_pose_reference_mismatch` if:
1.  Any body in the balance-critical chain exceeds **25.0 deg** mismatch for more than **100 ms**.
2.  The RMS mismatch across the entire chain exceeds **15.0 deg** for more than **100 ms**.
3.  Target discontinuity at blend start exceeds **15.0 deg**.

---

## COM / Support Proxy Definition

### Proxy definition
The `V0` support proxy is the world-space planar centroid of:
- `pelvis`, `thigh_l`, `thigh_r`, `spine_01`

### Support region definition
The `V0` support region is the convex hull of all qualifying `foot_*` and `ball_*` manifold points from the final qualifying Chaos substep.
