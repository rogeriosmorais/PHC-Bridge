# S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01 — Activation Proof Review and Lock

## Purpose

Review the full activation proof chain and lock the current baseline before any tuning or broader runtime integration.

Current proof state:

```text
S2-IMPL-ACTIVATION-PATH-WIRING-01: PASS
S2-PROOF-G2-SIDE-BY-SIDE-ACTIVATION-01: PASS
G2 presentation: stable 30s physics character in BalanceActive_Standing beside kinematic baseline
```

This task is a review/governance lock, not implementation.

## Classification

Proof review / baseline lock.

This is not:
- tuning
- activation rewrite
- support modification
- validator modification
- adapter modification
- PhysicsControl redesign
- locomotion work

## Assumed Current Head

```text
5d17fe5
```

Implementing agent must report exact base/head SHAs.

## Allowed Files

```text
plans/stage1/20-execution/task-packets/S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01.md
plans/stage1/20-execution/evidence/S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01.md
plans/stage1/20-execution/execution-log.md
```

Optional only if a typo or stale task reference is found in evidence:

```text
plans/stage1/20-execution/evidence/S2-IMPL-ACTIVATION-PATH-WIRING-01.md
plans/stage1/20-execution/evidence/S2-PROOF-G2-SIDE-BY-SIDE-ACTIVATION-01.md
```

## Forbidden Files

```text
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/*
PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/*
PhysAnimRuntimeAdapter.*
PhysAnimRuntimeOrchestrator.*
PhysAnimRuntimeTermination.*
PhysAnimRuntimeTerminationState.*
PhysAnimRuntimeTerminationPipeline.*
PhysAnimValidators.*
PhysAnimSupportTruth.*
PhysAnimFailureArbitration.*
control tuning files
locomotion tuning files
PoseSearch tuning files
mass tuning files
PhysicsControl redesign files
assets
ONNX files
```

## Required Work

### 1. Review proof chain

Confirm evidence exists for:

```text
Automated standing proof positive PASS
Automated negative support FAIL
Support hull area fix
State-machine Phase 1 entry gate
State-machine Phase 2 standing logic
Activation path wiring
G2 side-by-side activation proof
```

### 2. Confirm locked baseline facts

Record:

```text
SupportHullAreaCm2 = 1029.3 cm2 or latest verified value
Runtime hits = 10 or latest verified value
Positive standing proof reaches >= 3.0s
Negative support proof fails with ActivationSupportFailure
ActivationPath.Wiring passes 4/4
G2 physics character remains stable in BalanceActive_Standing for 30s
Kinematic and physics sides use same sequence/camera/start frame
```

### 3. Confirm no current blockers

The review must explicitly state whether any of these remain unresolved:

```text
stubbed capsule validation
stubbed continuity validation
support evidence fragility
weak StandingProof.Live assertions
JSON artifact mismatch risk
workflow stale blockers
activation bypass risk
negative case regression risk
```

If any are unresolved, do not mark baseline locked.

### 4. Lock baseline

If all checks pass, mark:

```text
Activation proof baseline: LOCKED
```

and update `execution-log.md`:

```text
Current Task ID = none
Current Task Packet = none
Status = complete
Workflow Note = Activation proof baseline locked after automated positive/negative proof and G2 30s side-by-side presentation.
Next Runnable Task = none
```

### 5. Recommend next phase only

Do not start the next phase in this task.

Recommended next phase should be one of:

```text
S2-PLAN-POST-ACTIVATION-TUNING-01
S2-PLAN-BROAD-RUNTIME-INTEGRATION-01
S2-CLEANUP-PROOF-INFRASTRUCTURE-01
```

Choose only one recommendation based on evidence.

## Required Commands

```powershell
.\scripts\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01.md -WorkingTree -AllowExecutionLog -AllowEvidence
.\scripts\check_workflow_state.ps1 -Mode status -Strict
```

No build is required unless evidence references are unclear.

## Required Evidence File

Create:

```text
plans/stage1/20-execution/evidence/S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01.md
```

Use this template:

```markdown
# S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01 Evidence

## Task

S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01

## Base / Head

```text
Base:
Head:
Commit:
```

## Reviewed Proof Chain

```text
Automated standing proof positive PASS:
Automated negative support FAIL:
Support hull area fix:
State-machine Phase1 entry:
State-machine Phase2 standing:
Activation path wiring:
G2 side-by-side proof:
```

## Locked Baseline Facts

```text
Positive proof duration:
Positive terminal reason:
Positive support mode:
Positive support hull area:
Positive runtime hit count:
Positive mapped support hit count:

Negative expected reason:
Negative actual reason:
Negative artifact validated:

ActivationPath.Wiring result:
G2 presentation result:
G2 stable duration:
G2 right-side state:
G2 left-side source:
Same sequence:
Same camera:
Same start frame:
```

## Risk Review

```text
Stubbed capsule validation unresolved:
Stubbed continuity validation unresolved:
Support evidence fragility unresolved:
Weak StandingProof.Live assertions unresolved:
JSON artifact mismatch risk unresolved:
Workflow stale blockers unresolved:
Activation bypass risk unresolved:
Negative case regression risk unresolved:
```

## Lock Decision

```text
Activation proof baseline locked: yes/no
Reason:
```

## Recommended Next Phase

```text
Recommended task:
Reason:
```

## Scope / Workflow

```text
Scope check:
Workflow check:
Files changed:
Forbidden files touched:
Working tree:
```
```

## Stop Conditions

Stop immediately if:

```text
1. Any required evidence file is missing.
2. Positive proof does not show >= 3.0s with terminal_reason=None.
3. Negative support proof does not fail with ActivationSupportFailure.
4. G2 proof does not show the physics character stable in BalanceActive_Standing.
5. Evidence shows kinematic and physics sides used different sequence/camera/start frame.
6. Any source/runtime file must be edited.
7. Workflow state cannot be made internally consistent.
```

## Definition Of Done

```text
1. Activation proof chain reviewed.
2. Risks explicitly checked.
3. Baseline locked or lock denied with reason.
4. Evidence file filled.
5. execution-log.md updated.
6. Scope check passes.
7. Workflow check passes.
8. One task commit created.
```

## Required Handoff

```text
Summary:
Task: S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01
Base:
Head:
Commit:
Working tree:
Proof chain reviewed:
Baseline locked:
Lock denial reason:
Recommended next phase:
Evidence file:
Scope check:
Workflow check:
Files changed:
Forbidden files touched:
```
