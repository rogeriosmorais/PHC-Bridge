---
name: UE Build And Test
description: Build the plugin or project and run the smallest relevant automated tests
---

# UE Build And Test

Use this for:
- plugin builds
- project compile validation
- automation tests
- quick UE-side regression checks

## Path Resolution

Preferred order for Unreal path:
1. command argument in the local script that invokes the command
2. local config file such as `scripts/local.paths.ps1`
3. `UE5_PATH` environment variable
4. fail with a clear error

Do not require a manual `PHC_BRIDGE_ROOT` variable.
Assume repo root is inferred automatically.

## Plugin Build

```powershell
& "$UE5_PATH\Build\BatchFiles\RunUAT.bat" BuildPlugin `
  -Plugin="PhysAnimUE5\Plugins\PhysAnimPlugin\PhysAnimPlugin.uplugin" `
  -TargetPlatforms=Win64 `
  -Rocket
```

## Project Build

```powershell
& "$UE5_PATH\Build\BatchFiles\Build.bat" PhysAnimUE5Editor Win64 Development `
  -Project="PhysAnimUE5\PhysAnimUE5.uproject" `
  -Progress -NoHotReloadFromIDE
```

## Python-Side Tests

Run only when relevant to the current change.

Retargeting:
```powershell
pytest Training/tests/test_retarget.py -v
```

ONNX export:
```powershell
pytest Training/tests/test_onnx_export.py -v
```

Training-side logic:
```powershell
pytest Training/tests/test_phc_training.py -v
```

## UE Automation Tests

All PhysAnim tests:
```powershell
& "$UE5_PATH\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "PhysAnimUE5\PhysAnimUE5.uproject" `
  -ExecCmds="Automation RunTests PhysAnim; Quit" `
  -NullRHI -NoSound -Unattended -Log
```

Component-focused pass:
```powershell
& "$UE5_PATH\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "PhysAnimUE5\PhysAnimUE5.uproject" `
  -ExecCmds="Automation RunTests PhysAnim.Component; Quit" `
  -NullRHI -NoSound -Unattended -Log
```

## Failure Reporting

If this fails, report:
- exact failing command
- first build or test failure
- failing file, module, or test name
- whether the issue is compile, configuration, missing dependency, or runtime assertion
