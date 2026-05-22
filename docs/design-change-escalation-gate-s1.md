# Design Change Escalation Gate - Stage 1 (Standing Stability)

This document defines the boundaries for design and implementation pivots during Stage 1. It ensures that agents do not silently drift from the **Architecture Lock** or **Product Success** benchmarks.

## 1. Classification of Design Moves

| Category | Allowed Moves | Action required |
|----------|---------------|-----------------|
| **Allowed-within-task** | Narrow Physics Control tuning (damping/gain), stability threshold adjustments, continuity logic refinement. | Proceed with TDD proof. |
| **Requires-new-task** | Mapping new terminal reasons, adding artifact fields, resolving support truth regressions, or fixing repeatability failures. | **BLOCK** current node; Create new task node. |
| **Requires-Architecture-Review** | Changing PoseSearch/NNE/ONNX/Chaos boundaries, moving actuation out of Physics Control, or changing the V0 proof target. | **STOP**; Escalate to USER. |
| **Forbidden** | Safe-denial as success, hiding physics with CMC/Shell/Kinematic assistance, TensorRT, or custom Python UE asset pipelines. | **REJECT**; Do not implement. |

## 2. Decision Criteria

### 2.1 When to pivot within a task
- The move is strictly numeric (tuning) or logical (bug fix) within an existing subsystem.
- The move does not increase the scope of the current task's Acceptance Criteria.
- The move is backed by a deterministic test case.

### 2.2 When to escalate to a new task
- Evidence (smoke telemetry) reveals a blocker NOT covered by the current task.
- Implementation requires a new diagnostic field in the artifact schema.
- The current fix hypothesis is invalidated but a narrow alternative is identified.

### 2.3 When to escalate to USER (Architecture Review)
- The "Kinematic Root" authority model is insufficient for the current goal.
- The ONNX/NNE inference path requires modification.
- Stability cannot be reached without manual "cheating" (assistance).

## 3. Enforcement & Verification
- **Audit**: Every `update_status(done)` rationale MUST state: *"No Forbidden or Architecture-Review-level moves were performed during this task."*
- **Traceability**: If a pivot occurred, the rationale MUST cite the `Saved/Logs` or `Artifact` evidence that triggered the move.

> [!CAUTION]
> Silently bypassing this gate to reach a "success" state is a critical violation of the project's governance model.
