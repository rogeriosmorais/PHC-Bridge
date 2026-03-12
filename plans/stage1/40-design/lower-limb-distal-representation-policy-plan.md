# Lower-Limb Distal Representation Policy Plan

## Situation

The current alignment state is:

- broad lower-limb occupancy under locomotion is materially improved
- the bridge remains stable in deterministic movement smoke
- the largest remaining lower-limb spikes still concentrate in the distal chain, especially `ball_*`

The previous distal-chain target-range pass helped the first forward burst, but it did not solve backward and strafe bursts. That means simple scalar target-range reduction is not sufficient by itself.

## Engine Reading

Official UE PhysicsControl docs and local UE 5.7 source agree on two important facts:

1. `FPhysicsControlTarget::TargetOrientation` is the target orientation of the child body relative to the parent body.
2. PhysicsControl applies the explicit target on top of the skeletal target transform for the control.

That means each distal control still receives its own explicit independent orientation target in parent space. The system does not automatically impose chain coherence between:

- `calf_* -> foot_*`
- `foot_* -> ball_*`

## ProtoMotions Reading

ProtoMotions confirms:

- `map_actions_to_pd_range = true`
- `use_biased_controller = false`
- knees, ankles, and toes in this SMPL asset are `3-DoF`
- the training-side lower-limb action mapping uses the symmetric `1.2x` expansion rule

It also confirms a lower-body hierarchy in stiffness/damping:

- knee: `800 / 80`
- ankle: `800 / 80`
- toe: `500 / 50`

That hierarchy suggests distal joints should remain more conservative than the upstream chain during dynamic locomotion.

## Hypothesis

The remaining mismatch is now explicit distal target representation under locomotion.

In practice:

- during actual locomotion, `foot_*` and especially `ball_*` are still being driven too independently from the moving parent chain
- under idle this is acceptable
- under forward/backward/strafe motion the same explicit distal targets become too aggressive

## Policy

Add a movement-only distal target representation policy:

- keep the current lower-limb target-range policy
- when planar owner speed exceeds a locomotion threshold:
  - attenuate `foot_*` policy influence toward the pre-policy baseline
  - attenuate `ball_*` policy influence even more
- leave the rest of the body unchanged

First-pass values:

- activation threshold: `50 cm/s`
- `foot_*` locomotion representation scale: `0.75`
- `ball_*` locomotion representation scale: `0.50`

This is intentionally narrower than another global lower-limb policy change. It only affects the distal chain, and only while locomotion is actually active.

## Success Criteria

The pass is good if deterministic movement smoke shows:

- no fail-stop
- no regression in total displacement or gross movement completion
- lower backward/strafe `ball_*` angular spikes than the current distal-range baseline
- lower or unchanged `foot_*` spikes in the same phases

## Failure Interpretation

- If backward/strafe distal spikes improve:
  - keep the policy and move on
- If the spikes persist with little change:
  - the next pass should inspect more structural distal target construction, not just dynamic attenuation
- If movement response becomes too dead or under-responsive:
  - the locomotion-only attenuation is too aggressive and should be reduced

## First Pass Result

First measured pass:

- locomotion activation threshold: `50 cm/s`
- `foot_*` locomotion representation scale: `0.75`
- `ball_*` locomotion representation scale: `0.50`

Outcome:

- no fail-stop in deterministic movement smoke
- early forward movement spikes improve somewhat relative to the current committed baseline
- but the pass is not a clean win:
  - backward bursts still produce very large `ball_*` angular spikes
  - strafe bursts still produce large distal spikes
  - the pass does not establish a new clearly better movement baseline

Interpretation:

- movement-only distal attenuation is not enough by itself
- the next pass should inspect more structural distal target construction under locomotion, not just another locomotion-time scalar attenuation
