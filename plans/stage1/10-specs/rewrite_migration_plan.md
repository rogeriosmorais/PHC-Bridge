# Phase 1 Rewrite Migration Plan

## Purpose

This document summarizes the migration from the legacy phase-based PhysAnim design to the continuous-balance architectural suite. It provides a mapping of decommissioned documents and the consolidation of their logic into the new Single Source of Truth (SSoT) files.

## Decommissioned Documents

The following documents have been retired and their logic moved to specialized contracts:

| Decommissioned Doc | Destination for Logic |
| :--- | :--- |
| `ue-bridge-implementation-spec.md` | `physics_asset_contract.md`, `character_capsule_contract.md`, `engine_execution_contract.md` |
| `bridge-spec.md` | (Superseded by implementation and physics contracts) |
| `retargeting-spec.md` | `physics_asset_contract.md` (Audit rules only) |
| `acceptance-thresholds.md` | `instrumentation_and_acceptance.md` |
| `onnx-export-spec.md` | (Moved to Offline Model Lifecycle skill) |
| `test-strategy.md` | `instrumentation_and_acceptance.md` (Must-fail gates) |
| `dependency-lock.md` | (Moved to project-level AGENTS.md / repository rules) |

## Logic Consolidation Summary

### 1. The Physical Plant Baseline
All rules regarding physics asset identity, mass tolerances, and skeleton audits now live in **[physics_asset_contract.md](physics_asset_contract.md)**.

### 2. The Architectural Shift
The decoupling of the mesh from the capsule and the "Mesh-Isolated Character" model is now owned exclusively by **[character_capsule_contract.md](character_capsule_contract.md)**.

### 3. Execution and Timing
The per-frame execution order, tick group requirements, and rebase/alpha-blend rollout math are now owned by **[engine_execution_contract.md](engine_execution_contract.md)**.

### 4. Failure Truth
The Master Precedence Table (1-14) and all physical failure criteria (continuity, support, mismatch) are owned by **[continuous_balance_truth_model.md](continuous_balance_truth_model.md)**.

### 5. Runtime Modes
The authoritative state machine and write-access rules are owned by **[authority_matrix.md](authority_matrix.md)**.

## Canonical Terminal Reason Surface

All documents now point to the following 14 leaf-level terminal reasons:

1. `activation_physics_asset_contract_violation`
2. `activation_capsule_contract_violation`
3. `activation_topology_change`
4. `activation_continuous_simulation_lost`
5. `activation_support_failure`
6. `activation_proxy_outside_support_region`
7. `activation_target_discontinuity`
8. `activation_unstable_gain_or_damping`
9. `activation_instability_threshold_breach`
10. `activation_pose_reference_mismatch`
11. `activation_movement_reclaim`
12. `activation_shell_helper_violation`
13. `activation_authority_conflict`
14. `activation_standing_validation_timeout`
