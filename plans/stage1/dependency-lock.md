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
| optional PyRoki env | `physanim_pyroki` |  | planned | only if needed |

## External Source Lock

| Item | Planned Value | Confirmed Value | Status | Notes |
|---|---|---|---|---|
| ProtoMotions release | `v2.3.2` | `v2.3.2` | confirmed | cloned locally at tag `v2.3.2` |
| ProtoMotions path | `F:\NewEngine\Training\ProtoMotions` | `F:\NewEngine\Training\ProtoMotions` | confirmed | repo cloned locally |
| simulator path | `isaaclab` |  | blocked | pretrained path is still pinned to IsaacLab, but no IsaacLab runtime was found locally |
| IsaacLab install path | user-chosen install path |  | blocked | no `isaaclab` launcher was found in common locations or on `PATH` |
| host platform for ProtoMotions | `Windows-native first; WSL only if blocked` | `Windows-native` | confirmed | current setup attempt stayed Windows-native |
| pretrained source | `ctessler/MaskedMimic` | `ProtoMotions repo bundle / ctessler MaskedMimic release lineage` | confirmed | the cloned repo already contains the pretrained model folder and README |
| pretrained checkpoint path | `Training/ProtoMotions/data/pretrained_models/masked_mimic/smpl/last.ckpt` | `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\masked_mimic\smpl\last.ckpt` | confirmed | checkpoint exists locally (`303850268` bytes) |
| ONNX opset | `17 -> 16 -> 15 fallback order` |  | planned | record first accepted opset |
| Unreal runtime target | `NNERuntimeORTDml` |  | planned | `NNERuntimeORTCpu` debug fallback only |

## Dataset Lock

| Item | Planned Value | Confirmed Value | Status | Notes |
|---|---|---|---|---|
| AMASS root | `F:\NewEngine\Training\data\amass` |  | blocked | not present, but not required for the current pretrained-first checkpoint |
| combat clips root | `F:\NewEngine\Training\data\mixamo_fight` |  | blocked | intentionally deferred until fine-tuning or motion-source lock requires it |
| UE project root | `F:\NewEngine\PhysAnimUE5` | `F:\NewEngine\PhysAnimUE5` | confirmed | project exists locally and `PhysAnimUE5.uproject` is present |

## Orchestrator Rule

When any planned value changes:

1. update this file
2. update [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
3. update [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md) if the change affects risk
