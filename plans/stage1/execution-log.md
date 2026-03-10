# Stage 1 Execution Log

## Purpose

This file is the orchestrator-owned live task-state board for Stage 1.

Use it to track:

- what is active
- what is blocked
- what is waiting on the user
- what frozen inputs are in effect
- what handoffs were accepted

## Current State

- `Current phase`: Phase 0 entry / frozen-input checkpoint
- `Overall status`: UE install, project scaffold, ProtoMotions checkout, pretrained checkpoint, and the Python `3.11` ProtoMotions environment are confirmed; Phase 0 is now blocked only on missing IsaacLab / Isaac Sim runtime availability
- `Last planning milestone`: orchestrator corrected the Python contract to `3.11` for Isaac Sim `5.x` and rebuilt the ProtoMotions environment successfully

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as of commit `0a9bf13` | `plans/stage1/execution-log.md`, `plans/stage1/assumption-ledger.md` | none |
| S1-P0-U1 | User | in_progress | `plans/stage1/environment-spec.md`, `plans/stage1/user-interventions.md`, `plans/stage1/user-return-template.md` | external tool/runtime setup and license acceptance | IsaacLab / Isaac Sim availability or an explicit alternate simulator decision |
| S1-P0-U2 | User | completed | `plans/stage1/ue-project-scaffold.md`, `plans/stage1/user-interventions.md`, `plans/stage1/user-return-template.md` | UE editor setup | none |
| S1-PLAN-05 | Orchestrator | in_progress | planning bundle as of commit `0a9bf13` | `plans/stage1/execution-log.md`, `plans/stage1/assumption-ledger.md` | real setup evidence |

## Frozen Inputs For Phase 0 Preparation

- `Freeze point`: commit `0a9bf13`
- `Frozen docs`:
  - `AGENTS.md`
  - `ENGINEERING_PLAN.md`
  - `plans/stage1/bridge-spec.md`
  - `plans/stage1/retargeting-spec.md`
  - `plans/stage1/test-strategy.md`
  - `plans/stage1/manual-verification.md`
  - `plans/stage1/assumption-ledger.md`
  - `plans/stage1/acceptance-thresholds.md`
  - `plans/stage1/pretrained-model-selection.md`
  - `plans/stage1/pretrained-checkpoint-retrieval.md`
  - `plans/stage1/environment-spec.md`
  - `plans/stage1/motion-set.md`
  - `plans/stage1/motion-source-map.md`
  - `plans/stage1/motion-source-lock-table.md`
  - `plans/stage1/ue-project-scaffold.md`
  - `plans/stage1/phase0-execution-package.md`
- `Unfreeze rule`: only unfreeze if the user returns setup evidence that changes a planned value or if an assumption moves materially in the ledger

## Next Runnable Tasks

| Priority | Task ID | Why Runnable / Not Runnable Yet |
|---|---|---|
| 1 | S1-P0-U1 | runnable now; pretrained eval is pinned to IsaacLab, but no IsaacLab runtime or launcher is present locally |
| 2 | S1-P0-A1 | partially prepared, but not runnable to completion until the simulator path is concrete enough to execute |
| 3 | S1-P0-A2 | depends on `S1-P0-A1` and the simulator/runtime blocker being resolved |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| IsaacLab / Isaac Sim state | install path if already present, or confirmation that you will install the required simulator/runtime and accept any NVIDIA terms needed for it |
| Simulator decision | stay on `isaaclab` as planned, or explicitly authorize a fallback to a different supported simulator if you want to abandon the primary path |
| Evidence bundle | install path, launcher path, any errors encountered, and anything that differed from the plan |

## Accepted Handoffs

| Task ID | Artifact | Accepted? | Notes |
|---|---|---|---|
| S1-PLAN-01 | Stage 1 planning bundle | yes | foundational planning artifacts in place |
| S1-PLAN-02 | `bridge-spec.md` | yes | planning-level contract defined, awaits config confirmation |
| S1-PLAN-03 | `retargeting-spec.md` | yes | planning-level mapping defined, awaits runtime validation |
| S1-PLAN-04 | `test-strategy.md` | yes | verification split defined |
| S1-PLAN-05 | environment / pretrained / scaffold / threshold bundle | yes | execution-planning gaps materially reduced |
| S1-PLAN-06 | task packets / lock sheets / user return path | yes | re-entry into Phase 0 is now operationally defined |
| S1-PLAN-07 | retrieval / export / comparison lock bundle | yes | remaining Phase 0-1 planning gaps materially reduced |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| S1-P0-A1 | blocked | waiting for a concrete simulator/runtime path; the Python environment is now ready, but no IsaacLab / Isaac Sim launcher is installed locally yet |
| S1-P0-A2 | blocked | depends on S1-P0-A1 |
| S1-P1-A1 | blocked | depends on G1 pass |
| S1-P1-A2 | blocked | depends on Phase 1 result |
| S1-P2-A1 | blocked | depends on G2 pass |
| S1-P2-A2 | blocked | depends on Phase 2 result |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks
