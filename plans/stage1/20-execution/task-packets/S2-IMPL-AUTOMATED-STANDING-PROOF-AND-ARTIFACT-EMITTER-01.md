# Task Packet: S2-IMPL-AUTOMATED-STANDING-PROOF-AND-ARTIFACT-EMITTER-01

## Purpose
Implement the machine-readable artifact emission system and the automated functional test harness to support headless Standing Proof validation.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimProofArtifactEmitter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimProofArtifactEmitter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/PhysAnimPlugin.Build.cs`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-AUTOMATED-STANDING-PROOF-AND-ARTIFACT-EMITTER-01.md`
- `plans/stage1/30-evidence/S2-PROOF-EDITOR-STANDING-ATTEMPT-01.md`

## Forbidden Files
- ALL other runtime files
- Skeleton or ONNX assets

## Required Work
1. **Emitter Implementation**:
   - Implement `FPhysAnimProofArtifactEmitter` with JSON serialization support via `JsonUtilities`.
   - Add `Json` and `JsonUtilities` dependencies to `Build.cs`.
2. **Component Refactor**:
   - Update `UPhysAnimComponent` to delegate telemetry and terminal artifact emission to the static Emitter.
   - Ensure `bEnableLiveRuntimeEvidenceProof` is correctly integrated into the component tick.
3. **Automated Test**:
   - Implement `PhysAnim.StandingProof.Live` functional test that:
     - Loads `Lvl_ThirdPerson`.
     - Finds the component.
     - Waits for simulation (3.0s+).
     - Validates that artifacts are produced.

## Definition of Done
- `PhysAnimPlugin` builds successfully with new dependencies.
- `PhysAnim.StandingProof.Live` test passes (loads map and runs without crash).
- `PhysAnimProof:` logs appear in the output log.
- JSON artifacts are produced in `Saved/PhysAnim/ProofArtifacts/`.

## Stop Conditions
- Build failure due to missing JSON modules.
- Test fails to find the component in the PIE world.
