# Task Packet: S2-IMPL-RUNTIME-STATE-MACHINE-PHASE1-ENTRY-01

## Purpose
Implement the first explicit runtime state-machine entry gate from `BalanceSafeDeny` to `BalanceActive_Standing`. This task must preserve the current truthful failure of the automated standing proof.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-STATE-MACHINE-PHASE1-ENTRY-01.md`

## Forbidden Files
- ALL other runtime files
- Skeleton or ONNX assets

## Required Work
1. **Decision Logic**: Add `bool CanEnterBalanceActiveStanding() const` to `UPhysAnimComponent`.
2. **Deny Conditions**:
   - `LiveRuntimeEvidenceTerminationState.bTerminated == true`
   - `LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportMode == EPhysAnimSupportMode::Airborne`
   - `LiveRuntimeEvidenceTerminationState.LatestArtifact.ActiveSupportSideCount == 0`
   - `LiveRuntimeEvidenceStandingSeconds < 3.0s`
3. **Transition Hook**:
   - In `TickComponent` or the state transition logic, call `CanEnterBalanceActiveStanding()`.
   - If denied, log: `[PhysAnimBalance] ENTRY_DENIED reason=...`.
4. **Preserve Failure**: Ensure the current `Airborne` 0.495s proof remains in `BalanceSafeDeny`.

## Definition of Done
- `PhysAnimPlugin` builds successfully.
- `PhysAnim.StandingProof.Live` test results in a `DENIED` log entry.
- State remains `BalanceSafeDeny` for the failing character.

## Stop Conditions
- State transitions to `BalanceActive_Standing` for the failing character (Governance Leak).
- Crash during state transition.
