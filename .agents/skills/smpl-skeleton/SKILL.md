---
name: SMPL Skeleton Reference
description: Stable SMPL-to-UE5 reference data for retargeting and coordinate discussions
---

# SMPL Skeleton Reference

Use this as reference data for naming, hierarchy, and coordinate-system discussions.

Do not treat this file as authority for exact final runtime transforms until validated against UE5 runtime evidence.

## Coordinate Systems

SMPL:
- right-handed
- Y-up

UE5:
- left-handed
- Z-up

Important:
- axis permutation alone is not enough
- handedness must be handled explicitly
- final conversion must be verified with runtime evidence

## SMPL Joint Indices

| Index | Joint Name | Parent Index | Approximate UE5 Bone |
|---|---|---:|---|
| 0  | Pelvis     | -1 | pelvis |
| 1  | L_Hip      | 0  | thigh_l |
| 2  | R_Hip      | 0  | thigh_r |
| 3  | Spine1     | 0  | spine_01 |
| 4  | L_Knee     | 1  | calf_l |
| 5  | R_Knee     | 2  | calf_r |
| 6  | Spine2     | 3  | spine_02 |
| 7  | L_Ankle    | 4  | foot_l |
| 8  | R_Ankle    | 5  | foot_r |
| 9  | Spine3     | 6  | spine_03 |
| 10 | L_Foot     | 7  | ball_l |
| 11 | R_Foot     | 8  | ball_r |
| 12 | Neck       | 9  | neck_01 |
| 13 | L_Collar   | 9  | clavicle_l |
| 14 | R_Collar   | 9  | clavicle_r |
| 15 | Head       | 12 | head |
| 16 | L_Shoulder | 13 | upperarm_l |
| 17 | R_Shoulder | 14 | upperarm_r |
| 18 | L_Elbow    | 16 | lowerarm_l |
| 19 | R_Elbow    | 17 | lowerarm_r |
| 20 | L_Wrist    | 18 | hand_l |
| 21 | R_Wrist    | 19 | hand_r |
| 22 | L_Hand     | 20 | unmapped / fingers |
| 23 | R_Hand     | 21 | unmapped / fingers |

## Pose Representation

SMPL body pose is commonly represented as:
- 24 joints
- axis-angle rotation vectors
- 3 floats per joint
- 72 floats total

Interpretation:
- vector direction = rotation axis
- vector magnitude = rotation angle in radians

Neutral pose convention:
- all-zero joint rotations correspond to the neutral reference pose in the pose parameterization

## Retargeting Notes

When implementing or reviewing retargeting:
- keep coordinate-frame conversion separate from bone-name mapping
- validate pelvis/root handling first
- validate left/right symmetry explicitly
- validate against UE5 runtime or imported reference poses before trusting the bridge

## This File Should Not Contain

- workflow steps
- local machine paths
- execution logs
- phase plans
- speculative observation/action contracts
