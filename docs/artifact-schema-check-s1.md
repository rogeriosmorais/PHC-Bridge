# Artifact Schema Acceptance Check - Stage 1 (Standing Stability)

This document validates that the proof artifacts emitted by the PhysAnim system contain all necessary fields to formally judge **BalanceActive_Standing** product success and provide evidence for future G2 integration.

## 1. Mandatory Field Verification
The following fields have been verified as present in the `FPhysAnimRunArtifactSnapshot` struct and correctly serialized to JSON in `PhysAnimProofArtifactEmitter.cpp`.

| Field | Presence | Purpose |
|-------|----------|---------|
| `hold_duration_sec` | **VERIFIED** | Tracks the total time in a stable balance state. |
| `terminal_reason` | **VERIFIED** | Canonical reason for termination (must be `None` for success). |
| `physical_continuity_validator_passed` | **VERIFIED** | Ensures no simulation jumps or energy injection occurred. |
| `support_mode` | **VERIFIED** | Confirms the character is in a valid support configuration. |
| `proxy_inside_hull` | **VERIFIED** | High-fidelity stability evidence (COM relative to support). |
| `authority_conflict_count` | **VERIFIED** | Detects hidden kinematic/sim root contention. |
| `controller_stability_failure_field` | **VERIFIED** | Identifies which PID/Stability threshold was breached first. |
| `shell_helper_used_count` | **VERIFIED** | Tracks "soft" authority assistance during the proof. |

## 2. Product Success Criteria (The "G2 Gate")
A "Product Success" claim is only valid if the following conditions are met within a single proof artifact:

1. **Duration**: `hold_duration_sec >= 3.0`
2. **State**: `terminal_reason == 0` (None)
3. **Continuity**: `physical_continuity_validator_passed == true`
4. **Authority**: `authority_conflict_count == 0`
5. **Assistance**: `shell_helper_used_count == 0`

> [!IMPORTANT]
> If any of these fields are missing or report failure, the attempt is classified as a **Diagnostic Pass** or **Contract Failure**, but NOT Product Success.

## 3. Reconciliation Logic
- **Artifact vs. Log**: If the human-readable log (e.g., `PASS_SMOKE`) disagrees with the JSON artifact fields, the artifact is the source of truth. The task must be marked **BLOCKED** until telemetry reconciliation is performed.
- **Missing Fields**: If future requirements demand new instrumentation (e.g., specific joint energy metrics), a narrow instrumentation task MUST be created. Manual log interpretation is forbidden as a substitute for schema evidence.

## 4. Verdict
**PASSED**. The current artifact schema (v1.0) is sufficient for Stage 1 stability proofing.
