# Continuous Balance Architecture

## Purpose

This is the main architecture doc for the continuous-balance rewrite.

Core rules:

- the balance-critical proximal chain remains continuously simulated during balance mode
- the controller is blended onto an already-physical state instead of triggering a discrete root-ownership flip
- success is defined only by sustained standing stability over time
- shell or reference diagnostics may be recorded, but may not certify success
- any topology or ownership change in the balance-critical chain is a diagnostic event, not normal operation

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

## Balance-Critical Proximal Chain

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

## What Is Explicitly Not Allowed

- treating phase completion as success
- treating shell status as proof of balance
- treating a protected ownership flip as the intended activation mechanism
- reintroducing grace logic to hide controller weakness
- adding distal or upper-body sophistication before proximal standing is honest

## Success Definition

The current rewrite success ladder is:

- Milestone 1: honest continuous-physics diagnostics
- Milestone 2: `1.0` second stable hold
- Milestone 3: `3.0` second stable hold
- Milestone 4: small perturbation recovery

The production benchmark remains:

- `BalanceActive_Standing` held continuously for `3.0` seconds

## Expected Early Regressions That Are Acceptable

The rewrite may initially:

- fail more obviously
- expose controller weakness earlier
- expose authority conflicts earlier
- remove protective guard or grace behavior that previously hid instability

That is acceptable if the new mode is more honest about why standing fails.
