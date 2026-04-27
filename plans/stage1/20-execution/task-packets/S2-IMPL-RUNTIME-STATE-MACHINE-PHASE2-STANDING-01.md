# Task Packet: S2-IMPL-RUNTIME-STATE-MACHINE-PHASE2-STANDING-01

## Purpose
Implement the runtime logic for the `BalanceActive_Standing` state. This involves adding an evaluation function to decide whether a character should remain standing, exit to `BalanceSafeDeny`, or trigger a terminal failure based on support evidence and termination states.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-STATE-MACHINE-PHASE2-STANDING-01.md`

## Forbidden Files
- ALL other runtime files
- Skeleton or ONNX assets

## Required Work
1. **Decision Logic**: Add `EPhysAnimRuntimeState EvaluateBalanceActiveStanding() const` to `UPhysAnimComponent`.
2. **Logic Boundaries**:
   - If `LiveRuntimeEvidenceTerminationState.bTerminated`: return `EPhysAnimRuntimeState::FailStopped`.
   - If `LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportMode == Airborne`: return `EPhysAnimRuntimeState::BalanceSafeDeny`.
   - If `LiveRuntimeEvidenceTerminationState.LatestArtifact.ActiveSupportSideCount == 0`: return `EPhysAnimRuntimeState::BalanceSafeDeny`.
   - Otherwise: return `EPhysAnimRuntimeState::BalanceActive_Standing`.
3. **Loop Integration**:
   - In the runtime state machine loop (likely `TickComponent` logic), when in `BalanceActive_Standing`, call `EvaluateBalanceActiveStanding()`.
   - Transition to the returned state if it differs from the current state.
4. **Logging**:
   - Log: `[PhysAnimBalance] STANDING_ACTIVE_EVAL result=...`.

## Definition of Done
- `PhysAnimPlugin` builds successfully.
- Functional test verifies that a character in `BalanceActive_Standing` exits correctly if support is lost.
- Regression: Current live proof (Airborne 0.495s) MUST NOT enter the standing state.

## Stop Conditions
- Character enters `BalanceActive_Standing` while `Airborne`.
- Crash during state transition.
