# Stage 1 UE Project Scaffold

## Purpose

This document defines the exact Unreal-side starting structure for Stage 1 so the user-owned UE setup is concrete and repeatable.

For the user-facing click path, use [eli5-ue-project-setup.md](/F:/NewEngine/plans/stage1/eli5-ue-project-setup.md).

## Project Creation Rule

Create the UE project from the **Third Person** template in UE `5.7.3`.

Use:

- category: `Games`
- template: `Third Person`
- variant: `None`

Why this template:

- official UE 5.7 docs confirm it includes Quinn by default and Manny as an included mannequin asset
- it gives a working third-person base with a character, map, movement, and camera
- it minimizes project-setup work that is unrelated to the Stage 1 thesis

## Project Location

Planning assumption:

- project root: `F:\NewEngine\PhysAnimUE5`

Expected project file:

- `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`

## Manny Source

Based on UE 5.7 Third Person template docs:

- Manny is available in the template content
- player character defaults to Quinn
- Manny can be selected in the project content

Planning rule:

- use **Manny** for Stage 1 once the project is created
- keep Quinn only as template baggage unless a setup issue forces a temporary fallback

## Initial Content Expectations

The first UE scaffold should contain:

- third-person starter map
- Manny skeletal mesh available in project content
- base character blueprint from the template
- default locomotion animations from the template

## Plugin / Feature Expectations

Enable or confirm these systems as relevant to Stage 1:

- `Pose Search`
- `Physics Control` (experimental plugin)
- `NNERuntimeORT`
- NNE base runtime support needed for `UNNEModelData`

Optional / defer-until-needed:

- `ML Deformer` only if Phase 1 visual-bonus decision says include

## Initial Unreal Tasks

The first UE scaffold pass should complete these:

1. create the project
2. confirm Manny content exists
3. confirm project opens and plays
4. create or identify the test map for control-path checks
5. create the plugin folder target location

## Expected Folder Targets

- project root: `F:\NewEngine\PhysAnimUE5`
- plugin root: `F:\NewEngine\PhysAnimUE5\Plugins\PhysAnimPlugin`
- map area: `F:\NewEngine\PhysAnimUE5\Content\Maps`
- character area: `F:\NewEngine\PhysAnimUE5\Content\Characters`
- model import area: `F:\NewEngine\PhysAnimUE5\Content\NNEModels`

## First User Evidence Needed

After the scaffold is created, the user should return:

- project path confirmation
- screenshot showing the project opens
- note confirming Manny is present or, if not, what asset path is available instead
- note confirming whether required plugins can be enabled cleanly

## Open UE Risk

The exact editor UI flow for NNE and Physics Control may vary slightly, but the project-template choice and Manny source are now locked strongly enough to proceed.
