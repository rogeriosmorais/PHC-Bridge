# Lesser Agent Execution Packet - Stage 1 (Standing Stability)

This packet defines the strict, branch-aware implementation path that a lesser agent must follow to reach the first truthful **BalanceActive_Standing >= 3.0s** benchmark.

## 1. Known Blocker Chain
The current state is blocked by instability during the Phase 3 (Simulated Root) settlement window.

- **Symptom**: `phase3_post_root_on_instability` reported in smoke telemetry.
- **Root Blocker**: `node_78833e2a534a` (S2-INVESTIGATE-BALANCE-PHASE3-INSTABILITY-DECISION-01).
- **Prerequisite**: `node_b895d7c254ff` (Contract: Continuous Balance Truth Model).
- **Artifacts**: Refer to `Saved/Logs/PhysAnimUE5.log` and `workflow-graph/memories/achievements/stage1-kinematic-root-standing-proof.md`.

## 2. Mandatory Task Sequence
Agents MUST execute these tasks in the following order. Do not skip or parallelize without explicit approval.

1. **`node_b895d7c254ff`**: **Finalize Continuous Balance Truth Model**. Map the `phase3_post_root_on_instability` reason to a canonical terminal set.
2. **`node_78833e2a534a`**: **Execute Phase 3 Decision**. Decide if the instability requires a code fix or if the scenario parameters are out-of-bounds for Stage 1.
3. **`node_41078a2fd9cf`**: **Implement Narrow Phase 3 Fix** (Only if step 2 decides "Implement").
4. **`node_02b012838316`**: **Re-execute Standing Stability Proof**. Run the formal 3.0s harness.

## 3. Branching & Decision Rules

| Outcome | Agent Action |
|---------|--------------|
| **Standing Success (>= 3.0s)** | Mark `node_02b012838316` DONE; Create S1-REGRESSION-SOAK task. |
| **Unchanged Instability** | **STOP/ESCALATE**. Do not continue implementation. Mark task BLOCKED. |
| **New Terminal Reason** | Map the reason to the Truth Model (Step 1) BEFORE continuing. |
| **Build/Test Failure** | Fix immediately using TDD; do not bypass with `skip`. |
| **Contradictory Evidence** | Mark task BLOCKED; Cite specific log lines that conflict with the graph status. |

## 4. Forbidden Moves (The "No" List)
- **NO** threshold-only success (e.g., claiming 3.0s success from a 0.5s pass).
- **NO** safe-deny-as-success (treating a "BalanceSafeDeny" as a successful stand).
- **NO** hidden authority assistance (disabling gravity, anchoring actors, or forcing transforms).
- **NO** shell/kinematic rescue (manually snapping the shell to "help" the physics).
- **NO** TensorRT or custom Python asset pipelines.

## 5. Execution Constraints
- **WIP = 1**: Complete exactly ONE bounded task per execution window.
- **Escalation**: If a branch fires, you MUST stop, mark the node blocked, and create the next named graph task. Do not attempt to fix the branch in the same session.
- **Evidence**: Every "DONE" status MUST include a cited log snippet from `scripts/read_logs.py` showing the terminal state and duration.

> [!CAUTION]
> Failure to follow this packet results in architectural drift and is grounds for immediate task reversion.
