# Task Packet: S2-FIX-LIVE-SUPPORT-EVIDENCE-MAPPING-01

## Purpose
Fix the truthful live support evidence mapping to resolve the `ActivationSupportFailure` (Airborne mode) being reported by the automated standing proof. The goal is to ensure the character correctly detects floor contact before attempting state transitions.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimRuntimeAdapter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/S2-FIX-LIVE-SUPPORT-EVIDENCE-MAPPING-01.md`

## Forbidden Files
- ALL other runtime files
- Skeleton or ONNX assets

## Required Work
1. **Instrument Live Proof**: Add granular logging to `TickLiveRuntimeEvidenceProof` to capture:
   - Sweep start/end points.
   - Hit results (actor hit, bone hit, distance).
   - Support side mapping outcomes.
2. **Diagnose Airborne Mode**: Determine why `LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportMode` is staying `Airborne`.
3. **Verify Sweep Parameters**:
   - Ensure the sweep depth is sufficient to reach the floor from the character's height.
   - Check if `foot_l` and `foot_r` names are correct in the runtime context.
4. **Fix Mapping Logic**:
   - Repair the logic that converts `FHitResult` into `FPhysAnimSupportArtifact`.
   - Ensure the `ActiveSupportSideCount` is correctly calculated based on real hits.

## Definition of Done
- `PhysAnimPlugin` builds successfully.
- `PhysAnim.StandingProof.Live` test reports `SupportMode != Airborne` (e.g., `Bipedal` or `SingleSupport`).
- No hardcoded support states; must be derived from physics sweeps.

## Stop Conditions
- Character remains `Airborne` after fix.
- Crash during physics sweep.
