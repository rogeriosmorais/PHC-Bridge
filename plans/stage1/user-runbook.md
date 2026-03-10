# Stage 1 User Runbook

## Purpose

This is the detailed user-facing runbook for every Stage 1 step owned by the human.

Use it to answer:

- what you need to do
- whether that step is runnable yet
- what exact evidence to send back
- when you should stop instead of inventing missing setup

If this file disagrees with a gate package, the gate package still owns the pass/fail rules. This file exists to make the user actions concrete.

## Current Status Map

| Step ID | Status Today | When To Use It |
|---|---|---|
| `S1-U-01` | complete locally, keep as verification fallback | only if tool paths change or setup must be rechecked |
| `S1-U-02` | complete locally, keep as verification fallback | only if repo/data paths change or a missing asset must be rechecked |
| `S1-U-03` | complete locally, keep as recreate guide | only if the UE project must be recreated or repaired |
| `S1-U-04 / MV-G1-01` | runnable now | run now for the training-side visual verdict |
| `S1-U-04 / MV-G1-02` | complete locally | frozen stationary control-path proof captured |
| `S1-U-04 / substep stability` | blocked on exact UE test asset | do not invent the scene or physics settings |
| `S1-U-04 / MV-G1-03` | runnable now | use the frozen stationary smoke harness exactly |
| `S1-U-05 / G2` | future-only | wait for Phase 1 handoff |
| `S1-U-06 / G3` | future-only | wait for Phase 2 handoff |

## Rule Zero

If a step below says an exact map, blueprint, command, or sequence must be named by the orchestrator, do not guess. Stop and ask for that missing named asset instead.

That is not user hesitation. That is the correct Stage 1 operating rule.

## S1-U-01: Verify Toolchain And Paths

Use this only if setup changed, needs to be rechecked, or must be reproduced on another machine.

### What To Verify

- Unreal Engine `5.7.3`
- `UE5_PATH` at `E:\UE_5.7\Engine`
- Python env at `F:\NewEngine\Training\.venv\physanim_proto311`
- ProtoMotions root at `F:\NewEngine\Training\ProtoMotions`
- Isaac Sim launcher at `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaacsim.exe`
- Isaac Lab launcher at `F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaaclab.exe`

### What To Run

Open PowerShell and run:

```powershell
Test-Path 'E:\UE_5.7\Engine'
Test-Path 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe'
Test-Path 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaacsim.exe'
Test-Path 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\isaaclab.exe'
Test-Path 'F:\NewEngine\Training\ProtoMotions'
& 'F:\NewEngine\Training\.venv\physanim_proto311\Scripts\python.exe' --version
```

### What Good Looks Like

- every `Test-Path` returns `True`
- Python reports `3.11.x`

### What To Send Back

- the command output text
- any path that returned `False`
- any path that differs from the locked plan

## S1-U-02: Verify External Repo / Data Availability

Use this only if the repo/data layout changed, or if we need to prove the current machine still matches the plan.

### What To Verify

- ProtoMotions checkout exists
- selected pretrained checkpoint exists
- frozen first motion file exists
- if AMASS or Mixamo are not present yet, say `not needed yet` instead of pretending they are available

### What To Run

```powershell
Test-Path 'F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt'
Test-Path 'F:\NewEngine\Training\ProtoMotions\data\motions\smpl_humanoid_walk.npy'
```

### What To Send Back

- whether each path exists
- any corrected path if the file moved
- any blocker such as a missing checkpoint or missing motion file

## S1-U-03: Create Or Recreate The UE Project Scaffold

Use [eli5-ue-project-setup.md](/F:/NewEngine/plans/stage1/eli5-ue-project-setup.md) for the click-by-click path.

Use this section only if the project must be recreated or if the current scaffold becomes invalid.

### Minimum Success Criteria

- project exists at `F:\NewEngine\PhysAnimUE5`
- `PhysAnimUE5.uproject` opens
- PIE runs once
- Manny assets are visible
- `PoseSearch`, `PhysicsControl`, and `NNERuntimeORT` can be enabled

### What To Send Back

- final project path
- one screenshot with the editor open
- one note saying whether Manny was found
- one note saying whether the required plugins enabled cleanly
- any blocking error text

## S1-U-04: G1 Manual Evidence

This is the current active user-owned work.

### MV-G1-01: Training-Side Visual Verdict

Run [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md) exactly.

Short version:

1. Open PowerShell in `F:\NewEngine\Training\ProtoMotions`.
2. Run the `phase0_eval_visual` command from [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md#L21).
3. Press `L` to start recording once the viewer is moving.
4. Let it run `10` to `15` seconds.
5. Press `L` again to save the mp4.
6. Press `Q` to quit after the save completes.
7. Send the clip plus one line: `pass`, `fail`, or `unclear`.

### MV-G1-02: Manny Responds To Programmatic Control

This step is now frozen to one exact path:

- exact UE map path:
  - `/Game/ThirdPerson/Lvl_ThirdPerson`
- exact runtime owner:
  - `UPhysAnimMvG102Subsystem`
- exact trigger method:
  - start PIE
  - open the in-game console with `` ` `` or `~`
  - run `PhysAnim.MVG102.Start`
- exact body region expected to move first:
  - left arm, especially the left hand / forearm chain
- exact evidence format expected back:
  - short clip preferred
  - screenshot acceptable if needed

The current Third Person scaffold is using `SKM_Quinn_Simple` on `CharacterMesh0`. That is acceptable for this checkpoint because Manny and Quinn share the same mannequin skeleton family.
The `PhysAnimPlugin` runtime harness also built successfully on this machine on March 10, 2026.

This check is narrower than `MV-G1-03`.

- `MV-G1-02` asks: can `UPhysicsControlComponent` move the intended body region at all?
- `MV-G1-03` asks: can the full SMPL/PHC-style bridge drive Manny without obvious mapping failure?
- `MV-G1-02` is a stationary proof only. Do not broaden it into a moving-character stability test.

Use this procedure:

1. Open `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`.
2. If Unreal prompts to rebuild the project plugin, allow it.
3. If the rebuild fails because Visual Studio 2022 / MSVC v143 is missing, stop and report `blocked`.
4. Open `/Game/ThirdPerson/Lvl_ThirdPerson`.
5. Start PIE.
6. Do not move the character yet.
7. Open the in-game console with `` ` `` or `~`.
8. Run `PhysAnim.MVG102.Start`.
9. Watch the first `3` to `5` seconds closely.
10. Check whether the left arm / left hand region moves first.
11. Keep the character stationary for this checkpoint.
12. Let the test continue for about `30` seconds unless it fails earlier. The harness auto-stops after that window.
13. Record a short clip if practical. If not, capture at least one screenshot.
14. Write one sentence saying:
   - what region was supposed to move: left arm / left hand
   - what actually moved
   - whether the stationary mannequin stayed readable and upright
   - final verdict: `pass`, `fail`, or `blocked`

Choose `fail` immediately if:

- nothing responds
- the wrong body region responds
- the ragdoll explodes or collapses immediately

Do not fail `MV-G1-02` only because the arm behaves poorly once you start moving the whole character. That broader behavior belongs to later UE checks, not this stationary proof.

Choose `blocked` instead of `fail` if:

- the plugin cannot be built because the required MSVC toolchain is missing
- the `PhysAnim.MVG102.Start` console command does not exist in PIE
- the capture is too unclear to tell what moved

### UE Substep Stability Check

This step is usually the same UE prototype run as `MV-G1-02`, but the judgment is different.

Do not run it until the orchestrator names:

- exact map or test scene
- exact substep-related settings to use
- exact runtime path to capture

When those fields are provided:

1. Open the named map.
2. Apply or confirm the named physics settings.
3. Start PIE.
4. Run the named prototype for about `30` seconds.
5. Capture a clip if the motion is visible enough to judge.
6. Write one short note covering:
   - the settings used
   - whether Manny stayed controllable
   - whether violent jitter, launch behavior, or ground penetration dominated the run

### MV-G1-03: Manny Smoke Test

This step is now frozen to one exact path:

- exact UE map path:
  - `/Game/ThirdPerson/Lvl_ThirdPerson`
- exact runtime owner:
  - `UPhysAnimMvG103Subsystem`
- exact trigger method:
  - start PIE
  - open the in-game console with `` ` `` or `~`
  - run `PhysAnim.MVG103.Start`
- exact expected pose:
  - `isolated left elbow flexion`
- exact mapped subset being exercised:
  - `L_Elbow -> lowerarm_l`
  - with `upperarm_l` as the parent-space context and `hand_l` held near neutral
- exact evidence format expected back:
  - short clip preferred
  - screenshot acceptable if the pose is clearly readable

This is a narrower smoke harness than the future full PHC runtime.

- `MV-G1-03` asks: does a minimal mapped-joint pose packet drive the expected Manny limb without obvious mirroring or wrong-limb errors?
- it does not ask whether the full learned policy bridge is already complete

Use this procedure:

1. Open `F:\NewEngine\PhysAnimUE5\PhysAnimUE5.uproject`.
2. If Unreal prompts to rebuild the project plugin, allow it.
3. If the rebuild fails because Visual Studio 2022 / MSVC v143 is missing, stop and report `blocked`.
4. Open `/Game/ThirdPerson/Lvl_ThirdPerson`.
5. Start PIE.
6. Keep the character stationary.
7. Open the in-game console with `` ` `` or `~`.
8. Run `PhysAnim.MVG103.Start`.
9. Watch the first `3` to `6` seconds closely.
10. Check whether the left elbow / left forearm moves into a recognizable left-elbow-flexed pose.
11. Check whether the right arm stays neutral enough to make mirroring mistakes obvious.
12. Let the full `10` second window finish unless it fails earlier. The harness auto-stops.
13. Record a short clip if practical. If not, capture at least one clear screenshot during the elbow-flexed hold.
14. Write one sentence saying:
   - what region was supposed to move: left elbow / left forearm
   - what actually moved
   - whether the right arm stayed neutral enough to rule out obvious mirroring
   - final verdict: `pass`, `fail`, or `blocked`

Choose `fail` immediately if:

- left and right are swapped or mirrored
- the wrong limb moves
- torso or unrelated limbs dominate the pose instead of the left elbow chain
- Manny becomes unstable before the pose can be judged

## S1-U-05: G2 Side-By-Side Judgment

Do not run this until Phase 1 is complete and the orchestrator explicitly says G2 evidence is ready.

When ready, use:

- [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md#L64)
- [g2-evaluation.md](/F:/NewEngine/plans/stage1/g2-evaluation.md)
- [comparison-sequence-lock.md](/F:/NewEngine/plans/stage1/comparison-sequence-lock.md)

### Required Inputs Before You Start

- one locked comparison sequence
- one kinematic capture
- one physics-driven capture
- confirmation that camera, speed, and environment match

### What To Do

1. Watch the kinematic version once without judging.
2. Watch the physics-driven version once without judging.
3. Watch them again side by side, or alternate between them in the same order.
4. Score whether the physics-driven version is clearly better on:
   - weight
   - momentum
   - balance recovery
   - contact response
   - overall non-robotic feel
5. Write down which of those points actually drove your judgment.
6. Return one final verdict: `pass`, `fail`, or `blocked`.

### Evidence To Send Back

- side-by-side clip or two comparable clips
- final verdict
- short reasons tied to the five rubric points above

## S1-U-06: G3 Observer Evaluation

Do not run this until Phase 2 is complete and the orchestrator explicitly says G3 evidence is ready.

When ready, use:

- [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md#L75)
- [g3-evaluation.md](/F:/NewEngine/plans/stage1/g3-evaluation.md)

### Observer Procedure

1. Show the demo clip or live demo.
2. Before explaining the tech, ask this first-impression prompt:

> You are looking only at the motion quality. Does this look robotic, or does it feel weighty and physically believable enough to be interesting?

3. Let the observer answer in their own words.
4. Do not argue with the first reaction.
5. Repeat for at least `3` observers when practical.
6. If you only get `1` or `2` observers, say so explicitly instead of pretending the sample is stronger.
7. Summarize the common positive and negative reactions.
8. Return one final user verdict: `pass`, `fail`, or `blocked`.

### Evidence To Send Back

- demo version shown
- observer count
- short observer notes
- common positive reactions
- common negative reactions
- your final continue / stop judgment

## Copyable Evidence Return

Use this when sending results back:

```md
# Stage 1 User Evidence Return

## Step Or Checkpoint
- ID:

## What I Ran
- map / command / clip:

## Evidence
- clip or screenshot path:
- settings used:

## Verdict
- pass / fail / blocked / unclear:

## Notes
- expected result:
- observed result:
- blockers or weird behavior:
```
