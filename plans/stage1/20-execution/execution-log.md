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
- `Current phase`: TDD planning and refactor sequencing for balance-first activation.
- **Stop Rule**: Do not proceed to runtime rewrite until the first pure-logic slice is green.
- `Overall status`: Specialized contracts (10-spec suite) are architecturally closed. Execution has moved from "documentation alignment" to "implementation planning." Implementation is blocked until:
  - Refactor plan exists
  - TDD strategy exists
  - Test matrix exists
  - First slice is defined
- `State naming`: preferred rewrite states are now `BalanceActivation_Ready -> BalanceActivation_BlendIn -> BalanceActivation_Validate -> BalanceActive_Standing`; older names remain compatibility labels only.
- `Latest runtime read`: the latest verified live smoke from `2026-04-22` still failed to reach sustained `BalanceActive_Standing`. That evidence remains useful as legacy forensic context, but it no longer justifies further refinement of the old flip-based ritual as the target design.
- `Tracked evidence sync`: the checked-in `test-results/phase1-autocalib/automation_phase1_smoke` artifacts still align with the standing benchmark and still show no passing candidate. They remain evidence that the product benchmark is unmet, not evidence that the old activation model should keep being refined.
- `Current interpretation`: if early balance-first runs look worse, that is not automatically regression. The new direction is expected to expose controller tuning weakness, authority conflicts, shell-truth problems, and long-lived oscillation more directly because it removes protective transition guards and grace logic.
- `Rewrite rule`: the new docs must be written around continuous-physics invariants, not around replacement phases. The old handoff pipeline is now legacy compatibility context.

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as initially frozen | `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | environment and setup specs | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | scaffold and user setup docs | UE editor setup | none |
| S1-P0-A1 | AI | completed | frozen Phase 0 inputs | Phase 0 package + spec paths | none |
| S1-P0-A2 | AI + User | completed | Phase 0 execution package + G1 evidence paths | evidence + log paths | none |
| S1-P1-A1 | AI | completed | Phase 1 implementation package, ONNX/export specs, UE bridge implementation spec | ONNX export/runtime bridge paths | none |
| S1-P1-A2 | AI | completed | accepted `S1-P1-A1` handoff, Phase 1 implementation package, manual verification, acceptance thresholds, bring-up runbook | `plans/stage1/30-evidence/g2-evaluation.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-DOCS-BALANCE-FIRST | AI | completed | balance-first activation direction, specialized 10-spec suite | `ENGINEERING_PLAN.md`, `STAGE1_PLAN.md`, `plans/stage1/10-specs/*.md`, `plans/stage1/20-execution/execution-log.md` | none |
| S1-PLAN-TDD | AI | pending | specialized 10-spec suite, standing benchmark | `plans/stage1/40-tasks/tdd-plan.md` | none |
| S1-PLAN-REFACTOR-ORDER | AI | pending | specialized 10-spec suite, existing bridge source | `plans/stage1/40-tasks/refactor-sequence.md` | none |
| S1-PLAN-TEST-MATRIX | AI | pending | instrumentation_and_acceptance.md, truth model | `plans/stage1/40-tasks/test-matrix.md` | none |
| S1-PLAN-FIRST-SLICE | AI | pending | specialized 10-spec suite | `plans/stage1/40-tasks/first-slice-definition.md` | none |
| S1-IMPL-BALANCE-FIRST | AI | blocked | TDD plan, refactor order, specialized 10-spec suite | runtime bridge and balance activation code paths | TDD/Refactor Plan acceptance |

## Frozen Inputs For Continuous Balance Rewrite

- `Authoritative Contract Suite`:
  - `plans/stage1/10-specs/continuous_balance_architecture.md`
  - `plans/stage1/10-specs/continuous_balance_truth_model.md`
  - `plans/stage1/10-specs/engine_execution_contract.md`
  - `plans/stage1/10-specs/authority_matrix.md`
  - `plans/stage1/10-specs/physics_asset_contract.md`
  - `plans/stage1/10-specs/instrumentation_and_acceptance.md`
  - `plans/stage1/10-specs/balance-mode-entry-spec.md`
- `Supporting Docs`:
  - `AGENTS.md`
  - `ENGINEERING_PLAN.md`
  - `STAGE1_PLAN.md`
  - `plans/stage1/60-user/manual-verification.md`
  - `plans/stage1/20-execution/assumption-ledger.md`
- `Unfreeze rule`: Only unfreeze if the user explicitly requests an architecture review or if an implementation spike proves a contract requirement is physically impossible.

## Next Runnable Tasks

| Priority | Task ID | Why Runnable / Not Runnable Yet |
|---|---|---|
| 1 | S1-PLAN-TDD | runnable now; 10-spec contracts are closed; TDD plan is the first requirement for implementation |
| 2 | S1-PLAN-REFACTOR-ORDER | runnable now; sequences the transition from legacy flip-path to continuous invariants |
| 3 | S1-IMPL-BALANCE-FIRST | blocked until TDD and Refactor plans are accepted |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| none | no setup evidence remains outstanding |

## Latest Evidence Progress

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
| S1-DOCS-BALANCE-FIRST | balance-first activation docs rewrite | yes | canonical docs now point to continuous physical ownership, gradual controller blend, and standing validation |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| S1-IMPL-BALANCE-FIRST | blocked | implementation is blocked until the TDD plan, refactor order, and test definitions are accepted |
| G2 | readying | comparison packaging must reflect the new activation model honestly |
| S1-P2-A1 | blocked | depends on G2 pass |
| Legacy flip-path tuning | deferred | archived; only worth touching for temporary backward-compat notes, not as the target design |
| Broad perturbation tuning by guesswork | deferred | explicit activation design and the standing benchmark remain higher priority |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks

---

## 2026-04-23 — Transition to TDD Planning

- the specialized 10-spec contract suite is now the single source of truth
- implementation is officially blocked until TDD and refactor sequencing are planned
- legacy design documents have been archived to `90-archive/40-design-legacy/`
- the execution focus is now on mapping contract requirements to deterministic tests
