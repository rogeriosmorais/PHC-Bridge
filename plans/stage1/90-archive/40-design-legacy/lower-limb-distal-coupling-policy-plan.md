# Lower-Limb Distal Coupling Policy Plan

## Situation

The knee/ankle-chain target-range pass materially improved lower-limb limit occupancy during movement, but the largest remaining movement spikes still concentrate in the distal chain:

- `ball_*` remains the most frequent peak angular offender.
- `foot_*` occasionally becomes the peak angular offender after the movement burst settles.
- `calf_*` occupancy is now commonly around `~0.7x - 1.2x`, so the broad knee/ankle over-range problem is no longer the dominant mismatch.

That means the current lower-limb problem is no longer basic occupancy or gross constraint authoring. It is the distal foot/toe representation under locomotion.

## Training-Side Reading

ProtoMotions confirms:

- `map_actions_to_pd_range = true`
- `use_biased_controller = false`
- SMPL knees, ankles, and toes in this asset are `3-DoF` joints
- `3-DoF` joints use the symmetric `1.2x` range expansion rule in `build_pd_action_offset_scale(...)`

The SMPL lower-body hierarchy also keeps ankle stiffness/damping aligned with knees, while toes are explicitly weaker:

- knee: `800 / 80`
- ankle: `800 / 80`
- toe: `500 / 50`

So the next runtime fit should keep knee and ankle in the same stronger family, while the toe remains more conservative.

## UE PhysicsControl Reading

UE PhysicsControl applies the explicit authored control target in the space of the skeletal target for each control. That means:

- every distal joint still receives its own independent explicit orientation target
- the current runtime can reduce target amplitude, but it does not automatically impose chain coherence between `calf_*`, `foot_*`, and `ball_*`

Given the current evidence, the next narrow pass should be target-range shaping for the distal chain, not more gain or authoring changes.

## Policy

Extend the existing training-aligned lower-limb target-range policy into the distal chain:

- keep `calf_*` at `0.50`
- reduce `foot_*` from `0.75` to `0.50`
- add `ball_*` at `0.35`

Why this shape:

- ankle should match the knee-family reduction more closely, because training stiffness/damping match
- toe should be weaker than both knee and ankle, because training stiffness/damping are lower
- this keeps the pass representation-focused and avoids reopening gains, limits, or authoring

## Success Criteria

The pass is good if deterministic movement smoke shows:

- no fail-stop
- lower peak `ball_*` angular spikes than the current knee/ankle-only baseline
- lower or unchanged peak `foot_*` angular spikes
- no meaningful regression in root stability or gross locomotion displacement

## Failure Interpretation

- If distal spikes drop and movement remains stable:
  - keep the distal-chain policy and move to the next alignment surface
- If occupancy stays good but distal spikes do not improve:
  - the next pass should inspect explicit distal target representation, not more scalar range tuning
- If movement destabilizes:
  - the distal-chain scales are too aggressive and should be backed off before any further interpretation

## First Pass Result

First measured pass:

- `calf_* = 0.50`
- `foot_* = 0.50`
- `ball_* = 0.35`

Outcome:

- no fail-stop in deterministic movement smoke
- lower-limb occupancy remained bounded, commonly around `~0.9x - 1.0x` during the first forward burst
- the first forward movement spike improved materially relative to the prior knee/ankle-only baseline
- but the pass did not solve distal instability across the whole movement script:
  - later backward/strafe phases still produce large `ball_*` angular spikes
  - some movement bursts shift the peak angular offender up the chain (`thigh_*`, `foot_*`, `hand_*`) rather than eliminating the distal problem

Interpretation:

- distal-chain range shaping helps, but only partially
- the remaining mismatch is now more likely explicit distal target representation under dynamic locomotion, not simple scalar range magnitude alone
