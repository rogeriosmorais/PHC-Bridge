# Stage 1 Plan

## Purpose

This document is the **Stage 1 index and control document**.

It owns the Stage 1 planning structure, execution model, and artifact map.

It is **not** the most detailed source for every execution lock. The detailed, phase-specific truth lives in the planning bundle under `plans/stage1/`.

Use it for:

- sequencing Stage 1 work
- delegating AI agent tasks
- defining the output artifact for each task
- identifying when the human user must intervene
- writing ELI5 manual verification instructions for anything that cannot be automated

Do not use `AGENTS.md` as a rolling status or next-steps document.
Do not expect this file to duplicate every detailed lock from the planning bundle.

## Current Execution Focus

The planning bundle is now execution-ready. The next step is **Phase 0 evidence collection for Gate G1**, not more broad planning or environment discovery.

The current Stage 1 execution focus is:

1. lock the exact PHC observation/action contract from the frozen local ProtoMotions sources
2. capture the G1 training-side and UE-side evidence package
3. update the assumption ledger with real setup and gate evidence
4. decide whether G1 is `pass`, `fail`, or `blocked`

## Stage 1 Planning Outputs

Before implementation starts, this plan must produce these concrete artifacts. The current planning bundle lives under `plans/stage1/`.

1. A task graph for Phase 0, Phase 1, and Phase 2
2. A delegation spec for each AI task
3. A handoff format for completed tasks
4. A user-intervention checklist
5. An ELI5 manual-test template for non-automatable validation

## Planning Bundle

These files are the detailed source of truth for Stage 1 execution planning and gate handling. When a detail in this file is summarized, the linked artifact below is the more precise reference.

- [Task Graph](/F:/NewEngine/plans/stage1/task-graph.md)
- [Orchestration Model](/F:/NewEngine/plans/stage1/orchestration.md)
- [Assumption Ledger](/F:/NewEngine/plans/stage1/assumption-ledger.md)
- [Bridge Spec](/F:/NewEngine/plans/stage1/bridge-spec.md)
- [Retargeting Spec](/F:/NewEngine/plans/stage1/retargeting-spec.md)
- [Test Strategy](/F:/NewEngine/plans/stage1/test-strategy.md)
- [Motion Set](/F:/NewEngine/plans/stage1/motion-set.md)
- [Pretrained Model Selection](/F:/NewEngine/plans/stage1/pretrained-model-selection.md)
- [Pretrained Checkpoint Retrieval](/F:/NewEngine/plans/stage1/pretrained-checkpoint-retrieval.md)
- [Environment Spec](/F:/NewEngine/plans/stage1/environment-spec.md)
- [Motion Source Map](/F:/NewEngine/plans/stage1/motion-source-map.md)
- [Motion Source Lock Table](/F:/NewEngine/plans/stage1/motion-source-lock-table.md)
- [Comparison Sequence Lock](/F:/NewEngine/plans/stage1/comparison-sequence-lock.md)
- [UE Project Scaffold](/F:/NewEngine/plans/stage1/ue-project-scaffold.md)
- [Execution Log](/F:/NewEngine/plans/stage1/execution-log.md)
- [Dependency Lock](/F:/NewEngine/plans/stage1/dependency-lock.md)
- [Acceptance Thresholds](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- [ONNX Export Spec](/F:/NewEngine/plans/stage1/onnx-export-spec.md)
- [Phase 0 Execution Package](/F:/NewEngine/plans/stage1/phase0-execution-package.md)
- [Phase 1 Implementation Package](/F:/NewEngine/plans/stage1/phase1-implementation-package.md)
- [Phase 2 Demo Package](/F:/NewEngine/plans/stage1/phase2-demo-package.md)
- [G1 Evidence](/F:/NewEngine/plans/stage1/g1-evidence.md)
- [G2 Evaluation](/F:/NewEngine/plans/stage1/g2-evaluation.md)
- [G3 Evaluation](/F:/NewEngine/plans/stage1/g3-evaluation.md)
- [Task Packet S1-P0-A1](/F:/NewEngine/plans/stage1/task-packet-s1-p0-a1.md)
- [Task Packet S1-P0-A2](/F:/NewEngine/plans/stage1/task-packet-s1-p0-a2.md)
- [Task Packet S1-P1-A1](/F:/NewEngine/plans/stage1/task-packet-s1-p1-a1.md)
- [Task Packet S1-P1-A2](/F:/NewEngine/plans/stage1/task-packet-s1-p1-a2.md)
- [Task Packet S1-P2-A1](/F:/NewEngine/plans/stage1/task-packet-s1-p2-a1.md)
- [Task Packet S1-P2-A2](/F:/NewEngine/plans/stage1/task-packet-s1-p2-a2.md)
- [User Return Template](/F:/NewEngine/plans/stage1/user-return-template.md)
- [ELI5 UE Project Setup](/F:/NewEngine/plans/stage1/eli5-ue-project-setup.md)
- [Delegation Spec](/F:/NewEngine/plans/stage1/delegation-spec.md)
- [Handoff Format](/F:/NewEngine/plans/stage1/handoff-format.md)
- [User Intervention Checklist](/F:/NewEngine/plans/stage1/user-interventions.md)
- [ELI5 Manual Verification](/F:/NewEngine/plans/stage1/manual-verification.md)

## Current Status

The Stage 1 bundle now covers the major planning, execution-package, and gate-evidence artifacts for Phase 0, Phase 1, and Phase 2, including the PHC bridge spec, the SMPL-to-UE5 retargeting spec, the pretrained-first path, checkpoint retrieval, the environment contract, the motion-source map and lock table, the G2 comparison sequence, the UE scaffold, the live execution log, the dependency lock sheet, the ONNX export path, explicit gate thresholds, and task packets for each AI-owned execution step.

Local Phase 0 setup evidence already confirms:

- UE `5.7.3` at `E:\UE_5.7`
- `UE5_PATH` at `E:\UE_5.7\Engine`
- UE project scaffold at `F:\NewEngine\PhysAnimUE5`
- ProtoMotions `v2.3.2` at `F:\NewEngine\Training\ProtoMotions`
- Python `3.11.9` environment at `F:\NewEngine\Training\.venv\physanim_proto311`
- Isaac Sim `5.1.0.0` and Isaac Lab `2.3.2.post1`
- local pretrained MaskedMimic checkpoint at `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\last.ckpt`

The next useful work is now:

1. completing `S1-P0-A2` by collecting G1 evidence in [g1-evidence.md](/F:/NewEngine/plans/stage1/g1-evidence.md)
2. updating [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md) and [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md) from that evidence
3. making the explicit G1 go / no-go decision before any Phase 1 implementation begins

## How To Read This File

Use this file to answer:

- what Stage 1 is trying to accomplish
- how work is organized
- which planning artifacts exist
- which role owns which kind of decision
- where current Phase 0 execution is anchored at a high level

Use the detailed planning bundle to answer:

- exact checkpoint retrieval path
- exact environment lock values
- exact current execution status and frozen inputs
- exact G1, G2, and G3 evidence requirements
- exact G2 comparison sequence
- exact ONNX export/import policy
- exact task packet and handoff rules

## Execution Model

Stage 1 uses a **hub-and-spoke** execution model:

- one **orchestrator agent** owns planning, sequencing, dependency control, and final integration
- the orchestrator also owns the live assumption and failure ledger
- one or more **worker agents** own individual task IDs and produce exactly one artifact per task
- the **user** intervenes only at explicitly defined checkpoints

Do not run Stage 1 as a group of peer agents editing shared work opportunistically. Use the orchestration rules in [orchestration.md](/F:/NewEngine/plans/stage1/orchestration.md).

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
