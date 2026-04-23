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

### Support region definition

The `V0` support region is the convex hull of the currently supporting `foot_*` and `ball_*` contact points.

### Failure threshold

The run fails support-proxy truth if:

- the planar support proxy remains outside the support region for more than `100 ms`

### Precedence rule

If support truth fails, the run fails even when the support proxy or root tilt still look temporarily acceptable.

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

Support truth is invalid if:

- the support set is not physical
- shell or helper behavior is required to preserve support

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
