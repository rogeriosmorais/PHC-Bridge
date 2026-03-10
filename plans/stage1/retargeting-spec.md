# Stage 1 SMPL To UE5 Retargeting Spec

## Purpose

This document defines the Stage 1 retargeting plan between the PHC/SMPL policy space and the UE5 Manny skeleton. It is a planning spec for implementation and validation, not a claim that the final transform math is already proven.

## Scope

This spec covers:

- the joint mapping subset used in Stage 1
- how unmapped bones are handled
- what transform risks remain unresolved
- what validation cases must pass before the mapping is trusted

## Skeleton Roles

- **SMPL side:** policy space and training-space representation
- **UE5 side:** Manny runtime skeleton and `UPhysicsControlComponent` target bones

Stage 1 only needs a mapping for the subset of Manny that corresponds to the SMPL body.

## Mapping Table

| SMPL Joint | UE5 Bone | Stage 1 Handling |
|---|---|---|
| Pelvis | pelvis | required |
| L_Hip | thigh_l | required |
| R_Hip | thigh_r | required |
| Spine1 | spine_01 | required |
| L_Knee | calf_l | required |
| R_Knee | calf_r | required |
| Spine2 | spine_02 | required |
| L_Ankle | foot_l | required |
| R_Ankle | foot_r | required |
| Spine3 | spine_03 | required |
| L_Foot | ball_l | preferred if the chosen control subset uses foot roll |
| R_Foot | ball_r | preferred if the chosen control subset uses foot roll |
| Neck | neck_01 | required |
| L_Collar | clavicle_l | required |
| R_Collar | clavicle_r | required |
| Head | head | required |
| L_Shoulder | upperarm_l | required |
| R_Shoulder | upperarm_r | required |
| L_Elbow | lowerarm_l | required |
| R_Elbow | lowerarm_r | required |
| L_Wrist | hand_l | required |
| R_Wrist | hand_r | required |
| L_Hand | unmapped | leave animation-driven |
| R_Hand | unmapped | leave animation-driven |

## Unmapped-Bone Policy

The following stay outside the Stage 1 policy-controlled subset:

- fingers
- twist bones
- other mannequin helper bones with no SMPL equivalent

These bones keep their animation-driven pose from the upstream animation/PoseSearch path.

## Transform Policy

### Locked Rules

- Frame conversion must be explicit and isolated.
- Left/right mapping must never be inferred implicitly from naming conventions at runtime.
- The bridge must preserve the SMPL joint order used by the selected PHC model.
- Mapping tables must be static data, not discovered heuristically at runtime.

### Still-Unresolved Risk

The final handedness-aware transform is not yet proven. The current skill explicitly warns that axis permutation alone is insufficient.

Until Phase 0 confirms the transform against UE5 runtime data:

- treat coordinate conversion as a dedicated validation target
- do not call the mapping implementation-safe
- do not assume round-trip correctness from naming or axis order alone

## Representation Policy

- The retargeting layer must accept whatever joint representation the selected PHC config actually uses.
- The conversion boundary between UE5 transforms and model representation must be centralized.
- If the model expects local parent-relative joint rotations, the mapping must preserve that relationship rather than converting to ad hoc world-space control values.

## Validation Cases

These cases must be exercised before the retargeting layer is trusted.

### Required Static Cases

- neutral identity case with all mapped joints at rest
- isolated left elbow flexion with the right arm held neutral
- isolated right hip rotation with the left leg held neutral
- pelvis yaw change without intentional spine or limb motion
- spine bend plus mild twist
- explicit left/right asymmetry case to catch mirroring mistakes

### Required Dynamic Cases

- small random-pose round-trip within the mapped subset
- simple multi-joint pose sent through the mapping and visualized on Manny
- frozen first training-side motion reference:
  - `F:\NewEngine\Training\ProtoMotions\data\motions\smpl_humanoid_walk.npy`
- minimal PHC-style output driving Manny in Chaos for the G1 smoke test

### Phase 0 Evidence Capture

Record the chosen validation-pose names, the handedness note, and the Manny smoke-test result in:

- `plans/stage1/retargeting-spec.md`
- `plans/stage1/g1-evidence.md`

## Acceptance Criteria

The retargeting plan is considered implementation-ready when:

- every required SMPL joint has an explicit UE5 destination or an explicit unmapped policy
- handedness handling is called out as an explicit conversion step
- left/right mapping is fixed and documented
- the validation matrix exists and is tied to G1 evidence

## Failure Signals

Treat the mapping as failed if any of these appear:

- left and right limbs are swapped or mirrored
- a joint update moves the wrong limb
- identity pose produces visibly non-identity mapped results without an explained reference-pose reason
- the Manny smoke test becomes unstable due to obviously incorrect transform conversion

## Deliverable For S1-PLAN-03

The implementation-ready output from this planning task should be:

- this document
- a final static mapping table in code-ready order
- a list of validation poses to encode in tests or prototypes
- a short note confirming the handedness transform chosen during Phase 0
