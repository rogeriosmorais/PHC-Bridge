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
- `What to click or run`: run the planned ProtoMotions baseline command and watch the resulting motion clip or visualization
- `What good looks like`: the character stays balanced, weight shifts look natural, and the movement does not jitter or explode
- `What bad looks like`: the character falls for no reason, jitters badly, or looks like a rigid puppet
- `What evidence to send back`: one short video or GIF and one sentence saying `pass`, `fail`, or `unclear`

### MV-G1-02: Manny Responds To Programmatic Control In UE5

- `What you are checking`: whether Manny's physics-driven body actually responds when control targets are updated
- `Why it matters`: if the control path is broken, the Stage 1 bridge cannot work
- `What to click or run`: open the UE5 project, start the relevant test or prototype scene, and trigger the control-path test
- `What good looks like`: Manny moves in response to target updates without exploding, freezing, or ignoring the controls
- `What bad looks like`: nothing happens, the ragdoll collapses immediately, or joints behave erratically
- `What evidence to send back`: screenshot or short clip plus one sentence describing the behavior

### MV-G1-03: End-To-End Bridge Smoke Test

- `What you are checking`: whether minimal SMPL/PHC-style output can drive Manny in Chaos without obvious mapping failure
- `Why it matters`: this is the integrated transfer check at the center of G1
- `What to click or run`: run the prototype bridge scene or tool that feeds simple mapped outputs into Manny
- `What good looks like`: the intended limbs move, left/right are not mirrored incorrectly, and the pose stays plausible
- `What bad looks like`: wrong limbs move, the body mirrors incorrectly, or the character becomes unstable immediately
- `What evidence to send back`: one clip and a short note naming the expected pose and what actually happened

### MV-G2-01: Physics-Driven Versus Kinematic Comparison

- `What you are checking`: whether the physics-driven version looks noticeably more natural than the kinematic PoseSearch version
- `Why it matters`: G2 is the core proof-of-quality gate for Stage 1
- `What to click or run`: play the side-by-side comparison build or capture, watching the same motion sequence in both modes
- `What good looks like`: the physics-driven version shows better weight, momentum, balance recovery, and contact response
- `What bad looks like`: the difference is negligible or the physics-driven version looks worse
- `What evidence to send back`: a short written verdict, plus screenshots or a split-screen clip

### MV-G3-01: Final Demo Observer Check

- `What you are checking`: whether outside viewers think the final demo looks compelling and non-robotic
- `Why it matters`: G3 decides whether Stage 2 is worth doing
- `What to click or run`: show the Stage 1 demo to observers and ask the prepared prompt from the G3 evaluation package
- `What good looks like`: observers describe the motion as physical, weighty, and interesting enough to justify more work
- `What bad looks like`: observers call it robotic, unconvincing, or not meaningfully better than standard animation
- `What evidence to send back`: short observer notes and a final go/no-go summary

## Evidence Rules

- Prefer short video clips over only screenshots when motion quality is the question.
- Keep each manual result tied to a named checkpoint ID.
- If the result is unclear, say `unclear` instead of forcing a pass/fail call.
