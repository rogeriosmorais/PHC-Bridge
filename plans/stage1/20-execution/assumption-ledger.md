# Stage 1 Assumption Ledger

## Purpose

This file tracks only high-risk assumptions that can change the plan, tests, contracts, instrumentation, dependency boundaries, or runtime rollout.

This is not:
- a task log
- a daily journal
- a commit history
- a place to record normal implementation progress
- a place to record every failed test

Git history is sufficient for old ledger history.

## Update Rule

Update this file only when an assumption is:

- new
- false
- weaker than expected
- stronger than expected
- blocked
- dangerous
- responsible for changing the plan

Do not update this file for:
- scaffold commits
- passing tests
- normal implementation work
- typo fixes
- expected compile fixes
- ordinary red/green TDD cycles
- implementation of already-planned behavior

## Required Handoff Line

Every repo-work handoff must declare one of:

- `Ledger impact: none`
- `Ledger impact: updated: A-XX`
- `Ledger impact: blocked: assumption decision needed`

## Status Values

Use only these status values:

- `Open`
- `Watching`
- `Resolved`
- `Falsified`
- `Blocked`

## Active Assumptions

| ID | Assumption | Status | Trigger For Update | Current Decision | Next Check |
|---|---|---|---|---|---|
| A-01 | Slice 1 pure support truth can be implemented without runtime object dependencies. | Watching | A pure support function needs `UObject`, `FBodyInstance`, component state, `UWorld`, `AActor`, or Chaos runtime handles. | Keep Slice 1 value-only. Stop if runtime data becomes necessary. | Commits 1-10 of Slice 1. |
| A-02 | The test matrix is sufficient to drive Slice 1 without implementation guessing. | Watching | A mapped test cannot be written cleanly, has missing expected behavior, or lacks required data. | Update the matrix before implementing behavior. | Each Slice 1 behavior commit. |
| A-03 | Runtime adapter capture can be delayed until pure support truth and validators are green. | Watching | Slice 1 or validator code requires live runtime reads before adapter commits. | Do not cross adapter boundary early. Update refactor plan if this fails. | Validator scaffolding and adapter planning. |
| A-04 | Artifact-backed failure classification is sufficient to prevent visual tuning spirals. | Watching | An in-engine failure cannot be explained by canonical terminal reason and required forensic fields. | Stop tuning. Fix instrumentation or contract coverage first. | First shadow-validation and runtime wiring runs. |
| A-05 | V0 activated-standing proof is a 10-body raw-sim contract, not all required body modifiers. | Watching | A proof tries to accept `simMax=0`, force all 22 body modifiers, or add staged runtime activation. | Require pelvis/spine/thigh/foot/ball raw-sim; exclude remaining bodies from V0 truth and prevent contamination. | Next V0 raw-sim body-contract change. |
| A-06 | Thigh restore variant coverage is diagnostic evidence, not physical viability evidence. | Watching | A task claims true balance progress from `PhysAnim.Diagnostics.ThighRestore` without `BalanceActive_Standing` hold evidence. | Use the test to prove observability and kinetic-gate behavior only; require balance smoke or activated-standing proof for viability. | Next thigh or proximal tuning task. |
| A-07 | The SMPL action and observation mapping constants match the locked Stage 1 policy contract. | Watching | ONNX export, retargeting, or bridge descriptor tests disagree on joint count, order, frame conversion, or distal hand handling. | Keep `PhysAnim.Bridge.*` and `Training/tests` as the contract authority before runtime tuning. | Next policy/export/retargeting contract change. |
| A-08 | Current smoke evidence must be read from the active checkout log, not a sibling repo log. | Open | A smoke script or helper reports evidence from outside `F:\NewEngine-AgentB\PhysAnimUE5\Saved\Logs`. | Treat direct current-repo log inspection as required until the helper path is corrected or proven checkout-local. | Next PIE smoke or diagnostic evidence run. |
| A-09 | Runtime ONNX assets are present, but original offline checkpoint provenance is not confirmed in this checkout. | Open | A task needs re-export, numerical checkpoint comparison, or offline training evidence from `Training\ProtoMotions`. | Runtime bridge work may use `Content\NNEModels`; checkpoint-dependent work must first resolve provenance. | Next offline-model/export task. |
| A-10 | `PhysAnim.PIE.BalanceModeSmoke` previously reported `BalanceActive_Standing` while metrics showed `PelvisSim=0`. | Resolved | A future smoke pass again reports product success without live physical-continuity evidence. | The false-positive success path has been superseded by stricter physical-continuity and smoke-outcome checks; do not use old memories as current blocker evidence. | `node_04eead4961bc` artifact schema acceptance check. |
| A-11 | The current Phase 3 `phase3_post_root_on_instability` label must map cleanly to canonical V0 terminal truth before weak-agent implementation. | Open | Phase 3 remains blocked, a new non-canonical terminal reason appears, or the artifact cannot reconstruct the failing invariant. | Stop broad tuning. Use truth arbitration and branch-map work before assigning a weak agent to the narrow Phase 3 fix. | `node_deea5e3bca44`, then `node_b9ce2f9d69e8`. |

## Entry Format

When adding a new assumption, use this format:

| ID | Assumption | Status | Trigger For Update | Current Decision | Next Check |
|---|---|---|---|---|---|
| A-XX | One sentence. | Open | Exact event that requires updating this row. | Current decision or stop rule. | Next test, commit, run, or review point. |

## Minimal Update Rules

When updating an existing row:
- change the status
- change the current decision
- change the next check
- keep the row short

Do not add long narrative history.
Do not paste logs.
Do not duplicate execution-log content.
Do not add run cards here.
