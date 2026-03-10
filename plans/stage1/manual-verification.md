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
- `Why it matters`: if the control path is broken, the Stage 1 bridge cannot work
- `What to click or run`:
  - first confirm the orchestrator has named the exact map, actor/blueprint, and trigger method
  - if those named assets are missing, stop and ask for them instead of improvising
  - once they are named:
    1. open the exact UE map
    2. confirm the named actor or blueprint is present
    3. start PIE
    4. trigger the control-path test exactly as instructed
    5. watch the first body region that is supposed to move
    6. let the test run for about `30` seconds unless it fails earlier
- `What good looks like`: Manny moves in response to target updates without exploding, freezing, or ignoring the controls
- `What bad looks like`: nothing happens, the ragdoll collapses immediately, or joints behave erratically
- `What evidence to send back`: screenshot or short clip plus one sentence naming the expected body region, the actual body region that moved, and whether Manny stayed controllable

### MV-G1-03: End-To-End Bridge Smoke Test

- `What you are checking`: whether minimal SMPL/PHC-style output can drive Manny in Chaos without obvious mapping failure
- `Why it matters`: this is the integrated transfer check at the center of G1
- `What to click or run`:
  - first confirm the orchestrator has named the exact map, smoke-test tool, and expected pose
  - if those named assets are missing, stop and ask for them instead of improvising
  - once they are named:
    1. open the exact UE map
    2. start PIE or run the named smoke-test tool
    3. trigger the named pose or pose sequence
    4. watch which limbs move first
    5. record a short clip
  - acceptable expected-pose names should come from `retargeting-spec.md`, for example:
    - neutral identity
    - isolated left elbow flexion
    - isolated right hip rotation
    - pelvis yaw
    - spine bend plus mild twist
    - explicit left/right asymmetry
- `What good looks like`: the intended limbs move, left/right are not mirrored incorrectly, and the pose stays plausible
- `What bad looks like`: wrong limbs move, the body mirrors incorrectly, or the character becomes unstable immediately
- `What evidence to send back`: one clip and a short note naming the expected pose, the observed result, and any mapping issues found

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
