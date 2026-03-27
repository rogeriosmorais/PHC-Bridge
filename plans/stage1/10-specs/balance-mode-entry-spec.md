# Balance Mode Entry Spec

## Purpose

This document defines the authoritative Stage 1 balance-entry contract.

It exists to separate:

- normal bridge startup behavior
- balance-entry state-machine behavior
- Phase 1 ownership and write-routing behavior
- Phase 2 RootOn ownership and guard-window behavior
- Phase 3 Settle continuity behavior
- the still-open physical-viability question

## Core interpretation

Balance entry is a distinct runtime contract layered on top of a running bridge.

The major contract question is no longer whether the runtime can represent entry at all.

The current contract surface is now split across:

- Phase 1 accepted topology and LateValidate truthfulness
- Phase 2 warm-start RootOn truthfulness
- Phase 3 post-RootOn Settle continuity before active mode

## Canonical phase sequence

Use the following canonical balance-entry phase sequence in docs, logs, and design discussion:

1. `Phase1_Prepare`
2. `Phase1_LateValidate`
3. `Phase2_RootOn`
4. `Phase2_ReadyForPhase3`
5. `Phase3_Settle`
6. `BalancePerturbationActive`

Interpretation rules:

- `Phase2_ReadyForPhase3` is an internal handoff boundary, not an active-balance phase
- runtime `BalanceEntry_Settle` maps to `Phase3_Settle`
- balance mode is not active until `Phase3_Settle` succeeds

## Phase 1 Truth Model Alignment

Phase 1 behavior is governed by the authoritative [Phase 1 / LateValidate Truth Model](../40-design/phase1-late-validate-truth-model.md).

The accepted Phase 1 topology is:

- `root = kinematic`
- `proximal = simulated`
- `distal = kinematic`
- `upper = kinematic`

Interpretation rules:

- the root side may remain kinematic during Phase 1
- `pelvisSimulating=false` is not, by itself, a Phase 1 deny condition
- Phase 1 topology intent and raw body sim state are not guaranteed to be frame-synchronous; violations must be tracked according to the truth-model confirmation rules
- topology changes are ownership changes, not mere tuning changes

## Authority And Suppression By Phase

| Phase | Authority source order | Target contract | Current implementation note |
| :--- | :--- | :--- | :--- |
| `Phase 1` | see the authoritative Phase 1 truth model | explicit frozen topology, hold-only writes, and convergence-snapshot gating | unchanged by this pass |
| `Phase2_RootOn` | `certified intent` -> same-tick `raw body end state` -> `modifier-record state` -> `shell/policy influence` | policy/reset/shell/move-smoke suppression must stay active through the guard window; same-tick raw continuity is the deciding proof of technical RootOn success | modifier disagreement remains important routing evidence and must still be logged/classified truthfully |
| `Phase3_Settle` | `certified post-RootOn topology` -> post-RootOn `raw body state` -> `modifier-record state` -> `shell/policy influence` | Settle is the continuity window between RootOn and active mode; hidden support must not mask instability | policy activity can resume in the current runtime, but Settle still requires idle locomotion, preserved shell lock, no shell reseed, no pending resets, no topology regression, and no material shell correction |

## Mismatch Outcome Boundary

Use the following boundary for all balance-entry phases:

| Outcome class | Meaning | Typical examples |
| :--- | :--- | :--- |
| `Tolerated diagnostic mismatch` | Observable disagreement that is recorded truthfully but is not the deciding failure source because authoritative end-state proof still holds | same-frame intent/raw lag during Phase 1 application; RootOn preserved proximal `modifier=Kinematic` with `rawSim=1`; Settle root modifier mismatch logged while raw continuity still holds |
| `Retryable transient failure` | The current tick or window is not yet admissible, but the attempt may continue because the contract is not yet falsified and recovery inside the active attempt remains allowed | quiet-window not yet satisfied; LateValidate shell-hold/readiness proof incomplete; RootOn topology not yet preserved but recovery prerequisites can still be met |
| `Hard terminal failure` | The current attempt has been falsified and must terminate truthfully rather than continue or be reclassified as generic no-convergence | certified topology contradicted by confirmed end state; root simulation dropped after RootOn decision point; policy leak; shell material influence; persistent instability/spike; preserved set no longer present after RootOn |

## Phase 1 Write-Routing Contract

During Prepare and LateValidate:

- normal policy target writes over the accepted Phase 1 set are suppressed
- only the explicit allowed hold path may publish to the allowed kinematic bones
- simulated Phase 1 bones must not receive held target writes
- diagnostics must distinguish `normal`, `held`, and `total`

## Phase 1 Freeze Contract

On transition accept:

- freeze is acquired

During the full Phase 1 attempt:

- freeze remains active
- Prepare / LateValidate bounces do not release it

Freeze releases only when the attempt reaches terminal success, terminal safe deny, explicit abort, or teardown.

## Convergence-Snapshot Contract

Prepare and LateValidate decisions must use an authoritative post-update convergence snapshot.

That snapshot is the source of truth for:

- authoritative root tilt
- root validity
- body-motion instability metrics
- target-delta metrics
- shell/reference deltas used by gating

## Phase 2 RootOn Alignment

Phase 2 behavior is governed by the authoritative [Phase 2 / RootOn Truth Model](../40-design/phase2-rooton-truth-model.md) and the implementation design in [balance_mode_phase2.md](../40-design/balance_mode_phase2.md).

Phase 2 consumes a still-valid Phase 1 handoff and attempts a warm-start RootOn.

### Required Phase 2 interpretation

At the moment RootOn begins:

- the certified Phase 1 handoff remains the topology source of truth
- Phase 2 may add root simulation only through the explicit RootOn choreography
- Phase 2 must not silently rewrite the preserved non-root topology under the guise of tuning
- the guard window ends at `Phase2_ReadyForPhase3`, not at active mode

### Phase 2 source-of-truth order

During RootOn and its guard window, the following must be treated as distinct observables:

1. frozen / certified topology intent
2. same-tick raw body end state
3. modifier-record ownership
4. shell / policy influence

Interpretation rules:

- intended ownership is not proof of applied ownership
- same-tick raw end state is the deciding proof of technical RootOn success or failure
- modifier-record ownership is still required routing evidence and must be classified truthfully when it disagrees
- shell state and shell influence remain separate observables

### Phase 2 guard-window contract

During the guard window, all of the following are forbidden unless the spec later says otherwise:

- normal policy writes into the transition set
- cached resets
- topology expansion not explicitly defined by the certified handoff + RootOn choreography
- hidden shell assistance on simulated bodies
- CharacterMovement correction influence on the simulated transition set
- shell-reference reseed used as same-frame support

## Phase 3 Settle Alignment

Phase 3 behavior is governed by [balance_mode_phase3_settle.md](../40-design/balance_mode_phase3_settle.md).

Phase 3 begins only after the runtime crosses the explicit `Phase2_ReadyForPhase3` handoff.

### Required Phase 3 interpretation

- runtime `BalanceEntry_Settle` is Phase 3
- Settle validates post-RootOn continuity before activation
- Settle success requires a contiguous ready hold over the accepted post-RootOn topology
- non-ready Settle frames reset the success hold timer
- the first truthful post-RootOn instability/spike frame is terminal
- Settle diagnostics may observe continuity loss, but must not repair physics/control state in the same frame to turn failure into success

### Phase 3 timers

- Phase 2 hands off after `BalancePhase2GuardWindowDuration`
- Settle succeeds only after `BalancePhase3RequiredStableHoldDuration`
- Settle fails with `phase3_no_convergence_path` after `BalancePhase3TimeoutDuration`

### Phase 3 emitted failure reasons

The current emitted Phase 3 reasons are:

- `phase3_root_simulation_dropped`
- `phase3_post_root_on_instability`
- `phase3_topology_regressed`
- `phase3_shell_lock_lost`
- `phase3_shell_reference_reseeded`
- `phase3_startup_or_gameplay_authority_reclaimed`
- `phase3_reset_pending`
- `phase3_material_shell_correction`
- `phase3_no_convergence_path`

Implementation note:

- `phase3_root_modifier_mismatch` is currently classified in tests/owner mapping, but the live Phase 3 validator does not emit it as a terminal failure; current runtime behavior logs the modifier disagreement diagnostically instead

## Contract Correctness Vs Physical Viability

### Contract correctness

Balance entry is contract-correct when:

- topology is accepted correctly
- suppression is correct
- hold-only semantics are correct
- freeze lifetime is correct
- convergence timing/source is correct
- RootOn source-of-truth evaluation is correct
- Settle continuity is evaluated truthfully
- terminal reasons are truthful

### Physical viability

Balance entry is physically viable only if the accepted setup remains dynamically quiet enough to survive entry under current:

- control tuning
- contact behavior
- sub-step regime
- hold/reference behavior
- RootOn shell/reference conditions
- post-RootOn Settle continuity conditions

A run may satisfy contract correctness and still fail physical viability.

## Required Terminal Truthfulness

When the accepted setup fails, the deny/reset path should identify that explicitly rather than collapsing everything to a generic no-convergence label.

For RootOn this includes distinguishing:

- policy leak
- reset violation
- raw/modifier disagreement
- root simulation drop
- shell material influence
- spike / motion instability
- topology not preserved

For Settle this includes distinguishing:

- root simulation dropped
- post-RootOn instability
- topology regressed
- shell lock lost
- shell reference reseeded
- startup or gameplay authority reclaimed
- reset pending
- material shell correction
- no convergence path

## Acceptance Criteria

This spec is satisfied only when:

- the accepted Phase 1 topology is explicit
- the write-routing contract is explicit
- the freeze contract is explicit
- the convergence snapshot contract is explicit
- the Phase 2 RootOn source-of-truth order is explicit
- Phase 3 Settle is explicit as its own post-RootOn continuity phase
- `BalanceEntry_Settle` is documented as Phase 3
- shell state versus shell influence is explicit
- the docs explicitly allow a contract-correct but physically non-viable Phase 1, Phase 2, or Phase 3 result
