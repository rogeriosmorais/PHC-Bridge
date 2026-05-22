# Phase 0 Feasibility Execution Package

## Status

This is a historical reconstruction package for graph task `S1-P0-A1`.

Gate G1 is already recorded as `pass` in `plans/stage1/30-evidence/g1-evidence.md`. This package does not override the current balance-first Stage 1 contracts in `STAGE1_PLAN.md`.

## Purpose

Phase 0 feasibility established that the project could proceed past initial environment, pretrained-policy, UE control-path, Manny mapping, and substep-stability checks.

## Execution Checks

| Check | Current package decision | Evidence source |
|---|---|---|
| Toolchain installed | Usable through `.\scripts\build.ps1` | `docs/evidence/toolchain-readiness.md` |
| UE5 project scaffold | Present under `PhysAnimUE5` | `docs/evidence/ue5-scaffold-readiness.md` |
| Required UE plugins | `PoseSearch`, `NNERuntimeORT`, `PhysicsControl`, `PhysAnimPlugin` enabled | `PhysAnimUE5\PhysAnimUE5.uproject` |
| Runtime policy asset | ONNX and NNE assets present in `Content\NNEModels` | `docs/evidence/ue5-scaffold-readiness.md` |
| Historical G1 evidence | Passed | `plans/stage1/30-evidence/g1-evidence.md` |

## Frozen Runtime Commands

Use these repo-supported commands for current verification:

```text
.\scripts\build.ps1
.\scripts\build.ps1 -Test PhysAnim.Bridge.SmplOrderContract
.\scripts\build.ps1 -Test PhysAnim.Bridge.FrameConversion
.\scripts\build.ps1 -Test PhysAnim.PIE.BalanceModeSmoke
```

For current balance-first work, the authoritative acceptance rule is still `BalanceActive_Standing` held continuously for `3.0` seconds.

## Offline / Pretrained Caveat

The historical planning path expected the original checkpoint under:

```text
Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt
```

That tree was not found in this checkout during `S1-P0-U2` verification. Current runtime bridge work may proceed from the imported `Content\NNEModels` assets, but any checkpoint re-export or numerical PyTorch-vs-ONNX comparison must resolve assumption `A-09` first.

## Next Consumer

Use `plans/stage1/30-evidence/g1-evidence.md` for the historical G1 verdict.

Use `STAGE1_PLAN.md` and active `10-specs` / `20-execution` documents for current balance-first implementation.
