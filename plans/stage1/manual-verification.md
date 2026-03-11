# Stage 1 ELI5 Manual Verification

## Purpose

This document standardizes the human-readable verification steps for anything that cannot be covered cleanly by automated tests or TDD.

## ELI5 Template

Use this format for every manual checkpoint:

- `What you are checking`
- `Why it matters`
- `What to click or run`
- `What good looks like`
- `What bad looks like`
- `What evidence to send back`

## Initial Stage 1 Manual Checks

### MV-G1-01: PHC Motion Looks Alive In Training

- `What you are checking`: whether the PHC-driven training result looks balanced and human-like instead of stiff or unstable
- `Why it matters`: G1 fails if the training-side motion already looks robotic or unstable
- `Known working preflight on March 10, 2026`:
  1. Close `MSI Afterburner`, `RTSS`, `RTSSHooksLoader64`, and any OBS game-capture session before launching Isaac Lab. External graphics hooks caused one earlier Vulkan crash on this machine.
  2. Keep `$env:OMNI_KIT_ACCEPT_EULA='YES'` in the same PowerShell session before running the command below.
  3. Use the locally patched ProtoMotions tree, not a clean upstream checkout. The working path now depends on local compatibility fixes for:
     - preloading `h5py` before Isaac Lab `AppLauncher`
     - excluding broken RTX sensor extensions at Kit launch
     - passing `Se2KeyboardCfg` into `Se2Keyboard`
     - handling the MoviePy `2.x` import path without `moviepy.editor`
- `What to click or run`:
  1. Open PowerShell.
  2. Change directory to `F:\NewEngine\Training\ProtoMotions`.
  3. Run this exact command:

```powershell
$env:OMNI_KIT_ACCEPT_EULA='YES'
& 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe' protomotions/eval_agent.py +robot=smpl +simulator=isaaclab +motion_file=F:\NewEngine\Training\ProtoMotions\data\motions\smpl_humanoid_walk.npy +checkpoint=F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt +terrain=flat +headless=False +num_envs=1 +agent.config.max_eval_steps=3000 +fabric.strategy=auto +experiment_name=phase0_eval_visual
```

  4. Wait for the Isaac Lab viewer window to appear and the character to start moving.
  5. Press `L` once to start recording.
  6. Let it run for about `10` to `15` seconds.
  7. Press `L` again to stop recording.
  8. Wait for a console line that says `Video saved to ...mp4`.
  9. Press `Q` to close the viewer.
  10. Find the clip in `F:\NewEngine\Training\ProtoMotions\output\renderings\phase0_eval_visual-<timestamp>.mp4`.
- `Known runtime notes`:
  - If you see `generic_mo_io.dll`, `isaacsim.sensors.rtx`, lidar, or radar DLL load errors in the console, treat them as noisy but currently non-blocking for `MV-G1-01` if the viewer still appears and recording still saves.
  - If the viewer crashes before rendering anything, first suspect an overlay or capture hook before suspecting the PHC checkpoint.
  - If the walk renders and recording saves, count that as the working path for this checkpoint even if the process prints extra Isaac sensor warnings during startup or shutdown.
- `If recording does not save`: send back the console error instead of guessing; the local code path uses the built-in `L` toggle in `protomotions/simulator/isaaclab/simulator.py`
- `What good looks like`: the character stays balanced, weight shifts look natural, and the movement does not jitter or explode
- `What bad looks like`: the character falls for no reason, jitters badly, or looks like a rigid puppet
- `What evidence to send back`: one short video or GIF and one sentence saying `pass`, `fail`, or `unclear`

### MV-G1-02: Manny Responds To Programmatic Control In UE5

- `What you are checking`: whether Manny's physics-driven body actually responds when control targets are updated
- `Why it matters`: if the Physics Control path is broken, the Stage 1 bridge cannot work even if the PHC model itself is fine
- `What this checkpoint is and is not`:
  - this checkpoint is only about proving that `UPhysicsControlComponent` can move the intended body region when a UE-side test updates targets
  - this checkpoint is a stationary control-path proof, not a locomotion or whole-character stability test
  - this checkpoint is not the full PHC bridge, not the SMPL mapping test, and not the final motion-quality comparison
- `Current truth on March 10, 2026`:
  - the exact Stage 1 test path is now frozen
  - the current scaffold pawn is `BP_ThirdPersonCharacter` using `CharacterMesh0` with `/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple`
  - Quinn is acceptable here because Stage 1 is locked to the Manny/Quinn mannequin skeleton family
  - the `PhysAnimPlugin` runtime harness built successfully on this machine on March 10, 2026
- `Frozen test inputs`:
  1. exact UE map path:
     - `/Game/ThirdPerson/Lvl_ThirdPerson`
  2. exact runtime owner:
     - `UPhysAnimMvG102Subsystem`
     - source path: `F:\NewEngine\PhysAnimUE5\Plugins\PhysAnimPlugin\Source\PhysAnimPlugin\Public\PhysAnimMvG102Subsystem.h`
  3. exact trigger method:
     - start PIE in `Lvl_ThirdPerson`
     - open the in-game console with `` ` `` or `~`
     - run `PhysAnim.MVG102.Start`
     - optional stop command: `PhysAnim.MVG102.Stop`
  4. exact body region expected to move first:
     - left arm, especially the left hand / forearm chain
  5. exact evidence format required back:
     - short clip preferred
     - screenshot acceptable if a clip is not practical
  6. exact local build note:
     - on this machine the plugin already built successfully on March 10, 2026
     - if Unreal prompts to build the new project plugin again, accept the build
     - if the build fails because this machine is missing the required Visual Studio 2022 MSVC v143 toolchain, mark the checkpoint `blocked`, not `fail`
- `What to click or run`:
  1. Open Unreal project `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`.
  2. If Unreal says the project or plugin needs to be rebuilt, allow it.
  3. If the rebuild fails with a missing MSVC / Visual Studio toolchain error, stop and report `blocked`.
  4. Open `/Game/ThirdPerson/Lvl_ThirdPerson`.
  5. Start PIE.
  6. Do not move the character yet. Let the template character idle so the arm response is easy to judge.
  7. Open the in-game console with `` ` `` or `~`.
  8. Run:

```text
PhysAnim.MVG102.Start
```

  9. Watch the first `3` to `5` seconds closely:
     - did anything move at all
     - did the left arm / left hand region move first
     - did the body stay roughly upright long enough to judge it
  10. Keep the character stationary for this checkpoint. Do not treat movement while manually driving the character as part of `MV-G1-02`.
  11. Let the test keep running for about `30` seconds unless it fails earlier. The harness auto-stops after that window.
  12. Record a short clip if possible. If a clip is not practical, capture at least one screenshot and write down exactly what happened in the first few seconds.
- `What good looks like`:
  - the current mannequin pawn visibly responds to repeated target updates
  - the left arm / left hand region clearly responds first
  - the mannequin stays upright and visually readable for about `30` seconds while stationary
- `What bad looks like`:
  - nothing responds at all
  - the wrong body region responds
  - Manny collapses or explodes immediately
  - joint behavior is erratic enough that you cannot tell what was being commanded
  - no stationary screenshot or clip can show a clean first-moving left-arm response
- `What does not fail this checkpoint by itself`:
  - artifacts that only appear once the user starts moving the character around
  - broader whole-body stability concerns that belong to the later smoke test or substep-stability check
- `When to mark blocked instead of fail`:
  - Unreal cannot build the plugin because the required MSVC toolchain is missing
  - the `PhysAnim.MVG102.Start` command does not exist in the console after the plugin should have loaded
  - the capture is too weak to tell what moved
- `What evidence to send back`:
  - exact map path used: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - exact runtime owner used: `UPhysAnimMvG102Subsystem`
  - exact trigger method used: `PhysAnim.MVG102.Start`
  - one short clip or screenshot
  - one sentence saying:
    - what body region was expected to move first: left arm / left hand
    - what actually moved first
    - whether the mannequin stayed controllable for about `30` seconds
    - final checkpoint verdict: `pass`, `fail`, or `blocked`

### MV-G1-03: End-To-End Bridge Smoke Test

- `What you are checking`: whether minimal SMPL/PHC-style output can drive Manny in Chaos without obvious mapping failure
- `Why it matters`: this is the integrated transfer check at the center of G1
- `What this checkpoint is and is not`:
  - this checkpoint is a deterministic mapped-joint smoke harness that stands in for minimal SMPL/PHC-style output
  - this checkpoint is not the final learned-policy bridge, not the final transform proof, and not the G2 quality comparison
- `Frozen test inputs`:
  1. exact UE map path:
     - `/Game/ThirdPerson/Lvl_ThirdPerson`
  2. exact runtime owner:
     - `UPhysAnimMvG103Subsystem`
     - source path: `F:\NewEngine\PhysAnimUE5\Plugins\PhysAnimPlugin\Source\PhysAnimPlugin\Public\PhysAnimMvG103Subsystem.h`
  3. exact trigger method:
     - start PIE in `Lvl_ThirdPerson`
     - open the in-game console with `` ` `` or `~`
     - run `PhysAnim.MVG103.Start`
     - optional stop command: `PhysAnim.MVG103.Stop`
  4. exact frozen validation pose:
     - `isolated left elbow flexion`
  5. exact mapped subset being exercised:
     - `L_Elbow -> lowerarm_l`
     - with `upperarm_l` as parent context and `hand_l` held near neutral
  6. exact evidence format required back:
     - short clip preferred
     - screenshot acceptable only if the pose is clearly readable
- `What to click or run`:
  1. Open `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`.
  2. If Unreal says the project or plugin needs to be rebuilt, allow it.
  3. If the rebuild fails because the required MSVC / Visual Studio toolchain is missing, stop and report `blocked`.
  4. Open `/Game/ThirdPerson/Lvl_ThirdPerson`.
  5. Start PIE.
  6. Keep the character stationary for this checkpoint.
  7. Open the in-game console with `` ` `` or `~`.
  8. Run:

```text
PhysAnim.MVG103.Start
```

  9. Watch the first `3` to `6` seconds closely:
     - does the left elbow / left forearm move into a recognizable elbow-flexed pose
     - does the right arm stay neutral enough to make mirroring mistakes obvious
     - does the pose remain readable rather than collapsing into instability
  10. Let the full `10` second window play once unless it fails earlier. The harness auto-stops.
  11. Record a short clip if practical. If not, capture at least one screenshot during the elbow-flexed hold.
- `What good looks like`:
  - the intended left arm region moves, especially the left elbow / forearm
  - the right arm does not mirror the motion unexpectedly
  - the resulting pose is recognizable as left elbow flexion
  - the character stays stable enough to judge mapping
- `What bad looks like`:
  - the wrong limb moves
  - left and right appear mirrored or swapped
  - torso or unrelated limbs dominate the motion instead of the left elbow chain
  - the character becomes unstable before the pose can be judged
- `When to mark blocked instead of fail`:
  - the plugin cannot be rebuilt because required MSVC tooling is missing
  - the `PhysAnim.MVG103.Start` command does not exist after the plugin should have loaded
  - the clip or screenshot is too unclear to tell which limb moved
- `What evidence to send back`:
  - exact map path used: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - exact runtime owner used: `UPhysAnimMvG103Subsystem`
  - exact trigger method used: `PhysAnim.MVG103.Start`
  - exact expected pose: `isolated left elbow flexion`
  - one clip or clearly readable screenshot
  - one sentence saying:
    - what body region was expected to move: left elbow / left forearm
    - what actually moved
    - whether the right arm stayed neutral enough to rule out obvious mirroring
    - final checkpoint verdict: `pass`, `fail`, or `blocked`

### MV-G1-04: UE Substep Stability Check

- `What you are checking`: whether the UE articulated-body setup stays controllable for about `30` seconds when Chaos synchronous substepping is enabled at a documented Stage 1 setting
- `Why it matters`: Stage 1 is not credible if the Physics Control path only works at render rate but becomes unstable once physics is stepped at the higher PD-friendly rates called for in [ENGINEERING_PLAN.md](/F:/NewEngine/ENGINEERING_PLAN.md)
- `What this checkpoint is and is not`:
  - this checkpoint is a UE physics-stability check, not the ONNX export step and not the final PHC bridge
  - this checkpoint uses the same known runtime harness as `MV-G1-02`, but the judgment is whole-body stability over time rather than “which limb moved first”
  - this checkpoint is only about the synchronous substepping path described in the engineering plan
- `Frozen test inputs`:
  1. exact UE map path:
     - `/Game/ThirdPerson/Lvl_ThirdPerson`
  2. exact runtime owner:
     - `UPhysAnimMvG102Subsystem`
     - source path: `F:\NewEngine\PhysAnimUE5\Plugins\PhysAnimPlugin\Source\PhysAnimPlugin\Public\PhysAnimMvG102Subsystem.h`
  3. exact trigger method:
     - start PIE in `Lvl_ThirdPerson`
     - open the in-game console with `` ` `` or `~`
     - run `PhysAnim.MVG102.Start`
     - optional stop command: `PhysAnim.MVG102.Stop`
  4. exact first substep configuration to test:
     - Project Settings -> Engine -> Physics
     - `Tick Physics Async = false`
     - `Substepping = true`
     - `Max Substep Delta Time = 0.008333`
     - `Max Substeps = 4`
     - this is the `120 Hz` synchronous substep path from the engineering plan
  5. exact second configuration to test only if the first one is clearly unstable:
     - keep `Tick Physics Async = false`
     - keep `Substepping = true`
     - set `Max Substep Delta Time = 0.004167`
     - set `Max Substeps = 8`
     - this is the `240 Hz` synchronous substep path from the engineering plan
  6. exact evidence format required back:
     - short clip preferred
     - screenshot acceptable only if the result is obviously stable and the written note explains the full `30` second behavior
- `What to click or run`:
  1. Open `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`.
  2. If Unreal says the project or plugin needs to be rebuilt, allow it.
  3. Open `/Game/ThirdPerson/Lvl_ThirdPerson`.
  4. Open Project Settings -> Engine -> Physics.
  5. Turn `Tick Physics Async` off.
  6. Turn `Substepping` on.
  7. Set:

```text
Max Substep Delta Time = 0.008333
Max Substeps = 4
```

  8. Save project settings if Unreal prompts.
  9. Start PIE.
  10. Keep the character stationary for this checkpoint.
  11. Open the in-game console with `` ` `` or `~`.
  12. Run:

```text
PhysAnim.MVG102.Start
```

  13. Let the run continue for about `30` seconds unless it fails earlier.
  14. Judge the whole run, not just the first seconds:
      - did the character remain upright and readable
      - did violent whole-body jitter dominate
      - did ground penetration or sudden launch dominate
      - could you still tell what the body was trying to do
  15. If the `120 Hz` configuration is obviously unstable, stop PIE, change to:

```text
Max Substep Delta Time = 0.004167
Max Substeps = 8
```

  16. Run the same PIE test once more with `PhysAnim.MVG102.Start`.
  17. Capture a short clip if practical. If not, write a short note immediately after the run while the result is still fresh.
- `What good looks like`:
  - at least one of the two documented synchronous-substep configurations remains controllable for about `30` seconds
  - the body may wobble or look imperfect, but violent jitter does not dominate the entire run
  - the character does not repeatedly tunnel into the ground or launch uncontrollably
  - the motion remains visually judgeable rather than turning into pure failure noise
- `What bad looks like`:
  - both documented synchronous-substep configurations become uncontrollable quickly
  - repeated whole-body jitter dominates most of the run
  - repeated ground penetration or launch behavior dominates the run
  - the character becomes unreadable as an articulated body rather than merely rough or under-tuned
- `When to mark blocked instead of fail`:
  - the plugin cannot be rebuilt because required MSVC tooling is missing
  - the `PhysAnim.MVG102.Start` command does not exist after the plugin should have loaded
  - the settings were not applied cleanly enough to know which configuration was actually tested
  - no clip or written note is clear enough to judge the `30` second behavior
- `What evidence to send back`:
  - exact map path used: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - exact runtime owner used: `UPhysAnimMvG102Subsystem`
  - exact trigger method used: `PhysAnim.MVG102.Start`
  - exact substep settings used for each run
  - one short clip or one clear written note
  - one sentence saying:
    - whether `120 Hz` passed, failed, or was not tested
    - whether `240 Hz` passed, failed, or was not tested
    - which configuration you consider the best documented Stage 1 setting on this machine
    - final checkpoint verdict: `pass`, `fail`, or `blocked`

### MV-P1-01: Phase 1 Runtime Stabilization Check

- `What you are checking`: whether the full Phase 1 runtime can start successfully in UE and remain visually controllable long enough to tune, instead of immediately flying, spinning, or exploding
- `Why it matters`: a startup-success line by itself is not enough for G2; if the character launches or spins uncontrollably right after startup, the physics-driven capture is not fair to compare yet
- `What this checkpoint is and is not`:
  - this checkpoint starts after the full bridge is already alive
  - this checkpoint is not about asset paths, ONNX export, or whether NNE exists
  - this checkpoint is the first Phase 1 post-startup stability gate before G2 packaging
  - this checkpoint now uses the bridge's built-in instability monitor as primary evidence for obvious launch / spin failures, not only subjective viewing
- `Frozen runtime state machine for this checkpoint`:
  - `Uninitialized`:
    - bridge does not own physics
  - `RuntimeReady`:
    - all startup prerequisites resolved
    - bridge still does not own physics
  - `WaitingForPoseSearch`:
    - bridge is waiting for the first valid `MotionMatch(...)` result
    - bridge still does not own physics
    - bridge must not have live runtime controls/body modifiers yet
  - `ReadyForActivation`:
    - startup succeeded and the first valid `MotionMatch(...)` result already exists
    - bridge still does not own physics
    - bridge must still have `liveControls=0` and `liveBodyModifiers=0`
    - zero-action safe mode now parks the runtime here until actions are explicitly enabled
  - `BridgeActive`:
    - bridge owns physics and may apply stabilization/tuning
    - bridge may now create live runtime controls/body modifiers
    - bridge may now suspend the normal `ACharacter` shell and apply bridge-owned tuning
  - `FailStopped`:
    - bridge released ownership after a fault
    - bridge must destroy live runtime controls/body modifiers on entry
  - frozen rule:
    - only `BridgeActive` is allowed to own bridge physics during `MV-P1-01`
- `Recommended stabilization order before declaring the runtime hopeless`:
  1. first prove the bridge can stay calm with zero actions:
     - the current default startup path already boots with `physanim.ForceZeroActions = 1`
     - operator-free safe mode now stops at `ReadyForActivation`
     - in that state the bridge does not create live runtime controls/body modifiers yet
     - in that state the bridge does not disable the capsule or `CharacterMovement`
  2. then re-enable actions conservatively:
     - `physanim.ForceZeroActions 0`
     - `physanim.ActionScale 0.10`
     - `physanim.ActionClampAbs 0.20`
     - `physanim.ActionSmoothingAlpha 0.25`
     - `physanim.StartupRampSeconds 1.0`
  3. if the runtime is still dominated by flight / spinning, lower control aggression next:
     - `physanim.AngularStrengthMultiplier 0.35`
     - `physanim.AngularDampingRatioMultiplier 1.50`
     - `physanim.AngularExtraDampingMultiplier 2.0`
  4. only after those steps fail should you investigate deeper mapping / frame faults
- `Frozen automated instability monitor for this checkpoint`:
  - the bridge now tracks the root body (`pelvis`) every tick
  - the bridge auto-fail-stops if any of these conditions persist longer than the grace window:
    - `root height delta > 120 cm`
    - `root linear speed > 1200 cm/s`
    - `root angular speed > 720 deg/s`
    - `grace window = 0.25 s`
  - the runtime log now emits periodic diagnostics for:
    - conditioned action magnitude
    - root height delta
    - root linear speed
    - root angular speed
    - accumulated unstable time
- `Frozen test inputs`:
  1. exact UE map path:
     - `/Game/ThirdPerson/Lvl_ThirdPerson`
  2. exact production character:
     - `/Game/Characters/Mannequins/Blueprints/BP_PhysAnimCharacter`
  3. exact expected startup-success line shape:
     - default safe-mode startup:
       - `[PhysAnim] Startup success. Runtime=... Model=/Game/NNEModels/phc_policy.phc_policy DeferredActivation=true`
     - direct action-enabled startup:
       - `[PhysAnim] Startup success. Runtime=... Model=/Game/NNEModels/phc_policy.phc_policy`
  4. exact physics settings:
     - use the current project defaults in `PhysAnimUE5/Config/DefaultEngine.ini`
     - `Tick Physics Async = false`
     - `Substepping = true`
     - `Max Substep Delta Time = 0.008333`
     - `Max Substeps = 4`
  5. exact evidence format required back:
     - short clip strongly preferred
     - startup success log line plus one written verdict
- `What to click or run`:
  1. Open `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`.
  2. Open `/Game/ThirdPerson/Lvl_ThirdPerson`.
  3. Confirm the placed/possessed pawn is `BP_PhysAnimCharacter`.
  4. Open the Output Log before PIE.
  5. Start PIE and do not move the character manually yet.
  6. Watch the Output Log for the startup-success line.
  7. If startup does not succeed, stop. This checkpoint is `blocked`, not `fail`.
  8. Confirm from the first `[PhysAnim] Runtime state: ...` lines that the runtime reaches `WaitingForPoseSearch` before it reaches `BridgeActive`.
     - with the default safe-mode path, confirm:
       - `WaitingForPoseSearch -> ReadyForActivation`
       - `liveControls=0`
       - `liveBodyModifiers=0`
       - `bridgeOwnsPhysics=false`
     - only after `physanim.ForceZeroActions 0` should the runtime move from `ReadyForActivation -> BridgeActive`
  9. Once startup succeeds, watch the first `10` seconds closely:
     - does the character stay roughly upright
     - does the body immediately launch upward
     - does the body enter continuous spinning or tumbling
     - is the motion still readable enough to tune
  10. If the first `10` seconds are readable, let the run continue for about `30` seconds total.
  11. Record a short clip if practical. If not, capture at least one screenshot and write down exactly what dominated the run.
- `Useful live knobs during this checkpoint`:
  - `physanim.ForceZeroActions`
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
- `What good looks like`:
  - the startup-success line appears
  - default safe-mode startup reaches `ReadyForActivation` without live operators
  - the character does not immediately launch, spin uncontrollably, or collapse into unreadable motion
  - the run remains visually controllable for about `30` seconds
  - the result is stable enough that tuning adjustments would be meaningful
- `What bad looks like`:
  - startup succeeds, but the character immediately flies away
  - startup succeeds, but the character spins or tumbles continuously
  - the first seconds are dominated by instability rather than recognizable motion
  - the result is too chaotic to use as a fair G2 candidate
  - the bridge triggers a fail-stop with `Runtime instability detected ...`
- `When to mark blocked instead of fail`:
  - the startup-success line never appears because the bridge is blocked earlier
  - the wrong pawn or map was used
  - the clip or written note is too weak to judge what dominated the run
- `What evidence to send back`:
  - exact map path used: `/Game/ThirdPerson/Lvl_ThirdPerson`
  - exact character used: `BP_PhysAnimCharacter`
  - the first `[PhysAnim]` startup success or failure line
  - the first `[PhysAnim] Runtime state: ...` transition lines
  - the first `[PhysAnim] Runtime diagnostics ...` line if one appears
  - the first `[PhysAnim] Fail-stop: Runtime instability detected ...` line if one appears
  - one short clip or screenshot
  - one sentence saying:
    - whether startup succeeded
    - whether the body stayed controllable for about `30` seconds
    - whether flying, spinning, tumbling, or another failure mode dominated
    - final checkpoint verdict: `pass`, `fail`, or `blocked`

### MV-G2-01: Physics-Driven Versus Kinematic Comparison

- `What you are checking`: whether the physics-driven version looks noticeably more natural than the kinematic PoseSearch version
- `Why it matters`: G2 is the core proof-of-quality gate for Stage 1
- `What to click or run`:
  1. confirm the orchestrator has frozen the exact G2 sequence and capture pair
  2. watch the kinematic clip once
  3. watch the physics-driven clip once
  4. watch them again side by side, or alternate between them in the same order
  5. judge the physics-driven version on:
     - weight
     - momentum
     - balance recovery
     - contact response
     - overall non-robotic feel
- `What good looks like`: the physics-driven version shows better weight, momentum, balance recovery, and contact response
- `What bad looks like`: the difference is negligible or the physics-driven version looks worse
- `What evidence to send back`: a short written verdict tied to the five rubric points above, plus screenshots or a split-screen clip

### MV-G3-01: Final Demo Observer Check

- `What you are checking`: whether outside viewers think the final demo looks compelling and non-robotic
- `Why it matters`: G3 decides whether Stage 2 is worth doing
- `What to click or run`:
  1. confirm the orchestrator has frozen the demo version to show
  2. show the clip or live demo
  3. before explaining the tech, ask:
     - `You are looking only at the motion quality. Does this look robotic, or does it feel weighty and physically believable enough to be interesting?`
  4. collect the observer's first reaction in their own words
  5. repeat for at least `3` observers when practical
- `What good looks like`: observers describe the motion as physical, weighty, and interesting enough to justify more work
- `What bad looks like`: observers call it robotic, unconvincing, or not meaningfully better than standard animation
- `What evidence to send back`: short observer notes, observer count, common positive/negative reactions, and a final go/no-go summary

## Evidence Rules

- Prefer short video clips over only screenshots when motion quality is the question.
- Keep each manual result tied to a named checkpoint ID.
- If the result is unclear, say `unclear` instead of forcing a pass/fail call.
