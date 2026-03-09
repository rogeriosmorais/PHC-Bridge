# Stage 1 Assumption Ledger

## Purpose

This is the orchestrator-owned control document for Stage 1. It tracks the assumptions that can kill the project, the signals that would prove those assumptions wrong, and the fallback or stop decision tied to each one.

This is not a passive risk list. The orchestrator uses it to decide whether downstream work is still safe.

## Status Scale

- `green`: assumption currently acceptable for downstream work
- `yellow`: assumption still plausible, but under active watch
- `red`: assumption is likely false or insufficiently supported; downstream work depending on it should stop
- `resolved`: assumption is no longer an open uncertainty because it was confirmed, rejected, or made irrelevant

## How To Use This Ledger

For each major Stage 1 assumption, track:

- what must be true
- why it matters
- how we will test it
- what evidence would falsify it
- what fallback exists
- whether the project should stop if it fails

Only the orchestrator updates status.

## Critical Assumptions

| ID | Assumption | Why It Matters | Current Status | How It Will Be Tested | Falsification Signal | Fallback | Stop If False |
|---|---|---|---|---|---|---|---|
| A-01 | Pretrained policy motion in training looks convincingly alive enough to justify Stage 1 | Stage 1 is pointless if the starting policy already looks robotic | yellow | G1 training visualization and manual check `MV-G1-01` | motion looks stiff, unstable, or puppet-like in training | narrow to locomotion-only test or stop before deeper integration | yes |
| A-02 | The selected ProtoMotions/PHC config can be mapped cleanly into a UE5 bridge contract | the plugin cannot be built safely without a stable observation/action contract | yellow | `bridge-spec.md` confirmed in Phase 0 | tensor fields, ordering, or representation stay unclear | narrow the chosen config or pause until a workable config is selected | yes |
| A-03 | SMPL <-> UE5 retargeting can be made stable enough for Manny | wrong transforms or mirroring will invalidate the entire bridge | yellow | `retargeting-spec.md` plus G1 Manny smoke test | wrong limbs move, mirroring appears, or instability comes from mapping errors | revise mapping and transform layer | yes |
| A-04 | `UPhysicsControlComponent` can express the policy intent well enough for Stage 1 | the entire low-custom-code thesis depends on it | yellow | control-path prototype and Manny response checks | targets are ignored, unstable, or too limited to drive plausible motion | explicit fallback to raw torque control if the project chooses it | yes |
| A-05 | Chaos substepping at 120-240 Hz is stable enough for the articulated body | unstable simulation breaks any quality comparison | yellow | Phase 0 substep stability check | persistent instability despite tuning and solver adjustments | try async physics, then reassess Stage 1 viability | yes |
| A-06 | NNE + ONNX Runtime can run the policy fast enough and compatibly in UE5 | inference must fit inside Stage 1 without adding a new runtime stack | yellow | dummy-model check, then real ONNX load | model fails to load or runtime cost is unacceptable | shrink/re-export model, reassess viability | yes |
| A-07 | PoseSearch output is sufficient as the upstream motion-intent source for Stage 1 | the bridge assumes PoseSearch can supply the needed target motion intent | yellow | bridge-spec confirmation and one-character integration | required target features are unavailable or unusable | narrow conditioning assumptions rather than inventing a new architecture | maybe |
| A-07b | The available pretrained policy covers enough broad motion to make pretrained-first worthwhile | if not, we should switch to fine-tuning or training earlier | yellow | pretrained evaluation on the locked motion set | pretrained result is obviously not useful even for locomotion / general motion | go straight to fine-tuning on the locked motion set | no |
| A-07c | The locked Stage 1 locomotion and combat motion set is sourceable without major scope creep | undefined or missing motions create silent planning drift | yellow | motion-source review against `motion-set.md` | key motions are missing or only available through major extra pipeline work | replace or remove missing motions explicitly | maybe |
| A-08 | Physics-driven motion will look noticeably better than kinematic playback | this is the thesis tested by G2 | yellow | G2 side-by-side evaluation | user judges difference as negligible or worse | ship only as experiment documentation, do not continue to Stage 2 | yes |
| A-09 | The final two-character demo will be compelling enough to justify Stage 2 | Stage 2 is optional and should not start on weak evidence | yellow | G3 observer evaluation | observers find the result unconvincing | stop at Stage 1 | no |

## Reassessment Triggers

The orchestrator must revisit ledger status when:

- a planning spec is locked
- a gate package is prepared
- user evidence arrives
- a worker reports a blocker
- a fallback path becomes more likely than the primary path

## Downstream Safety Rules

- `green`: downstream tasks may proceed normally
- `yellow`: downstream tasks may proceed only if they do not hard-code the uncertain assumption in irreversible ways
- `red`: downstream tasks that depend on the assumption must stop

## Immediate Orchestrator Duties

Before starting the next execution-planning pass, the orchestrator should:

1. review `bridge-spec.md`, `retargeting-spec.md`, and `test-strategy.md`
2. update each assumption above from `yellow` only if evidence justifies it
3. note which assumptions block the Phase 0 execution package

## First Expected Updates

The next meaningful ledger updates should come from:

- confirming the PHC observation/action contract
- choosing the retargeting validation cases for G1
- defining the exact evidence required to call `UPhysicsControlComponent` viable for Stage 1
