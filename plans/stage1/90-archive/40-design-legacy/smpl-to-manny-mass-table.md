# SMPL to Manny Mass Table

## Purpose

This file records the first explicit mass-distribution audit for the Stage 1 bridge.

It compares:

- the ProtoMotions SMPL training asset's implied body masses from [smpl_humanoid.xml](/F:/NewEngine/Training/ProtoMotions/protomotions/data/assets/mjcf/smpl_humanoid.xml)
- the current Manny physics-asset body masses measured through `PhysAnim.Component.MannyMassInventory`

This is an inventory and comparison sheet, not a final retuning table.

## Important Interpretation Rule

The ProtoMotions-side numbers are derived from MJCF geom density and volume. The Manny-side numbers come from UE body-setup mass calculation using the current physics asset and default physical material path.

So this table is good for:

- relative segment mass comparison
- center-of-mass ordering comparison
- identifying obviously overweight or underweight body families

It is not a claim that the two systems already share identical inertia tensors or exact per-shape materials.

## Data Sources

- ProtoMotions training asset:
  - [smpl_humanoid.xml](/F:/NewEngine/Training/ProtoMotions/protomotions/data/assets/mjcf/smpl_humanoid.xml)
- Manny runtime-side mass audit:
  - [PhysAnimUE5.log](/F:/NewEngine/PhysAnimUE5/Saved/Logs/PhysAnimUE5.log#L1210)

## Total Mass Snapshot

- ProtoMotions SMPL implied total: about `63.314`
- Manny current implied total: about `75.820`

Total kg mismatch is not the most important issue. The more important mismatch is where that mass is distributed.

## Family-Level Comparison

| Segment family | ProtoMotions SMPL mass | ProtoMotions % | Manny mass | Manny % | Read |
|---|---:|---:|---:|---:|---|
| pelvis | `5.125` | `8.09%` | `7.524` | `9.92%` | Manny pelvis is modestly heavier |
| left leg | `12.923` | `20.41%` | `9.862` | `13.01%` | Manny left leg is much lighter |
| right leg | `12.891` | `20.36%` | `9.862` | `13.01%` | Manny right leg is much lighter |
| spine / torso chain | `17.898` | `28.27%` | `25.070` | `33.06%` | Manny torso is materially heavier |
| neck + head | `4.896` | `7.73%` | `7.689` | `10.14%` | Manny neck/head are heavier |
| left shoulder / arm / hand | `4.727` | `7.47%` | `7.907` | `10.43%` | Manny left upper limb is materially heavier |
| right shoulder / arm / hand | `4.852` | `7.66%` | `7.907` | `10.43%` | Manny right upper limb is materially heavier |

## High-Confidence Takeaways

1. Manny is currently **torso-heavy and upper-body-heavy** relative to the ProtoMotions SMPL training asset.
2. Manny is currently **leg-light**, especially across the full thigh-calf-foot-ball chains.
3. That means the current UE runtime likely has:
   - lower lower-limb inertia than training
   - more upper-body contribution to total mass than training
4. This is exactly the kind of mismatch that can distort:
   - gait timing
   - recovery behavior
   - perturbation readability
   - PD target response under locomotion

## Notable Manny Detail

The raw Manny body inventory includes one especially suspicious value:

- `spine_01 massKg=0.005`

That suggests the current Manny spine chain is not mass-balanced the way the SMPL torso stack is. Most of the Manny torso mass sits higher in:

- `spine_03`
- `spine_04`
- `spine_05`

This does not automatically mean the asset is wrong, but it does mean the torso COM profile is not close to the SMPL training body.

## Recommended Next Step

Do not globally rescale total mass first.

The next alignment pass should:

1. define a Stage 1 family-level target mass policy
2. shift Manny toward:
   - heavier legs
   - relatively lighter torso / upper body
3. prefer family-level explicit overrides or controlled scales over ad hoc per-bone guesswork
4. re-run:
   - passive smoke
   - movement smoke
   - movement soak
   - G2 presentation regression

## Working Hypothesis

If the current mass mismatch matters materially, then bringing Manny closer to the SMPL family distribution should help more in locomotion and perturbation behavior than in idle.

That is consistent with the current stress-test evidence:

- idle is robust even under aggressive relaxation
- movement is much less tolerant

So the mass-distribution pass should be judged primarily against movement and perturbation behavior, not just idle. 
