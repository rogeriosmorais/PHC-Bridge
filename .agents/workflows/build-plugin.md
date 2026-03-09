---
description: How to build the PhysAnimPlugin for UE5
---

# Build PhysAnim Plugin

> Status: planned workflow. This may be blocked until the UE5 project and plugin files exist.

1. Ensure `UE5_PATH` is set once UE5 is installed:
```
echo $env:UE5_PATH
```
Expected meaning: `UE5_PATH` points to the UE `Engine` directory, not the install root. For the current local plan, once installation finishes, that should be `E:\UE_5.7\Engine`.

// turbo
2. Build the plugin once `PhysAnimUE5` and `PhysAnimPlugin.uplugin` exist:
```
& "$env:UE5_PATH\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="f:\NewEngine\PhysAnimUE5\Plugins\PhysAnimPlugin\PhysAnimPlugin.uplugin" -TargetPlatforms=Win64 -Rocket
```

3. Check build output for errors. Until the UE5 project exists, treat this as the intended future build command.
