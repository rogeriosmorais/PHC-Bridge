---
name: SMPL Skeleton Reference
description: SMPL body model joint definitions, coordinate system, and reference poses for retargeting
---

# SMPL Skeleton Reference

> Status: planning reference. Use this as a starting point for later implementation, but verify the final transform against UE5 runtime data before treating it as implementation-safe.

## Coordinate System

- **SMPL uses Y-up, right-handed** (Y is up, X is right, Z is forward)
- **UE5 uses Z-up, left-handed** (Z is up, X is forward, Y is right)
- The simple axis permutation `UE5.X = SMPL.Z`, `UE5.Y = SMPL.X`, `UE5.Z = SMPL.Y` only changes axis order. It does **not** by itself account for the handedness change.
- Treat the final handedness-aware transform as unresolved until it is verified against UE5 implementation data and reference poses.
- Safe planning assumption: the Stage 1 bridge must include an explicit, tested coordinate-frame conversion step rather than relying on axis permutation alone.

## SMPL Joint Indices (24 joints)

| Index | Joint Name | Parent Index | UE5 Bone Equivalent |
|---|---|---|---|
| 0 | Pelvis | -1 (root) | pelvis |
| 1 | L_Hip | 0 | thigh_l |
| 2 | R_Hip | 0 | thigh_r |
| 3 | Spine1 | 0 | spine_01 |
| 4 | L_Knee | 1 | calf_l |
| 5 | R_Knee | 2 | calf_r |
| 6 | Spine2 | 3 | spine_02 |
| 7 | L_Ankle | 4 | foot_l |
| 8 | R_Ankle | 5 | foot_r |
| 9 | Spine3 | 6 | spine_03 |
| 10 | L_Foot | 7 | ball_l |
| 11 | R_Foot | 8 | ball_r |
| 12 | Neck | 9 | neck_01 |
| 13 | L_Collar | 9 | clavicle_l |
| 14 | R_Collar | 9 | clavicle_r |
| 15 | Head | 12 | head |
| 16 | L_Shoulder | 13 | upperarm_l |
| 17 | R_Shoulder | 14 | upperarm_r |
| 18 | L_Elbow | 16 | lowerarm_l |
| 19 | R_Elbow | 17 | lowerarm_r |
| 20 | L_Wrist | 18 | hand_l |
| 21 | R_Wrist | 19 | hand_r |
| 22 | L_Hand | 20 | (unmapped - fingers) |
| 23 | R_Hand | 21 | (unmapped - fingers) |

## SMPL Pose Representation

- Pose is stored as **axis-angle** rotation vectors: 24 joints x 3 floats = 72 floats
- Each 3-float vector represents the axis of rotation (direction) and the angle (magnitude in radians)
- To convert to quaternion: use Rodrigues' rotation formula
- **T-pose** = all zeros (identity rotation for every joint)

## PHC Observation Space (approximate planning reference, ~131 floats)

The exact observation space depends on the PHC version, but typically includes:

- Root position (3)
- Root orientation as quaternion (4)
- Root linear velocity (3)
- Root angular velocity (3)
- Joint positions relative to root (23 x 3 = 69)
- Joint rotations as 6D rotation representation (23 x 6 = 138) or quaternions (23 x 4 = 92)
- Joint angular velocities (23 x 3 = 69)

Check the specific PHC config for the exact observation and action spaces before implementation.

## PHC Action Space (approximate planning reference, ~69 floats for joint targets)

- Typically outputs target joint rotations in axis-angle or quaternion format
- Some versions also output PD gains (stiffness/damping per joint)
