# Phase 1 / LateValidate Truth Model

Status: Authoritative design contract  
Scope: Ownership continuity truth sources for balance-first activation

## Legacy Filename Note

This filename is retained for compatibility.

The target design is no longer `Prepare` / `LateValidate`. This document now defines how the runtime truthfully measures ownership continuity before and during activation.

## 1. Purpose

This document defines how the runtime must interpret ownership continuity for the balance-critical chain.

It exists to prevent these mistakes:

- treating intended ownership as proof of raw physical continuity
- treating modifier-record ownership as proof of raw physical continuity
- treating shell bookkeeping as proof of shell influence

## 2. Observables

The runtime must keep these observables separate:

### A. Intended ownership continuity

What the activation contract expects for the balance-critical chain.

### B. Raw body simulation state

What Chaos actually reports for the balance-critical chain.

### C. Modifier-record ownership

What the control layer believes it has applied.

### D. Shell bookkeeping state

What shell lock, anchor, or reseed bookkeeping is active.

### E. Shell influence materiality

Whether shell behavior is materially affecting the balance-critical chain.

## 3. Source-Of-Truth Order

For ownership-continuity classification, use this order:

1. intended ownership continuity
2. raw body simulation state
3. modifier-record ownership
4. shell bookkeeping state
5. shell influence materiality

Interpretation:

- intended ownership defines what the runtime is trying to maintain
- raw body state is the deciding proof of continuity
- modifier state is routing evidence, not deciding proof
- shell bookkeeping is not shell influence

## 4. Classification Rules

Examples:

- intended continuity present, raw simulation present, modifier disagrees
  - classify as modifier-record disagreement
- intended continuity present, raw simulation lost
  - classify as ownership continuity failure
- shell bookkeeping present, shell influence not material
  - classify as bookkeeping only
- shell bookkeeping present, shell influence material
  - classify as shell influence failure

## 5. Acceptance

This truth model is satisfied only when ownership continuity decisions are based on explicit classification of:

- intended ownership
- raw body state
- modifier-record ownership
- shell bookkeeping
- shell influence

and no authoritative document treats a later topology flip as the target proof of success.
