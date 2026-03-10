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
| UE version | `5.7.3` |  | planned | current local plan |
| UE install root | `E:\UE_5.7` |  | planned | update if install path changed |
| `UE5_PATH` | `E:\UE_5.7\Engine` |  | planned | must point to `Engine` |
| Python version | `3.10` |  | planned | ProtoMotions default target |
| ProtoMotions env | `physanim_proto` |  | planned | conda env name |
| optional PyRoki env | `physanim_pyroki` |  | planned | only if needed |

## External Source Lock

| Item | Planned Value | Confirmed Value | Status | Notes |
|---|---|---|---|---|
| ProtoMotions release | `v2.3.2` |  | planned | record exact tag or commit |
| ProtoMotions path | `F:\NewEngine\Training\ProtoMotions` |  | planned | |
| simulator path | `isaaclab` |  | planned | `isaacgym` only with explicit note |
| pretrained source | `ctessler/MaskedMimic` |  | planned | |
| pretrained checkpoint path | `Training/ProtoMotions/data/pretrained_models/masked_mimic/smpl/last.ckpt` |  | planned | record actual local path if it differs |

## Dataset Lock

| Item | Planned Value | Confirmed Value | Status | Notes |
|---|---|---|---|---|
| AMASS root | `F:\NewEngine\Training\data\amass` |  | planned | |
| combat clips root | `F:\NewEngine\Training\data\mixamo_fight` |  | planned | or approved replacement |
| UE project root | `F:\NewEngine\PhysAnimUE5` |  | planned | |

## Orchestrator Rule

When any planned value changes:

1. update this file
2. update [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
3. update [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md) if the change affects risk
