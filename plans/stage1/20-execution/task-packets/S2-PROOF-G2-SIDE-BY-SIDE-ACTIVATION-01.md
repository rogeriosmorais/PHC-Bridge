# Task Packet: S2-PROOF-G2-SIDE-BY-SIDE-ACTIVATION-01

## Purpose
Record and validate the G2 side-by-side proof clip showing the physics-driven character correctly activated and standing alongside a kinematic baseline.

## Allowed Files
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnim.SmokeTests.cpp`
- `plans/stage1/20-execution/execution-log.md`
- `plans/stage1/20-execution/task-packets/S2-PROOF-G2-SIDE-BY-SIDE-ACTIVATION-01.md`
- `plans/stage1/30-evidence/S2-PROOF-G2-SIDE-BY-SIDE-ACTIVATION-01/` (new directory for evidence)

## Forbidden Files
- ALL runtime source files
- ALL assets

## Required Work
1. **Scenario Setup**: Run PIE in `Lvl_ThirdPerson`.
2. **Side-by-Side Activation**: Execute `PhysAnim.G2.StartPresentation` console command.
3. **Verification**: Confirm from logs that both Kinematic and Physics-Driven characters are spawned and synchronized.
4. **Evidence Capture**: Capture the log output of the presentation sequence (Idle, Walk, Jog, etc.).
5. **Stability Check**: Verify that the physics-driven character remains stable during the 3s proof window and subsequent movement.

## Required Tests
- `PhysAnim.G2.StartPresentation` (Console Command in PIE)

## Required Build
- `.\scripts\build.ps1` (to ensure everything is up to date)

## Definition Of Done
- Presentation sequence completes without a crash.
- Logs show `[PhysAnimG2] Started scripted G2 presentation`.
- Logs show `[PhysAnimG2] Scripted G2 presentation sequence started from steady BridgeActive state.`
- Physics character enters `BalanceActive_Standing` after proof satisfaction.
- `execution-log.md` updated.

## Stop Conditions
- Build failure.
- Character fail-stops during the presentation.
- Side-by-side fails to spawn the kinematic mirror.
