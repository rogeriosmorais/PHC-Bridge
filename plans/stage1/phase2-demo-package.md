# Stage 1 Phase 2 Demo Package

## Purpose

This package defines the implementation-ready scope for Phase 2: the full two-character Stage 1 demo.

Phase 2 exists to answer the final Stage 1 question:

Can the project produce a compelling proof-of-concept demo with two physics-driven fighters, strong enough to justify or reject further investment?

## Entry Criteria

Do not start Phase 2 until all of these are true:

- Gate G2 is explicitly `pass`
- the one-character implementation is stable enough to duplicate without changing the architecture
- the orchestrator has reviewed `assumption-ledger.md` and confirmed there is no `red` assumption blocking the demo package

## Phase 2 Deliverables

1. Second-character integration package
2. Impact-response integration package
3. Knockdown / recovery transition package
4. Minimal fight-presentation package for input, camera, HUD, and opponent behavior
5. Demo-ready handoff for G3 evaluation

## Scope

Phase 2 includes:

- second character using the same Stage 1 pipeline
- impact-response behavior if viable within Stage 1 scope
- knockdown / recovery transition setup
- a controllable player side
- a basic opponent
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

### P2-02: Second Character Integration

- Owner: AI worker
- Goal: define how the one-character Stage 1 pipeline is duplicated safely for a second fighter
- Output:
  - package covering shared pipeline assumptions, duplicated assets/components, and any asymmetry decisions
- Constraint:
  - do not fork the architecture between character one and character two

### P2-03: Impact Response Scope

- Owner: AI worker
- Goal: define the minimal viable impact-response plan for Stage 1
- Output:
  - implementation-ready scope for impact response or explicit defer decision
- Escalate if:
  - impact response requires a scope increase that no longer fits the Stage 1 proof-of-concept

### P2-04: Knockdown / Recovery Transition Scope

- Owner: AI worker
- Goal: define how `UPhysicalAnimationComponent` or equivalent Stage 1 tools handle state changes
- Output:
  - transition plan and evidence expectations

### P2-05: Demo Presentation Scope

- Owner: AI worker
- Goal: define the smallest presentation layer needed to make the demo understandable
- Output:
  - minimal plan for:
    - Player 1 input
    - basic opponent behavior
    - camera
    - HUD
- Constraint:
  - prioritize clarity of the animation thesis over game-feature completeness

### P2-06: Phase 2 Demo Readiness Handoff

- Owner: orchestrator
- Goal: package the full Stage 1 demo for G3 evaluation
- Inputs:
  - all Phase 2 outputs
  - current assumption ledger
- Output:
  - handoff to `plans/stage1/g3-evaluation.md`

## Required Evidence For Phase 2 Completion

Before G3 packaging is considered ready, there must be evidence for:

- both fighters run using the intended Stage 1 pipeline
- transitions are readable enough for the demo
- player input and basic opposition exist
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
