# Stage 1 Plan

## Purpose

This document is the **Stage 1 index and control document**.

It owns the Stage 1 planning structure, execution model, and artifact map.

It is **not** the most detailed source for every execution lock. The detailed, phase-specific truth lives in the planning bundle under `plans/stage1/`.

Use it for:

- sequencing Stage 1 work
- delegating AI agent tasks
- defining the output artifact for each task
- identifying when the human user must intervene
- writing ELI5 manual verification instructions for anything that cannot be automated

Do not use `AGENTS.md` as a rolling status or next-steps document.
Do not expect this file to duplicate every detailed lock from the planning bundle.

## Current Execution Focus

Phase 1 bridge implementation and stabilization are active. Gate G1 has passed.

Current priorities:

1. extend the stabilized bridge beyond the `10` second PIE smoke window
2. capture video evidence of Manny under policy control
3. prepare G2 side-by-side comparison (physics-driven vs. kinematic playback)

## Planning Bundle Freeze

> **The planning bundle under `plans/stage1/` is frozen as of March 11, 2026.**

Only these 3 files are living documents during active execution:

| Living Document | Purpose |
|---|---|
| [Execution Log](/F:/NewEngine/plans/stage1/20-execution/execution-log.md) | Active task state board |
| [Assumption Ledger](/F:/NewEngine/plans/stage1/20-execution/assumption-ledger.md) | Risk tracking and assumption status |
| [G2 Evaluation](/F:/NewEngine/plans/stage1/30-evidence/g2-evaluation.md) | Gate G2 evidence and verdict |

Everything else is frozen reference material. Update a frozen doc only if a code change directly invalidates a contract it defines, and do so in the same commit as the code change.

For the full freeze policy, see [plans/stage1/README.md](/F:/NewEngine/plans/stage1/README.md).

## Planning Bundle Index

Frozen reference — use for lookups, not as living documents:

| Category | Documents |
|---|---|
| Control | [Orchestration](/F:/NewEngine/plans/stage1/00-control/orchestration.md), [Delegation Spec](/F:/NewEngine/plans/stage1/00-control/delegation-spec.md), [Handoff Format](/F:/NewEngine/plans/stage1/00-control/handoff-format.md) |
| Specs | [Bridge](/F:/NewEngine/plans/stage1/10-specs/bridge-spec.md), [UE Bridge Impl](/F:/NewEngine/plans/stage1/10-specs/ue-bridge-implementation-spec.md), [Retargeting](/F:/NewEngine/plans/stage1/10-specs/retargeting-spec.md), [Environment](/F:/NewEngine/plans/stage1/10-specs/environment-spec.md), [ONNX Export](/F:/NewEngine/plans/stage1/10-specs/onnx-export-spec.md), [Test Strategy](/F:/NewEngine/plans/stage1/10-specs/test-strategy.md), [Thresholds](/F:/NewEngine/plans/stage1/10-specs/acceptance-thresholds.md), [Deps](/F:/NewEngine/plans/stage1/10-specs/dependency-lock.md) |
| Execution | [Phase 0 Package](/F:/NewEngine/plans/stage1/20-execution/phase0-execution-package.md), [Phase 1 Package](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md), [Policy Stabilization](/F:/NewEngine/plans/stage1/20-execution/phase1-policy-stabilization-plan.md), [Bridge Bring-Up](/F:/NewEngine/plans/stage1/20-execution/phase1-ue-bridge-bringup-runbook.md), [Phase 2 Package](/F:/NewEngine/plans/stage1/20-execution/phase2-demo-package.md), [S1-P1-A1 Handoff](/F:/NewEngine/plans/stage1/20-execution/s1-p1-a1-handoff.md) |
| Evidence | [G1 Evidence](/F:/NewEngine/plans/stage1/30-evidence/g1-evidence.md), [G3 Evaluation](/F:/NewEngine/plans/stage1/30-evidence/g3-evaluation.md) |
| Tasks | [Task Graph](/F:/NewEngine/plans/stage1/40-tasks/task-graph.md), [S1-P0-A1](/F:/NewEngine/plans/stage1/40-tasks/task-packet-s1-p0-a1.md), [S1-P0-A2](/F:/NewEngine/plans/stage1/40-tasks/task-packet-s1-p0-a2.md), [S1-P1-A1](/F:/NewEngine/plans/stage1/40-tasks/task-packet-s1-p1-a1.md), [S1-P1-A2](/F:/NewEngine/plans/stage1/40-tasks/task-packet-s1-p1-a2.md), [S1-P2-A1](/F:/NewEngine/plans/stage1/40-tasks/task-packet-s1-p2-a1.md), [S1-P2-A2](/F:/NewEngine/plans/stage1/40-tasks/task-packet-s1-p2-a2.md) |
| Content | [Motion Set](/F:/NewEngine/plans/stage1/50-content/motion-set.md), [Source Map](/F:/NewEngine/plans/stage1/50-content/motion-source-map.md), [Source Lock](/F:/NewEngine/plans/stage1/50-content/motion-source-lock-table.md), [Comparison Sequence](/F:/NewEngine/plans/stage1/50-content/comparison-sequence-lock.md), [Model Selection](/F:/NewEngine/plans/stage1/50-content/pretrained-model-selection.md), [Checkpoint Retrieval](/F:/NewEngine/plans/stage1/50-content/pretrained-checkpoint-retrieval.md), [UE Scaffold](/F:/NewEngine/plans/stage1/50-content/ue-project-scaffold.md) |
| User | [Interventions](/F:/NewEngine/plans/stage1/60-user/user-interventions.md), [Return Template](/F:/NewEngine/plans/stage1/60-user/user-return-template.md), [Runbook](/F:/NewEngine/plans/stage1/60-user/user-runbook.md), [UE Setup](/F:/NewEngine/plans/stage1/60-user/eli5-ue-project-setup.md), [Manual Verification](/F:/NewEngine/plans/stage1/60-user/manual-verification.md) |

## Current Bridge Status

The bridge is stabilized with:

- deferred activation via `ReadyForActivation`
- per-body instability telemetry and fail-stops
- kinematic root/pelvis strategy
- 5-group staged non-root bring-up
- per-group control-authority ramps
- corrected SMPL→UE quaternion basis conversion
- policy-phase skeletal-animation target switching with offset reset

The 10-second PIE smoke window passes without catastrophic instability.

## User Intervention Rules

The user must intervene for:

- external tool/dependency installation
- license and dataset acceptance
- UE5 editor-only workflows
- visual quality judgment (G2, G3)

The user does not need to intervene for code changes, doc updates, or task reshaping within the locked architecture.

