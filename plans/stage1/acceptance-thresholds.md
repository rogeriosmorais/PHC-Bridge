# Stage 1 Acceptance Thresholds

## Purpose

This document turns the softest Stage 1 checks into explicit pass / fail / blocked thresholds.

Use it when:

- deciding whether a planning artifact is truly locked
- deciding whether the pretrained shortcut is good enough to keep
- deciding whether G1 evidence is sufficient to continue
- keeping manual judgments anchored to the same standard over time

## Status Words

- `pass`: evidence meets the threshold strongly enough to proceed
- `fail`: evidence says the claim is false enough to stop or invoke fallback
- `blocked`: the threshold cannot be judged fairly because required evidence is missing or invalid

## Contract-Lock Threshold

The PHC bridge contract counts as `locked` only if all of these are written down in [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md):

- exact simulator path used for the chosen policy
- exact robot/body type
- exact config or experiment name
- exact checkpoint source or training starting point
- exact observation tensor shape
- exact action tensor shape
- exact field ordering or grouping
- exact coordinate-frame / reference-pose policy
- exact gain-output policy or explicit note that gains are fixed outside the model

If any of these are still unknown, the contract is not locked.

## Pretrained-Viable Threshold

The pretrained-first path counts as `pass` for Phase 0 only if:

- the selected pretrained checkpoint loads and runs
- at least `3` representative locomotion motions are evaluated
- at least `2` of those motions are judged broadly believable rather than obviously broken
- no catastrophic failure dominates the result:
  - persistent falling
  - severe foot sliding from the first seconds
  - major limb inversion / mirroring
  - repeated numerical instability

Choose `fail` if the pretrained result is clearly not useful even for broad locomotion.

Choose `blocked` if:

- the checkpoint cannot be retrieved or loaded yet
- the simulator path is not ready
- the captured evidence is too weak to judge

## Motion-Source Threshold

The motion-source map counts as `usable` only if:

- every locked locomotion-core motion has either:
  - a named source family, or
  - an explicit approved replacement
- every missing or risky motion is written down explicitly

If a motion is merely assumed to exist with no source note, the map is not usable.

## UE Control-Path Threshold

`UPhysicsControlComponent` counts as `viable for Stage 1` only if:

- Manny visibly responds to repeated programmatic target changes
- the commanded limb or torso region is the region that actually moves
- the test can run for roughly `30 seconds` without unrecoverable explosion or total loss of control
- any instability observed is limited enough that Stage 1 tuning still looks plausible

Choose `fail` if targets are ignored, obviously mapped to the wrong body region, or explode so consistently that the bridge thesis is no longer credible.

## Substep-Stability Threshold

Substep stability counts as `pass` only if:

- one documented Stage 1 configuration remains controllable for roughly `30 seconds`
- no persistent violent jitter dominates the whole body
- no repeated ground-penetration or launch behavior dominates the test
- the user can still visually judge the motion rather than only the failure mode

Choose `blocked` if the test setup itself is not comparable or not captured clearly enough.

## Manny Smoke-Test Threshold

The minimal SMPL/PHC smoke test counts as `pass` only if:

- the intended body pose is recognizable on Manny
- left/right are not mirrored unexpectedly
- upper and lower body regions map to the expected limbs
- unmapped bones do not dominate the visual result
- the character remains stable enough to judge the mapping

Choose `fail` if the smoke test shows obvious handedness, axis, or bone-selection errors.

## G1 Threshold

G1 counts as `pass` only if all of these are true:

- pretrained viability is `pass`
- bridge contract is `locked`
- UE control-path viability is `pass`
- Manny smoke test is `pass`
- substep stability is `pass`
- no `red` assumption remains on a G1-critical item

G1 counts as `fail` if any of the above are `fail`.

G1 counts as `blocked` if none fail outright but at least one is still `blocked`.

## G2 Threshold

G2 counts as `pass` only if:

- the kinematic and physics-driven captures are directly comparable
- the user judges the physics-driven version noticeably better on at least `3` of these `5` points:
  - weight
  - momentum
  - balance recovery
  - contact response
  - overall non-robotic feel
- no single severe artifact overwhelms the claimed improvement

## G3 Threshold

G3 counts as `pass` only if:

- at least `3` observer reactions are collected when practical
- the user summary concludes the demo is meaningfully non-robotic and worth continuing
- the common negative reactions do not reduce the result to "ordinary animation with noise"

If observer count is lower because of practical limits, note that clearly instead of pretending the sample is strong.
