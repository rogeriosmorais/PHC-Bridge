# Stage 1 Execution Log

## Purpose

This file is the orchestrator-owned live task-state board for Stage 1.

Use it to track:

- what is active
- what is blocked
- what is waiting on the user
- what frozen inputs are in effect
- what handoffs were accepted

Pre-pivot history remains available in git. This live log now tracks the current balance-first direction.

## Current State

- `Date`: `2026-04-23`
- `Direction change`: the previous flip-based activation model is now considered conceptually flawed as the target design.
- `Current phase`: documentation and execution alignment for balance-first activation.
- `Overall status`: UE startup is still stable and the standing benchmark is unchanged, but the active objective has changed. Stage 1 now aims to activate from continuous physical ownership of the balance-critical chain, ramp controller authority gradually onto that already-physical state, and measure sustained physical stability rather than certify a multi-phase handoff.
- `Latest runtime read`: the latest verified live smoke from `2026-04-22` still failed to reach sustained `BalanceActive_Standing`. That evidence remains useful as legacy forensic context, but it no longer justifies further refinement of the old flip-based ritual as the target design.
- `Tracked evidence sync`: the checked-in `test-results/phase1-autocalib/automation_phase1_smoke` artifacts still align with the standing benchmark and still show no passing candidate. They remain evidence that the product benchmark is unmet, not evidence that the old activation model should keep being refined.
- `Current interpretation`: if early balance-first runs look worse, that is not automatically regression. The new direction is expected to expose controller tuning weakness, authority conflicts, shell-truth problems, and long-lived oscillation more directly because it removes protective transition guards and grace logic.

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as initially frozen | `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | environment and setup specs | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | scaffold and user setup docs | UE editor setup | none |
| S1-P0-A1 | AI | completed | frozen Phase 0 inputs | Phase 0 package + spec paths | none |
| S1-P0-A2 | AI + User | completed | Phase 0 execution package + G1 evidence paths | evidence + log paths | none |
| S1-P1-A1 | AI | completed | Phase 1 implementation package, ONNX/export specs, UE bridge implementation spec | ONNX export/runtime bridge paths | none |
| S1-P1-A2 | AI | completed | accepted `S1-P1-A1` handoff, Phase 1 implementation package, manual verification, acceptance thresholds, bring-up runbook, newer balance design docs | `plans/stage1/40-design/*.md`, `plans/stage1/30-evidence/g2-evaluation.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-DOCS-BALANCE-FIRST | AI | completed | balance-first activation direction | `ENGINEERING_PLAN.md`, `STAGE1_PLAN.md`, `plans/stage1/10-specs/*.md`, `plans/stage1/40-design/*.md`, `plans/stage1/20-execution/execution-log.md` | none |
| S1-IMPL-BALANCE-FIRST | AI | pending | balance-first activation docs, standing benchmark, existing code symbols retained for compatibility | runtime bridge and balance activation code paths | none |

## Frozen Inputs For Phase 0 Preparation

- `Frozen docs`:
  - `AGENTS.md`
  - `ENGINEERING_PLAN.md`
  - `plans/stage1/10-specs/bridge-spec.md`
  - `plans/stage1/10-specs/retargeting-spec.md`
  - `plans/stage1/10-specs/test-strategy.md`
  - `plans/stage1/60-user/manual-verification.md`
  - `plans/stage1/20-execution/assumption-ledger.md`
  - `plans/stage1/10-specs/acceptance-thresholds.md`
  - `plans/stage1/50-content/pretrained-model-selection.md`
  - `plans/stage1/50-content/pretrained-checkpoint-retrieval.md`
  - `plans/stage1/10-specs/environment-spec.md`
  - `plans/stage1/50-content/motion-set.md`
  - `plans/stage1/50-content/motion-source-map.md`
  - `plans/stage1/50-content/motion-source-lock-table.md`
  - `plans/stage1/50-content/ue-project-scaffold.md`
  - `plans/stage1/20-execution/phase0-execution-package.md`
- `Unfreeze rule`: only unfreeze if the user returns setup evidence that changes a planned value or if an assumption moves materially in the ledger

## Next Runnable Tasks

| Priority | Task ID | Why Runnable / Not Runnable Yet |
|---|---|---|
| 1 | S1-IMPL-BALANCE-FIRST | runnable now; the target design is now documented and the next step is runtime implementation for continuous physical ownership plus gradual controller blend |
| 2 | G2 evidence capture | runnable once comparison packaging reflects the new activation model honestly |
| 3 | S1-P2-A1 | not runnable until G2 is explicitly passed |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| none | no setup evidence remains outstanding |

## Latest Phase 0 Evidence Progress

- the selected local `motion_tracker/smpl` checkpoint contract remains written down in `bridge-spec.md`
- G1 Criterion 2 remains `pass`
- the motion-source review remains `pass`
- the UE scaffold remains concretely verified:
  - `PhysAnimUE5.uproject` lists `PoseSearch` and `PhysicsControl`
  - Manny content exists under `Content/Characters/Mannequins`
  - editor logs show `NNERuntimeORT` runtime availability
  - PIE launched successfully
- Visual Studio Build Tools 2022 with MSVC v143 and Windows SDK `22621` remain installed and usable
- the UE startup-success path remains proven:
  - `[PhysAnim] Startup success. Runtime=NNERuntimeORTDml Model=/Game/NNEModels/phc_policy.phc_policy`
- Gate G1 remains `pass`
- Phase 0 evidence capture remains complete

## Accepted Handoffs

| Task ID | Artifact | Accepted? | Notes |
|---|---|---|---|
| S1-PLAN-01 | Stage 1 planning bundle | yes | foundational planning artifacts in place |
| S1-PLAN-02 | `bridge-spec.md` | yes | planning-level contract defined and updated with selected local runtime contract |
| S1-PLAN-03 | `retargeting-spec.md` | yes | planning-level mapping defined; runtime validation work followed |
| S1-PLAN-04 | `test-strategy.md` | yes | verification split defined |
| S1-PLAN-05 | environment / pretrained / scaffold / threshold bundle | yes | execution-planning gaps materially reduced |
| S1-PLAN-06 | task packets / lock sheets / user return path | yes | re-entry into Phase 0 operationally defined |
| S1-PLAN-07 | retrieval / export / comparison lock bundle | yes | Phase 0-1 planning gaps materially reduced |
| S1-P0-A1 | Phase 0 machine-specific execution package | yes | Windows-native Isaac Sim / Isaac Lab path frozen |
| S1-P1-A1 | single-character implementation package freeze + ONNX export path | yes | startup now succeeds through `NNERuntimeORTDml`; export/import discovery no longer critical path |
| S1-DOCS-BALANCE-FIRST | balance-first activation docs rewrite | yes | canonical docs now point to continuous physical ownership, gradual controller blend, and standing validation |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| G2 | readying | comparison packaging must reflect the new activation model honestly |
| S1-P2-A1 | blocked | depends on G2 pass |
| S1-P2-A2 | blocked | depends on Phase 2 result |
| Legacy flip-path tuning | deferred | only worth touching for temporary backward-compat notes, not as the target design |
| Broad perturbation tuning by guesswork | deferred | explicit activation design and the standing benchmark remain higher priority |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks

---

## 2026-04-23 — Balance-first activation pivot

- branch direction changed explicitly
- the previous flip-based activation model is now considered conceptually flawed as the target design
- the new principles are:
  - keep the balance-critical chain continuously simulated as much as possible
  - minimize topology and ownership flips
  - ramp controller authority gradually onto an already-physical state
  - use transition diagnostics to measure reality, not to manufacture a pass
  - define success as sustained physical stability, not completion of a multi-phase ritual
- the active product benchmark is unchanged:
  - reach `BalanceActive_Standing`
  - hold it continuously for `3.0` seconds
  - do not count safe deny as success
- practical effect:
  - further `RootOn`, `Settle`, grace-window, or flip-certification refinement is no longer the active direction
  - the next implementation target is balance-first activation on a continuously physical chain
  - the first likely failure surface is now “controller cannot stand in continuous physics” rather than “handoff is unsafe”
  - observability must stay strong enough to diagnose sustained-balance metrics, control effort, contact quality, COM behavior, and long-lived oscillation without falling back into vague visual debugging
