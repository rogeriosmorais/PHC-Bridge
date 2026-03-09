---
description: How to build the PhysAnimPlugin for UE5
---

# Build PhysAnim Plugin

1. Ensure UE5_PATH environment variable is set:
```
echo $env:UE5_PATH
```

// turbo
2. Build the plugin:
```
& "$env:UE5_PATH\Build\BatchFiles\RunUAT.bat" BuildPlugin -Plugin="f:\NewEngine\PhysAnimUE5\Plugins\PhysAnimPlugin\PhysAnimPlugin.uplugin" -TargetPlatforms=Win64 -Rocket
```

3. Check build output for errors. Exit code 0 = success.
