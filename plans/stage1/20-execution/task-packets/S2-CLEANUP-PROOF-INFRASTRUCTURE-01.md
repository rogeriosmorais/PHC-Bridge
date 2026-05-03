# S2-CLEANUP-PROOF-INFRASTRUCTURE-01 — Replace stubs and close activation bypass

## Purpose

Replace remaining LiveProof stubs with real runtime validation and close the activation bypass risk to ensure the system only activates on verified proof.

## Objective

1. Integrate real capsule validation into LiveProof.
2. Integrate real continuity validation into LiveProof.
3. Ensure bEnableLiveRuntimeEvidenceProof alone cannot activate.
4. Preserve positive proof, negative proof, activation wiring, and G2 presentation.
5. Clean execution-log.md so it reflects b590a70 and the next runnable task.

## Required First Step

Update `execution-log.md` to include the completed review task:
`S2-REVIEW-ACTIVATION-PROOF-AND-LOCK-01 = b590a70`

Then set:
- **Current Task ID**: S2-CLEANUP-PROOF-INFRASTRUCTURE-01
- **Current Task Packet**: plans/stage1/20-execution/task-packets/S2-CLEANUP-PROOF-INFRASTRUCTURE-01.md
- **Status**: runnable
- **Latest Technical Head**: b590a70
- **Workflow Note**: Activation proof baseline locked; cleanup now targets real capsule/continuity validation and activation bypass closure.

Fix the malformed "Next Action" section.

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimActivationPath.Tests.cpp`
- `plans/stage1/20-execution/task-packets/S2-CLEANUP-PROOF-INFRASTRUCTURE-01.md`
- `plans/stage1/20-execution/evidence/S2-CLEANUP-PROOF-INFRASTRUCTURE-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- `PhysAnimRuntimeAdapter.*`
- `PhysAnimRuntimeOrchestrator.*`
- `PhysAnimRuntimeTermination.*`
- `PhysAnimRuntimeTerminationState.*`
- `PhysAnimRuntimeTerminationPipeline.*`
- `PhysAnimValidators.*`
- `PhysAnimSupportTruth.*`
- `PhysAnimFailureArbitration.*`
- control tuning
- locomotion tuning
- PoseSearch tuning
- mass tuning
- PhysicsControl redesign
- assets
- ONNX files

## Core Requirements

### 1. Remove capsule stub
LiveProof must use real capsule capture/validation data. Do not fake `capsule_valid = true`.
Required live fields:
- capsule collision enabled
- capsule generate overlap events
- capsule world position
- capsule lock delta
- mesh absolute transform flags
- CMC active
- CMC tick enabled
- CMC updated component null
- capsule terminal reason if invalid

### 2. Remove continuity stub
LiveProof must stop hardcoding `bPhysicalContinuityValidatorPassed = true`. Do not fake `continuity_valid = true`.
Required live fields:
- pelvis/root body
- critical body names
- pelvis sleep duration ms
- bookkeeping continuity
- bookkeeping mismatch
- physical continuity validator result
- continuity terminal reason if invalid

### 3. Close activation bypass
Activation must not proceed merely because `bEnableLiveRuntimeEvidenceProof = true`.
Activation may proceed only when:
- proof complete
- duration >= 3.0s
- terminal_reason = None
- support valid
- capsule valid
- continuity valid
- entry gate allowed
- standing logic valid
- artifact/audit state consistent

### 4. Preserve locked proof behavior
Required regressions must remain green:
- StandingProof.Live: PASS
- StandingProof.NegativeSupport: FAIL with ActivationSupportFailure
- ActivationPath.Wiring: PASS
- PIE.G2Presentation: PASS
- SupportHullAreaCm2 > 0

## Required Commands

```powershell
.\scripts\build.ps1
.\scripts\build.ps1 -Test PhysAnim.StandingProof.Live
.\scripts\build.ps1 -Test PhysAnim.StandingProof.NegativeSupport
.\scripts\build.ps1 -Test PhysAnim.ActivationPath.Wiring
.\scripts\build.ps1 -Test PhysAnim.PIE.G2Presentation
.\scripts\build.ps1 -Test PhysAnim.RuntimeTermination
.\scripts\build.ps1 -Test PhysAnim.StateMachine.Phase1Entry
.\scripts\build.ps1 -Test PhysAnim.StateMachine.Phase2Standing
```

## Required Evidence

Create `plans/stage1/20-execution/evidence/S2-CLEANUP-PROOF-INFRASTRUCTURE-01.md` including:
- Base/Head/Commit/Build
- All Test results (Live, NegativeSupport, Wiring, G2, Termination, Phase1/2)
- Capsule/Continuity validation sources and stub removal confirmation
- Activation bypass closure confirmation
- Positive/Negative proof results
- G2 regression status
- JSON validation and Support Hull Area

## Stop Conditions

Stop immediately if:
1. Real capsule/continuity validation requires validator/adapter changes.
2. Capsule or continuity validity must be faked.
3. Activation can proceed from `bEnableLiveRuntimeEvidenceProof` alone.
4. Positive standing proof regresses.
5. Negative support proof stops failing with `ActivationSupportFailure`.
6. G2 presentation regresses.
7. Support hull area regresses to 0.0.
8. JSON/audit artifact disagrees with component state.

## Definition of Done

1. Capsule and continuity stubs removed and replaced with real runtime data.
2. Activation bypass closed.
3. All regression tests pass.
4. `execution-log.md` updated and fixed.
5. Evidence file completed.
6. Scope and workflow checks pass.

## Required Handoff

Task: S2-CLEANUP-PROOF-INFRASTRUCTURE-01
Base/Head/Commit/Working tree
Build and Test results
Capsule/Continuity validation status
Activation bypass status
Evidence file path
Next task recommendation: S2-PLAN-POST-ACTIVATION-TUNING-01
