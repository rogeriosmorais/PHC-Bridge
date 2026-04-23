# Lower-Limb Thigh De-Intensification Plan

## Situation

The first selective proximal-response experiment made calves stronger and thighs milder:

- `thigh_*`
  - damping ratio `1.10`
  - extra damping `1.15`
- `calf_*`
  - damping ratio `1.25`
  - extra damping `1.45`

That was not a clean win. The fresh movement smoke re-opened larger forward distal spikes, which means the stronger calf side of the split was too aggressive.

## Sources Re-Used

- official UE PhysicsControl docs
- local UE 5.7 PhysicsControl source
- ProtoMotions docs and local control code
- the current movement smoke evidence from [PhysAnimUE5_2.log](/F:/NewEngine/PhysAnimUE5/Saved/Logs/PhysAnimUE5_2.log)

Those sources still support the same seam:

- runtime control multipliers are the intended fit surface in UE
- ProtoMotions leg gains are a family contract, but they do not require this stronger calf-biased UE response
- the last keepable baseline was the shared proximal response profile, not the more aggressive calf-priority split

## Hypothesis

The useful part of the selective idea is not “stronger calves.” It is “less aggressive thighs.”

If that is true, then:

- restore calves to the prior keepable proximal-response baseline
- reduce only `thigh_*` response relative to that baseline
- retain the strafe/late-idle gains from the proximal-response work
- avoid reopening the forward distal spikes caused by the calf-priority pass

## Experiment

Keep unchanged:

- distal locomotion-time composition baseline
- distal target angular-velocity suppression baseline
- all current target-range shaping

Change only the proximal locomotion response profile:

- `thigh_*`
  - damping ratio scale `1.05`
  - extra damping scale `1.10`
- `calf_*`
  - damping ratio scale `1.20`
  - extra damping scale `1.35`

## Success Criteria

- `PhysAnim.PIE.MovementSmoke` stays green with no fail-stop
- forward does not reopen the larger distal spikes from the failed calf-priority split
- strafe stays at least as good as the shared-profile baseline
- late idle remains materially better than the old distal-only baseline

## Result

- implemented the selective profile:
  - `thigh_*`
    - damping ratio `1.05`
    - extra damping `1.10`
  - `calf_*`
    - damping ratio `1.20`
    - extra damping `1.35`
- verification:
  - UE build passed
  - `PhysAnim.Component` passed
  - `PhysAnim.PIE.MovementSmoke` passed
  - no fail-stop
- measured runtime result:
  - forward stayed stable, but did not materially beat the shared proximal-response baseline
  - some strafe samples improved
  - backward and late-idle re-opened larger outliers than the shared baseline
- current read:
  - thigh de-intensification is not a clean new baseline
  - runtime code should stay on the shared proximal-response profile (`thigh_*` + `calf_*` both `1.20 / 1.35`)
