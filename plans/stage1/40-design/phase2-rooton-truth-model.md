# Phase 2 / RootOn Truth Model

Status: Authoritative design analysis  
Scope: Controller-blend truth sources for balance-first activation

## Legacy Filename Note

This filename is retained for compatibility.

The target design is no longer `RootOn`. This document now defines how the runtime truthfully measures controller blend-in on an already-physical state.

## 1. Purpose

This document defines how controller blend-in must interpret authority and validity during balance activation.

It exists to prevent these mistakes:

- treating controller intent as if it were already applied safely
- treating rising control alpha as proof of physical stability
- treating shell bookkeeping as proof that blend-in is uncontaminated

## 2. Observables

During controller blend-in, the runtime must keep these observables separate:

### A. Blend intent

The target controller-authority ramp and activation contract.

### B. Raw physical state

The actual physical response of the balance-critical chain under the current blend level.

### C. Control-layer bookkeeping

What the bridge believes it is publishing and at what authority alpha.

### D. Shell bookkeeping state

Whether lock, anchor, or reseed bookkeeping exists.

### E. Shell influence materiality

Whether shell behavior materially affects the balance-critical chain during blend-in.

## 3. Source-Of-Truth Order

For blend-in failure classification, use this order:

1. blend intent
2. raw physical state
3. control-layer bookkeeping
4. shell bookkeeping state
5. shell influence materiality

Interpretation:

- blend intent is not proof of safe application
- raw physical state is the deciding proof of blend success or failure
- control-layer bookkeeping is routing evidence
- shell bookkeeping is not shell influence

## 4. Denial Rules

Blend-in should deny truthfully when the first material failure is one of:

- ownership continuity lost
- instability caused by the current blend level
- control application bypassed intended blend semantics
- shell influence became material

Do not collapse these into a generic no-convergence label.

## 5. Acceptance

This truth model is satisfied only when controller blend decisions and denial reasons are based on explicit classification of:

- blend intent
- raw physical state
- control-layer bookkeeping
- shell bookkeeping
- shell influence

and no authoritative document treats a certified handoff or ownership flip as the target proof of activation success.
