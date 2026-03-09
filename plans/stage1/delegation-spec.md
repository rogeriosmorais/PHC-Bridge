# Stage 1 Delegation Spec

## Purpose

This document defines what each AI-owned Stage 1 task is supposed to do, what it can touch, what it must produce, and when it has to escalate to the user.

## Delegation Rules

- AI tasks may reshape task sequencing only within the locked architecture.
- AI tasks must produce a concrete artifact, not just discussion.
- AI tasks must flag blockers instead of guessing when an external dependency, license, editor action, or subjective quality gate is involved.
- AI tasks that touch retargeting must use `.agents/skills/smpl-skeleton/SKILL.md` as a planning reference and must keep its transform caveats intact.

## AI Tasks

### S1-PLAN-01: Planning Bundle

- `Goal`: create the Stage 1 planning bundle and define the execution structure
- `Inputs`: `AGENTS.md`, `ENGINEERING_PLAN.md`, `STAGE1_PLAN.md`
- `Work`: create planning docs, link them together, and remove ambiguity about ownership and outputs
- `Output`: `plans/stage1/task-graph.md`, `plans/stage1/delegation-spec.md`, `plans/stage1/handoff-format.md`, `plans/stage1/user-interventions.md`, `plans/stage1/manual-verification.md`
- `Definition of done`: all five artifacts exist and align with the engineering plan
- `Escalate to user when`: the planning structure itself would change the locked architecture
- `Verification`: doc review for consistency across planning files

### S1-PLAN-02: PHC Bridge Spec

- `Goal`: define the exact `PoseSearch -> PHC -> Physics Control` bridge needed for Stage 1
- `Inputs`: `ENGINEERING_PLAN.md`, chosen ProtoMotions/PHC config, workflow docs
- `Work`: define required observations, action shape assumptions, reference-pose assumptions, coordinate assumptions, and bridge fallback rules
- `Output`: `plans/stage1/bridge-spec.md`
- `Definition of done`: the Stage 1 plugin contract is clear enough that implementation does not invent fields or transforms ad hoc
- `Escalate to user when`: the selected PHC config forces a scope or timeline tradeoff
- `Verification`: static review against G1 requirements and the Stage 1 bridge section in `ENGINEERING_PLAN.md`

### S1-PLAN-03: Retargeting Spec

- `Goal`: define the SMPL <-> UE5 mapping plan and its verification strategy
- `Inputs`: `ENGINEERING_PLAN.md`, `.agents/skills/smpl-skeleton/SKILL.md`
- `Work`: map joints, mark unresolved handedness handling, define round-trip expectations, and define hardcoded-pose validation cases
- `Output`: `plans/stage1/retargeting-spec.md`
- `Definition of done`: the mapping, transform risks, and validation cases are explicit
- `Escalate to user when`: the mannequin skeleton or unmapped-bone policy needs a product decision
- `Verification`: static review against the skill and the Phase 0 retargeting deliverable

### S1-PLAN-04: Test Strategy

- `Goal`: split Stage 1 validation into unit, integration, automation, and manual checks
- `Inputs`: `ENGINEERING_PLAN.md`, workflow docs, `plans/stage1/manual-verification.md`
- `Work`: decide what must be TDD, what waits for UE5 automation, and what remains human-evaluated
- `Output`: `plans/stage1/test-strategy.md`
- `Definition of done`: every major Stage 1 claim has an owner, a verification mode, and an evidence format
- `Escalate to user when`: a critical claim has no credible automated path
- `Verification`: static review against G1, G2, and G3

### S1-P0-A1: Feasibility Execution Package

- `Goal`: package the exact Phase 0 work so implementation can start without re-planning
- `Inputs`: bridge spec, retargeting spec, test strategy, workflow docs
- `Work`: write the commands, expected outputs, evidence requirements, and failure interpretation for Phase 0
- `Output`: `plans/stage1/phase0-execution-package.md`
- `Definition of done`: a separate engineer or agent could execute Phase 0 without making planning decisions
- `Escalate to user when`: a required tool, dataset, or editor step is blocked
- `Verification`: checklist review against all Phase 0 tasks and G1

### S1-P0-A2: G1 Evidence Package

- `Goal`: evaluate Phase 0 work against G1 and document the go/no-go result
- `Inputs`: Phase 0 execution results, user evidence, manual-check results
- `Work`: summarize results, list gaps, and determine whether G1 passed
- `Output`: `plans/stage1/g1-evidence.md`
- `Definition of done`: G1 status is explicit and backed by evidence
- `Escalate to user when`: visual quality or editor-only evidence is missing
- `Verification`: evidence review against the G1 gate text

### S1-P1-A1: Single-Character Implementation Package

- `Goal`: define the implementation-ready package for one physics-driven fighter
- `Inputs`: G1 evidence, bridge spec, retargeting spec, test strategy
- `Work`: package the expected code changes, asset dependencies, test coverage, and risks for Phase 1
- `Output`: `plans/stage1/phase1-implementation-package.md`
- `Definition of done`: implementation can begin without unresolved planning decisions
- `Escalate to user when`: asset, content, or scope choices exceed the Stage 1 boundaries
- `Verification`: static review against Phase 1 tasks and G2

### S1-P1-A2: G2 Evaluation Package

- `Goal`: define how to judge physics-driven versus kinematic quality
- `Inputs`: Phase 1 implementation result, manual verification template
- `Work`: specify comparison setup, expected evidence, and pass/fail rubric
- `Output`: `plans/stage1/g2-evaluation.md`
- `Definition of done`: the user can run G2 without inventing the comparison method
- `Escalate to user when`: comparison criteria remain subjective or contested
- `Verification`: review against the G2 gate text

### S1-P2-A1: Two-Character Demo Package

- `Goal`: define the implementation-ready package for the full Stage 1 demo
- `Inputs`: approved G2 result, prior specs, Phase 2 tasks in `ENGINEERING_PLAN.md`
- `Work`: package the second fighter, impact response, opponent behavior, camera, HUD, and verification steps
- `Output`: `plans/stage1/phase2-demo-package.md`
- `Definition of done`: implementation can begin without new structural planning work
- `Escalate to user when`: gameplay scope expands beyond the proof-of-concept target
- `Verification`: static review against Phase 2 tasks and G3

### S1-P2-A2: G3 Demo Verification Package

- `Goal`: define the final observer-facing evaluation package for Stage 1
- `Inputs`: Phase 2 demo result, manual verification template
- `Work`: specify demo script, observer prompt, evidence capture, and decision rubric
- `Output`: `plans/stage1/g3-evaluation.md`
- `Definition of done`: the user can run G3 and record a decision without inventing the process
- `Escalate to user when`: the observer criteria are too vague to support a decision
- `Verification`: review against the G3 gate text
