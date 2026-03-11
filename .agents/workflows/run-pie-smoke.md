---
description: Run one automated editor loop that opens the project, starts PIE, waits 10 seconds, and exits
---

# Run PIE Smoke

Use the repo runner:

```powershell
powershell -ExecutionPolicy Bypass -File F:\NewEngine\scripts\run-pie-smoke.ps1
```

What it does:

1. launches Unreal with the `PhysAnim.PIE.Smoke` automation test
2. opens `/Game/ThirdPerson/Lvl_ThirdPerson`
3. starts PIE
4. waits 10 seconds
5. ends PIE and exits Unreal

The script prints the latest project log path after Unreal exits. Review `PhysAnimUE5\Saved\Logs` if you want the log contents.

Useful options:

```powershell
powershell -ExecutionPolicy Bypass -File F:\NewEngine\scripts\run-pie-smoke.ps1 -UseEditorExe
```

By default the runner uses `UnrealEditor-Cmd.exe`, because that path is currently the one that reliably runs the PIE automation end to end.
