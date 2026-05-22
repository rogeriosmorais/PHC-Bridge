# UE5 Scaffold Readiness Evidence

## Scope

Graph task: `S1-P0-U2`

Acceptance criterion: `UE5 project scaffold created`

## Scaffold Verification

Commands were run from `F:\NewEngine-AgentB` with PowerShell.

| Check | Result |
|---|---|
| Project root | `PhysAnimUE5` exists |
| Project file | `PhysAnimUE5\PhysAnimUE5.uproject` exists |
| Plugin root | `PhysAnimUE5\Plugins\PhysAnimPlugin` exists |
| Plugin descriptor | `PhysAnimUE5\Plugins\PhysAnimPlugin\PhysAnimPlugin.uplugin` exists |
| Content folders | `Characters`, `NNEModels`, `PoseSearch`, `ThirdPerson` exist |
| Manny content | Manny mannequin assets exist under `Content\Characters\Mannequins` |
| NNE policy asset | `Content\NNEModels\phc_policy.onnx` and `phc_policy.uasset` exist |
| Project plugins | `PoseSearch`, `NNERuntimeORT`, `PhysicsControl`, and `PhysAnimPlugin` are enabled in the uproject |
| Plugin dependencies | `PoseSearch`, `NNERuntimeORT`, and `PhysicsControl` are declared in the plugin descriptor |

## Dataset / Checkpoint Note

The historical planning docs refer to `Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`. That tree was not found in this checkout during this verification.

This does not block the scaffold acceptance criterion because the UE project already contains the imported Stage 1 ONNX/NNE policy assets needed by the runtime bridge. It remains a separate offline-training/checkpoint provenance risk if future tasks need to re-export or numerically compare the policy from the original checkpoint.

## Decision

The UE5 project scaffold is present and usable for current Stage 1 runtime work.
