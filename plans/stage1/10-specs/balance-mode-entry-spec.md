# Balance Mode Entry Spec

## Purpose

This document defines the authoritative Stage 1 balance-entry contract.

It exists to separate:

- normal bridge startup behavior
- balance-entry state-machine behavior
- Phase 1 ownership and write-routing behavior
- Phase 2 RootOn ownership and guard-window behavior
- the still-open physical-viability question

## Core interpretation

Balance entry is a distinct runtime contract layered on top of a running bridge.

The major contract question is no longer whether the runtime can represent entry at all.

The current contract surface is now split across:

- Phase 1 accepted topology and LateValidate truthfulness
- Phase 2 warm-start RootOn truthfulness

## Phase 1 Truth Model Alignment

Phase 1 behavior is governed by the authoritative [Phase 1 / LateValidate Truth Model](../40-design/phase1-late-validate-truth-model.md).

The accepted Phase 1 topology is:

- `root = kinematic`
- `proximal = simulated`
- `distal = kinematic`
- `upper = kinematic`

Interpretation rules:

- The root side may remain kinematic during Phase 1.
- `pelvisSimulating=false` is not, by itself, a Phase 1 deny condition.
- Phase 1 topology intent and raw body sim state are not guaranteed to be frame-synchronous; violations must be tracked according to the truth-model confirmation rules.
- Topology changes are ownership changes, not mere tuning changes.

## Authority by phase

| Phase | Authority source order | Tolerated mismatches | Hard-failure mismatches |
| :--- | :--- | :--- | :--- |
| `Phase 1` | `topology intent` -> `raw body state` -> `modifier-record state` | Same-frame intent/raw disagreement during ownership application; stale modifier record that has not yet been next-frame confirmed; raw quietness still deciding physical viability while modifier state catches up | Frozen topology contradicted by confirmed post-update state; simulated bones receiving forbidden held writes; kinematic bones treated as success from stale modifier evidence alone |
| `RootOn` | `topology intent` -> same-tick `raw body end state` -> `modifier-record state` | Probe-time omission such as skipped `UpdateControls()` does not decide success by itself; preserved proximal bones may temporarily show `modifier=Kinematic` with `rawSim=1`; shell state without material shell influence | Certified topology not preserved by same-tick end state; root simulation dropped; policy leak; shell material influence; confirmed same-tick end-state evidence that the preserved set no longer satisfies RootOn |
| `Settle` | `topology intent` -> post-RootOn `raw body state` -> `modifier-record state` | Short-lived modifier lag while the accepted post-RootOn topology remains physically coherent; diagnostics may record modifier/raw disagreement without using it as the first deciding failure when continuity still holds | Post-RootOn continuity breaks the accepted topology; root simulation drops; instability/spike failure; confirmed preserved-set loss after RootOn |

## Forbidden writes by phase

| Phase | Forbidden writes / influence | Allowed exception |
| :--- | :--- | :--- |
| `Phase 1` | normal policy writes into simulated Phase 1 bones; held target writes into simulated bones; cached-reset effects that leak into LateValidate proof | explicit allowed hold path may write only to the allowed kinematic hold set |
| `RootOn` | normal policy writes into the transition set; shell material influence on simulated bodies; cached resets; topology expansion outside certified RootOn choreography; CharacterMovement correction influence on the simulated transition set | probe-only omission such as skipped `UpdateControls()` is allowed, but it does not relax same-tick end-state success rules |
| `Settle` | reintroduction of forbidden transition-time policy/shell/reset assistance before post-RootOn continuity is accepted; hidden support that masks instability | only the explicitly released post-RootOn steady-state path may resume after Settle acceptance |

## Mismatch outcome boundary

Use the following boundary for all balance-entry phases:

| Outcome class | Meaning | Typical examples |
| :--- | :--- | :--- |
| `Tolerated diagnostic mismatch` | Observable disagreement that is recorded truthfully but is not the deciding failure source because authoritative end-state proof still holds | same-frame intent/raw lag during Phase 1 application; RootOn preserved proximal `modifier=Kinematic` with `rawSim=1`; shell state present without material shell influence |
| `Retryable transient failure` | The current tick or window is not yet admissible, but the attempt may continue because the contract is not yet falsified and recovery inside the active attempt remains allowed | quiet-window not yet satisfied; LateValidate shell-hold/readiness proof incomplete; temporary application lag where end-state proof is still pending rather than contradicted |
| `Hard terminal failure` | The current attempt has been falsified and must terminate truthfully rather than continue or be reclassified as generic no-convergence | certified topology contradicted by confirmed end state; root simulation dropped after RootOn decision point; policy leak; shell material influence; persistent instability/spike; preserved set no longer present after RootOn |

## Phase 1 write-routing contract

During Prepare and LateValidate:

- normal policy target writes over the accepted Phase 1 set are suppressed
- only the explicit allowed hold path may publish to the allowed kinematic bones
- simulated Phase 1 bones must not receive held target writes
- diagnostics must distinguish `normal`, `held`, and `total`

## Phase 1 freeze contract

On transition accept:

- freeze is acquired

During the full Phase 1 attempt:

- freeze remains active
- Prepare / LateValidate bounces do not release it

Freeze releases only when the attempt reaches terminal success, terminal safe deny, explicit abort, or teardown.

## Convergence-snapshot contract

Prepare and LateValidate decisions must use an authoritative post-update convergence snapshot.

That snapshot is the source of truth for:

- authoritative root tilt
- root validity
- body-motion instability metrics
- target-delta metrics
- shell/reference deltas used by gating

## Phase 2 Truth Model Alignment

Phase 2 behavior is governed by the authoritative [Phase 2 / RootOn Truth Model](../40-design/phase2-rooton-truth-model.md).

Phase 2 consumes a still-valid Phase 1 handoff and attempts a warm-start RootOn.

### Required Phase 2 entry interpretation

At the moment RootOn begins:

- the certified Phase 1 handoff remains the topology source of truth
- Phase 2 may add root simulation only through the explicit RootOn choreography
- Phase 2 must not silently rewrite the preserved non-root topology under the guise of tuning

### Phase 2 source-of-truth order

During RootOn and its guard window, the following must be treated as distinct observables:

1. frozen / certified topology intent
2. modifier-record ownership
3. raw body simulation state

For tick-level RootOn pass / fail decisions, same-tick end-state evaluation must still resolve success from the certified topology plus the observed raw end state, even if an intermediate probe path such as `UpdateControls()` is skipped.

Interpretation rules:

- intended ownership is not proof of applied ownership
- modifier-record ownership is not proof of raw-body state
- raw-body state alone is not proof that write-routing and suppression were correct
- RootOn decisions and failure classification must state which layer failed first

### Phase 2 guard-window contract

During the guard window, all of the following are forbidden unless the spec later says otherwise:

- normal policy writes into the transition set
- cached resets
- topology expansion not explicitly defined by the certified handoff + RootOn choreography
- hidden shell assistance on simulated bodies
- CharacterMovement correction influence on the simulated transition set
- shell-reference reseed used as same-frame support

### Shell-state versus shell-influence rule

Phase 2 must distinguish:

- shell state (`locked`, `reanchored`, etc.)
- shell influence on simulated bodies

A locked or reanchored shell is not, by itself, a violation.

A violation exists only if shell/reference behavior materially influences the simulated transition set during the guard window in a way the contract forbids.

### Preserved-proximal rule

If the certified handoff says the proximal Phase 1 set remains simulated during RootOn, then for those preserved proximal bones:

- intended ownership must remain simulated
- modifier-record ownership must converge to simulated
- raw-body state must remain simulated

A disagreement among those three layers is a contract-relevant diagnostic event, not a harmless implementation detail.

## Contract correctness vs physical viability

### Contract correctness

Balance entry is contract-correct when:

- topology is accepted correctly
- suppression is correct
- hold-only semantics are correct
- freeze lifetime is correct
- convergence timing/source is correct
- RootOn source-of-truth evaluation is correct
- terminal reasons are truthful

### Physical viability

Balance entry is physically viable only if the accepted setup remains dynamically quiet enough to survive entry under current:

- control tuning
- contact behavior
- sub-step regime
- hold/reference behavior
- RootOn shell/reference conditions

A run may satisfy contract correctness and still fail physical viability.

## Required terminal truthfulness

When the accepted setup fails, the deny/reset path should identify that explicitly rather than collapsing everything to a generic no-convergence label.

For RootOn this includes distinguishing:

- policy leak
- reset violation
- raw/modifier disagreement
- root simulation drop
- shell material influence
- spike / motion instability
- topology not preserved

## Acceptance criteria

This spec is satisfied only when:

- the accepted Phase 1 topology is explicit
- the write-routing contract is explicit
- the freeze contract is explicit
- the convergence snapshot contract is explicit
- the Phase 2 RootOn source-of-truth order is explicit
- shell state versus shell influence is explicit
- the docs explicitly allow a contract-correct but physically non-viable Phase 1 or Phase 2 result
