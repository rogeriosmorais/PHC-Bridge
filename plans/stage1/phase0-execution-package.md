# Stage 1 Phase 0 Execution Package

## Purpose

This package turns the Stage 1 planning documents into an execution-ready Phase 0 sequence. It is the document the orchestrator should use to decide what work is runnable, what the user must do, what evidence must be captured, and how G1 will be judged.

## Scope

Phase 0 exists to answer one question:

Can the planned Stage 1 bridge survive first contact with reality strongly enough to justify implementation work?

The required outputs are:

- confirmed PHC bridge contract
- confirmed retargeting validation plan
- confirmed test/evidence shape for G1
- evidence that the UE5 control path and Manny/Chaos smoke path are viable

## Entry Criteria

Do not start Phase 0 until all of these are true:

- [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md) exists
- [retargeting-spec.md](/F:/NewEngine/plans/stage1/retargeting-spec.md) exists
- [test-strategy.md](/F:/NewEngine/plans/stage1/test-strategy.md) exists
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md) has been reviewed by the orchestrator for Phase 0
- the orchestrator has identified which assumptions Phase 0 is intended to confirm or falsify

## Phase 0 Deliverables

1. Updated `bridge-spec.md` with confirmed PHC config details
2. Updated `retargeting-spec.md` with the exact validation cases chosen for G1
3. A completed [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md) package
4. A short go / no-go conclusion for Gate G1

## Current Known Local Environment Note

- UE installation in progress: `UE 5.7.3`
- Planned install root: `E:\UE_5.7`
- Planned `UE5_PATH` value after install completes: `E:\UE_5.7\Engine`

## Work Breakdown

### P0-01: Freeze Inputs

- Owner: orchestrator
- Goal: freeze the exact planning inputs for Phase 0 execution
- Inputs:
  - `ENGINEERING_PLAN.md`
  - `plans/stage1/bridge-spec.md`
  - `plans/stage1/retargeting-spec.md`
  - `plans/stage1/test-strategy.md`
  - `plans/stage1/manual-verification.md`
  - `plans/stage1/assumption-ledger.md`
- Output:
  - short frozen-input note added to the Phase 0 handoff or execution log
- Done when:
  - no worker is allowed to reinterpret upstream assumptions during Phase 0

### P0-02: User Prerequisites

- Owner: user
- Goal: satisfy the Phase 0 prerequisites that AI cannot complete alone
- Required user actions:
  - install required tools and record versions / paths
  - obtain external repos and datasets
  - create the UE5 project scaffold
  - prepare editor-only setup for Manny and the physics-control path
- Evidence required back:
  - tool versions and install paths
  - confirmation of dataset / repo locations
  - confirmation that UE `5.7.3` finished installing at `E:\UE_5.7` or a corrected path if it changed
  - project path and brief note that the UE5 scaffold exists

### P0-03: Confirm PHC Contract

- Owner: AI worker
- Goal: replace planning-level PHC assumptions with config-specific facts
- Inputs:
  - selected ProtoMotions / PHC config
  - `plans/stage1/bridge-spec.md`
- Output:
  - updated `plans/stage1/bridge-spec.md`
  - handoff summarizing confirmed tensor shape, field order, and representation
- Must confirm:
  - observation tensor field order
  - observation tensor dimension
  - action tensor field order
  - action tensor dimension
  - representation format
  - gain-output policy
- Escalate if:
  - the chosen config does not support the intended Stage 1 bridge without scope changes

### P0-04: Confirm Retargeting Validation Set

- Owner: AI worker
- Goal: finalize which mapping and pose cases must be exercised in Phase 0
- Inputs:
  - `plans/stage1/retargeting-spec.md`
  - `.agents/skills/smpl-skeleton/SKILL.md`
- Output:
  - updated `plans/stage1/retargeting-spec.md`
  - explicit list of static and dynamic validation cases for G1
- Must confirm:
  - mapped joint subset
  - unmapped-bone policy
  - validation poses
  - failure signals to watch for in the Manny smoke test

### P0-05: Training-Side Feasibility Check

- Owner: AI + user
- Goal: confirm that PHC training-side output is worth continuing
- Inputs:
  - training workflow
  - manual check `MV-G1-01`
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - clip or visualization of PHC locomotion
  - verdict: `pass`, `fail`, or `unclear`
- Escalate if:
  - the motion is unstable or clearly robotic

### P0-06: UE5 Control-Path Check

- Owner: AI + user
- Goal: confirm that Manny responds to programmatic control updates
- Inputs:
  - UE5 scaffold
  - manual check `MV-G1-02`
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - clip or screenshot
  - note on whether `UPhysicsControlComponent` responds as expected
- Escalate if:
  - control updates are ignored or unstable

### P0-07: Substep Stability Check

- Owner: AI + user
- Goal: confirm that the articulated body can remain stable enough for Stage 1
- Inputs:
  - physics-substep plan from `ENGINEERING_PLAN.md`
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - substep settings used
  - clip or note on stability
- Escalate if:
  - stability is not achievable with the documented Stage 1 options

### P0-08: Manny End-To-End Smoke Test

- Owner: AI + user
- Goal: confirm that minimal SMPL/PHC-style outputs can drive Manny without obvious mapping failure
- Inputs:
  - confirmed bridge spec
  - confirmed retargeting validation set
  - manual check `MV-G1-03`
- Output:
  - evidence entry in `g1-evidence.md`
- Required evidence:
  - smoke-test clip
  - observed pose behavior
  - failure notes if any limbs map incorrectly
- Escalate if:
  - the mapping is mirrored, unstable, or otherwise obviously wrong

### P0-09: Orchestrator Gate Review

- Owner: orchestrator
- Goal: decide whether G1 is pass, fail, or blocked
- Inputs:
  - completed `g1-evidence.md`
  - current `assumption-ledger.md`
- Output:
  - final G1 decision note in `g1-evidence.md`
  - updated assumption statuses
- Done when:
  - the project has a clear go / no-go answer for Stage 1 Phase 1

## Required Evidence Checklist

The following must be present before G1 can be called `pass`:

- confirmed PHC bridge contract
- confirmed retargeting validation set
- PHC training-side visual evidence
- Manny control-path evidence
- substep-stability evidence
- end-to-end Manny smoke-test evidence
- explicit G1 verdict from the orchestrator

## Failure Handling

If any critical Phase 0 check fails:

- update `assumption-ledger.md`
- decide whether the failure is:
  - recoverable within Stage 1 scope
  - recoverable only by invoking an approved fallback
  - a stop condition
- do not begin the Phase 1 package until that decision is written down

## Handback Requirements

The worker or orchestrator closing Phase 0 must produce a handoff that includes:

- `Task ID`: `S1-P0-A1` or `S1-P0-A2`
- decision summary
- produced artifact(s)
- open risks
- blocked by user? yes/no
- verification summary
- next consumer

Use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md) exactly.
