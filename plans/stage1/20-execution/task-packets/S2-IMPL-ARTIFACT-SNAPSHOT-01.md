# S2-IMPL-ARTIFACT-SNAPSHOT-01 - Value-Only Run Artifact Snapshot

## Purpose

Create a value-only run artifact snapshot that mirrors the Stage 1 artifact schema without adding runtime emission behavior.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-IMPL-ARTIFACT-SNAPSHOT-01.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Forbidden Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComparisonSubsystem.cpp`
- runtime state-machine files
- bridge activation files
- runtime adapter files
- PhysicsControl setup files
- capsule/CMC runtime behavior files
- JSON/artifact emission files
- workflow/process files other than `execution-log.md`

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/20-execution/balance_first_refactor_plan.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

## Required Work

1. Add `FPhysAnimRunArtifactSnapshot` value fields named after `instrumentation_and_acceptance.md` where they represent artifact data.
2. Add a value-only builder input and `BuildRunArtifactSnapshot`.
3. Copy existing validator results into artifact fields without recomputing validator decisions.
4. Preserve nullable proxy semantics with `TOptional`.
5. Do not serialize JSON.
6. Do not emit artifacts.
7. Do not read live runtime state.
8. Do not alter bridge, state-machine, adapter, PhysicsControl, capsule/CMC, or comparison subsystem behavior.

## Required Commands

- `.\\scripts\\build.ps1`
- `.\\scripts\\build.ps1 -Test PhysAnim.Validators.ArtifactSnapshot`

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-IMPL-ARTIFACT-SNAPSHOT-01.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- artifact snapshot builder tests pass
- build passes
- snapshot remains value-only
- no artifact emission file is changed
- no runtime behavior file is changed
- scope check passes
- one task commit is created
- `execution-log.md` advances to the next explicitly created task or `none`

## Stop Conditions

Stop immediately if:
- the snapshot requires live runtime objects
- JSON emission or `PhysAnimComparisonSubsystem.cpp` changes are needed
- terminal arbitration beyond copying validator results is needed
- a runtime file appears in the diff
- the same conceptual failure happens twice

## Required Handoff

```text
Summary:
Task: S2-IMPL-ARTIFACT-SNAPSHOT-01
Base:
Head:
Commit:
Build:
Tests:
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task: next explicit packet, blocked, or none
```
