# SMPL to Manny Limit Table

## Purpose

This file records the first explicit joint-limit and action-range audit for the Stage 1 bridge.

It compares:

- the ProtoMotions SMPL training-side joint families from [smpl_humanoid.xml](/F:/NewEngine/Training/ProtoMotions/protomotions/data/assets/mjcf/smpl_humanoid.xml)
- the current Manny physics-asset direct constraint pairs logged by `PhysAnim.Component.MannyConstraintInventory`

This is an audit table, not a final retuning sheet. The goal is to make the mismatch visible before changing limits, masses, or PD-family profiles.

## Important Interpretation Rule

The comparison is approximate.

ProtoMotions SMPL uses three hinge axes per body region, while Manny's UE physics asset uses swing/twist constraints with cone-style limits. So this table compares:

- SMPL training-side amplitude class
- Manny direct-constraint safety envelope

It does **not** claim exact axis-to-axis equivalence.

## Data Sources

- ProtoMotions training asset:
  - [smpl_humanoid.xml](/F:/NewEngine/Training/ProtoMotions/protomotions/data/assets/mjcf/smpl_humanoid.xml)
- Manny direct-constraint inventory:
  - [PhysAnimUE5.log](/F:/NewEngine/PhysAnimUE5/Saved/Logs/PhysAnimUE5.log#L1332)

## Direct Constraint Inventory Summary

- Audited Stage 1 bridge controls: `21`
- Direct Manny constraint pairs found: `17`
- Missing direct Manny constraint pairs: `4`
  - `neck_01 <- spine_03`
  - `head <- neck_01`
  - `clavicle_l <- spine_03`
  - `clavicle_r <- spine_03`

Those four controls exist in the bridge, but the current Manny physics asset does not expose them as direct one-to-one constraint pairs for inventory/audit purposes.

## Comparison Table

| SMPL family | UE child | UE parent | ProtoMotions nominal range / PD family | Current Manny direct constraint | Mismatch note |
|---|---|---|---|---|---|
| hip | `thigh_l` | `pelvis` | `-180..180` per axis, `800/80` | twist `20`, swing1 `55`, swing2 `30` | Manny is much narrower than SMPL on hip rotation |
| knee | `calf_l` | `thigh_l` | `-180..180` per axis, `800/80` | twist `60`, swing1 `5`, swing2 `5` | very narrow knee cone relative to SMPL broad range |
| ankle | `foot_l` | `calf_l` | `-180..180` per axis, `800/80` | twist `35`, swing1 `10`, swing2 `20` | Manny ankle is far tighter than SMPL broad ankle target space |
| toe | `ball_l` | `foot_l` | `-180..180` per axis, `500/50` | twist `Free/45`, swing1 `Free/45`, swing2 `Free/45` | Manny toe is comparatively permissive |
| hip | `thigh_r` | `pelvis` | `-180..180` per axis, `800/80` | twist `20`, swing1 `55`, swing2 `30` | same left/right mismatch as left hip |
| knee | `calf_r` | `thigh_r` | `-180..180` per axis, `800/80` | twist `60`, swing1 `5`, swing2 `5` | same left/right mismatch as left knee |
| ankle | `foot_r` | `calf_r` | `-180..180` per axis, `800/80` | twist `35`, swing1 `10`, swing2 `20` | same left/right mismatch as left ankle |
| toe | `ball_r` | `foot_r` | `-180..180` per axis, `500/50` | twist `Free/45`, swing1 `Free/45`, swing2 `Free/45` | same left/right behavior as left toe |
| torso base | `spine_01` | `pelvis` | `-180..180` per axis, `1000/100` | twist `Free/45`, swing1 `Free/45`, swing2 `Free/45` | Manny torso base is moderate, not fully broad like SMPL |
| spine mid | `spine_02` | `spine_01` | `-180..180` per axis, `1000/100` | twist `10`, swing1 `10`, swing2 `10` | Manny mid-spine is extremely tight versus SMPL |
| chest | `spine_03` | `spine_02` | `-180..180` per axis, `1000/100` | twist `10`, swing1 `10`, swing2 `10` | Manny upper-spine is extremely tight versus SMPL |
| neck | `neck_01` | `spine_03` | `-180..180` per axis, `500/50` | direct constraint missing | bridge control has no direct Manny pair to audit; action semantics are currently indirect |
| head | `head` | `neck_01` | `-180..180` per axis, `500/50` | direct constraint missing | same issue as neck |
| thorax / clavicle | `clavicle_l` | `spine_03` | `-180..180` per axis, `500/50` | direct constraint missing | shoulder-girdle entry has no direct Manny pair to audit |
| shoulder | `upperarm_l` | `clavicle_l` | `-720..720` per axis, `500/50` | twist `35`, swing1 `45`, swing2 `45` | Manny shoulder is radically narrower than SMPL broad shoulder target space |
| elbow | `lowerarm_l` | `upperarm_l` | `-720..720` per axis, `500/50` | twist `70`, swing1 `5`, swing2 `5` | Manny elbow is strongly constrained compared with SMPL |
| wrist / hand | `hand_l` | `lowerarm_l` | `-180..180` per axis, `300/30` | twist `45`, swing1 `20`, swing2 `60` | Manny hand is narrower than SMPL, but less extreme than shoulder/elbow |
| thorax / clavicle | `clavicle_r` | `spine_03` | `-180..180` per axis, `500/50` | direct constraint missing | same issue as left clavicle |
| shoulder | `upperarm_r` | `clavicle_r` | `-720..720` per axis, `500/50` | twist `35`, swing1 `45`, swing2 `45` | same left/right mismatch as left shoulder |
| elbow | `lowerarm_r` | `upperarm_r` | `-720..720` per axis, `500/50` | twist `70`, swing1 `5`, swing2 `5` | same left/right mismatch as left elbow |
| wrist / hand | `hand_r` | `lowerarm_r` | `-180..180` per axis, `300/30` | twist `45`, swing1 `20`, swing2 `60` | same left/right mismatch as left hand |

## High-Confidence Takeaways

1. Manny's current lower-body and spine constraints are much narrower than the broad SMPL training ranges.
2. The tightest Manny regions relative to training are:
   - `spine_02`
   - `spine_03`
   - `calf_l`
   - `calf_r`
   - `upperarm_l`
   - `upperarm_r`
   - `lowerarm_l`
   - `lowerarm_r`
3. The current bridge controls for `neck_01`, `head`, and both clavicles do not line up with direct Manny constraint pairs, so their current UE action-range semantics are less transparent than legs, spine, forearms, and hands.
4. This makes it unsafe to interpret ProtoMotions action amplitudes in UE as if Manny already shared the same reachable target space.

## Recommended Next Step

Do not broaden Manny limits blindly.

The next alignment pass should define a Stage 1 operating-limit policy:

- keep a stricter hard safety envelope where needed for stability
- document where UE action-range semantics are intentionally tighter than training
- then move to mass-distribution audit before trying broad PD-family retuning
