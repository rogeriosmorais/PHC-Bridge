# Stage 1 UE Bridge Implementation Spec

## Purpose

This document freezes the Unreal-side implementation contract for the Stage 1 bridge and for balance activation.

It is implementation-facing and must match the runtime contract in the balance-mode design docs.

Primary operational references:

- `continuous_balance_truth_model.md`
- `authority_matrix.md`
- `instrumentation_and_acceptance.md`

## Runtime Owner

The live Stage 1 runtime owner is:

- `UPhysAnimComponent`

`UPhysAnimComponent` owns:

- startup validation
- PoseSearch query state
- NNE runtime/model/session lifetime
- observation packing
- action unpacking
- Physics Control writes
- bridge runtime state
- balance-activation state
- diagnostic snapshots
- smoke-visible terminal outcome

## Runtime States

The implementation must expose distinct runtime states or equivalent explicit sub-states for:

- `BridgeActive`
- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Standing`
- `BalanceActive_Recovery`
- `SafeDenied`
- `Failed`

Compatibility note:

- existing code symbols may still contain legacy `Phase1`, `Phase2`, `Phase3`, `RootOn`, or `Settle` names
- this documentation pass does not require renaming those symbols

## Required Balance-Activation Data

On balance request acceptance, the runtime must create a dedicated activation record containing:

- attempt-active state
- physics-asset baseline identifiers
- physics-asset contract valid flag
- standing-reference asset identifier
- standing-reference authored-space identifier
- standing-reference control-space projection identifier
- standing-reference mismatch accumulator state
- request-accepted timestamp
- balance-critical chain definition
- support-set definition
- ownership-continuity snapshot
- mesh-side-effect snapshot
- controller-authority blend state
- standing-validation timer state
- shell bookkeeping state
- shell influence diagnostics
- shell helper used flag
- mesh physics blend state if any
- mesh update-when-kinematic state if any
- mesh-wide side-effect event count
- topology change event count
- authority conflict count
- terminal outcome flag
- terminal reason family if any
- failure reason if any

## Required Truth Sources

The implementation must keep these sources separate:

- intended ownership
- raw body simulation state
- modifier-record or control-layer ownership bookkeeping
- mesh-wide physics/update side-effect state
- controller-authority alpha
- shell bookkeeping state
- shell influence materiality

These are different signals.

The implementation must also surface authority conflict events explicitly rather than letting subsystem fights remain implicit.

When activation terminates, the implementation must emit:

- `terminal_reason_family` for coarse rollup only
- `terminal_reason` as the leaf-level emitted reason used by logs and run artifacts

At minimum the leaf-level emitted reason set must distinguish:

- `activation_physics_asset_contract_violation`
- `activation_continuous_simulation_lost`
- `activation_topology_change`
- `activation_target_discontinuity`
- `activation_unstable_gain_or_damping`
- `activation_support_failure`
- `activation_proxy_outside_support_region`
- `activation_pose_reference_mismatch`
- `activation_authority_conflict`
- `activation_movement_reclaim`
- `activation_shell_helper_violation`
- `activation_instability_threshold_breach`
- `activation_standing_validation_timeout`

Bundled labels such as controller-strength-or-representation failure or generic continuity failure may exist only as derived rollups, not as the sole terminal reason recorded for a run.

`activation_authority_conflict` is itself a valid leaf-level terminal reason when subsystem fights are the primary terminating cause.

## Required Physics-Asset Contract

The implementation must validate the active physical plant before `BalanceActivation_Ready`.

### Baseline Definition: The Structural Fingerprint

For `V0`, the authoritative physical-plant baseline is defined in:
- `/Config/PhysAnim/V0_Plant_Baseline.json`

This file is a **Structural Fingerprint** that formally defines the expected physical tree. It must include:
- **Asset GUIDs**: Unique identifiers for the `USkeletalMesh` and `UPhysicsAsset`.
- **Inertial Fingerprint**: Per-body mass and principal inertia components (Ixx, Iyy, Izz) and their local offsets.
- **Topological Fingerprint**: The exact parent-child connectivity of all simulating bodies.
- **Constraint Fingerprint**: Active constraint profile names and the rotation/translation limits for all joints in the balance-critical chain.
- **Collision Fingerprint**: The exact collision-disable adjacency table for the mesh.

### Runtime Validation Rules

| Property | Check Type | **Check Cadence** | V0 Tolerance | Failure Action |
| :--- | :--- | :--- | :--- | :--- |
| **Asset Identity** | Live GUID check | **Continuous (Every Frame)** | Exact match | `activation_physics_asset_contract_violation` |
| **Topological Match**| Instance pointer audit | **Continuous (Every Frame)** | No pointer changes | `activation_topology_change` |
| **Total Mass** | Live raw summation | At `Ready` & `Blend Start` | `+/- 5%` | `activation_physics_asset_contract_violation` |
| **Family Mass** | Live raw summation | At `Ready` & `Blend Start` | `+/- 10%` | `activation_physics_asset_contract_violation` |
| **Principal Inertia**| Live raw read | At `Ready` & `Blend Start` | `+/- 15%` | `activation_physics_asset_contract_violation` |
| **Constraint Profile**| Live profile name | **Continuous (Every Frame)** | Exact match | `activation_physics_asset_contract_violation` |
| **Collision Disable**| Live adjacency check | **Continuous (Every Frame)** | Exact match | `activation_physics_asset_contract_violation` |
| **Support Geometry** | Binary Audit | At `Ready` & `Blend Start` | Exact match | `activation_physics_asset_contract_violation` |

### Interpretation Rules

- **Live vs Trusted**: Mass and inertia must be computed from the live `FPhysicsBodyInstance` states, not from authored config, to catch runtime overrides or scaling errors. Asset identities and physical materials are trusted once the GUID match is verified.
- **Continuous Check Point**: "Continuous" checks must be performed in the **Bridge Update (TG_PrePhysics)** step of every frame.
- **Binary Support Audit**: The support-geometry audit is **binary**. A body in the support set passes only if its collision shape and dimensions exactly match the fingerprint.
- **Mutation Block**: If any field marked as `Continuous` changes while in any `BalanceActivation` or `BalanceActive` state, the run must fail immediately.
- **Reporting**: The run artifact must record the `physics_asset_baseline_id` and the specific field that triggered a violation.

Weak assumption rejected:
- "The plant only needs to be checked once at spawn time." (Runtime overrides or scaling events can falsify the balance proof after the character has spawned).

Weak assumption rejected:
- "If the asset name matches, the physical mass and inertia are correct by default."

## Required Skeleton Mapping Admissibility Contract

The implementation must validate that the runtime skeleton is compatible with the authored standing-reference asset before reaching `BalanceActivation_Ready`.

### Skeleton Audit Baseline

The bridge recognizes a skeleton as "Audited Manny/Quinn-Derived" only if it passes this automated audit:

1.  **Bone Topology Check**: Every bone name and hierarchy relationship defined in the `Source -> Runtime` mapping must exist exactly in the runtime skeletal mesh.
2.  **Bone Axis Audit**: The local rotation axes of the runtime joints must match the source-reference axes within `+/- 0.5 deg`.
3.  **Segment Length Audit**: The distance between parent and child joints in the runtime skeleton must be within `+/- 5.0%` of the audited Manny/Quinn segments (e.g., Femur length, Tibia length).
4.  **GUID/Hash Match**: The skeleton's bone hierarchy and names must hash to a value that matches the declared "Audited Baseline ID" in the `V0_Plant_Baseline.json`.

### Operational Rules

- **Admissibility Point**: This audit must be performed at **Activation Start**. If it fails, the bridge must never enter `BalanceActivation_Ready`.
- **Failure Surface**: A mapping failure must emit `activation_physics_asset_contract_violation` with a secondary diagnostic field `skeleton_mapping_error_details` explaining the specific axis or length breach.
- **Variant Policy**: Any variant (e.g., a "buff" or "skinny" Manny) is only admissible if it stays within the `5%` segment-length and `0.5 deg` axis-alignment tolerances. If a variant exceeds these, it requires a dedicated, newly-audited standing-reference asset and a new baseline entry.

Weak assumption rejected:
- "If the skeleton uses the same bone names as Manny, the reference pose will project correctly." (Small axis differences in the skeleton can lead to significant physical instability when the reference is projected into control space).

## Required Continuity Snapshot

## Required Mesh-Wide Side-Effect Rule

The implementation must not model ownership as if Physics Control and body modifiers are purely local.

For `V0`:

- body-set ownership defines the truth boundary for standing evaluation
- mesh-wide effect tracking defines whether nominally local writes leaked into whole-skeletal-mesh behavior
- any path that changes skeletal-mesh-wide physics blending, `bUpdateMeshWhenKinematic`, deferred mesh following behavior, or equivalent whole-mesh state must be surfaced explicitly in diagnostics
- non-critical and excluded bodies remain in the mesh-wide contamination surface even when they are outside the primary truth sets

Truth rule:

- a run is not truth-clean merely because the balance-critical chain and support set kept their declared writers
- if mesh-wide side effects materially stabilize, drag, contain, or reposition those sets, the runtime must report an authority conflict and fail truthfully
- if the stabilizing path is routed indirectly through non-critical bodies, excluded bodies, shared mesh settings, or shared body-modifier state, the runtime must still fail truthfully rather than treating it as outside the contract

Weak assumption rejected:

- "Owning only the balance-critical chain and support set makes the rest of the mesh inert enough."

## Required Engine Execution Contract

The implementation must follow the authoritative execution-order and data-freshness contract defined in:

- [engine_execution_contract.md](engine_execution_contract.md)

This document defines the hard requirements for tick groups (`TG_PrePhysics`), pre-physics authority publication, substep truth resolution, and the post-simulation sampling point. Any implementation that deviates from this order is a contract violation.

## Required Movement-Component Non-Interference Rule

For `V0`, "movement component inert" means a concrete do-not-own and do-not-call contract, not merely "do not add velocity."

The following movement-component surfaces must not write, correct, reposition, or indirectly drag the balance-critical chain or support set during an active `V0` attempt:

- floor finding and floor adjustment paths
- based-movement updates and base-relative correction paths
- regular movement integration and velocity-driven capsule motion
- post-physics correction paths
- deferred skeletal-mesh movement or mesh-follow updates
- mesh smoothing (client-side interpolation)
- root-motion extraction, accumulation, or application paths
- network smoothing, prediction correction, or replay correction paths
- penetration resolution and depenetration paths owned by the movement component
- movement-mode transitions that reassert capsule or mesh authority
- any helper path that teleports, snaps, or reanchors the actor or mesh for movement correctness

Interpretation rules:

- disabling only one of these surfaces is not sufficient if another movement path still owns actor, capsule, or mesh correction
- if any movement-component-owned path changes actor transform, capsule transform, skeletal-mesh relative transform, or base-relative motion in a way that materially affects the balance-critical chain or support set, the run must report `activation_movement_reclaim`
- movement intent may still exist as data, but it may not become a live transform-authority surface during `V0`

## Required Capsule Contract Implementation

The character capsule (`UCapsuleComponent`) must be explicitly managed by `UPhysAnimComponent` during activation to ensure the standing proof is physically honest and non-contaminated by engine-side character logic.

For `V0`, the implementation must:

- **Root Component Role**: The capsule remains the **Root Component** and the **UpdatedComponent** for the actor to maintain engine compatibility, but it is treated as **authoritative but frozen**.
- **Disable Collision**: On activation entry, set the capsule's collision enabled state to `NoCollision`. It must not generate `WorldStatic` or `WorldDynamic` contacts. Any collision response against the character's own simulating bodies must also be disabled.
- **Disable Gravity**: `EnableGravity` must be set to `false` for the capsule component.
- **Freeze Transform**: The capsule world-space transform must be locked to the rebase origin/yaw captured at blend start. The implementation must not call `SetActorLocation`, `AddMovementInput`, or similar transform-modifying functions during the attempt.
- **Root Independence**: The skeletal mesh's root body must be simulating (`FPhysicsBodyInstance::SetInstanceSimulatePhysics(true)`). Its motion must be determined purely by the physics solver and Physics Control, not by kinematic attachment to the frozen capsule.

If any other system (e.g., CharacterMovement, Animation Blueprint, or Sequencer) attempts to re-enable capsule collision, apply velocity to the capsule, or move the capsule transform during the attempt, it must be reported as an `activation_capsule_contract_violation` and the run must terminate immediately.

Weak assumption rejected:
- "The capsule can shadow-follow the pelvis as long as it doesn't collide." (Shadowing still risks injecting forces via the mesh attachment or character-movement logic).

## Required Ownership Rule

Under the target Stage 1 design, the implementation must treat the balance-critical chain as continuously simulated through activation.

For `V0`, that means:

- raw body-instance validity must remain true for every truth-set body on every truth-sensitive sample
- raw simulation state must remain `true` for every truth-set body on every truth-sensitive sample
- truth-set body membership must remain stable for the whole attempt
- truth-set body recreation or replacement during the attempt is forbidden
- kinematic override or equivalent non-simulated movement type on a truth-set body is forbidden
- raw body-instance validity and raw simulation state outrank modifier or ownership bookkeeping
- bookkeeping disagreement is diagnostic unless raw continuity is also broken
- pelvis sleep is allowed during `BalanceActivation_Ready` only as part of quiet-state proof
- once `BalanceActivation_BlendIn` begins, a persistently sleeping pelvis is not admissible as "continuous simulation"
- during `BalanceActivation_Validate` and `BalanceActive_Standing`, support-set bodies may sleep only while raw body-instance validity, raw simulation state, truth-set membership, and accepted support truth all remain intact

The implementation must not silently change that contract under the guise of runtime tuning or diagnostics work.

If balance-critical ownership semantics change, the implementation spec and design docs must change in the same commit.

## Required Blend Rule

During `BalanceActivation_BlendIn`:

- controller authority must ramp gradually
- abrupt activation of full authority is not the intended path
- the default rollout scheduler is `ControlAuthorityAlpha`
- the default blend duration is `0.75` seconds
- the alpha is global across the balance-critical chain and support set in `V0`
- support-set targets use the same alpha in `V0`
- target source is the authored `V0` standing reference pose asset for the active Manny/Quinn-derived runtime skeleton
- the standing reference is authored parent-local pose data plus zero target velocities, not shell state, locomotion state, or a live sampled pose
- the runtime must compute one rebase frame from live `pelvis` position, gravity-up, and projected live `pelvis` yaw at blend entry
- the runtime must construct one gravity-aligned world/control frame from that captured gravity-up and projected pelvis yaw before projecting body targets
- the rebase frame remaps authored reference targets into runtime world placement; it does not mutate the authored parent-local pose values
- that rebased frame is frozen for the rest of the attempt
- the runtime must not perform per-body fitting or repeated rebasing after blend start
- the balance-critical chain, support set, and upper body all use that same authored stance source in `V0`; no per-set derived stance variants are allowed
- target-history initialization is rebuilt from the rebased standing reference at blend start
- pre-blend locomotion, shell, or legacy transition target history must not survive into the rebased activation history
- the runtime must treat the `V0` blend as a rollout of a fixed Physics Control primitive bundle, not as a single magical scalar
- the `V0` primitive bundle includes:
  - target orientation
  - target position
  - target angular velocity
  - target linear velocity
  - spring strength
  - damping
  - max torque
  - max force
- in `V0`, target position/orientation, target linear/angular velocity, spring strength, damping, max torque, and max force all roll out under the same global alpha
- in `V0`, parent/child dominance, control-point offsets, and target-space transforms are fixed per attempt and must not be alpha-ramped
- control-point offsets are applied only as fixed active Physics Control configuration after standing-reference projection; they are not authored by the standing-reference asset and may not be changed to rescue stance compatibility inside an attempt
- target history is rebased once on blend entry, including resetting target-velocity history to the standing reference zero-velocity contract
- target discontinuity greater than `15.0` degrees on the balance-critical chain is terminal
- pose/reference mismatch is measured as the shortest-arc quaternion orientation error between live body orientation and rebased target orientation in the same runtime world/control frame
- RMS mismatch is the unweighted root-mean-square of per-body mismatch across the balance-critical chain
- `activation_pose_reference_mismatch` is terminal when any balance-critical-chain body exceeds `25.0 deg` mismatch for more than `100 ms` or RMS balance-critical-chain mismatch exceeds `15.0 deg` for more than `100 ms`
- diagnostics may record blend instability, but may not reclassify that instability as success

Interpretation rule:

- `ControlAuthorityAlpha` is only the rollout coordinate for the `V0` primitive bundle
- if later work needs separate schedules for different primitives, that is a real contract change rather than hidden tuning

`BalanceActivation_Ready` must not exit until:

- the standing reference is present for every driven `V0` body
- the one-time rebase frame is valid
- the gravity-aligned control-space projection is valid for every driven `V0` body
- rebased targets have been materialized from that source
- the readiness quiet-state gate has passed

## Required Shell Rule

The runtime must distinguish:

- shell bookkeeping (`locked`, `reanchored`, `reseeded`, or equivalent)
- shell influence on the balance-critical chain

The presence of shell bookkeeping is not itself a failure.

Material shell influence on the balance-critical chain during activation is a failure.

For `V0`:

- shell helper use on the balance-critical chain or support set is forbidden
- any such use must emit a helper-used event and fail the run

## Required Standing-Validation Behavior

`BalanceActivation_Validate` may begin only after the bridge is in a physically ready state, the controller blend has reached its required activation range, and support truth remains valid.

Standing validation must:

- require contiguous readiness for the configured hold duration
- reset its hold timer on non-ready frames
- end truthfully on the first terminal failure
- preserve the first truthful leaf-level terminal reason in emitted artifacts

Default `V0` standing thresholds:

- root tilt envelope must remain at or below `20.0` degrees
- peak angular speed for the balance-critical chain must remain at or below `720.0 deg/s`
- peak angular speed for the support set must remain at or below `720.0 deg/s`

## Smoke-Test Evaluation Rule

The automation smoke must evaluate the final state using the balance-activation terminal state, not the generic bridge-running state.

Passing outcome:

- `BalanceActive_Standing`

Failing outcomes:

- `BridgeActive`
- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Recovery`
- `SafeDenied`
- unresolved activation state
- ambiguous failure state

## Documentation Alignment Rule

No implementation detail is allowed that requires a hidden balance-activation rule absent from the current design or spec docs.

If implementation adds or removes a real:

- ownership rule
- blend rule
- standing-validation rule
- diagnostic truth source
- terminal reason

the design and spec docs must be updated in the same change.

## Acceptance Criteria

This implementation spec is satisfied only when:

- balance activation is implemented as a distinct state path
- the balance-critical chain is explicit in runtime data
- a dedicated post-update truth snapshot exists
- ownership, bookkeeping, and shell influence remain distinct
- controller blend is explicit
- standing validation is explicit
- smoke evaluation uses terminal balance outcomes
- truthful safe deny is not a passing smoke result
