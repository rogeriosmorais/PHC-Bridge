# Training-to-UE Physics Alignment Plan

## Goal

Bring the UE5 Stage 1 runtime closer to the physical assumptions the ProtoMotions policy was trained under, without violating the locked Stage 1 architecture or blindly copying simulator settings that do not map 1:1 into UE PhysicsControl.

This plan is specifically about four alignment surfaces:

1. Mass distribution
2. PD gains / control response
3. Timestep and control cadence
4. Joint limits

## Non-Goals

- Do not change the locked Stage 1 architecture in [ENGINEERING_PLAN.md](/F:/NewEngine/ENGINEERING_PLAN.md).
- Do not replace UE PhysicsControl with a custom PD solver.
- Do not retrain the ProtoMotions policy as the first response.
- Do not treat the standing perturbation presentation problem as the primary target here. This plan is about reducing training/runtime mismatch first.

## Sources Consulted

### ProtoMotions online docs

- ProtoMotions docs: <https://nvlabs.github.io/ProtoMotions/>
- ProtoMotions GitHub README: <https://github.com/NVlabs/ProtoMotions>

### UE online docs

- UPhysicsControlComponent: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/UPhysicsControlComponent>
- FPhysicsControlData: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlData>
- FPhysicsControlMultiplier: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlMultiplier>
- FPhysicsControlModifierData: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PhysicsControl/FPhysicsControlModifierData>
- UPrimitiveComponent::SetMassOverrideInKg: <https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UPrimitiveComponent/SetMassOverrideInKg>
- Physics Sub-Stepping in Unreal Engine: <https://dev.epicgames.com/documentation/en-us/unreal-engine/physics-sub-stepping-in-unreal-engine>
- Constraints User Guide: <https://dev.epicgames.com/documentation/en-us/unreal-engine/constraints-user-guide-in-unreal-engine>
- Creating a New Physics Body in the Physics Asset Editor: <https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-a-new-physics-body-in-unreal-engine-by-using-the-physics-asset-editor>

### Local ProtoMotions code and assets

- [config.yaml](/F:/NewEngine/Training/ProtoMotions/data/pretrained_models/motion_tracker/smpl/config.yaml)
- [smpl.yaml](/F:/NewEngine/Training/ProtoMotions/protomotions/config/robot/smpl.yaml)
- [config.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/config.py)
- [simulator.py](/F:/NewEngine/Training/ProtoMotions/protomotions/simulator/base_simulator/simulator.py)
- [smpl_humanoid.xml](/F:/NewEngine/Training/ProtoMotions/protomotions/data/assets/mjcf/smpl_humanoid.xml)

### Local UE source

- [PhysicsControlData.h](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Public/PhysicsControlData.h)
- [PhysicsControlComponentImpl.cpp](/E:/UE_5.7/Engine/Plugins/Experimental/PhysicsControl/Source/PhysicsControl/Private/PhysicsControlComponentImpl.cpp)
- [PrimitiveComponent.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/PrimitiveComponent.h)
- [BodyInstance.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/PhysicsEngine/BodyInstance.h)
- [PrimitiveComponentPhysics.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/PrimitiveComponentPhysics.cpp)
- [BodyInstance.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/PhysicsEngine/BodyInstance.cpp)
- [PhysicsSettings.h](/E:/UE_5.7/Engine/Source/Runtime/Engine/Classes/PhysicsEngine/PhysicsSettings.h)
- [PhysLevel.cpp](/E:/UE_5.7/Engine/Source/Runtime/Engine/Private/PhysicsEngine/PhysLevel.cpp)

### Current bridge implementation

- [PhysAnimStage1InitializerComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStage1InitializerComponent.cpp)
- [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp)
- [PhysAnimBridge.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp)

## Training-Side Facts We Should Align To

### 1. Control cadence is not the same as simulator Hz

The pretrained SMPL checkpoint is configured for:

- `control_type: built_in_pd`
- `use_biased_controller: false`
- `map_actions_to_pd_range: true`
- IsaacLab simulator `fps: 120`
- IsaacLab `decimation: 4`

In ProtoMotions, `dt = decimation / simulator_fps`, so the effective policy/control interval for the pretrained IsaacLab path is:

- `4 / 120 = 0.033333... s`
- `30 Hz control`

This is the most important exact-match target in this plan.

### 2. PD actions are mapped into simulator joint target ranges

ProtoMotions does not directly output UE-style spring constants. It maps actions into PD target ranges using the simulator joint limits:

- `_action_to_pd_targets(...)`
- `build_pd_action_offset_scale(...)`

That means training assumed a specific joint range model, not just an arbitrary action scale.

### 3. The pretrained config does not declare per-joint gains in YAML

The checkpoint config sets:

- `stiffness: null`
- `damping: null`

So the effective built-in PD behavior comes from the robot/simulator asset path, not from an explicit table in the checkpoint YAML.

### 4. The SMPL MJCF asset still contains important physical priors

The local SMPL MJCF asset provides the best available training-side physical reference we have:

- broad joint ranges
- per-joint stiffness and damping patterns
- per-shape density values that imply body mass distribution

Representative joint-family values in the local asset are:

- torso/spine/chest: about `1000 / 100`
- hips/knees/ankles: about `800 / 80`
- shoulders/elbows/neck/head/toes: about `500 / 50`
- wrists/hands: about `300 / 30`

Those numbers should not be copied blindly into UE, but they are the correct starting reference.

## UE-Side Facts That Matter

### 1. PhysicsControl does not use the same PD parameterization

UE PhysicsControl uses:

- `AngularStrength`
- `AngularDampingRatio`
- `AngularExtraDamping`

and internally converts them into spring parameters. This means ProtoMotions `stiffness/damping` and UE PhysicsControl values are not literally the same quantity.

Implication:

- we should match UE response to ProtoMotions response
- we should not promise a direct numeric copy of `800 / 80` into `AngularStrength / AngularDampingRatio`

### 2. UE exposes the correct mass editing seams

UE supports:

- `SetMassOverrideInKg(BoneName, MassInKg, bOverrideMass)`
- `SetMassScale(BoneName, Scale)`
- `UpdateMassProperties()`

Implication:

- per-body mass alignment is feasible in Stage 1 without changing the architecture
- we should prefer explicit measured overrides or controlled scales, not ad hoc global mass changes

### 3. UE Physics substepping and policy cadence are different questions

UE substepping improves simulation stability, but it does not automatically align policy cadence to training cadence.

Implication:

- it is valid to keep Chaos at `120 Hz`
- while only evaluating the policy and updating targets at `30 Hz`
- and holding targets constant across substeps and/or render frames in between

### 4. Joint limits live in the physics asset / constraints, not in the policy

UE constraint limits can be authored and edited in the Physics Asset and constraint setup. This gives us a place to compare Manny against SMPL training ranges and deliberately choose a policy-compatible operating envelope.

## Current Likely Mismatches

### 1. Control/update cadence mismatch

The current bridge runs inference, target writes, and `PhysicsControl->UpdateControls(DeltaTime)` from `TickComponent(...)`. That strongly suggests the bridge is currently using render/game tick cadence, not a fixed `30 Hz` policy cadence.

This is the highest-confidence mismatch and should be addressed first.

### 2. Uniform Stage 1 gains

Current Stage 1 authored defaults are:

- `AngularStrength = 800`
- `AngularDampingRatio = 1.25`
- `AngularExtraDamping = 30`

applied broadly, with runtime multipliers on top.

This is convenient for stabilization work, but it is not a credible approximation of the SMPL training asset’s joint-family-specific behavior.

### 3. Manny body masses almost certainly do not match SMPL relative masses

Manny’s physics bodies and shapes were authored for a UE mannequin asset, not for SMPL’s training distribution.

Even if total mass is reasonable, segment-to-segment inertia is likely mismatched.

### 4. Manny joint limits are probably not the training limits

The local SMPL MJCF includes many very broad ranges, while UE mannequin constraints are likely narrower and authored for gameplay ragdoll behavior.

This can distort:

- mapped action amplitudes
- reachable target poses
- steady-state effort required to hold a pose

## Recommended Work Order

Do not align all four surfaces at once.

Work in this exact order:

1. Control cadence / timestep
2. Joint-limit inventory and action-range audit
3. Mass distribution alignment
4. PD response fitting
5. Integrated regression

Reason:

- cadence mismatch can invalidate every other measurement
- joint-limit mismatch directly changes action semantics
- mass and PD fitting only make sense once cadence and ranges are coherent

## Workstream A: Control Cadence and Timestep Alignment

### Goal

Match the pretrained control interval first.

### Recommendation

Keep UE physics simulation at `120 Hz` substepped, but evaluate the policy and update policy-derived targets at a fixed `30 Hz` interval.

### Why

The pretrained IsaacLab path is:

- `120 Hz` simulation
- `decimation = 4`
- `30 Hz` control

If UE applies new policy targets every game frame or every physics substep, the actions are effectively over-applied relative to training.

### Implementation plan

1. Add a fixed-step policy accumulator in the bridge runtime.
2. Run observation pack + model inference + action conditioning at exactly `1 / 30 s`.
3. Hold the last policy targets between control ticks.
4. Continue calling `PhysicsControl->UpdateControls(...)` every runtime tick / physics update as UE expects.
5. Log:
   - effective policy tick rate
   - effective physics substep rate
   - policy ticks skipped / accumulated

### Success criteria

- policy update cadence in logs is stable at `30 Hz`
- no dependence on render FPS for policy rate
- smoke and movement tests still pass after cadence lock

## Workstream B: Joint-Limit and Action-Range Alignment

### Goal

Make sure the UE mannequin sees roughly the same action-to-target semantics as the SMPL training setup.

### Recommendation

Audit and align limits joint-by-joint before touching masses or PD fits.

### Why

ProtoMotions maps actions into simulator PD target ranges using the simulator’s own joint limits. If Manny’s effective constraint ranges are very different, the same action will mean a different target rotation in UE.

### Implementation plan

1. Extract the training-side joint ranges from the local SMPL MJCF asset.
2. Extract current UE joint limits from the Manny physics asset / constraints.
3. Build a mapping table:
   - SMPL joint
   - UE bone / control
   - SMPL nominal range
   - UE current limit
   - mismatch severity
4. Categorize each mismatch:
   - safe to broaden
   - safe to tighten
   - leave as-is for gameplay/visual reasons
5. Author a Stage 1 operating-limit table for the mapped Manny chain.

### Important constraint

Do not blindly copy MJCF extremes like `-180..180` or `-720..720` into UE. Those ranges may be policy-compatible in the simulator but ugly or destabilizing in Manny.

Stage 1 should use:

- a policy-compatible operating range
- plus a stricter hard safety range where needed

### Success criteria

- every mapped joint has an explicit recorded training-vs-UE comparison
- action amplitude mapping becomes auditable instead of implicit
- no unexplained saturations in the first policy-active frame

## Workstream C: Mass Distribution Alignment

### Goal

Bring Manny’s relative segment masses closer to the SMPL training body.

### Recommendation

Match relative mass distribution and center-of-mass ordering, not exact kilograms.

### Why

The policy learned against a particular inertia distribution. If Manny’s pelvis, thighs, calves, feet, torso, and arms carry very different relative masses, the same PD targets and contacts produce different accelerations and balance behavior.

### Implementation plan

1. Compute or export current UE per-body masses from Manny’s physics asset / runtime body instances.
2. Estimate SMPL training-side relative body masses from the MJCF geom densities and volumes.
3. Build a mass map from SMPL bodies to Manny bodies:
   - pelvis
   - thighs
   - calves
   - feet / toes
   - spine / chest / head
   - upper arms / lower arms / hands
4. Compare relative percentages, not only raw kg.
5. Adjust UE using:
   - `SetMassOverrideInKg(...)` for explicit control, or
   - `SetMassScale(...)` where family-wide proportional adjustment is better
6. Recompute and log:
   - total mass
   - per-segment percentage
   - estimated COM shift

### Guardrails

- preserve a sane total actor mass
- keep COM near the expected pelvis-centered location
- do not overfit tiny upper-body bodies before the leg chain is aligned

### Success criteria

- major segment relative masses are within a deliberate tolerance band
- pelvis-to-leg and torso-to-leg proportions are no longer arbitrary
- movement/soak behavior does not regress catastrophically

## Workstream D: PD Response Alignment

### Goal

Make UE PhysicsControl joint response feel closer to ProtoMotions built-in PD response.

### Recommendation

Fit response curves by measurement. Do not try to numerically equate ProtoMotions `stiffness/damping` to UE `AngularStrength / AngularDampingRatio / AngularExtraDamping`.

### Why

The two systems do not use the same parameterization.

### Implementation plan

1. Split the character into control families:
   - torso/spine
   - hips/knees/ankles
   - toes
   - neck/head
   - shoulders/elbows
   - wrists/hands
2. Use the SMPL MJCF values as target family ordering:
   - torso strongest
   - legs next
   - shoulders/head/toes moderate
   - wrists/hands weakest
3. Build a small response-fit harness:
   - apply a known target step to a representative joint family
   - measure settle time, overshoot, and steady-state error in UE
4. Compare that response against a ProtoMotions-side reference run for the same joint family and target angle.
5. Solve for UE family settings that approximate the same shape of response.
6. Store the result as authored Stage 1 control-family profiles, not one global default.

### Success criteria

- torso, legs, arms, and hands no longer share one fake global PD profile
- UE response hierarchy matches the training asset hierarchy
- stability tests remain green at the new authored values

## Deliverables

Create these artifacts before broad runtime retuning:

1. `control-cadence-audit.md`
   - current rate vs target rate
   - exact chosen Stage 1 policy/control cadence
2. `smpl-to-manny-limit-table.md`
   - per-joint training-vs-UE range comparison
3. `smpl-to-manny-mass-table.md`
   - per-segment relative mass comparison and chosen overrides/scales
4. `pd-response-fit.md`
   - family-by-family response measurements and chosen UE settings
5. code/tests implementing the chosen cadence lock and authored family profiles

## Verification Matrix

Every alignment pass should be validated against:

1. passive idle smoke
2. 65-second idle soak
3. deterministic movement smoke
4. movement soak
5. first-policy-active diagnostics
6. G2 presentation regression

And for each pass we should log:

- policy rate
- action conditioning summary
- max target delta on first policy frame
- per-body offender data
- fail-stop state

## Recommended First Pass

Do not start with masses or gains.

Start with this exact first pass:

1. Audit current effective policy/control update rate in UE.
2. Lock policy update cadence to `30 Hz`.
3. Re-run idle smoke, movement smoke, and first-policy diagnostics.
4. Only after that, generate the joint-limit inventory table.

This gives us the highest-value correction with the least ambiguity.

## First Pass Status

- March 12, 2026
- implemented:
  - fixed policy/control cadence lock at `30 Hz`
  - held policy-derived targets between control updates
  - policy-step diagnostics in runtime logs
  - helper tests for cadence interval and accumulator behavior
- verified with:
  - `PhysAnim.Component`
  - `PhysAnim.PIE.Smoke`
- completed next audit:
  - direct Manny constraint inventory for all `21` Stage 1 bridge controls
  - explicit SMPL-vs-Manny comparison table in [smpl-to-manny-limit-table.md](/F:/NewEngine/plans/stage1/40-design/smpl-to-manny-limit-table.md)
- current findings:
  - only `17 / 21` Stage 1 controls map to direct Manny constraint pairs
  - missing direct Manny pairs:
    - `neck_01`
    - `head`
    - `clavicle_l`
    - `clavicle_r`
  - current Manny lower-body, mid-spine, upper-spine, shoulder, and elbow limits are much tighter than the broad SMPL training ranges
- completed next audit:
  - first family-level SMPL-vs-Manny mass comparison in [smpl-to-manny-mass-table.md](/F:/NewEngine/plans/stage1/40-design/smpl-to-manny-mass-table.md)
- current findings:
  - Manny is currently torso-heavy and upper-body-heavy relative to the ProtoMotions SMPL asset
  - Manny is currently leg-light relative to the ProtoMotions SMPL asset
  - `spine_01` is effectively massless in the current Manny inventory (`~0.005 kg` in the audit path), so the torso mass is concentrated higher in the chain than the training asset
- completed next runtime pass:
  - first family-level Manny mass-adjustment policy is now implemented in the live bridge runtime
  - the bridge applies training-aligned Manny family mass scales on bridge activation and restores original scales on bridge teardown
- completed next runtime pass:
  - first training-aligned control-family response fit is now enabled in the live bridge runtime
  - the Stage 1 baseline now keeps the family profile enabled at `blend=0.50`
  - tested movement-smoke sweep outcomes:
    - `blend=0.00`: passes, but shows higher forward lower-body spikes
    - `blend=0.25`: passes, but introduces a worse late angular outlier
    - `blend=0.50`: best measured nonzero fit in the first sweep
    - `blend=1.00`: passes, but does not improve the movement peaks versus `0.50`
- current verified result:
  - `PhysAnim.Component` passes with `PhysAnim.Component.MannyMassInventory` and the new mass-policy helper coverage
  - `PhysAnim.PIE.Smoke` passes
  - `PhysAnim.PIE.MovementSmoke` passes
  - `PhysAnim.PIE.G2Presentation` passes
- next alignment task:
  - keep the current family mass policy and `0.50` control-family blend as the Stage 1 baseline
  - then fit the remaining lower-limb outlier locally:
    - first `ball_*` family reassignment to the locomotion leg baseline
    - then toe-specific extra-damping fitting if toe angular peaks remain the dominant offender
  - toe local fitting result:
    - the toe-family reassignment is the best measured toe-focused runtime fit so far
    - isolated toe-only gain changes were worse
  - authoring audit result:
    - the manually created toe constraints are structurally sane and left/right symmetric
  - ankle authoring result:
    - the direct Manny ankle constraints are structurally sane and symmetric
    - there is no gross sign that `foot_* <- calf_*` was authored incorrectly
  - toe operating-limit result:
    - a full toe-limit blend is too aggressive
    - a `0.50` toe-limit blend is the first measured toe-limit baseline candidate
  - lower-limb occupancy result:
    - under deterministic movement, `calf_*` repeatedly over-occupies the current tightest direct-limit proxy
    - measured range is about `1.7x - 2.6x` once locomotion is active
  - next pass should keep the `0.50` toe-limit baseline and move to a knee/ankle-chain lower-limb target-range policy
  - first knee/ankle-chain target-range result:
    - training-side review confirmed the relevant ProtoMotions lower-limb path is the `3-DoF` symmetric `1.2x` action-range expansion used by the SMPL knee/ankle/toe joints
    - runtime policy now scales:
      - `calf_*` targets to `0.50`
      - `foot_*` targets to `0.75`
    - deterministic movement smoke stays green with no fail-stop
  - lower-limb occupancy falls materially, commonly to about `0.7x - 1.2x`
  - but the largest remaining body spikes still propagate into the distal foot/toe chain
  - next pass should inspect lower-limb representation / distal coupling, not revert to more constraint-authoring tweaks
  - distal-chain target-range fit result:
    - extending the lower-limb range policy into the distal chain keeps movement smoke stable
    - first forward-burst peaks improve materially versus the knee/ankle-only baseline
    - but later backward/strafe phases still produce large `ball_*` spikes
  - current read:
    - scalar distal range shaping helps, but it is not the final fix
    - the next pass should inspect explicit distal target representation under locomotion, not only more scalar range changes
  - first movement-only distal representation fit result:
    - added a locomotion-time distal attenuation seam:
      - activates above `50 cm/s` planar owner speed
      - `foot_*` locomotion representation scale `0.75`
      - `ball_*` locomotion representation scale `0.50`
    - deterministic movement smoke stays green with no fail-stop
    - measured result:
      - early forward movement spikes improve somewhat relative to the distal-range baseline
      - backward and strafe phases still produce large `ball_*` angular spikes
    - current read:
      - locomotion-time distal attenuation is a useful measured experiment, but not a clean new baseline
      - the next pass should move to a more structural distal target construction experiment, not another scalar attenuation
  - first distal locomotion target-composition result:
    - added a per-control locomotion composition policy:
      - above `50 cm/s`, `foot_*` and `ball_*` switch to explicit-only target mode
      - the rest of the body stays on the current policy-active skeletal-target composition path
    - deterministic movement smoke stays green with no fail-stop
    - measured result:
      - backward distal spikes improve materially relative to the locomotion-time attenuation baseline
      - strafe remains mixed, with some peaks shifting up the lower-limb chain
    - current read:
      - composition mode is a real lower-limb mismatch surface
      - distal-only composition switching is not yet a clean final baseline
      - the next pass should test whether the explicit-only composition policy needs to expand to the full knee/ankle/toe chain or whether locomotion-transition handling is the real remaining problem
  - full lower-limb locomotion composition result:
    - widened the same locomotion-time explicit-only target mode to:
      - `calf_*`
      - `foot_*`
      - `ball_*`
    - deterministic movement smoke stays green with no fail-stop
    - measured result:
      - no locomotion-start discontinuity
      - forward and backward remain mixed
      - one forward sample becomes materially worse, with `ball_l` peaking around `10104 deg/s`
      - late backward still shows large distal spikes, including `ball_r` around `9176 deg/s`
    - current read:
      - representation mode is still a valid mismatch surface
      - but simple whole-chain explicit-only switching is not a clean new baseline
      - the next pass should move to locomotion transition handling or a more proximal lower-limb composition experiment rather than more distal-set widening
  - lower-limb composition transition result:
    - reverted the affected set back to:
      - `foot_*`
      - `ball_*`
    - added stateful locomotion-time composition switching:
      - enter threshold `50 cm/s`
      - exit threshold `100 cm/s`
      - enter hold `0.20 s`
      - exit hold `0.20 s`
    - deterministic movement smoke stays green with no fail-stop
    - measured result:
      - no locomotion-start discontinuity
      - forward outliers are materially lower than the failed full-chain pass
      - backward outliers are also materially lower than the failed full-chain pass
      - some remaining peaks migrate proximally into `thigh_*` or other non-distal bodies instead of disappearing entirely
    - current read:
      - abrupt runtime representation flips were part of the locomotion problem
      - stateful hysteresis/dwell is the best measured locomotion-composition baseline so far
      - the next pass should inspect more proximal lower-limb composition or another locomotion-time policy seam, not widen the distal explicit-only set again
  - proximal lower-limb composition result:
    - tested adding `thigh_*` to the same explicit-only locomotion composition set while keeping `calf_*` on the composed path
    - deterministic movement smoke stayed green with no fail-stop
    - measured result:
      - forward regressed materially, with `ball_r` rising into roughly `~6177 - 7009 deg/s`
      - backward also regressed materially, with `foot_r` / `ball_r` angular spikes rising into roughly `~8700 - 11979 deg/s`
      - repeated `calf_r` linear spikes also rose into the `~2200 - 3080 cm/s` range
    - current read:
      - `thigh_* + foot_* + ball_*` explicit-only switching is not the right new baseline
      - the next pass should move to another locomotion-time mismatch surface, most likely lower-limb target write timing or another policy-side transition seam
  - distal explicit target-velocity result:
    - kept the committed `foot_*` / `ball_*` hysteresis+dwell locomotion-composition baseline unchanged
    - changed only one seam:
      - when distal explicit-only locomotion mode is active for `foot_*` / `ball_*`
      - pass `AngularVelocityDeltaTime = 0.0f` into `SetControlTargetOrientation(...)`
      - so UE does not synthesize explicit target angular velocity from target-orientation deltas for those controls
    - deterministic movement smoke stays green with no fail-stop
    - measured result:
      - first forward distal spikes drop materially into the low-thousands range
      - lower-limb occupancy remains mostly around `~0.9x - 1.1x`
      - some later peaks still migrate proximally into `calf_*`, `thigh_*`, and occasional `foot_*`
    - current read:
      - synthesized explicit target angular velocity was a real mismatch surface
      - this pass is a keepable improvement to the current locomotion-time distal baseline
      - the next pass should inspect per-bone write smoothing or another proximal lower-limb seam, not re-open whole-chain explicit-only switching
  - lower-limb composed target-velocity result:
    - kept the committed `foot_*` / `ball_*` hysteresis+dwell locomotion-composition baseline
    - widened explicit target angular-velocity suppression to:
      - `thigh_*`
      - `calf_*`
      - `foot_*`
      - `ball_*`
    - deterministic movement smoke stayed green with no fail-stop
    - measured result:
      - forward was mixed, with some distal improvement but continued large proximal spikes
      - backward regressed, with large `ball_*` spikes still present
      - late idle/strafe also showed new `ball_*` outliers
    - current read:
      - broad lower-limb explicit target angular-velocity suppression is not a clean new baseline
      - the runtime code should stay on the narrower distal-only suppression baseline
      - the next pass should target per-bone target-write smoothing or another proximal locomotion transition seam
  - locomotion-time proximal lower-limb response-fit result:
    - re-checked official UE PhysicsControl docs/source and ProtoMotions control code before changing runtime behavior
    - implemented a locomotion-time proximal response profile only for `thigh_*` / `calf_*`
    - first-pass scales:
      - damping ratio scale `1.20`
      - extra damping scale `1.35`
    - deterministic movement smoke stays green with no fail-stop
    - measured result:
      - forward remains mixed
      - backward stays difficult but does not reopen the old high-spike regime
      - strafe and late idle improve materially relative to the distal-only suppression baseline
    - current read:
      - this is a keepable improvement on top of the distal-only suppression baseline
      - the next pass should stay in locomotion-time response fitting, most likely with a more selective per-bone proximal profile rather than another target-semantics experiment

## Working Hypothesis

The most likely major mismatch today is not mass or joint limits first. It is that the Stage 1 bridge is still operating at a different control cadence and with a different effective action-range interpretation than the pretrained ProtoMotions SMPL policy expected.

If that hypothesis is correct:

- control cadence alignment should change runtime behavior immediately
- joint-limit alignment should reduce policy-target mismatch next
- mass and PD-family fitting should then become more interpretable instead of compensating for cadence/range errors
