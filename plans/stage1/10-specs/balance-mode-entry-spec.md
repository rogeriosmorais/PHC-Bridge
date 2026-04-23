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
| `BalanceActivation_Ready` | valid bridge context; balance-critical chain and support set continuously simulated; movement component idle; no pending reset | enter `BalanceActivation_BlendIn` once rebased targets are valid and quiet-state proof passes | topology change on critical chain, loss of continuous simulation, shell helper use on critical/support set, movement reclaim | locomotion-drive writes, shell helper writes, kinematic mode writes on critical/support set | balance activation runtime | topology change count, authority conflict count, shell helper used count, quiet-state duration |
| `BalanceActivation_BlendIn` | `Ready` satisfied; rebased targets exist; `ControlAuthorityAlpha=0.0` | enter `BalanceActivation_Validate` when `ControlAuthorityAlpha=1.0` and no fail condition fired | target discontinuity, gain/damping instability, contact/support failure, pose/reference mismatch, authority conflict, shell helper use | abrupt full-authority writes, shell helper writes, movement-component writes, reset writes | balance activation runtime | alpha, blend duration, target discontinuity, controller effort proxy, authority conflicts, support uptime |
| `BalanceActivation_Validate` | blend complete; support truth valid; no prior fail | enter `BalanceActive_Standing` after contiguous hold completes | any support failure, instability threshold breach, topology change, non-contiguous hold, shell helper use, movement reclaim | shell helper writes, movement-component writes, topology edits on critical chain, reset writes | balance activation runtime | contiguous hold time, root tilt envelope, peak angular speed by family, contact uptime, COM/support proxy drift |
| `BalanceActive_Standing` | contiguous hold complete | remain active or enter recovery/termination | loss of standing validity or explicit recovery trigger | legacy activation writes that bypass standing-mode ownership | balance mode runtime | sustained hold time, ongoing stability metrics |

Compatibility note:

- legacy `BridgeActive_Physical` maps to `BalanceActivation_Ready`
- legacy `BalanceActivation_StandingValidation` maps to `BalanceActivation_Validate`

Recovery note:

- `BalanceActive_Recovery`, `SafeDenied`, and `Failed` are named architectural states
- detailed recovery behavior is out of the current rewrite scope
- implementers must not improvise new standing-success logic inside those paths

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

The runtime must also distinguish:

- ownership-continuity problems
- controller-strength and control-shaping problems
- hidden authority conflicts between policy, Physics Control, locomotion authority, and startup logic

Primary truth precedence is defined in `continuous_balance_truth_model.md` and must be followed exactly.

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

## Blend-In Contract

`BalanceActivation_BlendIn` uses one explicit controller blend:

- blended quantity: `ControlAuthorityAlpha`
- alpha range: `0.0 -> 1.0`
- default duration: `0.75` seconds
- alpha scope: one global alpha for the balance-critical chain and support set in `V0`
- support-set targets: included in the same blend contract in `V0`
- damping/strength scaling: use the same alpha as target authority in `V0`; separate scaling is out of scope
- gating: time-based after `BalanceActivation_Ready` entry because `Ready` already owns the physical quietness proof; if a fail condition appears, the mode fails rather than pausing alpha
- history rebasing: one-time rebase on entry to `BalanceActivation_BlendIn`
- target discontinuity check: fail if `target_discontinuity_deg > 15.0` on the balance-critical chain during blend start

Failure reasons tied to the blend:

- `activation_target_discontinuity`
- `activation_controller_strength_or_representation_failure`
- `activation_authority_conflict`

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

- balance-critical ownership continuity lost
- controller blend instability
- controller-strength or target-shaping weakness
- shell influence material
- hidden multi-owner authority conflict
- gameplay or reset authority reclaimed
- standing validation timeout
- no path to sustained `BalanceActive_Standing`

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
