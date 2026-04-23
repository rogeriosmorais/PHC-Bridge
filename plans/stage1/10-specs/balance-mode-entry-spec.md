# Balance Mode Entry Spec

## Purpose

This document defines the authoritative Stage 1 balance-activation contract.

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

The target design is:

1. `BridgeActive_Physical`
2. `BalanceActivation_BlendIn`
3. `BalanceActivation_StandingValidation`
4. `BalanceActive_Standing`
5. `SafeDenied` or `Failed`

Success is only:

- reaching `BalanceActive_Standing`
- and holding it continuously for `3.0` seconds

Truthful safe deny remains a terminal failure outcome, not a success outcome.

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

## Canonical Activation Flow

### `BridgeActive_Physical`

The bridge is alive and the balance-critical chain is already physically owned.

Required properties:

- raw body state confirms continuous simulation of the balance-critical chain
- no pending topology flip is required to begin balance activation
- shell bookkeeping and shell influence remain separate observables

### `BalanceActivation_BlendIn`

The controller ramps authority onto the already-physical balance-critical chain.

Required properties:

- controller authority rises gradually
- policy/control writes are blended rather than abruptly asserted
- no diagnostic or grace rule may reinterpret a destabilizing blend as success

### `BalanceActivation_StandingValidation`

The runtime validates sustained physical standing after blend-in.

Required properties:

- standing readiness must be contiguous for the required hold duration
- non-ready frames reset the hold timer
- diagnostics may classify the first truthful failure but may not repair the state into success on the same frame

### `BalanceActive_Standing`

This is the only current passing publication state for balance activation.

## Authority And Diagnostics

The runtime must keep these observables distinct:

1. intended continuous ownership
2. raw body simulation state
3. modifier-record or control-layer ownership bookkeeping
4. controller-authority alpha / blend progress
5. shell bookkeeping state
6. shell influence materiality
7. locomotion or reset authority state

Interpretation rules:

- intended ownership is not proof of applied ownership
- bookkeeping is not proof of raw physical continuity
- shell bookkeeping is not proof of shell influence
- diagnostics are observability only

## Measurement-Only Rule

The activation contract explicitly forbids using diagnostics to manufacture a pass.

Therefore:

- grace windows may explain classification, but not convert instability into success
- transitional mismatch may be logged, but not treated as proof that activation worked
- safe deny remains preferred over dishonest activation

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
- tolerate the controller blend
- remain upright enough to satisfy the standing-validation window

A run may satisfy contract correctness and still fail physical viability.

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

- old `Prepare` / `LateValidate` map to physical-readiness checks before or at `BridgeActive_Physical`
- old `RootOn` maps to a superseded ownership-flip concept and is not the target design
- old `Settle` maps most closely to `BalanceActivation_StandingValidation`

No authoritative document may present the legacy phase sequence as the intended activation mechanism.

## Required Terminal Truthfulness

When activation fails, the deny or failure path should identify that explicitly rather than collapsing everything to a generic label.

At minimum this includes distinguishing:

- balance-critical ownership continuity lost
- controller blend instability
- shell influence material
- gameplay or reset authority reclaimed
- standing validation timeout
- no path to sustained `BalanceActive_Standing`

## Acceptance Criteria

This spec is satisfied only when:

- the balance-critical chain is explicit
- the target activation flow is explicit
- continuous physical ownership is defined as the target design
- controller authority is defined as a gradual blend
- diagnostics are explicitly observational only
- the docs explicitly allow a contract-correct but physically non-viable result
- the docs explicitly define success as `BalanceActive_Standing` held for `3.0` continuous seconds
