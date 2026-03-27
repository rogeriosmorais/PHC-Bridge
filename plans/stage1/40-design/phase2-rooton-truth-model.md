# Phase 2 / RootOn Truth Model

Status: Authoritative design analysis  
Scope: Source-of-truth order and failure classification during `BalanceTransition_Phase2_RootOn`

## 1. Purpose

This document defines how RootOn must interpret ownership and validity during the Phase 2 guard window.

It exists to prevent four recurring mistakes:

- treating certified intent as if it were already applied
- treating raw end-state proof as if it also proved routing correctness
- treating modifier-record ownership as if it guaranteed raw-body state
- treating shell state as if it implied shell influence

## 2. Observables

During RootOn, the runtime must keep these observables separate:

### A. Certified topology intent

The certified Phase 1 handoff plus explicit RootOn choreography define what the topology is supposed to be.

### B. Raw body end state

Chaos body instances define whether the underlying rigid bodies are actually simulating by the end of the current RootOn pass.

### C. Modifier-record ownership

The `PAMod_*` body-modifier records define what PhysicsControl currently believes the movement type is.

### D. Shell/reference state

Shell lock/reanchor/reseed state tells us what shell bookkeeping is active.

### E. Shell/reference influence

Material effect of shell/reference behavior on simulated bodies, measured through RootOn guard metrics such as shell deltas and induced motion.

## 3. Source-of-truth order

For RootOn failure classification, use this order:

1. certified topology intent
2. same-tick raw body end state
3. modifier-record ownership
4. shell / policy influence

Interpretation:

- intent mismatch is a contract mismatch
- raw mismatch under correct intent is a technical RootOn failure
- modifier mismatch under correct intent + raw state is a routing / ownership mismatch
- shell / policy influence is a separate guard-window materiality question

Current implementation note:

- the runtime still logs modifier disagreement aggressively because it remains important routing evidence, but same-tick raw continuity is the deciding proof of technical RootOn success or failure

## 4. Preserved-Proximal Rule

If RootOn is defined to preserve the Phase 1 proximal sim set, then for:

- `thigh_l`
- `thigh_r`
- `spine_01`
- `spine_02`
- `spine_03`

all of the following are required by the end of the current RootOn application pass:

- intent = simulated
- raw body state = simulated
- modifier record = simulated

A disagreement among those three layers is not a cosmetic issue. It is a RootOn contract-relevant violation.

## 5. Root Addition Rule

When RootOn adds root/pelvis simulation, that change does not implicitly authorize:

- changing the preserved proximal set
- releasing policy into the transition set
- cached resets
- shell/reference assistance on simulated bodies

## 6. Same-Frame Disagreement Rules

Same-frame disagreement may be observed transiently, but it must be classified explicitly.

Examples:

- intent = simulated, raw = simulated, modifier = kinematic
  - classify as modifier-record ownership disagreement
- intent = simulated, raw = kinematic, modifier = simulated
  - classify as raw-body application disagreement / delayed application
- shell locked = true, shell influence material = false
  - classify as shell state only, not shell-material violation
- shell locked = true, shell influence material = true
  - classify as shell-material violation

## 7. Guard-Window Denial Rules

RootOn guard should deny truthfully when the first material failure is one of:

- policy leak into the transition set
- shell material influence on simulated bodies
- root simulation dropped
- topology not preserved
- preserved-proximal modifier/raw disagreement
- spike / instability

Do not collapse these into one generic no-convergence label.

## 8. Evidence Rule

Every RootOn failure investigation must state:

- intended topology
- raw-body topology
- modifier-record topology
- whether shell state was present
- whether shell influence was material
- whether policy writes occurred
- first truthful failure reason

## 9. Retry Rule

Phase 2 documentation must preserve the current retry taxonomy:

- `phase2_topology_not_preserved` is retryable only if recovery completed, recovery changed material state, fresh quiet proof occurred, cooldown elapsed, and retry budget remains
- `phase2_root_on_spike` is not retryable
- `phase2_policy_write_leak` is not retryable

## 10. Acceptance

This truth model is satisfied only when RootOn decisions and denial reasons are based on explicit classification of:

- intended ownership
- raw body state
- modifier-record ownership
- shell / policy influence

and no hidden same-frame assistance is required to make the smoke appear successful.
