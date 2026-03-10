# ELI5 UE Project Setup

## What You Are Doing

You are creating the empty Unreal project we will use for Stage 1.

This is not the full game. It is just the starting container where Manny, physics, and the plugin will live.

## Why It Matters

Without this project, we cannot test:

- Manny in Unreal
- the physics-control path
- the plugin later
- the G1 smoke tests

## What To Click

1. Open Unreal Engine `5.7.3`.
2. In the project browser, choose:
   - category: `Games`
   - template: `Third Person`
   - variant: `None`
3. Set:
   - project name: `PhysAnimUE5`
   - location: `F:\NewEngine`
4. Create the project.
5. Wait for the editor to finish loading.
6. Press Play once just to confirm the project runs.
7. Open `Edit -> Plugins`.
8. Search for and enable if available:
   - `Pose Search`
   - `Physics Control`
   - `NNERuntimeORT`
9. Restart the editor if Unreal asks.
10. In the Content Browser, look for Manny content from the mannequin assets.

## What Good Looks Like

- the project opens without crashing
- the default level loads
- pressing Play works
- the plugin list opens
- the required plugins can be enabled cleanly
- Manny assets are visible in the content browser

## What Bad Looks Like

- Unreal fails to create the project
- the project opens but immediately errors or crashes
- a required plugin is missing or refuses to enable
- Manny assets are not present anywhere obvious

## What Evidence To Send Back

Send back:

- the final project path
- one screenshot of the project open in the editor
- one note saying whether Manny was found
- one note saying whether the required plugins enabled cleanly
- any error message that blocked progress
