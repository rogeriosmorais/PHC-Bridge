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

## Secondary Diagnostics

These may explain why the primary truth sources failed:

- shell bookkeeping state
- shell influence metrics
- modifier-record bookkeeping
- controller effort proxies
- authority conflict events

None of these secondary diagnostics can certify success by themselves.

## Continuous Simulation Contract

For `V0`, "continuously simulated" is a raw-physics continuity contract, not a vague synonym for "probably physical enough."

Continuous simulation requires all of the following for every body in the active truth sets:

- raw simulation state remains `true` on every truth-sensitive sample
- the body remains in the expected truth set for the active attempt
- the runtime does not recreate or replace that truth-set body instance during the attempt
- no kinematic override or equivalent non-simulated movement type is applied to that body

Interpretation rules:

- raw `IsInstanceSimulatingPhysics == true` or equivalent raw body-instance truth is the deciding source for simulation continuity
- modifier or bookkeeping disagreement is diagnostic by itself, not automatic proof that continuity was lost
- sleeping is not automatically a continuity failure
- sleeping is allowed during `BalanceActivation_Ready` because quiet-state proof may include low-motion or sleeping bodies
- during `BalanceActivation_BlendIn` and `BalanceActivation_Validate`, the pelvis must remain awake enough to participate physically; a persistent sleeping pelvis is treated as continuity lost rather than "successfully simulated"
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

Cadence rule:

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

Walkable-support contract for `V0`:

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
