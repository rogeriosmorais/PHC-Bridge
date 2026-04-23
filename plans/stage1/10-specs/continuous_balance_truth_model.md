# Truth Model For Continuous Balance

## Purpose

This document replaces the handoff-centered truth model for the new architecture.

Core rule:

- success is sustained physical stability under continuous simulation
- no phase completion, shell status, or compatibility label can substitute for that

## Primary Physical Signal Families

This list names the physical signal families used by the truth model.

It is not a precedence order.

The primary physical signal families for continuous balance are:

1. raw body continuity
2. contact persistence and support truth
3. root pose and root tilt
4. COM behavior or support proxy
5. body angular and linear stability
6. sustained hold duration

Shell metrics may explain failure, but cannot certify balance.

## COM / Support Proxy Contract

The continuous-balance rewrite uses one explicit support proxy for `V0`.

### Bodies contributing

Use the world-space positions of:

- `pelvis`
- `thigh_l`
- `thigh_r`
- `spine_01`

to build the proximal support proxy.

### Measurement frame

Measure the proxy in world frame projected onto the ground plane.

### Proxy definition

The `V0` support proxy is:

- the planar centroid of the contributing bodies

If a later full COM computation becomes trustworthy, it may replace this proxy only in a documented contract change.

### Why this proxy is acceptable in `V0`

This proxy is preferred over full-body COM in `V0` because:

- the rewrite is intentionally centered on the proximal standing problem first
- the contributing bodies are the architectural center of the `V0` balance-critical chain
- full-body COM would be more sensitive to distal and upper-body behavior that `V0` explicitly does not want to optimize yet
- the planar centroid is cheaper to compute, easier to audit, and easier to compare across runs during the first rewrite milestone

The proxy is therefore not claiming to be the physically perfect COM. It is a deliberately narrower measure chosen to keep the first rewrite honest about proximal standing without prematurely reintroducing full-body complexity.

### Support region definition

The `V0` support region is the convex hull of the currently supporting `foot_*` and `ball_*` contact points.

Support-hull construction rule for `V0`:

- source contacts come only from accepted Chaos manifold contacts
- accepted contacts must be between a support-set body and walkable world support
- walkable world support means non-character `WorldStatic` level geometry only
- the accepted contact normal must be within `5.0 deg` of gravity-up
- contacts against `WorldDynamic`, simulated rigid bodies, moving platforms, other characters, or the character's own bodies are excluded before hull construction
- if multiple accepted manifold points exist for one support body in a qualifying substep, use the world-space point with greatest penetration depth for that body
- the support region for a sample is built from those reduced per-body points from the final qualifying Chaos substep of that frame

### Failure threshold

The run fails support-proxy truth if:

- the planar support proxy remains outside the support region for more than `100 ms`

### Known proxy limitations

Known false positives:

- upper-body or distal motion may move the real full-body COM unfavorably while the proximal proxy still looks acceptable
- asymmetric limb motion outside the contributing set may understate true whole-body lean risk

Known false negatives:

- transient proximal motion may push the proxy outside the support region even when the full-body COM would still remain acceptable
- local proximal oscillation can make the proxy noisier than a trusted full-body COM estimate

These limitations are acceptable in `V0` because support truth and raw contact persistence still remain primary gates. The proxy is an additional physical indicator, not a permission slip to ignore support failure.

### Precedence rule

If support truth fails, the run fails even when the support proxy or root tilt still look temporarily acceptable.

### Replacement criteria

Replace the `V0` support proxy with a fuller COM measure only when all are true:

- the full-body COM computation is derived from trustworthy raw physical state
- its contributing bodies and frame definition are explicit and stable across runs
- it improves failure explanation quality versus the proximal proxy on recorded artifacts
- it does not reintroduce distal or upper-body sophistication as a hidden prerequisite for honest `V0` standing evaluation

Until then, the proximal support proxy remains the contract measure for `V0`.

## Source-Of-Truth Precedence

Use this precedence order whenever observables disagree:

1. raw body continuity and raw contact state
2. derived physical metrics built from raw state
3. bookkeeping state
4. declared intent
5. shell state

Within derived physical metrics, use this conflict-resolution order:

1. support truth from accepted contact state
2. root pose and root tilt
3. COM/support-proxy behavior
4. body angular and linear stability
5. sustained hold duration

Interpretation rules:

- raw body continuity beats bookkeeping
- bookkeeping beats declared intent only for diagnostics, not success
- control-target publication is never proof of achieved pose
- shell state is never proof of standing
- movement-component non-interference is required, not inferred
- support truth beats acceptable-looking root tilt, COM/support-proxy behavior, and stability metrics when the support set is not actually sustaining the body
- contact persistence outranks the support proxy
- root tilt is derived physical evidence, not a substitute for valid support contact
- if root tilt is acceptable, the support proxy is acceptable, and contact fails, the run fails on support truth
- sustained hold duration counts only while higher-precedence truth sources remain valid; time alone never rescues an invalid support or continuity state

## Terminal Reason Arbitration

When multiple failure conditions are simultaneously active, exactly one leaf-level `terminal_reason` must be emitted.

### Master Terminal Reason Precedence Table

Use this table to decide the "winning" `terminal_reason` when multiple conditions are observed in the same or co-occurring truth-evaluation windows. **Rank 1 is the highest precedence.**

| Rank | Reason Class | Leaf-level `terminal_reason` | Arbitration Logic |
| :--- | :--- | :--- | :--- |
| **1** | **Plant Contract** | `activation_physics_asset_contract_violation` | Always wins; pre-empts all other classes. |
| **2** | **Raw Continuity** | `activation_topology_change` | Wins over `simulation_lost` if both occur. |
| **3** | **Raw Continuity** | `activation_continuous_simulation_lost` | Wins over support/controller classes. |
| **4** | **Support Truth** | `activation_support_failure` | Wins over `proxy_outside_region`. |
| **5** | **Support Truth** | `activation_proxy_outside_support_region` | Wins over controller stability classes. |
| **6** | **Controller Stability** | `activation_target_discontinuity` | Wins over gain/damping instability. |
| **7** | **Controller Stability** | `activation_unstable_gain_or_damping` | Wins over threshold breaches. |
| **8** | **Controller Stability** | `activation_instability_threshold_breach` | Wins over pose mismatch. |
| **9** | **Pose/Reference** | `activation_pose_reference_mismatch` | Wins over authority conflict. |
| **10** | **Authority/Ownership** | `activation_movement_reclaim` | Wins over generic authority conflict. |
| **11** | **Authority/Ownership** | `activation_shell_helper_violation` | Wins over generic authority conflict. |
| **12** | **Authority/Ownership** | `activation_authority_conflict` | Wins only over timeout. |
| **13** | **Time/Duration** | `activation_standing_validation_timeout` | Only emitted if no physical failure occurred. |

### Arbitration Rules

Use the following rules to apply the precedence table.

### Rule 1 — Plant contract violation pre-empts all other classification

If the active physics asset violates the `V0` plant contract, emit `activation_physics_asset_contract_violation` and stop.
No controller-class or support-class reason may be emitted instead.

### Rule 2 — Raw continuity vs modifier/bookkeeping disagreement

| Observable state | Classification |
| :--- | :--- |
| Modifier record disagrees with raw body state; raw `IsInstanceSimulatingPhysics` remains `true`; body instance is valid and in the expected truth set | `Diagnostic mismatch` only — log `continuity_bookkeeping_mismatch`; do not emit a terminal reason on this alone |
| Raw `IsInstanceSimulatingPhysics` is `false` or body instance is invalid; no explicit truth-set membership change | `activation_continuous_simulation_lost` |
| Truth-set membership has changed explicitly: a body has been added to or removed from the declared truth set, or a body instance has been recreated or replaced | `activation_topology_change` — takes precedence over `activation_continuous_simulation_lost` per Rank 2. |

"Explicitly" means: a body-instance pointer changed, a body was destroyed and rebuilt, or the runtime truth-set registry no longer contains a previously-registered body.

### Rule 3 — Pose/reference mismatch escalation

| Observable state | Classification |
| :--- | :--- |
| Per-body mismatch is below `25.0 deg` AND RMS is below `15.0 deg` | `Diagnostic mismatch` — log `reference_mismatch_*` fields; do not emit a terminal reason |
| Per-body mismatch exceeds `25.0 deg` on any balance-critical-chain body for longer than `100 ms`, OR RMS exceeds `15.0 deg` for longer than `100 ms` | `activation_pose_reference_mismatch` — terminal |
| The pose mismatch is caused by a prior authority conflict or topology change that already fired | Emit the higher-ranked reason; log reference mismatch as secondary context only |

### Rule 4 — Authority conflict vs support failure co-occurrence

Authority conflict is the emitted leaf reason only when it is the proximate cause of the terminal condition and rank-precedes the support failure.

| Co-occurrence scenario | Emitted reason |
| :--- | :--- |
| Support failure preceded or coincided with authority conflict; no evidence that the conflict caused the loss | `activation_support_failure` (Rank 4) — emit; log authority conflict events as secondary context |
| Authority conflict is the clear proximate cause: movement reclaim or competing locomotion write is observed first, and support failure follows within the same or next frame | `activation_authority_conflict` (Rank 12) or `activation_movement_reclaim` (Rank 10) only if the physical failure has not yet been logged in the substep stream. If a higher-ranked physical failure co-occurs, the physical failure wins. |

### Rule 5 — General tie-break when two terminal reasons fire simultaneously

When two terminal conditions are observed in the same truth-evaluation window:

1.  Consult the **Master Terminal Reason Precedence Table**.
2.  Emit the reason with the **highest Rank (lowest number)**.
3.  If two reasons have equal Rank or are in the same family, emit the one whose triggering condition was first observed in the substep-level truth stream.
4.  Record all co-occurring reasons in the secondary `co_terminal_reasons` array in the run artifact.

This ensures that different implementations follow the same deterministic path when arbitrating multiple simultaneous failures.

## Secondary Diagnostics

These may explain why the primary truth sources failed:

- shell bookkeeping state
- shell influence metrics
- modifier-record bookkeeping
- controller effort proxies
- authority conflict events (when not the proximate terminal cause)

None of these secondary diagnostics can certify success by themselves.
Authority conflict events are both secondary diagnostics and a valid leaf terminal reason; the distinction is whether the conflict is the proximate cause of termination or a contributing context.

## Continuous Simulation Contract

For `V0`, "continuously simulated" is a raw-physics continuity contract, not a vague synonym for "probably physical enough."

Continuous simulation requires all of the following for every body in the active truth sets:

- the raw body instance exists and is valid on every truth-sensitive sample
- raw simulation state remains `true` on every truth-sensitive sample
- the body remains in the expected truth set for the active attempt
- the runtime does not recreate or replace that truth-set body instance during the attempt
- no kinematic override or equivalent non-simulated movement type is applied to that body

Interpretation rules:

- "continuous simulation" in engine terms therefore means: valid raw body instance, raw simulate-physics true, stable truth-set membership, no body recreation/replacement, and no non-simulated override
- raw `IsInstanceSimulatingPhysics == true` or equivalent raw body-instance truth is the deciding source for simulation continuity
- raw body-instance validity beats modifier or ownership bookkeeping
- modifier or bookkeeping disagreement is diagnostic by itself, not automatic proof that continuity was lost
- sleeping is not automatically a continuity failure
- sleeping is allowed during `BalanceActivation_Ready` because quiet-state proof may include low-motion or sleeping bodies
- during `BalanceActivation_BlendIn`, `BalanceActivation_Validate`, and `BalanceActive_Standing`, the pelvis must remain awake enough to participate physically; a persistent sleeping pelvis is treated as continuity lost rather than "successfully simulated"
- during `BalanceActivation_Validate` and `BalanceActive_Standing`, support-set bodies may sleep only if their raw body instances remain valid, raw simulation remains enabled, truth-set membership stays intact, and accepted support/contact truth remains valid; sleep is therefore conditionally allowed for non-pelvis truth-set bodies, not categorically forbidden
- mesh-wide update settings may still contaminate participation truth and must be evaluated separately from raw simulation continuity

`activation_continuous_simulation_lost` is the required leaf reason when raw simulation continuity fails without a separate topology change being the primary cause.

`activation_topology_change` is the required leaf reason when truth-set membership or topology changes explicitly.

## Control-First Failure Taxonomy

Plant-contract failure must be separated before this taxonomy is applied.

Interpretation rule:

- if the active physics asset violates the documented `V0` plant contract, classify the run as a physics-asset contract failure before discussing controller weakness

Every failure in the new mode must be classified first as one of:

- target discontinuity
- gain or damping instability
- contact or support failure
- pose or reference mismatch
- authority conflict

Only after that may the runtime add secondary context such as shell influence or bookkeeping disagreement.

## Support Truth

Support truth is primary only because `V0` requires the support set to remain simulated.

Execution and Cadence Rule:

The truth evaluation cadence, frame execution order, and data-freshness requirements are defined in the authoritative:

- [engine_execution_contract.md](engine_execution_contract.md)

Interpretation:

- truth evaluation cadence is Chaos substep resolution, not artifact cadence
- the `30 Hz` artifact stream is a reporting/downsample layer only
- short support losses, churn events, and continuity violations must be detected from substep-level truth before they are summarized into frame or artifact outputs

Contact persistence is measured from:

- active contact state on `foot_*` and `ball_*`
- support uptime across the validation window
- support-loss event count

Active-contact semantics:

- source of truth: Chaos contact manifolds for the support-set rigid bodies
- active contact means one or more accepted body contacts between `foot_*` or `ball_*` and walkable world support at substep truth cadence
- traces, overlap checks, or shell-side heuristics may be logged as secondary diagnostics only
- side support state is `true` when `foot_* OR ball_*` is in active contact on that side
- minimum support-contact count is the count of support sides currently in active contact, not the raw manifold count

## One-Foot Support Policy

`V0` explicitly allows one-foot support (`support_contact_count == 1`) to be admissible as a successful standing state.

### Justification

- **Physical Honesty**: A physical humanoid should be capable of one-foot stability. Allowing it forces the controller to prove it can balance the COM over a reduced support hull without hidden assistance.
- **Transient Robustness**: Real standing involves minor weight-shifts and transient "foot-popping." Requiring two-foot contact at all times would produce brittle results that fail on non-material physics noise.

### Constraints and Honest-Truth Rules

One-foot standing is only admissible if it satisfies the full "honest standing" contract:

1.  **Reduced Support Hull**: When only one foot is in contact, the "support region" for the proxy check is reduced to the boundary of that single foot's contact manifold.
2.  **Proxy Convergence**: The planar support proxy (COM projection) must remain inside that reduced hull. If the proxy drifts outside the single foot's support area for more than `100 ms`, the run fails on `activation_proxy_outside_support_region`.
3.  **Stability Thresholds**: The character must still satisfy the `20 deg` root-tilt and `720 deg/s` angular speed envelopes. "Surviving" on one foot while flailing or tilting excessively is terminal.
4.  **Churn Penalty**: Rapidly alternating between one-foot and zero-foot or one-foot and two-foot support will trigger the `12 Hz` churn threshold.

### Interpretation for V0

For `V0`, one-foot standing is allowed for the entire duration of the hold if it remains stable. However, successful artifacts will be audited to distinguish between "two-foot stable" and "one-foot survival" regimes.

Weak assumption rejected:
- "Standing is only valid when both feet are firmly planted."

## Walkable-support contract for `V0`

- accepted support comes only from non-character `WorldStatic` level geometry
- the support surface must satisfy the `5.0 deg` flat-ground tolerance relative to gravity-up
- moving platforms are banned in `V0`; any contacted support surface with point velocity greater than `5.0 cm/s` is excluded
- contacts with dynamic bodies may be logged, but they are diagnostic-only and may not contribute to support truth
- self-contacts and body-on-body contacts within the same character must be excluded explicitly

Contact churn semantics:

- Chaos contacts are evaluated at physics-substep resolution and debounced before the `30 Hz` sample reduction
- a side-support state change is accepted only when the new state persists for `2` consecutive Chaos substeps
- churn is counted from those debounced side-support state changes at substep truth cadence and then summarized for reporting
- each false->true or true->false transition for left or right support contributes one churn event
- the reported churn rate is the total churn events divided by elapsed validation time in seconds

Substep reduction rule:

- support-loss timing uses the debounced substep-level support state, not only the `30 Hz` sampled boolean
- continuity loss, support failure, and timer advancement are decided from substep-level truth, not from the `30 Hz` artifact stream
- `samples[].support_contact_active` and `samples[].support_contact_count` publish the debounced side-support state at the frame's truth-sensitive sample point
- transient single-substep contact appearance or disappearance that does not survive the debounce rule is treated as substep noise, not as truth-state churn
- longer substep gaps still accumulate toward `support_loss_gap_max_ms` even though the artifact is only emitted at `30 Hz`

Support truth is invalid if:

- the support set is not physical
- shell or helper behavior is required to preserve support

Timer rule:

- the support-loss timer and the support-proxy-outside-support-region timer are independent
- either timer exceeding `100 ms` is terminal

## Legacy Mapping

The old labels remain available only for migration and comparison.

| Legacy label | New primary meaning |
| :--- | :--- |
| `RootOn spike` | controller instability, target discontinuity, or authority conflict |
| `Settle instability` | sustained-balance failure under continuous physics |
| `phase3_material_shell_correction` | shell influence material, but still secondary to primary physical truth |
| `topology regressed` | topology change event on the balance-critical chain |

## Success Rule

A run is successful only when:

- the balance-critical chain remained continuously simulated
- the support set remained physically valid for support truth
- the controller remained applied without forbidden authority conflict
- standing stability held continuously for the configured duration

No secondary diagnostic may override that rule.
