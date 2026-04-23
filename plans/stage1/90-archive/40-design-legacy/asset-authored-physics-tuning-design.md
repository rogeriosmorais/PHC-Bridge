# Asset-Authored Physics Tuning Design (FUTURE, we are not doing it right now)

## Purpose

This document turns two design rules into an implementation plan:

1. Push more tuning into physics asset profiles and PhysicsControl sets, less into custom runtime branches.
2. Treat contact/friction, constraint profiles, and control-family sets as authored assets, not code-only policies.

The goal is not to replace the locked Stage 1 architecture in [ENGINEERING_PLAN.md](/F:/NewEngine/ENGINEERING_PLAN.md).

The goal is to move Stage 1 tuning out of ad hoc per-bone runtime code and into authored UE assets wherever UE already provides the seam.

## Problem Statement

The current bridge is solving real alignment problems, but some of the recent fixes are expressed as runtime branches in [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp):

- direct per-bone control-family scaling
- direct per-bone toe operating-limit mutation
- direct per-bone target-range policies
- runtime-only interpretation of lower-limb tuning intent

That is useful for diagnosis, but it is a weak long-term design because:

- it hides tuning intent in code instead of assets
- it increases bridge complexity
- it makes multi-character safety harder
- it makes UE-side iteration slower because content changes require code changes
- it bypasses built-in Unreal authoring systems that already exist for constraints, body materials, and grouped control updates

## Current Path Vs Asset-Authored Path

This section compares:

- the current path:
  - runtime-first tuning
  - code-owned lower-limb policy branches
  - direct numeric edits in the bridge
- the alternative path in this document:
  - asset-authored constraint/contact tuning
  - set-authored control-family tuning
  - thinner runtime selector logic

### Current Path: Pros

- fastest way to test a hypothesis once a new offender is found
- easiest path for highly temporary experiments
- does not require immediate editor authoring work for every new idea
- useful during the diagnosis phase when the problem is still unknown
- keeps all experiment logic close to the runtime telemetry that motivated it

### Current Path: Cons

- bridge code grows every time a new tuning branch is added
- tuning intent becomes hard to read because it is distributed across code paths and cvars
- direct runtime mutation of shared physics-asset defaults is unsafe for multi-character scenarios
- editor iteration is weak because many changes require code edits and rebuilds
- hard to distinguish:
  - what is a temporary experiment
  - what is the intended long-term Stage 1 baseline
- easy to accumulate overlapping compensations:
  - mass policy
  - control-family policy
  - toe limit policy
  - target-range policy
  - future contact work
- less aligned with the project rule to avoid rebuilding what UE already provides

### Asset-Authored Path: Pros

- uses UE's intended authoring surfaces for constraints, contact, and grouped control response
- safer for two-character runtime because profiles are applied per component instance
- tuning becomes visible and editable in the editor instead of hidden in code
- easier for future maintainers to inspect:
  - which constraint profile is active
  - which bodies use which physical materials
  - which control sets receive which multipliers
- makes the bridge smaller and more defensible as "minimal custom code"
- separates:
  - authored content decisions
  - runtime policy-selection decisions
- makes it easier to reuse the same tuning on:
  - multiple characters
  - multiple maps
  - multiple regression scenarios

### Asset-Authored Path: Cons

- slower initial migration because authoring infrastructure has to be set up first
- requires disciplined asset naming and profile management in the editor
- some experiment surfaces do not map cleanly to a built-in authored UE asset:
  - PHC target-range scaling is still bridge-owned
- debugging can require looking in both code and content until the migration is complete
- poorly managed profiles can create their own complexity if too many near-duplicate variants are created

### What The Comparison Means In Practice

The current path is better for:

- short-lived diagnosis
- first-pass falsification
- rapidly checking whether a mismatch surface is real

The asset-authored path is better for:

- the committed Stage 1 baseline
- multi-character correctness
- maintainability
- future G2/G3 evidence production

### Recommendation

Do not pretend the current path was wrong.

It was the right path for discovering:

- toe authoring was not grossly broken
- ankle authoring was not grossly broken
- lower-limb occupancy was real
- target-range mismatch was real

But once a mismatch surface graduates from:

- "maybe this is the bug"

to:

- "this is now part of the baseline tuning story"

it should move out of bespoke runtime code and into authored UE surfaces where possible.

That means:

- keep runtime-first experimentation as the discovery tool
- move confirmed baseline behavior into constraint profiles, physical materials, and set-based tuning as soon as the experiment stabilizes

## Design Goal

The Stage 1 bridge should become a thin selector/orchestrator layer.

It should:

- load and validate authored assets
- apply authored constraint profiles per skeletal mesh instance
- apply authored PhysicsControl family multipliers through named sets
- read authored contact/friction configuration from physical materials and the physics asset
- keep only the minimum custom code needed for:
  - PHC observation packing
  - PHC inference
  - action-to-target conversion
  - activation / fail-stop state transitions
  - temporary experiments where UE has no built-in authored equivalent

## What Should Be Authored Instead Of Hardcoded

### 1. Constraint Behavior

Use Physics Asset constraint profiles instead of mutating `UPhysicsConstraintTemplate::DefaultInstance` at runtime.

Relevant UE seams:

- [SkeletalMeshComponent.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/SkeletalMeshComponent.h#L2265)
- [PhysicsConstraintTemplate.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/PhysicsEngine/PhysicsConstraintTemplate.h)
- [ConstraintInstance.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/PhysicsEngine/ConstraintInstance.h)

This is the correct home for:

- toe limit variants
- lower-limb conservative profiles
- projection settings
- shock propagation settings
- parent-dominates flags
- mass conditioning flags

This is explicitly safer than the current direct runtime mutation path in [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L1997).

### 2. Contact And Friction

Use authored `UPhysicalMaterial` assets on the character bodies and the floor, not runtime friction logic buried in the bridge.

Relevant UE seams:

- [BodySetup.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/PhysicsEngine/BodySetup.h)
- [BodyInstance.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/PhysicsEngine/BodyInstance.h#L654)
- [PrimitiveComponent.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h#L2911)

This is the correct home for:

- foot vs toe surface friction
- floor grip levels
- restitution
- combine mode choices

### 3. Control-Family Response

Use named PhysicsControl sets plus set-wide multiplier updates instead of per-bone `if` trees.

Relevant UE seams:

- [PhysicsControlComponent.h](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Public/PhysicsControlComponent.h#L510)
- [PhysicsControlComponent.h](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Public/PhysicsControlComponent.h#L555)
- [PhysicsControlComponent.h](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Public/PhysicsControlComponent.h#L1277)
- [PhysicsControlData.h](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Public/PhysicsControlData.h#L417)

This is the correct home for:

- leg-chain gain families
- arm-chain gain families
- toe-chain extra damping families
- body-modifier movement/collision/gravity group behavior

## Asset Inventory

Create or formalize these authored assets.

### Physics Asset Profiles

Asset:

- `/Game/Characters/Mannequins/Rigs/PA_Mannequin`

Required constraint profile names:

- `Stage1_Default`
- `Stage1_LocomotionBase`
- `Stage1_LowerLimbConservative`
- `Stage1_Recovery`
- `Stage1_FailStop`

Initial intended use:

- `Stage1_Default`
  - mirror the current Manny defaults
- `Stage1_LocomotionBase`
  - current best locomotion baseline
  - includes the current measured toe half-clamp equivalent on `ball_*`
- `Stage1_LowerLimbConservative`
  - stricter lower-limb constraint behavior for movement stress cases
- `Stage1_Recovery`
  - relaxed profile for visual recovery/reblending if needed later
- `Stage1_FailStop`
  - defensive profile used only after runtime instability or manual bridge stop

Important rule:

- these profiles are per-joint authored variants inside the physics asset
- the bridge applies them per component instance with `SetConstraintProfile(...)`
- the bridge must not edit `DefaultInstance` limits directly once this design lands

### Physical Materials

Create these assets:

- `/Game/Physics/Materials/PM_Stage1_Foot`
- `/Game/Physics/Materials/PM_Stage1_Toe`
- `/Game/Physics/Materials/PM_Stage1_Floor_Default`
- `/Game/Physics/Materials/PM_Stage1_Floor_LowGrip`

Initial assignment policy:

- `foot_l`, `foot_r` physics bodies use `PM_Stage1_Foot`
- `ball_l`, `ball_r` physics bodies use `PM_Stage1_Toe`
- the locomotion test floor uses `PM_Stage1_Floor_Default`
- special diagnostic maps or actors may use `PM_Stage1_Floor_LowGrip`

These assets should own:

- static friction
- dynamic friction
- restitution
- friction combine mode
- restitution combine mode

### PhysicsControl Family Sets

The current initializer already recreates controls and body modifiers and can preserve set membership in [PhysAnimStage1InitializerComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStage1InitializerComponent.cpp#L200).

Formalize these sets.

Control sets:

- `All`
- `LowerLimb`
- `LowerLimbLeft`
- `LowerLimbRight`
- `HipChain`
- `KneeAnkleChain`
- `ToeChain`
- `SpineChain`
- `NeckHead`
- `UpperLimb`
- `UpperLimbLeft`
- `UpperLimbRight`
- `HandChain`

Body modifier sets:

- `All`
- `Root`
- `LowerLimb`
- `UpperLimb`
- `SpineChain`

Exact bone membership:

- `LowerLimb`
  - `thigh_l`
  - `calf_l`
  - `foot_l`
  - `ball_l`
  - `thigh_r`
  - `calf_r`
  - `foot_r`
  - `ball_r`
- `HipChain`
  - `thigh_l`
  - `thigh_r`
- `KneeAnkleChain`
  - `calf_l`
  - `foot_l`
  - `calf_r`
  - `foot_r`
- `ToeChain`
  - `ball_l`
  - `ball_r`
- `SpineChain`
  - `spine_01`
  - `spine_02`
  - `spine_03`
- `NeckHead`
  - `neck_01`
  - `head`
- `UpperLimbLeft`
  - `clavicle_l`
  - `upperarm_l`
  - `lowerarm_l`
  - `hand_l`
- `UpperLimbRight`
  - `clavicle_r`
  - `upperarm_r`
  - `lowerarm_r`
  - `hand_r`
- `HandChain`
  - `hand_l`
  - `hand_r`

## Runtime Design

### Bridge Responsibilities After Refactor

`UPhysAnimComponent` should do four things only in this tuning area:

1. Resolve which authored profile bundle is active.
2. Apply authored constraint profiles to the live skeletal mesh instance.
3. Apply set-wide control/body-modifier multipliers through `UPhysicsControlComponent`.
4. Log which authored bundle is active.

It should not:

- rewrite constraint limits numerically bone by bone
- encode friction decisions in code
- special-case every lower-limb bone in scaling functions unless UE has no authored seam

### New Runtime Concept: Stage 1 Physics Tuning Bundle

Introduce a thin runtime concept named `Stage1PhysicsTuningBundle`.

This is not a new UE asset type.

It is a small bridge-side struct that names authored assets and set multipliers.

Suggested fields:

- `ConstraintProfileAll`
- `ConstraintProfileLowerLimb`
- `ConstraintProfileToeChain`
- `ControlSetMultipliers`
- `BodyModifierSetOverrides`
- `ExpectedBodyPhysicalMaterials`

The bundle does not store raw per-bone limit numbers.
It stores:

- profile names
- set names
- expected authored asset names

That keeps the bridge in selector mode instead of policy-author mode.

## Concrete Implementation

### Step 1. Add Set Taxonomy To The Initializer

File to change:

- [PhysAnimStage1InitializerComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStage1InitializerComponent.cpp)

Implementation:

1. Add helper functions that return set names for each control bone and body modifier bone.
2. After `CreateNamedControl(...)`, add the control to every declared authored set.
3. After `CreateNamedBodyModifier(...)`, add the modifier to every declared authored set.
4. Keep `All` as a universal fallback set.

Result:

- the bridge can tune entire families with:
  - `SetControlSparseMultipliersInSet(...)`
  - `SetBodyModifierSparseDatasInSet(...)`
  - `SetBodyModifiersInSetMovementType(...)`
  - `SetBodyModifiersInSetCollisionType(...)`

### Step 2. Replace Per-Bone Control-Family Branches With Set-Wide Multipliers

Files to change:

- [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp)
- [PhysAnimComponent.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h)

Current branchy seams to retire after migration:

- `ResolveTrainingAlignedControlStrengthScaleForBone(...)`
- `ResolveTrainingAlignedControlExtraDampingScaleForBone(...)`

Replacement design:

1. Keep one authored baseline control data when controls are created.
2. Apply family tuning through set-wide sparse multipliers:
   - `LowerLimb`
   - `KneeAnkleChain`
   - `ToeChain`
   - `SpineChain`
   - `UpperLimb`
3. Make the runtime store only the blend amount and bundle selection.
4. Let the bundle define which sets receive which multipliers.

Example:

- `Stage1_LocomotionBase` bundle
  - `LowerLimb`: angular strength `1.00`, damping ratio `1.00`, extra damping `1.00`
  - `ToeChain`: angular strength `1.00`, damping ratio `1.10`, extra damping `1.15`
  - `UpperLimb`: angular strength `0.72`, damping ratio `1.00`, extra damping `0.90`

These values should be kept in one compact table in code until a later assetized config is justified.
The important point is that the table is set-based, not bone-based.

### Step 3. Replace Runtime Toe Limit Mutation With Constraint Profile Application

Files to change:

- [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp)
- [PhysAnimComponent.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h)

Current seam to retire:

- `ApplyTrainingAlignedToeLimitPolicy(...)`
- `ResetTrainingAlignedToeLimitPolicy(...)`

Replacement design:

1. Author `Stage1_Default`, `Stage1_LocomotionBase`, and `Stage1_LowerLimbConservative` inside `PA_Mannequin`.
2. On bridge activation:
   - call `USkeletalMeshComponent::SetConstraintProfile(...)` on each relevant joint
   - do not mutate template defaults
3. On bridge teardown:
   - restore the default authored profile by name
4. On fail-stop:
   - optionally switch to `Stage1_FailStop`

Joint application scope:

- `foot_l <- calf_l`
- `ball_l <- foot_l`
- `foot_r <- calf_r`
- `ball_r <- foot_r`

Recommended first authored profile contents:

- `Stage1_LocomotionBase`
  - toe joints use the current measured half-clamp equivalent rather than the full clamp
  - ankle joints initially remain at the current authored Manny values
- `Stage1_LowerLimbConservative`
  - toe joints use the same half-clamp or slightly stricter version
  - ankle joints may add projection or mass conditioning only after measurement supports it

### Step 4. Author Contact/Friction Instead Of Runtime Guessing

Files to change:

- none required in the bridge for the first pass

Editor work:

1. Assign `PM_Stage1_Foot` to `foot_l` and `foot_r` bodies in `PA_Mannequin`.
2. Assign `PM_Stage1_Toe` to `ball_l` and `ball_r`.
3. Assign `PM_Stage1_Floor_Default` to the locomotion floor.
4. Add one low-grip diagnostic floor variant for friction sensitivity tests.

Bridge role:

- validate that expected materials are present
- log which material names are active on activation

Bridge should not:

- compute friction policy values itself
- swap physical materials frequently during normal runtime

### Step 5. Keep Target-Range Policy As The Only Remaining Bridge-Owned Experiment Surface

There is no built-in UE authored asset that directly represents "PHC action-to-target range scaling by body family."

So this surface may stay in the bridge for now.

But even here, reduce branchiness:

1. express target-range policies by named family table:
   - `LowerLimb_None`
   - `LowerLimb_KneeAnkleConservative`
   - `LowerLimb_FullConservative`
2. store scales by family set, not by arbitrary per-bone conditionals
3. apply the family table only in the policy-target conversion stage

This keeps the one truly custom surface small and explicit.

## Authoring Workflow

### Constraint Profiles

Inside the Physics Asset Editor:

1. open `PA_Mannequin`
2. create the named constraint profiles
3. author profile values only on the constraints that need variants
4. leave unaffected joints equivalent to `Stage1_Default`
5. verify left/right symmetry for all lower-limb profiles

### PhysicsControl Sets

Inside plugin code:

1. create controls with stable names
2. assign them to stable named sets in the initializer
3. verify set membership with automation

This is code-driven set creation, but asset-authored tuning uses those sets as the public surface.

### Contact/Friction

Inside the editor:

1. create physical materials
2. assign them to the relevant bodies and floor assets
3. validate in automation that the expected materials are still bound

## Verification

Add automation coverage for authoring, not just runtime math.

Suggested tests:

- `PhysAnim.Component.ControlSetAuthoring`
  - every control belongs to `All`
  - lower-limb controls belong to the correct family sets
- `PhysAnim.Component.BodyModifierSetAuthoring`
  - root and lower-limb modifier sets are present and correct
- `PhysAnim.Component.MannyConstraintProfiles`
  - required profile names exist on relevant lower-limb constraints
- `PhysAnim.Component.MannyBodyPhysicalMaterials`
  - foot and toe bodies resolve to the expected physical materials
- `PhysAnim.Component.ConstraintProfileApplication`
  - applying the profile names on a live mesh instance succeeds

Runtime verification matrix:

- passive idle smoke
- movement smoke
- movement soak
- G2 presentation
- low-grip floor diagnostic run

## Migration Plan

### Phase A. Infrastructure

Implement first:

- set taxonomy in the initializer
- set authoring tests
- profile existence tests
- physical material existence/binding tests

No behavior change yet.

### Phase B. Runtime Selection

Implement second:

- bridge-side tuning bundle selection
- set-based multiplier application
- constraint profile application by name

Keep existing branchy paths behind temporary fallback toggles.

### Phase C. Cleanup

Remove once parity is confirmed:

- direct toe constraint mutation code
- per-bone control-family scaling functions that are now set-driven
- temporary code-only friction assumptions if any appear later

## Rules

### What Must Be Authored

Use authored UE assets for:

- hard and operating constraint variants
- contact/friction behavior
- group-level PhysicsControl tuning surfaces

### What May Stay In Code

Keep in the bridge only if UE has no clean authored equivalent:

- PHC tensor contract
- PHC action unpacking
- target-range scaling tables
- runtime activation/fail-stop sequencing

### What Must Not Happen

Do not:

- keep adding one-off lower-limb runtime branches when a profile or set can represent the same idea
- mutate shared physics asset defaults during play
- hide contact behavior in undocumented runtime experiments

## Expected Outcome

If this design is implemented, Stage 1 tuning becomes:

- more multi-character safe
- easier to reason about
- faster to iterate in the editor
- more aligned with "use UE built-ins when they exist"
- less likely to turn the bridge into a custom physics-policy framework

The bridge remains custom where it must be custom.
The tuning layer stops pretending it must be custom when UE already has the right authoring surface.
