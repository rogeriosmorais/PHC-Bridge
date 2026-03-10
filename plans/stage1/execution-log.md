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

- `Current phase`: Phase 0 / `S1-P0-A1` complete / `S1-P0-A2` ready
- `Overall status`: UE install, project scaffold, ProtoMotions checkout, pretrained checkpoint, Python `3.11` environment, and the Isaac Sim / Isaac Lab runtime are confirmed locally; the Windows command path for Phase 0 is now frozen and the next step is evidence collection for G1
- `Last planning milestone`: orchestrator installed Isaac Sim `5.1.0.0` and Isaac Lab `2.3.2.post1`, patched the local ProtoMotions clone for Python `3.11` dataclass compatibility, and validated a short IsaacLab smoke run with the pretrained checkpoint using a single-device Fabric override on Windows

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as of commit `0a9bf13` | `plans/stage1/execution-log.md`, `plans/stage1/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | `plans/stage1/environment-spec.md`, `plans/stage1/user-interventions.md`, `plans/stage1/user-return-template.md` | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | `plans/stage1/ue-project-scaffold.md`, `plans/stage1/user-interventions.md`, `plans/stage1/user-return-template.md` | UE editor setup | none |
| S1-P0-A1 | AI | completed | `plans/stage1/task-packet-s1-p0-a1.md` plus frozen Phase 0 inputs | `plans/stage1/phase0-execution-package.md`, `plans/stage1/bridge-spec.md`, `plans/stage1/retargeting-spec.md`, `plans/stage1/assumption-ledger.md`, `plans/stage1/execution-log.md` | none |
| S1-P0-A2 | AI + User | in_progress | `plans/stage1/phase0-execution-package.md`, `plans/stage1/manual-verification.md`, `plans/stage1/acceptance-thresholds.md`, `plans/stage1/g1-evidence.md` | `plans/stage1/g1-evidence.md`, `plans/stage1/assumption-ledger.md`, `plans/stage1/execution-log.md` | G1 evidence capture, including user-observed manual checks |

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
| 1 | S1-P0-A2 | runnable now; the simulator/runtime path is concrete, the Windows eval command is frozen, and the remaining work is evidence collection plus manual judgment |
| 2 | S1-P1-A1 | not runnable until G1 is explicitly passed |
| 3 | S1-P1-A2 | not runnable until Phase 1 starts |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| `MV-G1-01` verdict | short note saying `pass`, `fail`, or `unclear` after reviewing the first pretrained evaluation clip or visualization |
| `MV-G1-02` evidence | screenshot or short clip showing whether Manny responds to control-target updates in UE |
| `MV-G1-03` evidence | clip plus short note naming the expected pose or mapped output and what Manny actually did |

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
| S1-P0-A1 | Phase 0 machine-specific execution package | yes | Windows-native Isaac Sim / Isaac Lab path is frozen and Phase 0 can advance without more setup replanning |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| S1-P1-A1 | blocked | depends on G1 pass |
| S1-P1-A2 | blocked | depends on Phase 1 result |
| S1-P2-A1 | blocked | depends on G2 pass |
| S1-P2-A2 | blocked | depends on Phase 2 result |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks
