---
description: How to export trained PHC model to ONNX format
---

# Export Trained Model to ONNX

## Steps

1. Activate environment:
```
conda activate physanim
```

// turbo
2. Export to ONNX:
```
python f:\NewEngine\Training\scripts\export_onnx.py --checkpoint f:\NewEngine\Training\output\latest.pt --output f:\NewEngine\Training\output\phc_policy.onnx
```

// turbo
3. Validate the exported model:
```
pytest f:\NewEngine\Training\tests\test_onnx_export.py -v
```

4. Copy to UE5 project (if UE5 project exists):
```
Copy-Item f:\NewEngine\Training\output\phc_policy.onnx f:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx
```
