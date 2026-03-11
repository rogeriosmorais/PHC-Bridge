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
3. one character Blueprint that hosts `UPhysAnimComponent`, `UPhysicsControlComponent`, and `UPhysAnimStage1InitializerComponent`
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

### 4A.1. Verify Required Physics Bodies In `PA_Mannequin`

Open `/Game/Characters/Mannequins/Rigs/PA_Mannequin` in the Physics Asset Editor.

In the Skeleton Tree, verify that physics bodies exist for these bones:

- `spine_01`
- `ball_l`
- `ball_r`

If any of those bones show up in the tree with no body assigned, create the missing body manually:

1. select the missing bone in the Skeleton Tree
2. right-click the bone
3. choose `Create Bodies / Constraints`
4. choose `Create Bodies with Capsule`
5. leave `Create Constraints` enabled
6. save `PA_Mannequin`

The current local Manny setup required manual body creation for those three bones during bring-up. If you skip this, the bridge will fail before model loading.

### 4B. Add The Required Components

Add exactly these components to `BP_PhysAnimCharacter`:

- one `PhysAnimComponent`
- one `PhysicsControlComponent`
- one `PhysAnim Stage1 Physics Control Initializer`

The Stage 1 initializer component owns the exact `InitialControls` and `InitialBodyModifiers` defaults required by the bridge.

Important current ownership rule:

- the initializer is defaults-only
- it must not create live Physics Control operators at `BeginPlay`
- the bridge creates and destroys the live controls/body modifiers itself on runtime state transitions

### 4C. Leave `UPhysAnimComponent` Model Asset On Its Default Path

`UPhysAnimComponent` defaults to:

- `/Game/NNEModels/phc_policy`

Leave that as-is unless you are deliberately overriding it with the exact same asset path.

## Step 5: Verify The Stage 1 Physics Control Defaults

The Stage 1 initializer component now owns the required map defaults directly. Do not hand-author the numeric tuning fields or the control/body-modifier names for the standard bring-up path.

### 5A. Check The Pre-Populated Defaults

1. Open `BP_PhysAnimCharacter`.
2. Select the `PhysAnim Stage1 Physics Control Initializer` component.
3. Check these values:
   - `bCreateControlsAtBeginPlay = false`
   - `InitialControls` contains `21` entries
   - `InitialBodyModifiers` contains `22` entries
4. Spot-check these entries:
   - `InitialControls` contains `PACtrl_thigh_l` with `Parent Bone = pelvis` and `Child Bone = thigh_l`
   - `InitialBodyModifiers` contains `PAMod_pelvis` with `Bone Name = pelvis`
5. Leave the pre-populated numeric fields and mesh component names as-is unless you are deliberately debugging the bridge contract.

### 5B. Actor Reference Overrides

The only user-facing override fields on the initializer are:

- `DefaultControlParentActor`
- `DefaultControlChildActor`
- `DefaultBodyModifierActor`

For the normal single-character bring-up path, leave all three blank. At runtime the initializer resolves blank actor references to the owning character automatically when the bridge activates runtime Physics Control ownership.

Only set those three fields if you deliberately need the controls or body modifiers to target a different actor than the owner.

### 5C. Compile And Save

Compile `BP_PhysAnimCharacter` and save.

### What Good Looks Like

- the Blueprint compiles
- the character now contains one `PhysAnimComponent`
- the character now contains one `PhysicsControlComponent`
- the character now contains one `PhysAnim Stage1 Physics Control Initializer`
- the initializer map entries exist for every required `PACtrl_*` and `PAMod_*`
- no live Physics Control operators should exist before the bridge reaches `BridgeActive`
- `PA_Mannequin` contains physics bodies for `spine_01`, `ball_l`, and `ball_r`

### What To Send Back If Blocked

- one screenshot of the component list
- one screenshot of `InitialControls`
- one screenshot of `InitialBodyModifiers`
- the exact compile error text, if any

## Step 6: Import The PHC ONNX As `phc_policy`

Import the current Stage 1 PHC `.onnx` model into Unreal so it becomes a `UNNEModelData` asset at:

- `/Game/NNEModels/phc_policy`

If the first PIE run after fixing the Physics Control setup reports:

```text
[PhysAnim] Startup blocked: Failed to load model asset '/Game/NNEModels/phc_policy.phc_policy'.
```

that means the bridge has progressed to the model-loading step and the PHC policy asset has not been imported yet.

Use the ONNX file produced by the locked export path in [onnx-export-spec.md](/F:/NewEngine/plans/stage1/10-specs/onnx-export-spec.md).

If no current Stage 1 ONNX file exists yet, stop and report `blocked`.

If Unreal assigns a different auto-generated name, rename or reimport it so the asset path is exactly `/Game/NNEModels/phc_policy`.

### What To Check Before Moving On

- the asset type is `NNE Model Data`
- the asset path is exactly `/Game/NNEModels/phc_policy`
- PIE no longer reports `Failed to load model asset '/Game/NNEModels/phc_policy.phc_policy'`

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

With the default zero-action safe-mode startup, the current bridge now reports:

```text
[PhysAnim] Startup success. Runtime=... Model=/Game/NNEModels/phc_policy.phc_policy DeferredActivation=true
```

If you see either success form, the minimum content contract is now satisfied and the bridge is alive.

Important current truth:

- startup success does not automatically mean the runtime is stable enough for comparison
- if the character immediately flies, spins, or tumbles after this line appears, that is a Phase 1 stabilization failure, not an asset bring-up failure
- do not go back and improvise asset-path changes once this line is proven; the next work is tuning / stabilization

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
3. confirm the runtime-state log transitions make sense:
   - `Uninitialized -> RuntimeReady`
   - `RuntimeReady -> WaitingForPoseSearch`
   - with the default safe-mode startup:
     - `WaitingForPoseSearch -> ReadyForActivation`
     - snapshots in that state should show `liveControls=0`, `liveBodyModifiers=0`, and `bridgeOwnsPhysics=false`
   - only after actions are explicitly enabled:
     - `ReadyForActivation -> BridgeActive`
4. watch whether the character remains stable enough to judge basic bridge activity
5. capture the first `10` to `20` seconds with a short clip if possible
6. if the character immediately flies or spins uncontrollably, classify the run as `runtime unstable after startup`

### Live Stabilization Knobs

The current bridge includes a thin runtime stabilization layer inside `UPhysAnimComponent`.

Use these console variables for the first stabilization pass before changing assets or reopening the model path:

- `physanim.ForceZeroActions`
- `physanim.UseSkeletalAnimationTargets`
- `physanim.ActionScale`
- `physanim.ActionClampAbs`
- `physanim.ActionSmoothingAlpha`
- `physanim.StartupRampSeconds`
- `physanim.MaxAngularStepDegPerSec`
- `physanim.AngularStrengthMultiplier`
- `physanim.AngularDampingRatioMultiplier`
- `physanim.AngularExtraDampingMultiplier`
- `physanim.EnableInstabilityFailStop`
- `physanim.MaxRootHeightDeltaCm`
- `physanim.MaxRootLinearSpeedCmPerSec`
- `physanim.MaxRootAngularSpeedDegPerSec`
- `physanim.InstabilityGracePeriodSeconds`

The bridge now also includes an automated instability monitor on the root body (`pelvis`).

Default fail-stop thresholds:

- `root height delta > 120 cm`
- `root linear speed > 1200 cm/s`
- `root angular speed > 720 deg/s`
- `grace window = 0.25 s`

If one or more thresholds stay exceeded longer than that grace window, the bridge now triggers:

```text
[PhysAnim] Fail-stop: Runtime instability detected ...
```

That means the startup path is still valid, but the current tuning is not stable enough yet.

Recommended first-pass commands:

```text
physanim.ForceZeroActions 1
```

That is now also the default component startup behavior, so the bridge should boot in zero-action mode unless you explicitly override it.

In the current bridge, zero-action mode is now a true safe mode:

- startup succeeds into `ReadyForActivation`
- no live runtime controls/body modifiers are created yet
- bridge-owned physics ownership is deferred
- the normal `ACharacter` capsule and `CharacterMovement` shell remain untouched until activation

Only after you explicitly run `physanim.ForceZeroActions 0` should the runtime create operators, take bridge-owned physics ownership, and apply the mesh/shell overrides. At that point the production bridge applies the same mesh collision safety used by the earlier UE harnesses:

- mesh collision profile switches to `PhysicsActor`
- mesh collision response to `Pawn` switches to `Ignore`
- explicit targets are seeded from the current pose before the bridge allows the first full simulation handoff
- the bridge now defaults to explicit targets instead of skeletal-animation target blending during active runtime

So if the character still launches or spins before `ReadyForActivation`, the remaining cause is outside bridge-owned Physics Control ownership. If instability starts only after `physanim.ForceZeroActions 0`, then the problem is inside the active bridge-owned path.

Frozen runtime ownership rule:

- `WaitingForPoseSearch` must not take bridge-owned physics ownership
- `ReadyForActivation` must not take bridge-owned physics ownership
- only `BridgeActive` may disable the normal character shell and own the physics-control path
- `FailStopped` must destroy live runtime controls/body modifiers and release bridge-owned physics before returning control

If zero-action mode is calm, re-enable the policy conservatively:

```text
physanim.ForceZeroActions 0
physanim.UseSkeletalAnimationTargets 0
physanim.ActionScale 0.10
physanim.ActionClampAbs 0.20
physanim.ActionSmoothingAlpha 0.25
physanim.StartupRampSeconds 1.0
physanim.AngularStrengthMultiplier 0.35
physanim.AngularDampingRatioMultiplier 1.50
physanim.AngularExtraDampingMultiplier 2.0
```

Only move on to deeper mapping/frame debugging if those safe settings still produce immediate flight, spinning, or tumbling.

If startup is blocked or fail-stops:

1. copy the first relevant `[PhysAnim]` log line
2. capture one screenshot of the Output Log

## Fast Diagnosis Map

Use this to classify the first failure reason from the log.

| Log Text Pattern | Meaning |
|---|---|
| `Expected Manny mesh` | the character is still using the wrong skeletal mesh asset |
| `Expected physics asset` | the mesh is not using `PA_Mannequin` |
| `Missing required physics bodies` | `PA_Mannequin` is missing one or more required bodies; on the current Manny setup, `spine_01`, `ball_l`, and `ball_r` had to be created manually |
| `Expected AnimBlueprint` | the live anim class is not `/Game/Characters/Mannequins/Animations/ABP_PhysAnim` |
| `PoseHistory_Stage1 was not found` | the AnimBP is missing the exact pose history node name |
| `Missing required authored controls` | one or more `PACtrl_*` map entries are missing from the initializer defaults |
| `Missing required authored body modifiers` | one or more `PAMod_*` map entries are missing from the initializer defaults |
| `Failed to load model asset` | the bridge reached model loading, but `/Game/NNEModels/phc_policy` does not exist yet or the component override points at the wrong asset |
| `Could not create an NNE model instance` | ORT runtime/plugin/model compatibility is broken |
| `PoseSearch query was invalid for two consecutive ticks` | the direct `MotionMatch(...)` query did not produce a valid result from the current pose history + database |
| `UPoseSearchLibrary::MotionMatch returned no selected animation` | PoseSearch ran but did not select any clip from `PSDB_Stage1_Locomotion` |
| `Initial PoseSearch result was never produced within 2.00s` | startup prerequisites resolved, but the bridge never received the first valid PoseSearch result, so it correctly refused to take physics ownership |
| `Fail-stop: Runtime instability detected` | the bridge is alive, but the automated instability monitor observed sustained launch / spin metrics; stay in Phase 1 tuning and use the runtime diagnostics plus stabilization CVars |
| `Startup success` followed by immediate flight/spin/tumble | the bridge and NNE path are alive; move to the Phase 1 stabilization/tuning task instead of revisiting bring-up assets |

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
   - `runtime unstable after startup`

## Future Maintenance Rule

If the bridge code later changes any of these hard-coded expectations:

- asset paths
- control names
- body modifier names
- PoseSearch hook names
- model asset path

then this runbook must be updated in the same change.
