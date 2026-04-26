# Task Packet: S2-IMPL-PROOF-ARTIFACT-JSON-EMITTER-01

## Purpose
Implement a centralized proof logging and JSON artifact emission system. This enables machine-readable terminal artifacts for offline pipeline ingestion.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/PhysAnimPlugin.Build.cs`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimProofArtifactEmitter.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimProofArtifactEmitter.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/30-evidence/S2-IMPL-PROOF-ARTIFACT-JSON-EMITTER-01.md`

## Forbidden Files
- `PhysAnimSupportTruth.h` / `.cpp` (Logic stability)
- `PhysAnimComponent.Core.cpp`

## Required Work
1. **Module Dependency**: Add `Json` and `JsonUtilities` to `PhysAnimPlugin.Build.cs`.
2. **New Class**: Implement `FPhysAnimProofArtifactEmitter` (Static helper).
3. **JSON Serialization**: Manually serialize `FPhysAnimRunArtifactSnapshot` to a JSON object.
4. **File I/O**: Write JSON to `FPaths::ProjectSavedDir() / TEXT("PhysAnim/ProofArtifacts/")`.
5. **Refactor Component**:
   - Remove direct `UE_LOG` calls from `UPhysAnimComponent`.
   - Call `FPhysAnimProofArtifactEmitter::EmitTerminalArtifact` on failure.
   - Call `FPhysAnimProofArtifactEmitter::LogProgress` for per-tick telemetry.

## Required Tests/Build
- Build must pass.
- Smoke tests `PhysAnim.RuntimeTermination` must pass.

## Definition of Done
- `PhysAnimProofArtifactEmitter` implemented and integrated.
- JSON file successfully created in `Saved/` upon simulated failure in smoke tests (if applicable) or confirmed via code review.
- Handoff report with commit SHA.

## Stop Conditions
- Build failure in `Json` module linkage.
- Logic regression in `PhysAnim.RuntimeTermination`.
