# Toolchain Readiness Evidence

## Scope

Graph task: `S1-P0-U1`

Acceptance criterion: `Toolchain installed`

## Local Verification

Commands were run from `F:\NewEngine-AgentB` with PowerShell.

| Check | Result |
|---|---|
| `UE5_PATH` from `scripts\local.paths.ps1` | `E:\UE_5.7\Engine` |
| Unreal build wrapper | `E:\UE_5.7\Engine\Build\BatchFiles\Build.bat` exists |
| Unreal commandlet | `E:\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe` exists |
| Project file | `PhysAnimUE5\PhysAnimUE5.uproject` exists |
| Plugin file | `PhysAnimUE5\Plugins\PhysAnimPlugin\PhysAnimPlugin.uplugin` exists |
| Python | `Python 3.9.6` |
| Git | `git version 2.47.0.windows.2` |

Ambient `cl.exe` lookup from a plain shell did not find MSVC on `PATH`. This does not block the repo-supported build path because `.\scripts\build.ps1` invokes Unreal Build Tool through the configured UE installation, and the latest UE compile gate has already passed in this checkout.

## Decision

The Stage 1 toolchain is usable for current UE plugin work through the project-supported command:

```text
.\scripts\build.ps1
```

No external license click-through was performed by the agent. The installed Unreal commandlet and successful prior automation runs are treated as evidence that the local environment is already usable for non-interactive project work.
