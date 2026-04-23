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

### Terminal Reason Arbitration

All terminal reason arbitration, rank-ordered precedence, and simultaneous-failure tie-break rules are defined in the authoritative:

- [engine_execution_contract.md](engine_execution_contract.md)

Interpretation:

- the rank-ordered precedence table in the contract is the final source for the "winning" `terminal_reason`
- all co-occurring reasons must be recorded in the `co_terminal_reasons` field per the contract rule

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

Contact-measurement rules:

- source of truth: Chaos contact manifolds for the support-set rigid bodies
- active contact means one or more accepted body contacts between `foot_*` or `ball_*` and walkable world support at substep truth cadence
- traces, overlap checks, or shell-side heuristics may be logged as secondary diagnostics only
- side support state is `true` when `foot_* OR ball_*` is in active contact on that side
- minimum support-contact count is the count of support sides currently in active contact, not the raw manifold count

## One-Foot Support Policy: Survival vs. Idle Standing

`V0` explicitly allows one-foot support (`support_contact_count == 1`) to be admissible as a successful state. This is a deliberate design choice to validate **Honest Balance Survival** rather than only a narrow definition of "Idle Standing."

### Benchmark Definition

For `V0`, the distinction is:
- **Honest Idle Standing**: Maintaining two-foot contact with minimal COM oscillation (Quality Target).
- **Honest Balance Survival**: Maintaining equilibrium over a reduced support hull (one foot) without hidden assistance (Technical Gate).

**`V0` is primarily a validation of Honest Balance Survival.** 

### Justification for the Survival Benchmark

- **Higher Physical Rigor**: Balancing a physical humanoid over one foot is significantly harder than two. If the controller can survive on one foot without capsule/helper contamination, the "honesty" of the bridge is proven more strongly.
- **Motion Continuity**: A character that can transiently survive on one foot is capable of weight-shifting and recovery. A character that requires two feet is brittle and non-physical.
- **Truth over Stance**: "Standing" is a stance policy; "Equilibrium" is a physical truth. `V0` prioritizes the proof of physical equilibrium.

### Constraints and Honest-Truth Rules

One-foot standing is only admissible if it satisfies the full "honest survival" contract:

1.  **Reduced Support Hull**: When only one foot is in contact, the "support region" for the proxy check is reduced to the boundary of that single foot's contact manifold.
2.  **Proxy Convergence**: The planar support proxy (COM projection) must remain inside that reduced hull. If the proxy drifts outside the single foot's support area for more than `100 ms`, the run fails on `activation_proxy_outside_support_region`.
3.  **Stability Thresholds**: The character must still satisfy the `20 deg` root-tilt and `720 deg/s` angular speed envelopes. "Surviving" on one foot while flailing or tilting excessively is terminal.
4.  **Churn Penalty**: Rapidly alternating between one-foot and zero-foot or one-foot and two-foot support will trigger the `12 Hz` churn threshold.

### Interpretation for V0 Artifacts

Successful `V0` artifacts will be audited to distinguish between these classes. A run that survives for 30 seconds on one foot is a **Technical Pass (Survival)**, but it may be flagged for further tuning if the goal of that specific test was **Idle Standing**.

Weak assumption rejected:
- "The bridge only needs to prove it can stand with both feet planted." (This masks solver and authority issues that one-foot survival exposes).

## Walkable-support contract for `V0`

- accepted support comes only from non-character `WorldStatic` level geometry
- the support surface must satisfy the `5.0 deg` flat-ground tolerance relative to gravity-up
- moving platforms are banned in `V0`; any contacted support surface with point velocity greater than `5.0 cm/s` is excluded
- contacts with dynamic bodies may be logged, but they are diagnostic-only and may not contribute to support truth
- self-contacts and body-on-body contacts within the same character must be excluded explicitly

Contact Churn and Substep Reduction Rule:

The algorithm for substep-level debounce, churn counting, and frame-reduction is defined in the authoritative:

- [engine_execution_contract.md](engine_execution_contract.md)

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
