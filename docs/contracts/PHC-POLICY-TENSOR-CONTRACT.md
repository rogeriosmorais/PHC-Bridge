# PHC Policy Tensor and Frame Contract

Baseline: `409f8971e250212e3d8ccadbee2c675a1b82ee89`

This document separates what the checkout **proves about the current UE runtime** from what remains an inference about the original ProtoMotions checkpoint. The machine-readable companion is `phc-policy-tensor-contract.v1.json`.

## Confidence vocabulary

- **Proven current runtime**: directly established by executable source and deterministic tests in this checkout.
- **Supported inference**: consistent with source, historical plans, tensor dimensions, and captured evidence, but not checked against the original training implementation.
- **Unresolved**: the authoritative checkpoint configuration or preprocessing source is absent or not pinned.

## Runtime model interface

| Tensor | Shape | Current-runtime confidence |
|---|---:|---|
| `self_obs` | `[1, 358]` | Proven |
| `mimic_target_poses` | `[1, 6495]` | Proven |
| `terrain` | `[1, 256]` | Proven |
| `actions` | `[1, 69]` | Proven |

The export wrapper names the same three inputs. The runtime resolves them by name rather than assuming descriptor order.

## Coordinate systems

The bridge converts Unreal world data into the Proto/Isaac runtime basis before tensor construction.

| Quantity | UE → Proto conversion |
|---|---|
| Position and linear velocity | `(x, y, z) → (x, -y, z)`, then centimeters to meters |
| Quaternion | basis conversion from UE left-handed coordinates to Proto right-handed coordinates |
| Angular velocity | `(-x, y, -z)` because angular velocity is an axial vector |

After conversion, root heading is extracted by rotating Proto `+X` through the canonical root rotation and computing `atan2(y, x)`. Heading-local quantities are rotated by the inverse of that yaw.

## Body order

The tensor uses 24 SMPL bodies in this order:

`Pelvis, L_Hip, L_Knee, L_Ankle, L_Toe, R_Hip, R_Knee, R_Ankle, R_Toe, Torso, Spine, Chest, Neck, Head, L_Thorax, L_Shoulder, L_Elbow, L_Wrist, L_Hand, R_Thorax, R_Shoulder, R_Elbow, R_Wrist, R_Hand`.

Manny has no separate controlled hand body for the final SMPL hand entries. `L_Wrist/L_Hand` both map to `hand_l`; `R_Wrist/R_Hand` both map to `hand_r`, with distal rotations collapsed by the adapter.

## `self_obs` layout

| Offset | Width | Block |
|---:|---:|---|
| 0 | 1 | Root height above ground, meters |
| 1 | 69 | 23 non-root positions relative to root, heading-local XYZ |
| 70 | 144 | 24 heading-relative global rotations, tangent-plus-normal six-vector |
| 214 | 72 | 24 heading-local linear velocities |
| 286 | 72 | 24 heading-local angular velocities |

Total: `1 + 23×3 + 24×6 + 24×3 + 24×3 = 358`.

## `mimic_target_poses` layout

The tensor contains 15 future steps at nominal offsets `1/30 … 15/30` seconds. Each step contains 433 floats:

| Per-step offset | Width | Block |
|---:|---:|---|
| 0 | 72 | Target position minus the same body's previous-reference position, previous-root-heading-local |
| 72 | 72 | Target position minus the previous-reference root position, previous-root-heading-local |
| 144 | 144 | Previous same-body rotation inverse × target rotation, tangent-plus-normal |
| 288 | 144 | Previous root heading inverse × target global rotation, tangent-plus-normal |
| 432 | 1 | Actual future time offset after animation-end clamping |

The first future step uses the current canonical physical body samples as its reference. Every later step uses the immediately preceding sampled future pose. Total width: `15 × 433 = 6495`.

This autoregressive reference choice is important: the tensor does **not** encode every future pose directly against the current root. It mixes previous-frame body deltas with previous-root-relative global pose features.

## `terrain` layout

The terrain tensor is a 16×16 grid. Each value is `root_height - sampled_ground_height`, in meters. Grid offsets are generated over `[-1, +1]` meters on both local axes.

## Actions

The output contains 23 three-value exponential-map joint actions, 69 floats total. The current path is:

`raw ONNX output → finite check → force-zero/clamp/scale/smoothing → regional causal scales → Proto quaternion decode → Manny bind/neutral composition → range and constraint adaptation → Physics Control target`.

The joint order begins with left leg, right leg, torso/spine/chest, neck/head, then left and right arm chains. Wrist and hand Proto joints share one Manny control on each side.

## What this contract does not prove

1. The original checkpoint's `smpl.yaml`, motion-tracker config, complete motion library, and preprocessing code are not all present and pinned in this checkout.
2. UE Pose Search future samples are adapted through Manny rest frames into canonical SMPL rotations. Exact equality with the training `motion_lib` frame is not established.
3. The original action normalization and PD target-range transform remain partially provenance-based rather than checkpoint-config-verified.
4. The training sampler's behavior near animation ends may wrap, clip, or select a different continuation. UE currently clamps and shortens the time channel.
5. A full synthetic parity test has not yet followed one world-space root translation and turn through Pose Search sampling, canonical adaptation, tensor construction, ONNX inference, and published joint targets.

## Reference implementation

`Training/physanim/policy_tensor_reference.py` is an independent, pure-Python transcription of the current tensor packing. It is intentionally free of Unreal types and supports synthetic frame tests. It is a runtime-parity oracle, not evidence that the current runtime matches the missing original training preprocessing.

## Runtime parity enforcement

`scripts/validate_policy_tensor_parity.py` consumes the UE `policy-input-provenance.json` and `policy-input-snapshot.json` artifacts. It independently rebuilds `self_observation`, `mimic_target_poses`, and `terrain` with the Python reference and reports per-tensor maximum and mean absolute error, mismatch count, and the worst element index. The locked default tolerance is `1e-5`.

The validator intentionally uses `mimic_reference_body_samples`, not the live canonical world samples, as the current reference for `mimic_target_poses`. That distinction is required by the data-frame contract and is covered by regression tests.

Synthetic tests lock actor-forward, lateral, and 30-degree-turn behavior. A fresh authoritative UE capture and its parity report must be retained as evidence for any runtime revision that changes observation construction, Pose Search adaptation, or frame placement.

This runtime parity proof localizes remaining disagreement to either:

- Pose Search sampling/canonical adaptation before the builder, or
- the builder itself.

It still does not prove identity with missing original training preprocessing or checkpoint configuration.
