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

For `V0`, that validation must at minimum verify:

- active skeletal mesh and physics asset identity match the declared audited Manny/Quinn-derived baseline
- active authored constraint-profile set matches the declared `V0` standing baseline
- active physical-material set and collision-disable table match the declared `V0` baseline
- the exact authored standing-reference asset and its gravity-aligned control-space projection are part of that audited baseline
- total body mass remains within `+/- 5%` of the audited `V0` baseline
- family mass totals for the balance-critical chain and support set remain within `+/- 10%` of the audited baseline
- principal inertia components for truth-set bodies remain within `+/- 15%` of the audited baseline
- upper-body collision is disabled for `V0` acceptance
- support-set collision geometry passed the authored support-geometry audit for flat-ground standing

Interpretation rules:

- this is a plant contract, not a controller-quality heuristic
- the authored standing-reference asset is not assumed physically compatible by default; compatibility exists only when it is admitted by the audited plant/control baseline
- if the plant contract fails, the run must deny or fail as `activation_physics_asset_contract_violation`
- runtime tuning or control diagnostics may not relabel a plant-contract violation as controller weakness
- physics-asset swaps, runtime mass edits, constraint-profile swaps, collision-profile edits, or other plant mutations during an active attempt are forbidden

## Required Continuity Snapshot

Balance activation must use a dedicated authoritative post-update snapshot for truth-sensitive decisions.

That snapshot must at minimum be able to report:

- raw body-instance validity for every body in the active truth sets
- raw simulation state for the balance-critical chain
- raw simulation state for the support set
- truth-set membership intact/not-intact state
- truth-set body recreation or replacement events
- raw awake/sleep state for the pelvis and support-set bodies
- bookkeeping-versus-raw continuity disagreement state
- mesh-level physics blend and update flags that could change how the skeletal mesh follows simulated or kinematic bodies
- worst-body linear and angular stability metrics
- controller-authority alpha / blend progress
- shell offset and velocity deltas
- shell influence materiality
- locomotion or reset authority contamination
- standing-validation accumulated hold time

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

## Required Engine Update Order

`V0` balance activation must use one explicit frame-order contract.

Required per-frame order:

1. animation evaluation: `USkeletalMeshComponent` updates the authored standing-reference pose inputs and any required bone-space state for the active frame
2. Physics Control cached-pose update: any Physics Control pose capture, cached target-pose refresh, or equivalent skeletal-pose read used to derive current-frame control inputs must occur after skeletal-mesh evaluation and before any activation ownership/truth decision for that frame
3. `UPhysAnimComponent` tick: runs in `TG_PrePhysics`, after skeletal-mesh evaluation and after the required Physics Control cached-pose update, and owns activation-state updates, standing-reference rebasing, quiet-state gating, observations, and bridge-side control decisions
4. `UPhysicsControlComponent` application point: Physics Control target writes and body-modifier writes issued by `UPhysAnimComponent` must be applied in the same pre-physics window before Chaos simulation for that frame; no later component may overwrite them on the balance-critical chain or support set
5. CharacterMovement regular tick: during `V0` activation it may exist, but it must be inert with respect to the balance-critical chain and support set; no movement-component transform, floor-correction, based-movement, or deferred mesh write may alter those bodies
6. Chaos simulation: physics substeps consume the already-published Physics Control and body-modifier state for the frame
7. substep truth accumulation: during Chaos simulation, the runtime accumulates support truth, continuity truth, churn events, timer advancement, and other terminal-condition evidence at substep resolution
8. instrumentation sample: the truth-sensitive activation snapshot is taken once per frame immediately after the final Chaos substep and before any post-physics movement correction, deferred mesh movement, or recovery/termination cleanup writes
9. CharacterMovement post-physics behavior: post-physics based movement, deferred mesh movement, or similar late corrections must remain inert during `V0` activation; any such write is an authority conflict and, if it touches the balance-critical chain or support set, an `activation_movement_reclaim`

Interpretation rules:

- if the skeletal mesh has not evaluated for the frame yet, `UPhysAnimComponent` must not publish new standing-reference targets for that frame
- if the Physics Control cached pose or equivalent source pose is stale relative to the current skeletal-mesh evaluation, ownership and truth decisions for that frame are not admissible
- ownership and truth for a frame must be evaluated against the post-Chaos raw state produced from the same frame's skeletal-mesh evaluation, Physics Control cached pose, target writes, and body-modifier writes
- if Physics Control or body-modifier writes land after Chaos has already simulated the frame, they count for the next frame and must not be treated as current-frame truth
- terminal truth must be accumulated at Chaos substep resolution before the post-update snapshot is emitted
- the post-update activation snapshot is the authoritative per-frame publication surface for state advancement and artifacts, but it must reflect the already-accumulated substep truth for that frame

## Required Movement-Component Non-Interference Rule

For `V0`, "movement component inert" means a concrete do-not-own and do-not-call contract, not merely "do not add velocity."

The following movement-component surfaces must not write, correct, reposition, or indirectly drag the balance-critical chain or support set during an active `V0` attempt:

- floor finding and floor adjustment paths
- based-movement updates and base-relative correction paths
- regular movement integration and velocity-driven capsule motion
- post-physics correction paths
- deferred skeletal-mesh movement or mesh-follow updates
- root-motion extraction, accumulation, or application paths
- network smoothing, prediction correction, or replay correction paths
- penetration resolution and depenetration paths owned by the movement component
- movement-mode transitions that reassert capsule or mesh authority
- any helper path that teleports, snaps, or reanchors the actor or mesh for movement correctness

Interpretation rules:

- disabling only one of these surfaces is not sufficient if another movement path still owns actor, capsule, or mesh correction
- if any movement-component-owned path changes actor transform, capsule transform, skeletal-mesh relative transform, or base-relative motion in a way that materially affects the balance-critical chain or support set, the run must report `activation_movement_reclaim`
- movement intent may still exist as data, but it may not become a live transform-authority surface during `V0`

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
