# Stage 1 Plan

## Purpose

This document owns Stage 1 planning and task decomposition.

Use it for:

- sequencing Stage 1 work
- delegating AI agent tasks
- defining the output artifact for each task
- identifying when the human user must intervene
- writing ELI5 manual verification instructions for anything that cannot be automated

Do not use `AGENTS.md` as a rolling status or next-steps document.

## Current Planning Focus

The next step is **planning Stage 1 execution**, not starting environment setup yet.

Stage 1 planning must answer:

1. How the work is split into AI-agent-sized tasks
2. What artifact each task must produce
3. What depends on human intervention
4. What can be automated with tests versus what needs ELI5 manual verification

## Stage 1 Planning Outputs

Before implementation starts, this plan must produce these concrete artifacts. The current planning bundle lives under `plans/stage1/`.

1. A task graph for Phase 0, Phase 1, and Phase 2
2. A delegation spec for each AI task
3. A handoff format for completed tasks
4. A user-intervention checklist
5. An ELI5 manual-test template for non-automatable validation

## Planning Bundle

- [Task Graph](/F:/NewEngine/plans/stage1/task-graph.md)
- [Delegation Spec](/F:/NewEngine/plans/stage1/delegation-spec.md)
- [Handoff Format](/F:/NewEngine/plans/stage1/handoff-format.md)
- [User Intervention Checklist](/F:/NewEngine/plans/stage1/user-interventions.md)
- [ELI5 Manual Verification](/F:/NewEngine/plans/stage1/manual-verification.md)

## Current Status

The Stage 1 planning bundle now exists. The next planning pass should fill in the task-specific specs referenced by the delegation document, starting with:

1. the Phase 0 execution package
2. the G1 evidence package template
3. the Phase 1 implementation package

## AI Task Template

Every delegated AI task should define:

- `Goal`: the narrow problem being solved
- `Inputs`: docs, files, assumptions, and dependencies
- `Work`: what the agent is allowed to do
- `Output`: the exact artifact to produce
- `Definition of done`: objective completion criteria
- `Escalate to user when`: the blocking conditions that require human input
- `Verification`: automated test, static review, or ELI5 manual instructions

## Planned Stage 1 Delegation

### Task 1: Stage 1 Work Breakdown

- `Goal`: break Stage 1 into execution-ready tasks across Phase 0, Phase 1, and Phase 2
- `Output`: a dependency-ordered task graph with task IDs and acceptance criteria
- `Escalate to user when`: task boundaries materially change architecture or timeline assumptions

### Task 2: PHC Bridge Spec

- `Goal`: define the `PoseSearch -> PHC -> Physics Control` bridge at planning level
- `Output`: bridge spec covering observation fields, pose/velocity representation, coordinate assumptions, output mapping, and fallback assumptions
- `Escalate to user when`: the chosen PHC/ProtoMotions contract creates a tradeoff that changes Stage 1 scope

### Task 3: SMPL <-> UE5 Retargeting Spec

- `Goal`: define the mapping and validation plan for retargeting
- `Output`: mapping table, unresolved transform risks, and verification cases
- `Escalate to user when`: the mannequin skeleton choice or mapping policy needs product-level approval

### Task 4: Test Strategy

- `Goal`: decide what is covered by TDD, what is covered by integration tests, and what remains manual
- `Output`: test matrix covering unit, integration, automation, and manual verification
- `Escalate to user when`: a critical claim cannot be automated and needs explicit manual signoff

### Task 5: Human Intervention Plan

- `Goal`: identify the exact points where the user must act
- `Output`: checklist of user-owned actions, prerequisites, and expected evidence of completion
- `Typical user-owned work`: installing UE5, supplying datasets, judging visual quality gates, and running editor-only workflows

### Task 6: ELI5 Manual Verification Pack

- `Goal`: standardize human-readable test steps for all non-automatable checks
- `Output`: ELI5 instructions per manual checkpoint, including expected good/bad outcomes
- `Escalate to user when`: a manual check is ambiguous or subjective enough to need a clearer rubric

## Expected Output Format Per AI Task

Each task should end with:

1. `Decision summary`
2. `Produced artifact`
3. `Open risks`
4. `Blocked by user? yes/no`
5. `If manual verification is needed`: an ELI5 checklist

## User Intervention Rules

The user is required to intervene when:

- a tool or dependency must be installed outside the repo
- a license, dataset, or external download must be accepted
- a UE5 editor workflow must be performed manually
- visual quality must be judged by a human
- an ambiguous planning tradeoff changes scope, time, or architecture

The user is not required to intervene for:

- internal planning decomposition
- doc updates
- task reshaping within the locked architecture
- drafting automated or manual verification instructions

## ELI5 Manual Verification Template

Use this format for every non-automatable check:

- `What you are checking`
- `Why it matters`
- `What to click or run`
- `What good looks like`
- `What bad looks like`
- `What evidence to send back`

## Exit Condition For Planning

Stage 1 planning is complete when:

- every planned task has an owner type (`AI` or `User`)
- every task has a required output artifact
- every user intervention point is explicit
- every non-automatable check has ELI5 instructions
- implementation can start without inventing the execution structure on the fly
