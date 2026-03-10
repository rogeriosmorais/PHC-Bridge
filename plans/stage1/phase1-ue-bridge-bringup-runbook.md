# Phase 1 UE Bridge Bring-Up Runbook

## Purpose

This is the human-facing runbook for bringing the new Stage 1 bridge online inside Unreal Editor.

Use it when the code-side bridge is already in the repo, but the required UE assets and Blueprint wiring do not exist yet.

This runbook is intentionally strict because the current `UPhysAnimComponent` startup path validates exact asset paths, the live Anim Blueprint class path, exact PoseSearch hooks, and exact Physics Control names.

If you improvise the content layout, the bridge will fail at startup.

In the Unreal Content Browser, `/Game/...` means the project's `Content/...` root. There is no literal top-level folder named `Game` in the browser UI.

Important design update on March 10, 2026:

- Phase 1 is now frozen to the direct-query PoseSearch path
- the bridge is expected to call `UPoseSearchLibrary::MotionMatch(...)` itself
- the Anim Blueprint no longer needs an authored Motion Matching node
- the Anim Blueprint does still need a `Pose History` node tagged `PoseHistory_Stage1`

## Applies To

- UE project: `F:\NewEngine\PhysAnimUE5`
- UE version: `5.7.3`
- bridge runtime: the component-based Stage 1 path introduced after the smoke-test subsystems
- test map: `/Game/ThirdPerson/Lvl_ThirdPerson`

The Phase 1 bridge starts automatically from `UPhysAnimComponent::BeginPlay()`.

## Rule Zero

If a step below says an asset must live at one exact path, use that exact path.

If the editor workflow available on this machine cannot produce that asset cleanly, stop and report the blocker instead of inventing a substitute name or location.

## Frozen Runtime Contract

The current code will only accept this exact content contract:

| Item | Exact Requirement |
|---|---|
| character blueprint | `/Game/Characters/Mannequins/Blueprints/BP_PhysAnimCharacter` |
| skeletal mesh | `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple` |
| physics asset | `/Game/Characters/Mannequins/Rigs/PA_Mannequin` |
| Anim Blueprint | `/Game/Characters/Mannequins/Animations/ABP_PhysAnim` |
| PoseSearch schema | `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion` |
| PoseSearch database | `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion` |
| NNE model asset | `/Game/NNEModels/phc_policy` |
| pose history node tag | `PoseHistory_Stage1` |
| controlled mesh component name for Physics Control initialization | `CharacterMesh0` |
| Physics Control component count | exactly one on the character |
| required control naming | `PACtrl_<BoneName>` |
| required body modifier naming | `PAMod_<BoneName>` |

## What You Will Build

You will create:

1. one PoseSearch database for locomotion
2. one Anim Blueprint that contains a `Pose History` node tagged `PoseHistory_Stage1`
3. one character Blueprint that hosts `UPhysAnimComponent`, `UPhysicsControlComponent`, and `UPhysicsControlInitializerComponent`
4. one imported `UNNEModelData` asset from the PHC `.onnx`
5. one live map setup that spawns the new character and lets the bridge auto-start in PIE

## Before You Start

### Preflight

1. Close Unreal Editor if it is already open.
2. Confirm the project opens from `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`.
3. If Unreal asks to rebuild the plugin, allow it.
4. If the rebuild fails because Visual Studio 2022 / MSVC v143 is missing, stop and report `blocked`.
5. In the editor, enable the Output Log window before testing. You will need the exact startup lines later.

### Existing Assets You Can Reuse

These assets already exist locally and are the intended starting point:

- `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`
- `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`
- `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`
- `/Game/Characters/Mannequins/Rigs/PA_Mannequin`
- `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle`
- all `MF_Unarmed_Walk_*` clips under `/Game/Characters/Mannequins/Anims/Unarmed/Walk`
- all `MF_Unarmed_Jog_*` clips under `/Game/Characters/Mannequins/Anims/Unarmed/Jog`

Important current local truth on March 10, 2026:

- `ABP_Unarmed` is a locomotion Anim Blueprint reference, not a pre-existing Motion Matching Anim Blueprint
- local asset inspection does not show an authored `Motion Matching` node or existing PoseSearch database binding inside `ABP_Unarmed`
- the direct-query Phase 1 path does not require that Motion Matching node

## Step 1: Create The Required Content Folders

Create these folders in the Content Browser if they do not already exist:

- `/Game/Characters/Mannequins/Animations`
- `/Game/Characters/Mannequins/Blueprints`
- `/Game/PoseSearch/Schemas`
- `/Game/PoseSearch/Databases`
- `/Game/NNEModels`

## Step 2: Create The PoseSearch Schema And Database

### 2A. Create The Schema

Create a new Pose Search Schema asset at:

- `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion`

When Unreal asks for the skeleton, choose:

- `/Game/Characters/Mannequins/Meshes/SK_Mannequin`

On UE `5.7.3`, the schema factory automatically adds the default locomotion channels after you pick the skeleton.

For the first bring-up pass, keep the schema simple:

- Skeleton: `SK_Mannequin`
- Mirror Data Table: leave empty
- Sample Rate: `30`
- Data Preprocessor: leave the default `Normalize`
- Channels: keep the default locomotion channels the factory created

This first pass only needs a valid locomotion schema so the database and AnimBP can be authored cleanly.

### 2B. Create The Database

Create a new PoseSearch database asset at:

- `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion`

Set its schema to:

- `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion`

For the first bring-up pass, populate it only with the built-in unarmed locomotion clips already in the project:

- `/Game/Characters/Mannequins/Anims/Unarmed/MM_Idle`
- every `MF_Unarmed_Walk_*` asset in `/Game/Characters/Mannequins/Anims/Unarmed/Walk`
- every `MF_Unarmed_Jog_*` asset in `/Game/Characters/Mannequins/Anims/Unarmed/Jog`

### What To Check Before Moving On

- the schema asset saves cleanly
- the schema uses `SK_Mannequin`
- the database points at `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion`
- the database asset saves cleanly
- the database points only at Manny/Quinn mannequin-compatible locomotion clips
- there is exactly one asset at `/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion`
- there is exactly one asset at `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion`

### What To Send Back If Blocked

- a screenshot of the schema asset
- a screenshot of the database asset
- the exact error text if Unreal rejects any clip or schema setting

## Step 3: Create `ABP_PhysAnim`

Duplicate this asset:

- source: `/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed`

Save the duplicate here:

- target: `/Game/Characters/Mannequins/Animations/ABP_PhysAnim`

### 3A. Keep The Existing Parent Class

Open `ABP_PhysAnim`.

There is no reparent step for the revised direct-query path.

Keep the duplicated AnimBP on its existing parent class.

Compile and save.

### 3B. Keep The Existing Locomotion Graph

Keep the duplicated `ABP_Unarmed` locomotion graph in place if it already previews locomotion correctly.

The point of this AnimBP is:

- keep Epic's existing locomotion graph as much as possible
- expose a live pose-history stream to the bridge
- keep the visual animation and the bridge sampling the same authored locomotion database

### 3C. Add A `Pose History` Node

The revised Phase 1 path requires a `Pose History` node in the `Anim Graph`.

The practical goal is:

- take the final locomotion pose already produced by the graph
- pass it through a `Pose History` node
- send that node's output to the final `Output Pose`
- tag that node as `PoseHistory_Stage1`

Use this sequence:

1. In `ABP_PhysAnim`, open the `Anim Graph`.
2. Find the final pose wire that currently feeds the graph's output pose.
3. Add the node named `Pose History`.
4. Insert it into that final pose wire so:
   - existing locomotion result -> `Pose History` -> final output pose
5. In the `Details` panel for that `Pose History` node, set its node tag / tag name to:
   - `PoseHistory_Stage1`
6. Leave its sampling settings at defaults unless Unreal forces a required value.

### 3D. Database Binding For This Phase

The direct-query Phase 1 path queries PoseSearch from code. The authored AnimBP change for this phase is the `Pose History` node above.

### 3E. Point The Content At The Stage 1 Database

The authored locomotion content for this branch is still:

- `/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion`

The current production bridge queries that database directly through `UPoseSearchLibrary::MotionMatch(...)`.

So at the AnimBP/content level, the required authored asset is the database itself plus the `Pose History` node. There is no extra database binding step to a Motion Matching node in this revised path.

### 3F. Compile And Save

Compile and save `ABP_PhysAnim` before moving to Step 4.

### What Good Looks Like

- the AnimBP still previews locomotion
- the final locomotion pose now passes through one `Pose History` node
- the `Pose History` node tag is exactly `PoseHistory_Stage1`

### What To Send Back If Blocked

- a screenshot of the Anim Graph
- a screenshot of the `Pose History` node details
- the exact compile error or Blueprint error text

## Step 4: Create `BP_PhysAnimCharacter`

Duplicate this asset:

- source: `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`

Save the duplicate here:

- target: `/Game/Characters/Mannequins/Blueprints/BP_PhysAnimCharacter`

Open the new Blueprint and make these changes.

### 4A. Mesh And Animation

On the inherited skeletal mesh component:

- set Skeletal Mesh to `/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple`
- set Physics Asset to `/Game/Characters/Mannequins/Rigs/PA_Mannequin`
- set Anim Class to `/Game/Characters/Mannequins/Animations/ABP_PhysAnim`
- confirm the component name remains `CharacterMesh0`

### 4B. Add The Required Components

Add exactly these components to `BP_PhysAnimCharacter`:

- one `PhysAnimComponent`
- one `PhysicsControlComponent`
- one `PhysicsControlInitializerComponent`

The initializer component is the editor-authored helper for building the required named controls and modifiers at BeginPlay. The bridge code itself still only validates and uses the resulting `UPhysicsControlComponent`.

### 4C. Leave `UPhysAnimComponent` Model Asset On Its Default Path

`UPhysAnimComponent` defaults to:

- `/Game/NNEModels/phc_policy`

Leave that as-is unless you are deliberately overriding it with the exact same asset path.

## Step 5: Populate The Required Physics Control Set

The Stage 1 bridge expects the required controls and body modifiers to already exist in `PhysicsControlInitializerComponent`.

### 5A. Run The Helper

1. Open `BP_PhysAnimCharacter`.
2. Select the `PhysAnim` component.
3. In the Details panel, click `Populate Stage 1 Physics Control Defaults`.
4. Select the `PhysicsControlInitializer` component.
5. Check these values immediately after the helper runs:
   - `bCreateControlsAtBeginPlay = true`
   - `InitialControls` contains `21` entries
   - `InitialBodyModifiers` contains `22` entries
6. Spot-check these entries:
   - `InitialControls` contains `PACtrl_thigh_l` with `Parent Bone = pelvis` and `Child Bone = thigh_l`
   - `InitialBodyModifiers` contains `PAMod_pelvis` with `Bone Name = pelvis`
7. Compile `BP_PhysAnimCharacter` and save it.

If the helper button is unavailable or the counts do not match, use the fallback tables below.

### 5B. Initial Control Defaults

For the first bring-up pass, use the same initial control data for every required control:

| Field | Value |
|---|---|
| `bEnabled` | `true` |
| `LinearStrength` | `0.0` |
| `LinearDampingRatio` | `1.0` |
| `LinearExtraDamping` | `0.0` |
| `MaxForce` | `0.0` |
| `AngularStrength` | `800.0` |
| `AngularDampingRatio` | `1.25` |
| `AngularExtraDamping` | `30.0` |
| `MaxTorque` | `0.0` |
| `LinearTargetVelocityMultiplier` | `0.0` |
| `AngularTargetVelocityMultiplier` | `0.0` |
| `bUseSkeletalAnimation` | `true` |
| `bDisableCollision` | `true` |
| `bOnlyControlChildObject` | `true` |

These are initial bring-up values, not frozen final tuning values.

Use these same non-name fields for every control entry unless a later tuning pass says otherwise.

### 5C. Initial Body Modifier Defaults

For every required body modifier, use:

| Field | Value |
|---|---|
| `MovementType` | `Simulated` |
| `CollisionType` | `QueryAndPhysics` |
| `GravityMultiplier` | `1.0` |
| `PhysicsBlendWeight` | `1.0` |
| `KinematicTargetSpace` | `OffsetInBoneSpace` |
| `bUpdateKinematicFromSimulation` | `true` |

### 5D. Fallback Control Entries

In `PhysicsControlInitializerComponent.InitialControls`, create one map entry per row below.

Use these common values for every row:

- `ParentActor = self`
- `ParentMeshComponentName = CharacterMesh0`
- `ChildActor = self`
- `ChildMeshComponentName = CharacterMesh0`

Then set the exact names and bones from this table:

| Map Key | Parent Bone | Child Bone |
|---|---|---|
| `PACtrl_thigh_l` | `pelvis` | `thigh_l` |
| `PACtrl_calf_l` | `thigh_l` | `calf_l` |
| `PACtrl_foot_l` | `calf_l` | `foot_l` |
| `PACtrl_ball_l` | `foot_l` | `ball_l` |
| `PACtrl_thigh_r` | `pelvis` | `thigh_r` |
| `PACtrl_calf_r` | `thigh_r` | `calf_r` |
| `PACtrl_foot_r` | `calf_r` | `foot_r` |
| `PACtrl_ball_r` | `foot_r` | `ball_r` |
| `PACtrl_spine_01` | `pelvis` | `spine_01` |
| `PACtrl_spine_02` | `spine_01` | `spine_02` |
| `PACtrl_spine_03` | `spine_02` | `spine_03` |
| `PACtrl_neck_01` | `spine_03` | `neck_01` |
| `PACtrl_head` | `neck_01` | `head` |
| `PACtrl_clavicle_l` | `spine_03` | `clavicle_l` |
| `PACtrl_upperarm_l` | `clavicle_l` | `upperarm_l` |
| `PACtrl_lowerarm_l` | `upperarm_l` | `lowerarm_l` |
| `PACtrl_hand_l` | `lowerarm_l` | `hand_l` |
| `PACtrl_clavicle_r` | `spine_03` | `clavicle_r` |
| `PACtrl_upperarm_r` | `clavicle_r` | `upperarm_r` |
| `PACtrl_lowerarm_r` | `upperarm_r` | `lowerarm_r` |
| `PACtrl_hand_r` | `lowerarm_r` | `hand_r` |

### 5E. Fallback Body Modifier Entries

In `PhysicsControlInitializerComponent.InitialBodyModifiers`, create one map entry per row below.

Use these common values for every row:

- `Actor = self`
- `MeshComponentName = CharacterMesh0`

Then set the exact map key and bone name from this table:

| Map Key | Bone Name |
|---|---|
| `PAMod_pelvis` | `pelvis` |
| `PAMod_thigh_l` | `thigh_l` |
| `PAMod_calf_l` | `calf_l` |
| `PAMod_foot_l` | `foot_l` |
| `PAMod_ball_l` | `ball_l` |
| `PAMod_thigh_r` | `thigh_r` |
| `PAMod_calf_r` | `calf_r` |
| `PAMod_foot_r` | `foot_r` |
| `PAMod_ball_r` | `ball_r` |
| `PAMod_spine_01` | `spine_01` |
| `PAMod_spine_02` | `spine_02` |
| `PAMod_spine_03` | `spine_03` |
| `PAMod_neck_01` | `neck_01` |
| `PAMod_head` | `head` |
| `PAMod_clavicle_l` | `clavicle_l` |
| `PAMod_upperarm_l` | `upperarm_l` |
| `PAMod_lowerarm_l` | `lowerarm_l` |
| `PAMod_hand_l` | `hand_l` |
| `PAMod_clavicle_r` | `clavicle_r` |
| `PAMod_upperarm_r` | `upperarm_r` |
| `PAMod_lowerarm_r` | `lowerarm_r` |
| `PAMod_hand_r` | `hand_r` |

### 5F. Compile And Save

Compile `BP_PhysAnimCharacter` and save.

### What Good Looks Like

- the Blueprint compiles
- the character now contains one `PhysAnimComponent`
- the character now contains one `PhysicsControlComponent`
- the initializer map entries exist for every required `PACtrl_*` and `PAMod_*`

### What To Send Back If Blocked

- one screenshot of the component list
- one screenshot of `InitialControls`
- one screenshot of `InitialBodyModifiers`
- the exact compile error text, if any

## Step 6: Import The PHC ONNX As `phc_policy`

Import the current Stage 1 PHC `.onnx` model into Unreal so it becomes a `UNNEModelData` asset at:

- `/Game/NNEModels/phc_policy`

Use the ONNX file produced by the locked export path in [onnx-export-spec.md](/F:/NewEngine/plans/stage1/onnx-export-spec.md).

If no current Stage 1 ONNX file exists yet, stop and report `blocked`.

If Unreal assigns a different auto-generated name, rename or reimport it so the asset path is exactly `/Game/NNEModels/phc_policy`.

### What To Check Before Moving On

- the asset type is `NNE Model Data`
- the asset path is exactly `/Game/NNEModels/phc_policy`

### What To Send Back If Blocked

- a screenshot of the imported asset in the Content Browser
- the exact import error text

## Step 7: Put `BP_PhysAnimCharacter` In The Test Map

Open:

- `/Game/ThirdPerson/Lvl_ThirdPerson`

For the first bring-up pass, use the simplest spawn path:

1. Place `BP_PhysAnimCharacter` in the level.
2. Set `Auto Possess Player` on that placed actor to `Player 0`.
3. Remove or disable any other pawn path that would take possession during PIE.

The goal is simple:

- when PIE starts, the pawn you are controlling must be `BP_PhysAnimCharacter`

## Step 8: Run The First PIE Bring-Up

Open the Output Log.

Then:

1. Save all assets.
2. Start PIE in `/Game/ThirdPerson/Lvl_ThirdPerson`.
3. Watch the Output Log immediately.

### Expected Success Line

The current bridge reports startup success with a log line in this shape:

```text
[PhysAnim] Startup success. Runtime=... Model=/Game/NNEModels/phc_policy.phc_policy
```

If you see that line, the minimum content contract is now satisfied and the bridge is alive.

### Expected Failure Shape

The current bridge reports startup failure with:

```text
[PhysAnim] Startup blocked: ...
```

or

```text
[PhysAnim] Fail-stop: ...
```

## Step 9: First Validation Checklist

If startup succeeds:

1. confirm the possessed pawn is `BP_PhysAnimCharacter`
2. confirm the Output Log shows the startup success line
3. watch whether the character remains stable enough to judge basic bridge activity
4. capture the first `10` to `20` seconds with a short clip if possible

If startup is blocked or fail-stops:

1. copy the first relevant `[PhysAnim]` log line
2. capture one screenshot of the Output Log

## Fast Diagnosis Map

Use this to classify the first failure reason from the log.

| Log Text Pattern | Meaning |
|---|---|
| `Expected Manny mesh` | the character is still using the wrong skeletal mesh asset |
| `Expected physics asset` | the mesh is not using `PA_Mannequin` |
| `Expected AnimBlueprint` | the live anim class is not `/Game/Characters/Mannequins/Animations/ABP_PhysAnim` |
| `PoseHistory_Stage1 was not found` | the AnimBP is missing the exact pose history node name |
| `Missing required pre-authored controls` | one or more `PACtrl_*` map entries did not get created |
| `Missing required pre-authored body modifiers` | one or more `PAMod_*` map entries did not get created |
| `Failed to load model asset` | `/Game/NNEModels/phc_policy` is missing or the component override is wrong |
| `Could not create an NNE model instance` | ORT runtime/plugin/model compatibility is broken |
| `PoseSearch query was invalid for two consecutive ticks` | the direct `MotionMatch(...)` query did not produce a valid result from the current pose history + database |
| `UPoseSearchLibrary::MotionMatch returned no selected animation` | PoseSearch ran but did not select any clip from `PSDB_Stage1_Locomotion` |

## What To Send Back After The Full Pass

Return exactly this evidence bundle:

1. one note saying which step you reached
2. one screenshot of `ABP_PhysAnim`
3. one screenshot of `BP_PhysAnimCharacter` components
4. one screenshot of the Output Log
5. if startup succeeded, one short clip or screenshot from PIE
6. one final verdict:
   - `startup success`
   - `blocked at step <n>`
   - `runtime fail-stop after startup`

## Future Maintenance Rule

If the bridge code later changes any of these hard-coded expectations:

- asset paths
- control names
- body modifier names
- PoseSearch hook names
- model asset path

then this runbook must be updated in the same change.
