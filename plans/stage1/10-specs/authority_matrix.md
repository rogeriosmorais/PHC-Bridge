# Authority Matrix

## Purpose

This document defines subsystem ownership by runtime mode.

It is the primary operational doc for the continuous-balance rewrite.

Hard invariant:

- no subsystem may silently reclaim authority over the balance-critical chain

## Runtime Modes

| Runtime mode | Body simulation mode | Control targets | Locomotion intent | Movement component | Shell correction | Resets |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BridgeActive` | startup or normal runtime ownership; balance chain may be preparing but not yet rewrite-valid | startup or normal bridge | allowed by normal bridge | normal runtime ownership | diagnostic or disabled per runtime | normal runtime ownership |
| `BridgeActive_Physical` | balance activation owns continuous simulation for the balance-critical chain | startup-safe or zeroed blend targets only | forced idle for continuous-balance mode | must not reclaim balance-critical chain | diagnostic only, not success-defining | blocked unless explicitly terminating attempt |
| `BalanceActivation_BlendIn` | balance activation owns continuous simulation for the balance-critical chain | balance activation owns blended control targets | forced idle | must not reclaim balance-critical chain | helper or diagnostic only; cannot certify success | blocked unless explicitly terminating attempt |
| `BalanceActivation_StandingValidation` | balance activation owns continuous simulation for the balance-critical chain | balance activation owns live standing targets | forced idle | must not reclaim balance-critical chain | diagnostic only; may explain failure | blocked unless explicitly terminating attempt |
| `BalanceActive_Standing` | balance mode owns continuous simulation for the balance-critical chain | balance mode owns live control targets | standing only unless a later mode expands scope | must not reclaim balance-critical chain | diagnostic only | explicit only |
| `BalanceActive_Recovery` | balance mode owns continuous simulation unless the attempt is being terminated | recovery-specific control targets | recovery only | must not silently reclaim | diagnostic only | explicit recovery semantics only |
| `SafeDenied` / `Failed` | no ambiguous overlap allowed; balance activation releases cleanly | no further standing targets | inactive | restored only after termination is explicit | diagnostic only | explicit termination handling only |

## Interpretation Rules

- any overlap that is not explicitly named in this matrix is a bug
- shell correction is never a primary truth source
- resets are termination or recovery tools, not hidden continuity helpers
- movement-component ownership must not silently drag or contain the balance-critical chain during balance mode

## Required Runtime Event Counters

The runtime must surface at least:

- authority conflict count
- topology change event count
- reset event count
- shell influence event count
