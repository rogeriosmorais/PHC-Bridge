# Task Packet: S2-IMPL-RUNTIME-STATE-MACHINE-PHASE3-ACTIVE-SUPPORT-ENFORCEMENT-01

## Purpose
Harden the standing state machine by enforcing active support evidence (hull area, gap timers, proxy location) during both entry and maintenance. This ensures the character only sim-activates when truly supported and exits immediately if stability is lost.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-STATE-MACHINE-PHASE3-ACTIVE-SUPPORT-ENFORCEMENT-01.md`

## Forbidden Files
- ALL other runtime files
- Skeleton or ONNX assets

## Required Work
1. **Decision Decision**: Add `bool ShouldExitStandingToSafeDeny(const FPhysAnimRuntimeTerminationState& TerminationState) const` to `UPhysAnimComponent`.
2. **Harden Entry**: Update `CanEnterBalanceActiveStanding()` to enforce:
   - `bEnableLiveRuntimeEvidenceProof == true`
   - `bLiveRuntimeEvidenceProofActive || bLiveRuntimeEvidenceProofComplete`
   - `LatestArtifact.SupportHullAreaCm2 > 0`
   - `LatestArtifact.SupportGapTimerMs` < `BalancePhase1AdmissionMaxSupportGapMs` (100.0ms)
   - `LatestArtifact.ProxyInsideHull` is true (if set)
   - `LatestArtifact.ProxyOutsideHullDurationMs` < `ProxyDriftLimitMs` (100.0ms)
3. **Harden Maintenance**: Update `EvaluateBalanceActiveStanding()` to enforce:
   - `bTerminated -> FailStopped`
   - `ShouldExitStandingToSafeDeny() -> BalanceSafeDeny`
4. **Implementation of ShouldExit**:
   - Deny if `SupportHullAreaCm2 <= 0`
   - Deny if `SupportGapTimerMs >= 100.0ms`
   - Deny if `ProxyInsideHull` is false (and set)
   - Deny if `ProxyOutsideHullDurationMs >= 100.0ms`

## Definition of Done
- `PhysAnimPlugin` builds successfully.
- `PhysAnim.StandingProof.Live` remains `BalanceSafeDeny` (due to failing 3s duration or settlement).
- Code audit confirms all Phase 3 evidence fields are checked.

## Stop Conditions
- Premature entry into Active Standing for failing proof.
- Crash during state transition.
