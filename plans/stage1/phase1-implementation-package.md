# Stage 1 Phase 1 Implementation Package

## Purpose

This package defines the implementation-ready scope for Phase 1: one physics-driven character in UE5, using the locked Stage 1 architecture and only the minimum custom bridge code required.

Phase 1 exists to answer the next question after G1:

Can one Manny character run the full `PoseSearch -> PHC -> Physics Control -> Chaos` loop well enough to support a credible quality comparison for G2?

## Entry Criteria

Do not start Phase 1 implementation until all of these are true:

- Gate G1 is explicitly `pass` in [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md)
- `bridge-spec.md` is locked for the chosen PHC config
- `retargeting-spec.md` is locked for the Stage 1 mapped subset
- `test-strategy.md` is stable enough to define Phase 1 evidence
- the orchestrator has reviewed `assumption-ledger.md` and confirmed there is no `red` assumption blocking one-character implementation

## Phase 1 Deliverables

1. One-character implementation package for the Stage 1 bridge
2. ONNX / NNE integration plan for the selected PHC model
3. PoseSearch content-integration plan for locomotion and fight clips
4. PD tuning plan for one-character stability and comparison readiness
5. A clear setup for the G2 side-by-side evaluation package

## Scope

Phase 1 includes:

- a single Manny character
- the runtime bridge from `PoseSearch` output to PHC input
- PHC inference through UE5 NNE
- action-to-`UPhysicsControlComponent` mapping
- PoseSearch content setup sufficient for comparison
- stability / tuning work needed to make G2 meaningful

Phase 1 does not include:

- two-character gameplay
- impact response policy for combat interactions
- opponent behavior
- camera / HUD / demo packaging
- any Stage 2 GPU work

## Work Breakdown

### P1-01: Freeze Phase 1 Inputs

- Owner: orchestrator
- Goal: freeze the exact assumptions and artifacts Phase 1 is allowed to build on
- Inputs:
  - `plans/stage1/g1-evidence.md`
  - `plans/stage1/bridge-spec.md`
  - `plans/stage1/retargeting-spec.md`
  - `plans/stage1/test-strategy.md`
  - `plans/stage1/assumption-ledger.md`
- Output:
  - short frozen-input note in the Phase 1 handoff

### P1-02: Plugin Bridge Implementation Scope

- Owner: AI worker
- Goal: define the exact bridge responsibilities for `PhysAnimPlugin`
- Output:
  - implementation-ready breakdown for:
    - state gathering
    - observation packing
    - NNE inference call
    - action unpacking
    - physics-control writes
- Constraints:
  - keep custom code to the low hundreds where feasible
  - do not add a new inference runtime
  - do not widen scope into Stage 2 systems

### P1-03: ONNX / NNE Integration Scope

- Owner: AI worker
- Goal: define how the chosen PHC model becomes a UE5-usable NNE asset
- Inputs:
  - locked bridge spec
  - export workflow assumptions
- Output:
  - model import / validation plan
  - expected runtime evidence for successful loading and inference
- Escalate if:
  - the selected model export path no longer fits the Stage 1 assumptions

### P1-04: PoseSearch Content Scope

- Owner: AI worker
- Goal: define the minimal PoseSearch setup required for the one-character comparison
- Inputs:
  - `ENGINEERING_PLAN.md`
  - G1 outcome
- Output:
  - clip / database setup plan
  - clear statement of which locomotion and fight content is needed in Phase 1
- Constraint:
  - keep content scope small enough that G2 is a quality test, not a content-production project

### P1-05: PD Tuning Scope

- Owner: AI worker
- Goal: define the tuning work needed to make the one-character loop stable and comparable
- Inputs:
  - G1 evidence
  - substep evidence
- Output:
  - tuning checklist for gains, damping, and stability settings
  - definition of what "stable enough for G2" means
- Escalate if:
  - tuning work would effectively require architectural changes

### P1-06: Optional Visual Bonus Decision

- Owner: orchestrator
- Goal: decide whether Chaos Flesh + ML Deformer belongs in Phase 1 or should be deferred
- Output:
  - explicit `include` or `defer` note
- Default:
  - defer unless it materially improves the G2 comparison without destabilizing scope

### P1-07: G2 Readiness Handoff

- Owner: orchestrator
- Goal: package the one-character implementation result so G2 can be evaluated cleanly
- Inputs:
  - all Phase 1 outputs
  - current assumption ledger
- Output:
  - handoff to `plans/stage1/g2-evaluation.md`

## Required Evidence For Phase 1 Completion

Before Phase 1 can be considered complete enough for G2 packaging, there must be evidence for:

- one-character bridge works end to end
- PHC model runs through NNE in UE5
- PoseSearch content is available for the chosen comparison sequence
- tuning reaches a stable-enough state for comparison
- no unresolved `red` assumptions remain for G2

## Failure Handling

If Phase 1 stalls or fails:

- update `assumption-ledger.md`
- decide whether the failure is:
  - tuning-related and recoverable
  - bridge-related and recoverable
  - content-related and recoverable by narrowing scope
  - a stop condition for Stage 1
- do not advance to G2 packaging until that decision is written down

## Handback Requirements

The worker or orchestrator closing Phase 1 must produce a handoff that includes:

- `Task ID`: `S1-P1-A1`
- decision summary
- produced artifact(s)
- open risks
- blocked by user? yes/no
- verification summary
- next consumer

Use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md) exactly.
