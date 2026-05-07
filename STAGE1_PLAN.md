# Stage 1 Plan

## Purpose

This document is the Stage 1 index and control document.

It defines the frozen document hierarchy, the active execution focus, and the rule for which document wins when two documents overlap.

Live task state is not stored in this document or in Stage 1 mirror logs. Live task state is owned by mcp-graph.

## Stage 1 Document Hierarchy

Authority order:

1. mcp-graph for live workflow state: task status, WIP, dependencies, blockers, acceptance criteria, and next action
2. Active contract documents explicitly listed in this file
3. `STAGE1_PLAN.md`
4. Active execution-planning documents explicitly listed in this file

**Historical reference (Non-authoritative)**:
- `plans/stage1/90-archive/40-design-legacy/*`
- frozen task, content, user, evidence, and control documents under `plans/stage1/` unless explicitly reactivated here

Interpretation rules:

- if `STAGE1_PLAN.md` conflicts with an active `10-specs` contract, the contract wins
- if execution-planning docs conflict with active contracts, the contracts win
- balance activation rules are contract rules and must exist in `10-specs`

## Current Execution Focus

The active Stage 1 direction is now balance-first activation.

Current focus:

1. keep the balance-critical chain continuously simulated as much as possible
2. minimize topology and ownership flips during activation
3. ramp controller authority gradually onto an already-physical state
4. preserve truthful diagnostics as observability only
5. distinguish controller tuning failures from ownership-continuity failures
6. replace shell-lock-dependent truth with a cleaner activation truth model
7. stop hidden authority conflicts between policy, Physics Control, locomotion authority, and startup logic
8. require strong observability around sustained-balance metrics, control effort, contact quality, COM behavior, and long-lived oscillation
9. require balance activation to reach `BalanceActive_Standing` and hold it for `3.0` seconds before any run counts as success
10. defer legacy flip-path refinement except where needed for temporary backward-compat notes

## Rewrite Ladder

The rewrite proceeds in this order:

1. write the continuous-balance truth model
2. write the authority matrix
3. delete success criteria tied to phase completion
4. build instrumentation for continuous mode before tuning behavior
5. implement the smallest always-simulated proximal prototype
6. run the new mode in parallel with the old one until the metrics are trustworthy
7. only then begin deleting old handoff logic

## Current Stage 1 Truth

The Stage 1 balance investigation now assumes:

- the earlier flip-based `Prepare -> LateValidate -> RootOn -> Settle` model is conceptually flawed as the target design
- moving away from that model is an architecture rewrite, not a tuning tweak
- the new target design is activation onto a continuously physical balance-critical chain
- the hard problem is no longer “make the handoff safe,” but “make the controller stand on its own in continuous physics”
- truthful diagnostics are still required, but diagnostics must not justify grace-based passing
- controller instability will now appear more directly as gains, damping, target representation, action scaling, latency, or pose-discontinuity problems
- early results may look worse because the new design removes protective transition guards and grace logic that previously hid those problems
- truthful safe denial remains useful forensics, not product success
- the only passing benchmark remains `BalanceActive_Standing` held continuously for `3.0` seconds
- the next engineering slice is balance-first activation implementation, not further certification of ownership flips

The first scoped target is intentionally narrow:

- always-sim proximal chain
- simulated support set for honest contact truth
- idle stance
- flat ground
- no perturbation
- no locomotion authority
- no shell cleverness

## Frozen Balance Rule

Stage 1 treats balance activation as a separate contract from normal bridge startup.

Normal bridge startup may still use staged bring-up for non-critical systems.

Balance activation must not silently treat the old multi-phase handoff model as its target source of truth.

The authoritative balance-activation contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

## Required Smoke Outcome

The `PhysAnim.PIE.BalanceModeSmoke` test is successful only if the run ends as:

- `BalanceActive_Standing`

The test is a failure if the run ends in:

- `BridgeActive`
- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Recovery`
- explicit safe denial
- unresolved entry ambiguity
- misleading success caused by hidden assistance

## Planning Bundle Freeze

The planning bundle under `plans/stage1/` remains frozen except for contract corrections and design updates required to reflect proven evidence.

Only these categories may change during the current stabilization loop:

- `10-specs` documents that define runtime contract
- active balance-first execution-planning documents listed below
- evidence documents that record results

## Planning Bundle Index

### Contract documents

- `plans/stage1/10-specs/continuous_balance_architecture.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/10-specs/authority_matrix.md`
- `plans/stage1/10-specs/engine_execution_contract.md`
- `plans/stage1/10-specs/physics_asset_contract.md`
- `plans/stage1/10-specs/character_capsule_contract.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`
- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `plans/stage1/10-specs/smpl-ue5-retargeting-spec.md`
- `plans/stage1/10-specs/rewrite_migration_plan.md`

### Active execution-planning documents

- `plans/stage1/20-execution/assumption-ledger.md` (risk notes only; not task state)
- `plans/stage1/20-execution/balance_first_refactor_plan.md`
- `plans/stage1/20-execution/single-character-implementation-package.md`
- `plans/stage1/20-execution/stabilization-and-tuning-package.md`
- `plans/stage1/20-execution/balance_first_tdd_strategy.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `plans/stage1/20-execution/stage1-test-strategy.md`
- `plans/stage1/20-execution/first-slice-definition.md`
- `plans/stage1/20-execution/weak-agent-balanceactive-protocol.md`

### Legacy mirror workflow documents

These documents may be useful for historical reconstruction, but they are not live task authority:

- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/*`
- `plans/stage1/20-execution/checkpoints/*`
- `plans/stage1/40-tasks/*`

Do not use them to decide current task status, next action, blockers, dependencies, or acceptance criteria. Use mcp-graph.

### Archived design documents (Historical)

- See `plans/stage1/90-archive/40-design-legacy/` for historical design rationale and research notes.

## Documentation Acceptance Rule

The design is considered documented only when all of the following are true:

- the balance-activation contract is explicit in `10-specs`
- the continuous-balance architecture, truth model, authority matrix, and instrumentation docs exist and are referenced as the primary rewrite documents
- `STAGE1_PLAN.md` points to that contract
- archived design docs are clearly non-authoritative and are not used as implementation inputs
- the docs explicitly distinguish contract correctness from physical viability
- no authoritative document implies a flip-based handoff is the intended activation mechanism
- the docs explicitly define continuous physical ownership of the balance-critical chain as the target design
- the docs explicitly define controller authority as a gradual blend onto an already-physical state
- the docs explicitly say this is a rewrite of the old transition-state-machine assumption, not a small extension
- the docs explicitly separate controller-strength problems from ownership-continuity problems
- the docs explicitly require a truth model that does not secretly depend on shell-maintained containment
- the docs explicitly define the smallest always-simulated proximal prototype as the first rewrite target
- the docs explicitly define how support/contact truth works in that prototype
- the docs explicitly say no distal or upper-body sophistication is in scope before proximal standing is honest
- the docs explicitly call out hidden multi-owner authority fights as a primary implementation risk
- the docs explicitly require observability strong enough to debug sustained standing honestly
- the docs explicitly state that diagnostics are observational and cannot justify grace-based passing
- the docs explicitly state that truthful safe deny is not a passing outcome
- the docs explicitly state the active benchmark: `BalanceActive_Standing` held for `3.0` continuous seconds

## Rewrite Success Ladder

- Milestone 1: honest continuous-physics diagnostics
- Milestone 2: `1.0` second stable hold
- Milestone 3: `3.0` second stable hold
- Milestone 4: small perturbation recovery

## Architectural Direction

The active direction and the long-term target are now the same:

- a balance-first runtime
- continuously physical balance-critical bodies
- gradual controller blend-in
- standing validation before active mode
- recovery or denial based on truthful physical outcomes
