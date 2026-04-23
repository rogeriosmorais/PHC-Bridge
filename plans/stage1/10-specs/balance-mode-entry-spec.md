# Balance Mode Entry Spec

## Purpose

This document defines the authoritative Stage 1 balance-activation contract.

This document is now a contract wrapper around the continuous-balance rewrite docs, not the primary place to center the architecture on replacement phases.

It exists to separate:

- normal bridge startup behavior
- balance activation behavior
- continuous physical ownership rules
- controller blend rules
- standing-validation rules
- the still-open physical-viability question

## Core Interpretation

Balance activation is a distinct runtime contract layered on top of a running bridge.

The target design is not a flip-based `Prepare -> LateValidate -> RootOn -> Settle` ritual.

That means the old transition-state-machine assumption is being replaced, not merely relaxed.

The target design is:

1. `BalanceActivation_Ready`
2. `BalanceActivation_BlendIn`
3. `BalanceActivation_Validate`
4. `BalanceActive_Standing`
5. `SafeDenied` or `Failed`

Success is only:

- reaching `BalanceActive_Standing`
- and holding it continuously for `3.0` seconds

Truthful safe deny remains a terminal failure outcome, not a success outcome.

The design intent is to prove the controller can stand on its own in continuous physics, not to prove a protected handoff moment was safe.

Primary architecture and truth sources live in:

- `continuous_balance_architecture.md`
- `continuous_balance_truth_model.md`
- `authority_matrix.md`
- `instrumentation_and_acceptance.md`

Use this file to tie those documents into the broader Stage 1 contract.

## Balance-Critical Chain

The default Stage 1 balance-critical chain is:

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

Interpretation rules:

- this chain must stay continuously simulated through activation
- no temporary kinematic re-ownership of this chain is part of the target activation contract
- changes to this chain must be documented as real contract changes, not tuning tweaks
- "continuously simulated" uses the raw-physics continuity contract defined in `continuous_balance_truth_model.md`, not a loose bookkeeping label
- in `V0`, that means valid raw body instances, raw simulate-physics enabled, stable truth-set membership, no body recreation/replacement, raw state winning over bookkeeping, and the documented pelvis/support-set sleep rules

## Canonical Activation Flow

These runtime labels are operational states, not the primary architecture. The primary architecture is defined by continuous-physics invariants.

### `BalanceActivation_Ready`

The bridge is alive and the balance-critical chain is already physically owned.

Required properties:

- raw body state confirms continuous simulation of the balance-critical chain
- raw body state confirms continuous simulation of the support set in `V0`
- no pending topology flip is required to begin balance activation
- shell bookkeeping and shell influence remain separate observables
- shell assistance on the balance-critical chain or support set is disabled in `V0`

### `BalanceActivation_BlendIn`

The controller ramps authority onto the already-physical balance-critical chain.

Required properties:

- controller authority rises gradually
- policy/control writes are blended rather than abruptly asserted
- no diagnostic or grace rule may reinterpret a destabilizing blend as success

### `BalanceActivation_Validate`

The runtime validates sustained physical standing after blend-in.

Required properties:

- standing readiness must be contiguous for the required hold duration
- non-ready frames reset the hold timer
- diagnostics may classify the first truthful failure but may not repair the state into success on the same frame

### `BalanceActive_Standing`

This is the only current passing publication state for balance activation.

## Runtime Mode Contract

| Runtime mode | Entry preconditions | Exit conditions | Fail conditions | Forbidden writes | Authoritative owner | Required emitted metrics |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BalanceActivation_Ready` | valid bridge context; physics-asset contract satisfied; balance-critical chain and support set continuously simulated; movement component idle; no pending reset | enter `BalanceActivation_BlendIn` once rebased targets are valid and quiet-state proof passes | physics-asset contract violation, topology change on critical chain, loss of continuous simulation, shell helper use on critical/support set, movement reclaim | locomotion-drive writes, shell helper writes, kinematic mode writes on critical/support set, plant-profile swaps during attempt | balance activation runtime | topology change count, authority conflict count, shell helper used count, quiet-state duration, standing-reference id, rebase frame, physics-asset baseline id, constraint-profile id |
| `BalanceActivation_BlendIn` | `Ready` satisfied; rebased targets exist; `ControlAuthorityAlpha=0.0` | enter `BalanceActivation_Validate` when `ControlAuthorityAlpha=1.0` and no fail condition fired | physics-asset contract violation, target discontinuity, unstable gains/damping, support failure, proxy outside support region, pose/reference mismatch, authority conflict, movement reclaim, shell-helper violation | abrupt full-authority writes, shell helper writes, movement-component writes, reset writes, plant-profile swaps during attempt | balance activation runtime | alpha, blend duration, target discontinuity, controller effort proxy, authority conflicts, support uptime, standing-reference id, terminal-reason detail |
| `BalanceActivation_Validate` | blend complete; support truth valid; no prior fail | enter `BalanceActive_Standing` after contiguous hold completes | physics-asset contract violation, support failure, proxy outside support region, instability threshold breach, topology change, authority conflict, non-contiguous hold, shell-helper violation, movement reclaim | shell helper writes, movement-component writes, topology edits on critical chain, reset writes, plant-profile swaps during attempt | balance activation runtime | contiguous hold time, root tilt envelope, peak angular speed by family, contact uptime, COM/support proxy drift, standing-reference id, terminal-reason detail |
| `BalanceActive_Standing` | contiguous hold complete | remain active or enter recovery/termination | loss of standing validity or explicit recovery trigger | legacy activation writes that bypass standing-mode ownership | balance mode runtime | sustained hold time, ongoing stability metrics |

Compatibility note:

- legacy `BridgeActive_Physical` maps to `BalanceActivation_Ready`
- legacy `BalanceActivation_StandingValidation` maps to `BalanceActivation_Validate`

Recovery note:

- `BalanceActive_Recovery`, `SafeDenied`, and `Failed` are named architectural states
- detailed recovery behavior is out of the current rewrite scope
- `BalanceActive_Recovery` is non-authoritative for `V0` acceptance
- recovery behavior must not be used to satisfy standing success
- implementers must not improvise new standing-success logic inside those paths

## Authority And Diagnostics

The runtime must keep these observables distinct:

1. intended continuous ownership
2. raw body simulation state
3. modifier-record or control-layer ownership bookkeeping
4. mesh-wide physics/update side-effect state
5. controller-authority alpha / blend progress
6. shell bookkeeping state
7. shell influence materiality
8. locomotion or reset authority state

Interpretation rules:

- intended ownership is not proof of applied ownership
- bookkeeping is not proof of raw physical continuity
- truth-set ownership is not proof that whole-mesh side effects stayed inert
- shell bookkeeping is not proof of shell influence
- diagnostics are observability only
- non-critical or excluded bodies are not outside falsification scope if they materially alter the standing outcome of the truth sets

The runtime must also distinguish:

- ownership-continuity problems
- controller-strength and control-shaping problems
- mesh-wide side-effect contamination from Physics Control or body-modifier paths
- contamination routed indirectly through non-critical bodies, excluded bodies, or skeletal-mesh-wide update settings
- hidden authority conflicts between policy, Physics Control, locomotion authority, and startup logic
- movement-component reclaim through floor finding, based movement, root motion, post-physics correction, deferred mesh movement, network correction, or depenetration paths

Primary truth precedence is defined in `continuous_balance_truth_model.md` and must be followed exactly.

Frame-order rule for `V0`:

- ownership and truth are evaluated only after current-frame skeletal-mesh evaluation, Physics Control cached-pose update, control-target publication, and body-modifier writes have all happened in the documented pre-physics order, and after Chaos has produced the corresponding post-step raw state
- current-frame truth may not be inferred from stale cached pose, pre-write intent, or pre-physics bookkeeping alone

Movement-component non-interference rule for `V0`:

- "movement component idle" means the movement path must not own floor finding, based movement, regular movement integration, post-physics correction, deferred mesh movement, mesh smoothing (client-side interpolation), root-motion application, network correction, or depenetration for the active attempt
- any such path that materially changes actor, capsule, or mesh motion relative to the balance-critical chain or support set is `activation_movement_reclaim`

## Measurement-Only Rule

The activation contract explicitly forbids using diagnostics to manufacture a pass.

Therefore:

- grace windows may explain classification, but not convert instability into success
- transitional mismatch may be logged, but not treated as proof that activation worked
- safe deny remains preferred over dishonest activation
- phase completion by itself is not a valid success metric

## Contract Correctness Vs Physical Viability

### Contract correctness

Balance activation is contract-correct when:

- continuous physical ownership is evaluated truthfully
- controller blend is evaluated truthfully
- standing-validation timing is evaluated truthfully
- failure reasons are explicit and truthful

### Physical viability

Balance activation is physically viable only if the live physical state can:

- remain continuously simulated on the balance-critical chain
- tolerate the controller blend under current gains, damping, target representation, action scaling, latency, and pose continuity
- remain upright enough to satisfy the standing-validation window

A run may satisfy contract correctness and still fail physical viability.

Removing the old flip-based ritual will often expose controller weakness more directly. That should be treated as more honest evidence, not as a reason to restore protective transition logic.

## Standing Reference Contract

`V0` uses one exact standing-reference source for all fixed-stance targets:

- source asset: the versioned authored idle-standing pose asset for the active Manny/Quinn-derived runtime skeleton
- authored-versus-live rule: the reference is authored data only; live sampled poses may be compared diagnostically but may not become the standing reference
- pose space: parent-local rotations for all `V0` driven bodies in authored skeleton-reference space
- translation rule: no authored per-body translation offsets are consumed in `V0`; only the rebased pelvis/world anchor places the reference in world space
- target velocities: zero desired angular and linear velocity in the reference
- set-variant rule: the balance-critical chain, support set, and upper body all use that same authored stance source in `V0`; there are no per-set derived stance variants
- forbidden sources: shell state, locomotion intent, prior phase state, and ad hoc helper corrections

Rebasing contract:

- capture once at the `BalanceActivation_Ready -> BalanceActivation_BlendIn` boundary
- use live `pelvis` world position as the rebase origin
- use gravity-up as the rebase up axis
- use live `pelvis` yaw projected around gravity-up as the rebase yaw
- rebase affects only target publication and target-history initialization for the activation attempt
- authored parent-local pose values are not modified by rebasing
- target position frame, target orientation frame, and zero-velocity target frame are all rebuilt from that one rebase capture
- pre-blend locomotion, shell, or legacy handoff target history must not survive into the rebased standing-reference history
- freeze that rebased frame for the rest of the activation attempt
- do not run per-body fitting or repeated rebasing after blend start

Projection-to-control-space contract:

- under arbitrary gravity-up, the runtime constructs one gravity-aligned world frame from the captured gravity-up axis and projected pelvis yaw
- each driven body's runtime target orientation is then produced by composing:
  - the authored parent-local rotation
  - the resolved runtime skeleton hierarchy
  - the one-time rebased pelvis/world frame
- control-point offsets are not authored by the stance asset and are not solved from live pose
- in `V0`, control-point offsets, target spaces, and parent dominance come from the fixed active Physics Control setup and are applied after standing-reference projection
- changing offsets, target spaces, or parent dominance to rescue the stance is a real contract change, not tuning inside `V0`

Reference mismatch contract:

- `activation_pose_reference_mismatch` is reserved for disagreement between the live achieved pose and the rebased authored standing reference, not for support loss or controller-effort failure
- mismatch is computed on the balance-critical chain and support set only
- per-body mismatch is the shortest-arc quaternion angle in degrees between live body orientation and the rebased target orientation for that same body
- live body orientation and target orientation must be compared in the same runtime world/control frame after the one-time rebase and fixed control-space projection are applied
- RMS mismatch is the unweighted root-mean-square of the per-body mismatch angles across the balance-critical chain
- the run publishes per-body mismatch detail, max-body mismatch, and the RMS mismatch scalar
- `V0` treats reference mismatch as terminal when either:
  - any balance-critical-chain body exceeds `25.0 deg` mismatch for longer than `100 ms`, or
  - the RMS mismatch across the balance-critical chain exceeds `15.0 deg` for longer than `100 ms`

`BalanceActivation_Ready` may exit only after:

- the authored standing reference exists for every driven `V0` body
- the one-time rebase frame has been computed
- the gravity-aligned control-space projection is valid for every driven `V0` body
- rebased targets have been materialized from that source
- the quiet-state proof has passed on live physical state

## Physics Asset Prerequisites

`V0` activation is admissible only on the audited physical plant defined in `continuous_balance_architecture.md`.

Required preconditions before `BalanceActivation_Ready`:

- the active physics asset, authored constraint profile set, physical-material set, and collision-disable table match the declared `V0` baseline
- total mass, truth-set family mass totals, and truth-set principal inertias remain within the documented `V0` tolerances
- upper-body collision is disabled for `V0` acceptance
- support-set collision geometry has passed the authored support-geometry audit
- the exact authored standing-reference asset and its projected control-space mapping are part of the audited `V0` baseline
- no runtime plant-profile swap, ad hoc mass edit, or hidden constraint retune is pending for the activation attempt

Interpretation rule:

- if these prerequisites are not satisfied, or the authored stance cannot be projected through the audited control configuration cleanly, the run must fail or deny as a plant/reference-contract problem before being classified as controller weakness

## `V0` Standing Thresholds

`BalanceActivation_Validate` and `BalanceActive_Standing` use these default physical thresholds in addition to the support-truth thresholds defined elsewhere:

- maximum root tilt envelope: `20.0 deg`
- maximum peak angular speed for the balance-critical chain: `720.0 deg/s`
- maximum peak angular speed for the support set: `720.0 deg/s`

An instability threshold breach means any of those limits is exceeded during the active validation or standing window.

## Blend-In Contract

`BalanceActivation_BlendIn` uses one explicit primitive-bundle rollout, not a claim that Physics Control is inherently a single-alpha system.

`ControlAuthorityAlpha` is the `V0` rollout scheduler for a fixed bundle of Physics Control primitives:

- target orientation
- target position
- target angular velocity
- target linear velocity
- spring strength
- damping
- max torque
- max force

These primitives remain fixed during `V0` blend start and are not independently retuned within an attempt:

- parent/child dominance stays fixed for the whole attempt
- control-point offsets stay fixed for the whole attempt
- target-space transforms stay fixed for the whole attempt

Weak assumption explicitly rejected:

- "A single global alpha over support plus proximal chain is the right activation primitive."

`V0` therefore defines one global alpha only as the contract for how that primitive bundle is rolled out:

- blended quantity: `ControlAuthorityAlpha`
- alpha range: `0.0 -> 1.0`
- default duration: `0.75` seconds
- alpha scope: one global alpha for the balance-critical chain and support set in `V0`
- support-set targets: included in the same blend contract in `V0`
- target source during blend: the authored `V0` standing reference expressed in the one rebased frame captured at blend start
- target position/orientation rollout: interpolated by the same global alpha in `V0`
- target linear/angular velocity rollout: interpolated by the same global alpha in `V0`
- spring strength rollout: scaled by the same global alpha in `V0`
- damping rollout: scaled by the same global alpha in `V0`
- max force/max torque rollout: scaled by the same global alpha in `V0`
- parent/child dominance: fixed during the attempt; not alpha-ramped in `V0`
- control-point offsets: fixed during the attempt; not alpha-ramped in `V0`
- target-space transforms: fixed during the attempt; not alpha-ramped in `V0`
- gating: time-based after `BalanceActivation_Ready` entry because `Ready` already owns the physical quietness proof; if a fail condition appears, the mode fails rather than pausing alpha
- history rebasing: one-time rebase on entry to `BalanceActivation_BlendIn`
- target discontinuity check: fail if `target_discontinuity_deg > 15.0` on the balance-critical chain during blend start

Canonical leaf-level activation terminal reasons:

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

## Failure Boundary

Use the following boundary for all balance-activation stages:

| Outcome class | Meaning | Examples |
| :--- | :--- | :--- |
| `Diagnostic mismatch` | truthfully logged disagreement that is not itself proof of success or the deciding failure | modifier/raw disagreement while raw continuity still holds |
| `Non-admissible state` | activation cannot proceed yet because required conditions are not satisfied | bridge not yet physically ready, blend preconditions not met |
| `Terminal failure` | the attempt has been falsified and must end truthfully | balance-critical ownership lost, blend causes instability, shell influence becomes material, standing validation fails or times out |

## Legacy Mapping

Legacy filenames and old code symbols may still refer to `Phase1`, `Phase2`, `Phase3`, `LateValidate`, `RootOn`, or `Settle`.

Those names are compatibility labels only.

For design intent:

- old `Prepare` / `LateValidate` map to physical-readiness checks before or at `BalanceActivation_Ready`
- old `RootOn` maps to a superseded ownership-flip concept and is not the target design
- old `Settle` maps most closely to `BalanceActivation_Validate`

No authoritative document may present the legacy phase sequence as the intended activation mechanism.

The new truth model must also avoid silently depending on shell-maintained containment that used to live inside the old readiness and continuity checks.

## Required Terminal Truthfulness

When activation fails, the deny or failure path should identify that explicitly rather than collapsing everything to a generic label.

At minimum this includes distinguishing:

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

Broad families may still be used for dashboards or aggregation, but `terminal_reason` on the emitted run artifact must carry the leaf-level reason rather than a bundled umbrella label.

## Acceptance Criteria

This spec is satisfied only when:

- the continuous-balance architecture, truth model, authority matrix, and instrumentation docs are treated as primary references
- the balance-critical chain is explicit
- the target activation flow is explicit
- continuous physical ownership is defined as the target design
- controller authority is defined as a gradual blend
- diagnostics are explicitly observational only
- the docs explicitly allow a contract-correct but physically non-viable result
- the docs explicitly define success as `BalanceActive_Standing` held for `3.0` continuous seconds
