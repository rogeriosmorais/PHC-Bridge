---
description: How to export trained PHC model to ONNX format
---

# Export Trained Model to ONNX

> Status: planned workflow. This may be blocked until the training code, checkpoints, and export script exist.

Use [onnx-export-spec.md](/F:/NewEngine/plans/stage1/onnx-export-spec.md) as the source of truth for opset choice, runtime target, validation rules, and handoff requirements.

## Steps

1. Activate the training environment once it exists:
```
conda activate physanim_proto
```

// turbo
2. Export to ONNX once the checkpoint and export script exist:
```
python f:\NewEngine\Training\scripts\export_onnx.py --checkpoint f:\NewEngine\Training\output\latest.pt --output f:\NewEngine\Training\output\phc_policy.onnx
```
Treat the checkpoint path above as an example. Update `--checkpoint` to match the actual training output path for the selected ProtoMotions version/config. If the active export entry point is not `f:\NewEngine\Training\scripts\export_onnx.py`, update [onnx-export-spec.md](/F:/NewEngine/plans/stage1/onnx-export-spec.md) and the dependency lock first.

// turbo
3. Validate the exported model once the validation test exists for the current phase:
```
pytest f:\NewEngine\Training\tests\test_onnx_export.py -v
```

4. Copy to the UE5 project if and when the project exists:
```
Copy-Item f:\NewEngine\Training\output\phc_policy.onnx f:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx
```
