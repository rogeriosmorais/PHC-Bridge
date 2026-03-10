---
description: How to run all automated tests (Python + UE5)
---
// turbo-all

# Run All Tests

> Status: planned workflow. This may be blocked until the relevant phase assets, code, tests, and UE5 project exist.

## Python Tests (Training side)

1. Activate the conda environment when it exists:
```
conda activate physanim_proto
```

2. Run retargeting tests when the test file and implementation exist:
```
pytest f:\NewEngine\Training\tests\test_retarget.py -v
```

3. Run ONNX export tests when the export path exists:
```
pytest f:\NewEngine\Training\tests\test_onnx_export.py -v
```

4. Run PHC training tests if that suite exists for the current phase:
```
pytest f:\NewEngine\Training\tests\test_phc_training.py -v
```

## UE5 Tests (Plugin side)

5. Run UE5 Automation tests once the UE5 project and automation tests exist:
```
& "$env:UE5_PATH\Binaries\Win64\UnrealEditor-Cmd.exe" "f:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject" -ExecCmds="Automation RunTests PhysAnim" -NullRHI -NoSound -Unattended -Log
```

Note: Set `UE5_PATH` to the UE `Engine` directory first. For the current local plan, once installation finishes, that should be `E:\UE_5.7\Engine`. During pre-Phase 0, treat these as intended commands rather than guaranteed-runnable steps.
