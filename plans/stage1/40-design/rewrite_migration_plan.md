# Rewrite Migration Plan

## Purpose

This document defines what stays, what is wrapped, what is deleted, and what is shadow-run only during the continuous-balance rewrite.

This is the explicit migration and deletion contract for the rewrite.

## Legacy Codepaths That Remain Temporarily

- existing bridge startup and inference path
- existing flip-centered handoff implementation as a legacy compatibility subsystem
- compatibility log labels that still mention `Phase1`, `Phase2`, `Phase3`, `RootOn`, or `Settle`

## What May Be Wrapped But Not Extended

- old handoff state-machine logic
- legacy labels used only for comparison or compatibility
- shell-heavy transition diagnostics that are still useful as secondary failure explanations

Do not extend these paths with new primary-balance behavior.

Freeze rule:

- legacy codepaths may be wrapped for comparison or compatibility only
- they must not receive new success logic, new assistance logic, or new architecture-defining behavior

## Compatibility-Only Symbols And Labels

These are compatibility-only until removed:

- `BridgeActive_Physical`
- `BalanceActivation_StandingValidation`
- legacy `RootOn` / `Settle` labels
- legacy phase-oriented counters or audits

Preferred rewrite names are:

- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Standing`

## What Is Being Deleted, Not Ported

- flip-centered success criteria
- handoff completion as the architectural center
- shell status as a success substitute
- any assumption that balance-critical topology changes are normal operation

## What Is Scheduled For Deletion

Delete the old path only after all stop-using criteria are met:

- phase-completion counters as success signals
- shell-certification logic as a success signal
- retry logic that exists only to rescue the legacy handoff path
- legacy top-level design docs as primary references

## Stop-Using Criteria For The Old Path

The old handoff path stops being a primary execution path only when:

- instrumentation is trustworthy
- the authority matrix is enforced
- the continuous-balance truth model explains failures honestly
- the smallest always-sim proximal prototype is stable enough to compare
- the new mode can be shadow-run against the legacy mode with consistent artifact output

## Shadow-Run Only

Run the new continuous-balance mode in parallel with the legacy mode until:

- instrumentation is trustworthy
- the authority matrix is enforced
- the new truth model is stable enough to explain failures honestly

Only then begin deleting old handoff logic.

## Recommended Order

1. truth model
2. authority matrix
3. instrumentation and acceptance
4. smallest always-simulated proximal prototype
5. shadow-run comparison with the legacy path
6. deletion of old handoff logic after trust is earned
