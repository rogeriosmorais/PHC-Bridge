# Stage 2 Coordinate-Frame Glossary

Coordinate values are not interchangeable merely because they use `FVector`, `FQuat`, or `FTransform`. Every Stage 2 boundary must state the frame, handedness, units, and whether a vector is polar or axial.

| Frame | Meaning | Units / handedness | Producers | Consumers | Required conversion |
|---|---|---|---|---|---|
| UE world | Unreal level coordinates | centimeters, Z-up, X-forward, Y-right, left-handed | actor, mesh, Chaos, Pose Search trajectory | shell integration, world telemetry, asset sampler origin | Explicit conversion before policy tensors |
| Actor | Character actor-local basis | centimeters, UE handedness | actor transform | shell intent, desired actor facing | `ActorTransform.InverseTransform*` / `Transform*` |
| Skeletal mesh | Manny component basis relative to actor | centimeters, UE handedness; standard yaw offset approximately -90° | skeletal mesh component | Pose Search facing basis, animation placement | Compose actor world rotation with mesh-relative rotation |
| Animation data | Authored sequence/root-motion basis with identity sampler origin | centimeters, UE handedness; authored forward is +Y for current unarmed corpus | Pose Search asset sampler | future reference sampling, asset audits | Apply mesh/root origin only at the named sampling boundary |
| Sampled animation world | Animation data placed under the skeletal mesh component transform | centimeters, UE handedness | asset sampler with mesh root origin | reference alignment and visualization | Never confuse with identity-origin animation data |
| Manny component-rest | Manny bone component transforms at captured neutral/rest pose | centimeters, UE handedness | skeleton/rest cache | Manny-to-SMPL canonical adapter | Remove rest component rotation explicitly |
| Canonical SMPL / Proto world | 24-body canonical policy representation | meters, Z-up, X-forward, Y-left, right-handed | observation and future-pose adapter | tensor builders | Polar: `(x,y,z)->(x,-y,z)`; axial: `(-x,y,-z)`; cm→m |
| Policy heading-local | Canonical Proto values rotated by inverse root heading | meters or SI velocities, right-handed | `BuildSelfObservation`, `BuildMimicTargetPoses` | ONNX policy | Heading from canonical root +X; apply one named inverse-yaw rotation |
| Action exponential map | Three policy outputs per Proto joint | radians in Proto joint coordinates | ONNX output | action decoder | Exp-map to quaternion, then Proto basis to UE basis |
| Manny parent-relative target | Physics Control target relative to the controlled bone's parent/control frame | UE quaternion convention | action adapter and target composer | Physics Control | Compose around captured Manny neutral/bind; do not write world rotation directly |
| Kinematic shell | Actor/root route authority for Stage 2A | centimeters and degrees in UE world | scripted bridge shell | actor transform, route evaluator | Must not directly pose simulated bodies |
| Physical root | Simulated pelvis/root body observed from Chaos | centimeters in UE world before policy conversion | Chaos/Physics Control readback | shell-tracking and causal metrics | Compare with actor-transformed initial local offset |

## Naming rule

Frame-sensitive values should carry their frame and units in the symbol name where practical, for example:

- `WorldVelocityCmPerSecond`
- `ActorLocalIntentDirection`
- `MeshWorldFacing`
- `AnimationDataRootTransform`
- `ProtoCanonicalRootRotation`
- `HeadingLocalBodyPositionMeters`
- `ParentRelativeControlTarget`

Generic names such as `Velocity`, `RootTransform`, `Facing`, or `Reference` are acceptable only inside a function whose signature and documentation establish one unambiguous frame.

## Boundary rule

Each conversion function should have one semantic responsibility. Preferred boundary names include:

- `WorldTrajectoryToPoseSearchQuery`
- `AnimationPoseToCanonicalProtoPose`
- `CanonicalBodyToPolicyObservation`
- `AnimationFutureToPolicyMimicTarget`
- `ProtoActionToMannyParentRelativeTarget`

A conversion should be unit tested with basis vectors, exact zero, nonzero yaw, inverse roundtrip, and a turn case. Runtime product episodes should validate integration, not discover elementary frame algebra.

## Current high-risk boundaries

1. Actor versus skeletal-mesh facing supplied to Pose Search.
2. Identity-origin animation root motion versus mesh-placed sampled root motion.
3. Sampled Manny bone rotations versus canonical SMPL global rotations.
4. Current physical-body reference versus first sampled future reference.
5. Previous-future-step root heading used by the 6,495-value mimic tensor.
6. Proto action coordinates versus Manny parent-relative Physics Control targets.
7. Kinematic actor/shell pose versus simulated physical-root pose used by tracking evidence.
