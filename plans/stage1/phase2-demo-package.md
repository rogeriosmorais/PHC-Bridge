# Stage 1 Phase 2 Demo Package

## Purpose

This package defines the implementation-ready scope for Phase 2: the Stage 1 locomotion showcase demo.

Phase 2 exists to answer the final Stage 1 question:

Can the project produce a compelling proof-of-concept demo from the locomotion-only Stage 1 pipeline, strong enough to justify or reject further investment?

## Entry Criteria

Do not start Phase 2 until all of these are true:

- Gate G2 is explicitly `pass`
- the one-character implementation is stable enough to duplicate without changing the architecture
- the orchestrator has reviewed `assumption-ledger.md` and confirmed there is no `red` assumption blocking the demo package

## Phase 2 Deliverables

1. Demo-presentation package for the locomotion-only Stage 1 showcase
2. Optional multi-character showcase note if duplication still helps the visual comparison
3. Demo-ready handoff for G3 evaluation

## Scope

Phase 2 includes:

- the Stage 1 locomotion pipeline presented clearly enough for observers
- optional duplication of the locomotion character if that strengthens the presentation without widening scope
- camera and HUD sufficient for a proof-of-concept demo

Phase 2 does not include:

- Stage 2 GPU work
- broad gameplay systems beyond what is needed for the demo
- production polish outside what is necessary for G3

## Work Breakdown

### P2-01: Freeze Phase 2 Inputs

- Owner: orchestrator
- Goal: freeze the assumptions and artifacts Phase 2 is allowed to build on
- Inputs:
  - `plans/stage1/g2-evaluation.md`
  - `plans/stage1/assumption-ledger.md`
  - Phase 1 handoff
- Output:
  - short frozen-input note in the Phase 2 handoff

### P2-02: Demo Presentation Scope

- Owner: AI worker
- Goal: define the smallest presentation layer needed to make the locomotion-only demo understandable
- Output:
  - minimal plan for:
    - demo flow
    - camera
    - HUD or labels if useful
    - optional duplication of the character for presentation only
- Constraint:
  - prioritize clarity of the animation thesis over feature growth

### P2-03: Optional Showcase Duplication

- Owner: AI worker
- Goal: decide whether duplicating the locomotion showcase materially helps the final demo
- Output:
  - explicit `include` or `omit` note plus any packaging implications
- Escalate if:
  - duplication would effectively reintroduce the removed combat/demo scope

### P2-04: Phase 2 Demo Readiness Handoff

- Owner: orchestrator
- Goal: package the full Stage 1 demo for G3 evaluation
- Inputs:
  - all Phase 2 outputs
  - current assumption ledger
- Output:
  - handoff to `plans/stage1/g3-evaluation.md`

## Required Evidence For Phase 2 Completion

Before G3 packaging is considered ready, there must be evidence for:

- the locomotion showcase runs using the intended Stage 1 pipeline
- the presentation layer is clear enough that observers can judge the motion thesis

## Failure Handling

If Phase 2 stalls or fails:

- update `assumption-ledger.md`
- decide whether the failure is:
  - demo-scope related and recoverable by narrowing scope
  - system-related and recoverable within Stage 1
  - a stop condition for the Stage 1 demo
- do not advance to G3 until that decision is written down

## Handback Requirements

The worker or orchestrator closing Phase 2 must produce a handoff that includes:

- `Task ID`: `S1-P2-A1`
- decision summary
- produced artifact(s)
- open risks
- blocked by user? yes/no
- verification summary
- next consumer

Use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md) exactly.
