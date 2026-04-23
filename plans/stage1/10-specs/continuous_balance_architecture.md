# Continuous Balance Architecture

## Purpose

This is the main architecture doc for the continuous-balance rewrite.

The new system must not be described as `Prepare/LateValidate/RootOn/Settle, but cleaner`.

Core rules:

- the balance-critical proximal chain remains continuously simulated during balance mode
- the controller is blended onto an already-physical state instead of triggering a discrete ownership flip
- success is defined only by sustained standing stability over time
- shell or reference diagnostics may be recorded, but may not certify success
- any topology or ownership change in the balance-critical chain is a diagnostic event, not normal operation

## Preferred Runtime States

These are the preferred design names for the rewrite:

1. `BalanceActivation_Ready`
2. `BalanceActivation_BlendIn`
3. `BalanceActivation_Validate`
4. `BalanceActive_Standing`
5. `BalanceActive_Recovery`
6. `SafeDenied`
7. `Failed`

Compatibility mapping:

- `BridgeActive_Physical` maps to `BalanceActivation_Ready`
- `BalanceActivation_StandingValidation` maps to `BalanceActivation_Validate`

## First Rewrite Scope

The first rewrite goal is intentionally narrow.

### `V0`

- always-sim proximal chain
- idle stance
- flat ground
- no perturbation
- no locomotion authority
- no shell cleverness

### `V1`

- sustained standing

### `V2`

- recovery from small pushes

### `V3`

- locomotion coupling

Hard non-goal:

- no distal or upper-body sophistication before proximal standing is honest

## Body Sets

### Balance-Critical Proximal Chain

The first rewrite target keeps this chain continuously simulated:

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

Interpretation rules:

- this chain is the architectural center of the rewrite
- topology or ownership changes in this chain are diagnostic events
- they are not normal operation

### Support Set

The support set is required so support truth is physically meaningful in `V0`:

- `calf_l`
- `calf_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

Interpretation rules:

- the support set is continuously simulated in `V0`
- support truth may be primary only because these bodies are physical in `V0`
- kinematic containment of the support set is not allowed in `V0`

### Upper-Body Set

- `clavicle_l`
- `upperarm_l`
- `lowerarm_l`
- `hand_l`
- `clavicle_r`
- `upperarm_r`
- `lowerarm_r`
- `hand_r`
- `neck_01`
- `head`

### Excluded Or Non-Essential Bodies

- any cosmetic, helper, or non-contact body not listed above

## Non-Critical Body Ownership

| Body set | Allowed movement type in `V0` | Target source in `V0` | May inject force indirectly into critical chain? |
| :--- | :--- | :--- | :--- |
| Upper body | simulated only in `V0` | fixed stance reference only; no advanced policy shaping | yes through normal articulation only; no per-tick kinematic forcing that stabilizes the proximal chain |
| Distal lower limbs / support set | simulated only in `V0` | fixed stance reference only; no advanced policy shaping | yes through honest contact only; no helper forcing |
| Root-adjacent but non-critical bodies | match nearest articulated parent; no ad hoc flips during an active attempt | fixed stance reference or none | yes only through normal articulation |
| Excluded bodies | disabled or passive only | none | no |

Upper-body rule:

- upper-body kinematic hold is banned in `V0`
- any future compatibility path must use a separate explicit mode name and must emit `compatibility_path_used=true`

## Shell Rule

Pick one rule and make it executable:

- in `V0`, shell assistance is fully disabled on the balance-critical chain and the support set

Interpretation rules:

- shell metrics may still be logged
- shell helper application on the balance-critical chain or support set is a failure
- no helper-ceiling path exists for `V0`

There is no compatibility backdoor for shell help in `V0`.

## Support And Contact Rule

`V0` standing uses a support model built from the physical support set.

Support truth is measured by:

- contact persistence on `foot_*` and `ball_*`
- support-side uptime over the active validation window
- support loss events

Active-contact measurement contract for `V0`:

- contact source: Chaos rigid-body contact data for the support-set bodies
- an active contact means at least one live Chaos contact on `foot_*` or `ball_*` against walkable world support during the current sample window
- traces may be logged as secondary diagnostics, but they do not define support truth in `V0`
- contact aggregation rule: `foot_* OR ball_*` on the same side counts as that side being in support
- churn counting is evaluated on the `30 Hz` instrumentation cadence; each false->true or true->false side-support state transition counts as one churn event

`V0` numeric support thresholds:

- minimum support-contact count: `1`
- one foot is sufficient in `V0`; both feet are not required
- max support-loss gap: `100 ms`
- max contact-churn rate: `12 Hz` aggregated across both sides during `BalanceActivation_Validate`
- support-loss timer and support-proxy-outside-region timer are independent timers; either one may fail the run first

Support failure means any of:

- all support contacts absent for longer than the configured tolerance window
- contact churn exceeds the allowed event rate while standing still fails
- support truth depends on non-physical helper behavior

Support truth precedence:

- if support truth fails, the run fails even if COM or root-side metrics still look temporarily acceptable

## What Is Explicitly Not Allowed

- treating phase completion as success
- treating shell status as proof of balance
- treating a protected ownership flip as the intended activation mechanism
- reintroducing grace logic to hide controller weakness
- adding distal or upper-body sophistication before proximal standing is honest
- allowing hidden shell assistance on the balance-critical chain or support set in `V0`

## Success Definition

The current rewrite success ladder is:

- Milestone 1: honest continuous-physics diagnostics
- Milestone 2: `1.0` second stable hold
- Milestone 3: `3.0` second stable hold
- Milestone 4: small perturbation recovery

The production benchmark remains:

- `BalanceActive_Standing` held continuously for `3.0` seconds

For `V0` acceptance:

- `BalanceActive_Recovery` is non-authoritative
- recovery behavior must not be used to satisfy standing success

## Expected Early Regressions That Are Acceptable

The rewrite may initially:

- fail more obviously
- expose controller weakness earlier
- expose authority conflicts earlier
- remove protective guard or grace behavior that previously hid instability

That is acceptable if the new mode is more honest about why standing fails.
