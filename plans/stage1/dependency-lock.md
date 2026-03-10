# Stage 1 Dependency Lock

## Purpose

This is the exact-version and exact-path lock sheet for Stage 1 execution.

Use it after the user returns with setup evidence. Do not treat the environment as stable until this file is filled with real values.

## Status Meanings

- `planned`: expected value from the planning docs
- `confirmed`: observed value on the real machine
- `blocked`: required value not yet available

## Toolchain Lock

| Item | Planned Value | Confirmed Value | Status | Notes |
|---|---|---|---|---|
| UE version | `5.7.3` | `5.7.3` | confirmed | read from `E:\UE_5.7\Engine\Build\Build.version` |
| UE install root | `E:\UE_5.7` | `E:\UE_5.7` | confirmed | path exists locally |
| `UE5_PATH` | `E:\UE_5.7\Engine` | `E:\UE_5.7\Engine` | confirmed | path exists locally |
| Python version | `3.11` | `3.11.9` | confirmed | updated to match the Isaac Sim `5.x` requirement |
| ProtoMotions env | `physanim_proto` | `F:\NewEngine\Training\.venv\physanim_proto311` | confirmed | editable installs and `requirements_isaaclab.txt` completed successfully in the `3.11` environment |
| Python ONNX validation runtime | `onnxruntime` | `1.24.3` | confirmed | installed into the locked training env for offline export parity validation |
| optional PyRoki env | `physanim_pyroki` |  | planned | only if needed |

## External Source Lock

| Item | Planned Value | Confirmed Value | Status | Notes |
|---|---|---|---|---|
| ProtoMotions release | `v2.3.2` | `v2.3.2` | confirmed | cloned locally at tag `v2.3.2` |
| ProtoMotions path | `F:\NewEngine\Training\ProtoMotions` | `F:\NewEngine\Training\ProtoMotions` | confirmed | repo cloned locally; local compatibility patches now include Python `3.11` dataclass fixes, IsaacLab app-launch and keyboard compatibility, and MoviePy `2.x` import compatibility |
| simulator path | `isaaclab` | `isaaclab` | confirmed | pretrained path remains pinned to IsaacLab |
| Isaac Sim package | `5.1.x` | `5.1.0.0` | confirmed | installed into the locked Python `3.11` env via pip |
| Isaac Lab package | `2.3.x` | `2.3.2.post1` | confirmed | installed into the locked Python `3.11` env via pip; local visual eval path excludes broken RTX sensor extensions because this machine currently reports missing dependent DLLs for those plugins |
| IsaacLab install path | user-chosen install path | `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaaclab.exe` | confirmed | Windows launcher exists locally |
| IsaacSim install path | user-chosen install path | `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaacsim.exe` | confirmed | Windows launcher exists locally |
| host platform for ProtoMotions | `Windows-native first; WSL only if blocked` | `Windows-native` | confirmed | current setup attempt stayed Windows-native |
| pretrained source | `ProtoMotions repo-bundled motion_tracker/smpl asset` | `ProtoMotions repo-bundled motion_tracker/smpl asset` | confirmed | the cloned repo already contains the pretrained model folder and README |
| pretrained checkpoint path | `Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/last.ckpt` | `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt` | confirmed | checkpoint exists locally |
| ONNX export entry point | `F:\NewEngine\Training\scripts\export_onnx.py` | `F:\NewEngine\Training\scripts\export_onnx.py` | confirmed | explicit Stage 1 export script now exists and exports the pretrained `motion_tracker/smpl` actor path |
| ONNX opset | `17 -> 16 -> 15 fallback order` | `17` | confirmed | exported successfully with opset `17`; no fallback was needed |
| Unreal runtime target | `NNERuntimeORTDml` | `NNERuntimeORTDml` | confirmed | startup-success line reached in UE on March 10, 2026; CPU fallback remains debug-only |

## Dataset Lock

| Item | Planned Value | Confirmed Value | Status | Notes |
|---|---|---|---|---|
| AMASS root | `F:\NewEngine\Training\data\amass` |  | blocked | not present, but not required for the current pretrained-first checkpoint |
| combat clips root | not required for locomotion-only Stage 1 |  | planned | deferred out of Stage 1 scope |
| UE project root | `F:\NewEngine\PhysAnimUE5` | `F:\NewEngine\PhysAnimUE5` | confirmed | project exists locally and `PhysAnimUE5.uproject` is present |
| exported Stage 1 ONNX path | `F:\NewEngine\Training\output\phc_policy.onnx` | `F:\NewEngine\Training\output\phc_policy.onnx` | confirmed | export completed on March 10, 2026 with numeric parity max abs diff `1.64e-7` against the PyTorch actor wrapper |
| UE ONNX import source path | `F:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx` | `F:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx` | confirmed | copied into the UE content import location; the `UNNEModelData` asset still requires Unreal import/editor validation |

## Orchestrator Rule

When any planned value changes:

1. update this file
2. update [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
3. update [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md) if the change affects risk
