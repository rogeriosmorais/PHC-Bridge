---
description: How to run all automated tests (Python + UE5)
---
// turbo-all

# Run All Tests

## Python Tests (Training side)

1. Activate the conda environment:
```
conda activate physanim
```

2. Run retargeting tests:
```
pytest f:\NewEngine\Training\tests\test_retarget.py -v
```

3. Run ONNX export tests:
```
pytest f:\NewEngine\Training\tests\test_onnx_export.py -v
```

4. Run PHC training tests:
```
pytest f:\NewEngine\Training\tests\test_phc_training.py -v
```

## UE5 Tests (Plugin side)

5. Run UE5 Automation tests (requires UE5 installed):
```
& "$env:UE5_PATH\Binaries\Win64\UnrealEditor-Cmd.exe" "f:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject" -ExecCmds="Automation RunTests PhysAnim" -NullRHI -NoSound -Unattended -Log
```
Note: Set `UE5_PATH` environment variable to your UE5 install directory first.
