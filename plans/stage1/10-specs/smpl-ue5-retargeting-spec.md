# SMPL to UE5 Retargeting Spec

## Purpose

This spec defines the Stage 1 retargeting contract between the PHC-family SMPL policy interface and the UE5 Manny/Quinn runtime skeleton.

It exists to prevent balance failures caused by silent coordinate, order, handedness, side, or reference-pose mistakes. It does not authorize asset retuning, mass retuning, or physics-asset limit changes.

## Authority

This file owns the active SMPL <-> UE5 naming, order, coordinate, and validation rules for Stage 1.

Related contracts:

- Bridge module boundaries: [ue-bridge-implementation-spec.md](ue-bridge-implementation-spec.md)
- Continuous balance architecture: [continuous_balance_architecture.md](continuous_balance_architecture.md)
- Physics asset contract: [physics_asset_contract.md](physics_asset_contract.md)
- Instrumentation and acceptance: [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)

Reference data:

- `.agents/skills/smpl-skeleton/SKILL.md`
- `PhysAnimBridge::GetSmplObservationBoneNames()`
- `PhysAnim.Bridge.SmplOrderContract`
- `PhysAnim.Bridge.FrameConversion`

## Coordinate Contract

SMPL authoring space:

- right-handed
- Y-up
- axis-angle local joint rotations

UE5 runtime space:

- left-handed
- Z-up
- Manny/Quinn bone names
- `FQuat` local control rotations

The conversion must handle both axis permutation and handedness. Axis permutation alone is not a valid implementation.

Current Stage 1 vector basis:

| SMPL vector | UE vector |
| --- | --- |
| `(1, 0, 0)` | `(0, 1, 0)` |
| `(0, 1, 0)` | `(0, 0, 1)` |
| `(0, 0, 1)` | `(1, 0, 0)` |

Implementation functions:

- `SmplVectorToUe(SmplVector) = FVector(Smpl.Z, Smpl.X, Smpl.Y)`
- `UeVectorToSmpl(UeVector) = FVector(Ue.Y, Ue.Z, Ue.X)`

Quaternion conversion must be basis-derived and must preserve identity:

- `SmplQuaternionToUe(FQuat::Identity) == FQuat::Identity`
- `UeQuaternionToSmpl(FQuat::Identity) == FQuat::Identity`
- `UeQuaternionToSmpl(SmplQuaternionToUe(Q)) ~= Q`

The required behavior is already guarded by `PhysAnim.Bridge.FrameConversion`.

## SMPL Observation Order

Observation packing uses the 24-body SMPL order. This order is separate from the 23-action order.

| Index | SMPL joint | UE5 bone |
| ---: | --- | --- |
| 0 | Pelvis | `pelvis` |
| 1 | L_Hip | `thigh_l` |
| 2 | R_Hip | `thigh_r` |
| 3 | Spine1 | `spine_01` |
| 4 | L_Knee | `calf_l` |
| 5 | R_Knee | `calf_r` |
| 6 | Spine2 | `spine_02` |
| 7 | L_Ankle | `foot_l` |
| 8 | R_Ankle | `foot_r` |
| 9 | Spine3 | `spine_03` |
| 10 | L_Foot | `ball_l` |
| 11 | R_Foot | `ball_r` |
| 12 | Neck | `neck_01` |
| 13 | L_Collar | `clavicle_l` |
| 14 | R_Collar | `clavicle_r` |
| 15 | Head | `head` |
| 16 | L_Shoulder | `upperarm_l` |
| 17 | R_Shoulder | `upperarm_r` |
| 18 | L_Elbow | `lowerarm_l` |
| 19 | R_Elbow | `lowerarm_r` |
| 20 | L_Wrist | `hand_l` |
| 21 | R_Wrist | `hand_r` |
| 22 | L_Hand | no separate Manny/Quinn runtime control |
| 23 | R_Hand | no separate Manny/Quinn runtime control |

`PhysAnimBridge::NumSmplBodies` must remain `24`.

## PHC Action Order

The PHC policy output is `69` floats: `23` joints x `3` axis-angle floats.

Action joint rotations are scaled from normalized model output into exp-map radians before conversion:

- `ExpMap = PI * FVector(ActionX, ActionY, ActionZ)`
- `ExpMapToQuaternion(ExpMap)`
- `SmplQuaternionToUe(...)`

Current Stage 1 action-to-control order:

| Action joint index | UE5 control bone |
| ---: | --- |
| 0 | `thigh_l` |
| 1 | `calf_l` |
| 2 | `foot_l` |
| 3 | `ball_l` |
| 4 | `thigh_r` |
| 5 | `calf_r` |
| 6 | `foot_r` |
| 7 | `ball_r` |
| 8 | `spine_01` |
| 9 | `spine_02` |
| 10 | `spine_03` |
| 11 | `neck_01` |
| 12 | `head` |
| 13 | `clavicle_l` |
| 14 | `upperarm_l` |
| 15 | `lowerarm_l` |
| 16 + 17 | collapsed into `hand_l` |
| 18 | `clavicle_r` |
| 19 | `upperarm_r` |
| 20 | `lowerarm_r` |
| 21 + 22 | collapsed into `hand_r` |

`PhysAnimBridge::NumActionJoints` must remain `23`, and `NumActionFloats` must remain `69`.

The action order is intentionally not the same as the 24-body observation order. Any implementation that assumes they are identical is invalid.

## Distal Hand Rule

SMPL has wrist and hand-tip joints for each side. Manny/Quinn has one runtime hand control per side for the Stage 1 bridge.

Required behavior:

- left wrist and left hand-tip rotations collapse into `hand_l`
- right wrist and right hand-tip rotations collapse into `hand_r`
- the collapse function must be deterministic and tested
- no finger or hand-tip-only runtime controls are introduced in V0

## Pelvis and Root Rule

The pelvis is present in the 24-body observation order and is part of the balance-critical chain.

The pelvis is not a direct PHC action output in the current 23-action control order. Runtime root/pelvis handling must therefore stay explicit:

- pelvis pose contributes to observations, heading, support proxy, and balance truth
- pelvis/root transforms must not be silently driven as if they were a regular action joint
- any pelvis/root target rebasing belongs to the bridge's standing-reference and activation contracts, not to ad hoc retargeting math

## Manny/Quinn Runtime Skeleton Rule

Stage 1 defaults to Manny/Quinn names. Retargeting work must not introduce a new runtime skeleton unless a graph node explicitly changes the Stage 1 skeleton target.

Required runtime bones:

- `pelvis`
- `spine_01`, `spine_02`, `spine_03`
- `thigh_l`, `calf_l`, `foot_l`, `ball_l`
- `thigh_r`, `calf_r`, `foot_r`, `ball_r`
- `neck_01`, `head`
- `clavicle_l`, `upperarm_l`, `lowerarm_l`, `hand_l`
- `clavicle_r`, `upperarm_r`, `lowerarm_r`, `hand_r`

Missing required bones are startup failures, not runtime fallbacks.

## Limit and Mass Interpretation

The archived SMPL/Manny limit and mass tables are diagnostic references only. They do not authorize broadening Manny limits or changing mass distribution.

Known high-risk mismatches:

- Manny lower-body and spine limits are materially narrower than the broad SMPL training ranges.
- Manny is torso/upper-body heavy and leg-light relative to the ProtoMotions SMPL training body.
- Some neck/head/clavicle controls lack direct Manny one-to-one physics-asset constraint pairs in prior audits.

Implementation rule:

- retargeting converts names and frames
- operating-limit policy constrains target authority
- physics-asset and mass changes require separate graph tasks

## Validation Requirements

Every retargeting change must preserve or add deterministic tests for:

1. 24-body SMPL observation count and expected sentinel indices.
2. SMPL Y-up to UE Z-up vector conversion.
3. identity quaternion preservation in both directions.
4. quaternion roundtrip tolerance.
5. action tensor length of 69 floats.
6. action order sentinel mapping for `thigh_l`, `spine_01`, `hand_l`, and `hand_r`.
7. left/right symmetry for mirrored lower-limb rotations.
8. distal hand collapse behavior.
9. finite output validation before publishing Physics Control targets.

Functional PIE evidence may supplement these tests but cannot replace them.

## Failure Signals

A retargeting implementation is suspect if any of these appear:

- identity action produces non-identity local controls
- first policy-enabled frame reports large raw lower-body offsets
- left/right mirrored inputs produce asymmetric target magnitudes
- pelvis/root is treated as an ordinary action joint
- observation body order differs from policy training expectations
- hand-tip joints create extra runtime controls
- apparent balance gains require shell, capsule, CMC, or kinematic assistance

## Evidence Requirements

Retargeting evidence must include:

- test names and command lines
- the observed SMPL body/action counts
- at least one identity-pose conversion result
- at least one non-identity sentinel joint result
- current model tensor shape contract
- any known Manny/SMPL limit or mass mismatch relevant to the run

Evidence must not claim physical viability from mapping tests alone. A correct retargeting contract is necessary for true balance mode, but true balance remains proven only by the balance acceptance gates.
