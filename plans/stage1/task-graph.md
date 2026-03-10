# Stage 1 Task Graph

## Purpose

This is the execution-order graph for Stage 1 planning and delivery. It covers the planning tasks that must happen before implementation and the execution tasks that follow in Phase 0, Phase 1, and Phase 2.

The graph assumes a single orchestrator agent assigns work, freezes inputs, and accepts handoffs from worker agents.

## Task Graph

| ID | Phase | Owner | Task | Depends On | Output Artifact | User Intervention |
|---|---|---|---|---|---|---|
| S1-PLAN-01 | Planning | AI | Publish the Stage 1 planning bundle | None | `plans/stage1/*` | No |
| S1-PLAN-02 | Planning | AI | Produce the PHC bridge spec | S1-PLAN-01 | `plans/stage1/bridge-spec.md` | Only if the PHC contract changes Stage 1 scope |
| S1-PLAN-03 | Planning | AI | Produce the SMPL <-> UE5 retargeting spec | S1-PLAN-01 | `plans/stage1/retargeting-spec.md` | Only if bone mapping policy needs approval |
| S1-PLAN-04 | Planning | AI | Produce the Stage 1 test strategy | S1-PLAN-01 | `plans/stage1/test-strategy.md` | Only if a critical claim cannot be automated |
| S1-PLAN-05 | Planning | Orchestrator | Maintain the live assumption and failure ledger | S1-PLAN-02, S1-PLAN-03, S1-PLAN-04 | `plans/stage1/assumption-ledger.md` | Only if a user decision is required to resolve a red assumption |
| S1-P0-U1 | Phase 0 | User | Install toolchain and accept external licenses | S1-PLAN-02, S1-PLAN-04 | evidence note with versions, paths, and accepted prerequisites | Yes |
| S1-P0-U2 | Phase 0 | User | Provision datasets and create UE5 project scaffold | S1-P0-U1 | dataset and project availability confirmed | Yes |
| S1-P0-A1 | Phase 0 | AI | Prepare feasibility execution package for ProtoMotions, UE5 control-path checks, and G1 evidence capture | S1-PLAN-02, S1-PLAN-03, S1-PLAN-04, S1-PLAN-05, S1-P0-U2 | implementation-ready command and validation package | No |
| S1-P0-A2 | Phase 0 | AI | Execute or guide feasibility validation and collect results against G1 | S1-P0-A1 | G1 evidence package and gap list | Yes for editor-only steps and visual judgment |
| S1-P1-A1 | Phase 1 | AI | Produce the single-character implementation package | S1-P0-A2 | implementation-ready package for `PhysAnimPlugin`, NNE integration, and PoseSearch wiring | No |
| S1-P1-A2 | Phase 1 | AI | Produce the stabilization and tuning package that makes the first end-to-end UE runtime safe to judge | S1-P1-A1 | stabilization checkpoint, tuning order, evidence checklist, and updated G2 entry criteria | Yes for runtime evidence and later visual comparison |
| S1-P1-U1 | Phase 1 | User | Run G2 evaluation and approve or reject Stage 1 quality | S1-P1-A2 | human judgment with evidence | Yes |
| S1-P2-A1 | Phase 2 | AI | Produce the locomotion showcase demo package | S1-P1-U1 | implementation-ready package for demo flow, optional duplication, camera, and HUD | No |
| S1-P2-A2 | Phase 2 | AI | Produce the final demo verification package for G3 | S1-P2-A1 | demo checklist, observer script, and evidence checklist | Yes for observer feedback |
| S1-P2-U1 | Phase 2 | User | Run G3 evaluation and decide whether Stage 2 work is justified | S1-P2-A2 | human judgment with evidence | Yes |

## Gating Rules

- Do not start Phase 0 execution until the bridge spec, retargeting spec, and test strategy exist.
- Do not start Phase 0 execution until the orchestrator has reviewed those specs into the live assumption ledger.
- Do not start Phase 1 until G1 evidence shows the Manny/Chaos path works and the bridge contract is locked.
- Do not start Phase 2 until the user approves G2.
- Do not consider Stage 1 complete until the user runs G3 and records a decision.
- Do not launch parallel worker tasks unless the orchestrator has confirmed that dependencies are frozen and writable outputs do not overlap.

## Output Rules

- Every AI-owned task must end with a written artifact in the repo or a clearly named result bundle.
- Every user-owned task must specify what evidence the user needs to provide back.
- Every gate must have both objective checks and a human-readable conclusion.
- The orchestrator is the only role allowed to accept, reject, or merge outputs across multiple worker tasks.
