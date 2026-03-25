# Phase 2 / RootOn Truth Model

Status: Authoritative design analysis  
Scope: Source-of-truth order and failure classification during `BalanceTransition_Phase2_RootOn`

## 1. Purpose

This document defines how RootOn must interpret ownership and validity during the Phase 2 guard window.

It exists to prevent three recurring mistakes:

- treating intended ownership as if it were already applied
- treating modifier-record ownership as if it guaranteed raw-body state
- treating shell state as if it implied shell influence

## 2. Observables

During RootOn, the runtime must keep these observables separate:

### A. Certified topology intent
The certified Phase 1 handoff plus explicit RootOn choreography define what the topology is supposed to be.

### B. Modifier-record ownership
The `PAMod_*` body-modifier records define what PhysicsControl currently believes the movement type is.

### C. Raw body state
Chaos body instances define whether the underlying rigid bodies are actually simulating.

### D. Shell/reference state
Shell lock/reanchor/reseed state tells us what shell bookkeeping is active.

### E. Shell/reference influence
Material effect of shell/reference behavior on simulated bodies, measured through RootOn guard metrics such as shell deltas and induced motion.

## 3. Source-of-truth order

For RootOn failure classification, use this order:

1. certified topology intent
2. modifier-record ownership
3. raw body state
4. shell/policy influence metrics

Interpretation:

- intent mismatch is a contract mismatch
- modifier mismatch under correct intent is a write-routing / ownership mismatch
- raw mismatch under correct intent + modifier is an application / timing mismatch
- shell/policy influence is a separate guard-window materiality question

## 4. Preserved-proximal rule

If RootOn is defined to preserve the Phase 1 proximal sim set, then for:

- `thigh_l`
- `thigh_r`
- `spine_01`
- `spine_02`
- `spine_03`

all of the following are required by the end of the current RootOn application pass:

- intent = simulated
- modifier record = simulated
- raw body state = simulated

A disagreement among those three layers is not a cosmetic issue. It is a RootOn contract-relevant violation.

## 5. Root addition rule

When RootOn adds root/pelvis simulation, that change does not implicitly authorize:

- changing the preserved proximal set
- releasing policy into the transition set
- cached resets
- shell/reference assistance on simulated bodies

## 6. Same-frame disagreement rules

Same-frame disagreement may be observed transiently, but it must be classified explicitly.

Examples:

- intent = simulated, modifier = kinematic, raw = simulated
  - classify as modifier-record ownership disagreement
- intent = simulated, modifier = simulated, raw = kinematic
  - classify as raw-body application disagreement / delayed application
- shell locked = true, shell influence material = false
  - classify as shell state only, not shell-material violation
- shell locked = true, shell influence material = true
  - classify as shell-material violation

## 7. Guard-window denial rules

RootOn guard should deny truthfully when the first material failure is one of:

- policy leak into the transition set
- shell material influence on simulated bodies
- root simulation dropped
- topology not preserved
- preserved-proximal modifier/raw disagreement
- spike / instability

Do not collapse these into one generic no-convergence label.

## 8. Evidence rule

Every RootOn failure investigation must state:

- intended topology
- modifier-record topology
- raw-body topology
- whether shell state was present
- whether shell influence was material
- whether policy writes occurred
- first truthful failure reason

## 9. Acceptance

This truth model is satisfied only when RootOn decisions and denial reasons are based on explicit classification of:

- intended ownership
- modifier-record ownership
- raw body state
- shell/policy influence

and no hidden same-frame assistance is required to make the smoke appear successful.
