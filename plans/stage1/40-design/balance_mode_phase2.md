# Balance Mode Phase 2 Root-On Spec

Status: Authoritative implementation design  
Scope: Stage 1 controller blend-in behavior

## Legacy Filename Note

This filename is retained for compatibility.

The target design is no longer `RootOn`. This document now defines how controller authority blends onto an already-physical balance-critical chain.

## 1. Purpose

This document defines the controller blend-in choreography for Stage 1 balance activation.

It is authoritative for:

- blend-in entry preconditions
- controller-authority ramp behavior
- failure and retry semantics during blend-in
- shell influence evaluation during blend-in

## 2. Relationship To Physical Readiness And Standing Validation

Blend-in consumes a still-valid `BridgeActive_Physical` state.

If physical readiness is contract-correct but physically non-viable, blend-in must not pretend otherwise.

Blend-in is also not standing validation:

- controller blend-in means authority ramp only
- standing validation means post-blend sustained physical stability

## 3. Core Rule

Blend-in is not “assert full controller authority and hope.”

Blend-in may begin only from a still-valid physically simulated state.

If the required ownership continuity or quiet-state proof is absent, the runtime must deny safely before blend-in.

## 4. Required Entry Preconditions

Blend-in may begin only if all are true:

- a valid balance-critical chain exists
- raw state confirms continuous simulation of that chain
- no reset is pending
- no conflicting locomotion authority is active
- shell influence is not already materially active
- the runtime has a real path to standing validation

## 5. Controller-Authority Rule

During `BalanceActivation_BlendIn`:

- control authority must ramp gradually
- abrupt full-strength activation is not the intended path
- action publishing must be interpreted together with control-authority alpha
- topology flips are not the intended success mechanism

## 6. Shell Rule

During blend-in, distinguish:

- shell bookkeeping (`locked`, `reanchored`, `reseeded`, or equivalent)
- shell influence on the balance-critical chain

The former may exist without failure.

Material shell influence on the balance-critical chain is terminal.

## 7. Failure Taxonomy

Use the following current target taxonomy:

| Failure reason | Meaning | Retry status |
| :--- | :--- | :--- |
| `activation_physical_ownership_lost` | the balance-critical chain lost continuous simulation | terminal |
| `activation_blend_instability` | controller blend caused truthful instability | not retryable |
| `activation_policy_write_leak` | writes bypassed intended blend semantics | terminal |
| `activation_shell_influence_material` | shell influence became materially active | terminal |
| `activation_reset_violation` | reset or conflicting authority contaminated the attempt | terminal |

Do not collapse these into a generic no-convergence label.

## 8. Acceptance Criteria

This spec is satisfied only when blend-in:

- begins from an already-physical balance-critical chain
- ramps controller authority gradually
- distinguishes shell bookkeeping from shell influence
- treats topology flips as non-target legacy behavior rather than the intended path
