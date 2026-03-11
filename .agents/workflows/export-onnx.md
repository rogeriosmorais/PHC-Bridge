---
description: How to export trained PHC model to ONNX format
---

# Export Trained Model to ONNX

> Status: planned workflow. This may be blocked until the training code, checkpoints, and export script exist.

Use [onnx-export-spec.md](/F:/NewEngine/plans/stage1/10-specs/onnx-export-spec.md) as the source of truth for opset choice, runtime target, validation rules, and handoff requirements.

## Steps

1. Activate the locked training environment:
```
F:\NewEngine\Training\.venv\physanim_proto311\Scripts\activate
```

// turbo
2. Export the locked pretrained `motion_tracker/smpl` checkpoint:
```
python f:\NewEngine\Training\scripts\export_onnx.py --checkpoint f:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt --output f:\NewEngine\Training\output\phc_policy.onnx --copy-to-ue
```
This writes the offline ONNX artifact, validates PyTorch-vs-ONNX parity, and copies the `.onnx` file into `f:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx` for Unreal import. If the active export entry point is not `f:\NewEngine\Training\scripts\export_onnx.py`, update [onnx-export-spec.md](/F:/NewEngine/plans/stage1/10-specs/onnx-export-spec.md) and the dependency lock first.

// turbo
3. Validate the export contract:
```
python -m pytest f:\NewEngine\Training\tests\test_onnx_export.py -v
```

4. Import the copied `.onnx` into Unreal as a `UNNEModelData` asset at `/Game/NNEModels/phc_policy`.

5. If you need to copy manually instead of using `--copy-to-ue`:
```
Copy-Item f:\NewEngine\Training\output\phc_policy.onnx f:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx
```
