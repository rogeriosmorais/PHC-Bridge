# Truth Model For Continuous Balance

## Purpose

This document replaces the handoff-centered truth model for the new architecture.

Core rule:

- success is sustained physical stability under continuous simulation
- no phase completion, shell status, or compatibility label can substitute for that

## Primary Truth Sources

The primary truth sources for continuous balance are:

1. root pose and root tilt
2. COM behavior or support proxy
3. contact persistence
4. body angular and linear stability
5. sustained hold duration

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

Interpretation rules:

- raw body continuity beats bookkeeping
- bookkeeping beats declared intent only for diagnostics, not success
- control-target publication is never proof of achieved pose
- shell state is never proof of standing
- movement-component non-interference is required, not inferred
- support truth beats acceptable-looking COM/support-proxy behavior when the support set is not actually sustaining the body

## Secondary Diagnostics

These may explain why the primary truth sources failed:

- shell bookkeeping state
- shell influence metrics
- modifier-record bookkeeping
- controller effort proxies
- authority conflict events

None of these secondary diagnostics can certify success by themselves.

## Control-First Failure Taxonomy

Every failure in the new mode must be classified first as one of:

- target discontinuity
- gain or damping instability
- contact or support failure
- pose or reference mismatch
- authority conflict

Only after that may the runtime add secondary context such as shell influence or bookkeeping disagreement.

## Support Truth

Support truth is primary only because `V0` requires the support set to remain simulated.

Contact persistence is measured from:

- active contact state on `foot_*` and `ball_*`
- support uptime across the validation window
- support-loss event count

Active-contact semantics:

- source of truth: Chaos contact manifolds for the support-set rigid bodies
- active contact means one or more live body contacts between `foot_*` or `ball_*` and walkable world support during the current `30 Hz` sample window
- traces, overlap checks, or shell-side heuristics may be logged as secondary diagnostics only
- side support state is `true` when `foot_* OR ball_*` is in active contact on that side
- minimum support-contact count is the count of support sides currently in active contact, not the raw manifold count

Contact churn semantics:

- churn is counted from side-support state changes at the `30 Hz` sample cadence
- each false->true or true->false transition for left or right support contributes one churn event
- the reported churn rate is the total churn events divided by elapsed validation time in seconds

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
