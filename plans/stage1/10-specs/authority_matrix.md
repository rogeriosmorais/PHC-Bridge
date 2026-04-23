# Authority Matrix

## Purpose

This document defines subsystem ownership by runtime mode.

It is the primary operational doc for the continuous-balance rewrite.

Hard invariant:

- no subsystem may silently reclaim authority over the balance-critical chain

## Runtime Modes

| Runtime mode | Body simulation mode | Control targets | Locomotion intent | Movement component | Shell correction | Resets |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BridgeActive` | startup or normal runtime ownership; continuous-balance mode inactive | startup or normal bridge | allowed by normal bridge | normal runtime ownership | diagnostic or disabled per runtime | normal runtime ownership |
| `BalanceActivation_Ready` | balance activation owns continuous simulation for the balance-critical chain and support set | zeroed or rebased readiness targets only | forced idle | must not reclaim balance-critical chain or support set | disabled on balance-critical chain and support set in `V0` | blocked unless explicitly terminating attempt |
| `BalanceActivation_BlendIn` | balance activation owns continuous simulation for the balance-critical chain and support set | balance activation owns blended control targets | forced idle | must not reclaim balance-critical chain or support set | disabled on balance-critical chain and support set in `V0`; helper use is failure | blocked unless explicitly terminating attempt |
| `BalanceActivation_Validate` | balance activation owns continuous simulation for the balance-critical chain and support set | balance activation owns live standing targets | forced idle | must not reclaim balance-critical chain or support set | disabled on balance-critical chain and support set in `V0`; helper use is failure | blocked unless explicitly terminating attempt |
| `BalanceActive_Standing` | balance mode owns continuous simulation for the balance-critical chain and support set | balance mode owns live control targets | standing only unless a later mode expands scope | must not reclaim balance-critical chain or support set | disabled in `V0` | explicit only |
| `BalanceActive_Recovery` | balance mode owns continuous simulation unless the attempt is being terminated | recovery-specific control targets | recovery only | must not silently reclaim | disabled in `V0` | explicit recovery semantics only |
| `SafeDenied` / `Failed` | no ambiguous overlap allowed; balance activation releases cleanly | no further standing targets | inactive | restored only after termination is explicit | diagnostic only after termination | explicit termination handling only |

## Per-Mode Contract

| Runtime mode | Entry preconditions | Exit conditions | Fail conditions | Forbidden writes | Authoritative owner | Required emitted metrics |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BalanceActivation_Ready` | valid bridge context; balance-critical chain and support set simulated; no movement-component reclaim; no pending reset | enter `BalanceActivation_BlendIn` when rebased targets are valid and readiness quiet window passes | any topology change on critical chain; loss of continuous simulation; shell helper use on critical/support set; movement reclaim | locomotion-drive writes; shell helper writes; kinematic body-mode writes on critical/support set | balance activation runtime | topology change count, authority conflict count, readiness quiet time, shell helper used flag |
| `BalanceActivation_BlendIn` | all `Ready` conditions hold; rebased target history exists; `ControlAuthorityAlpha=0.0` | enter `BalanceActivation_Validate` when `ControlAuthorityAlpha=1.0` and no fail condition fired | target discontinuity, gain/damping instability, contact/support failure, pose/reference mismatch, authority conflict, shell helper use | abrupt full-authority writes, movement-component writes, shell helper writes, reset writes | balance activation runtime | alpha, blend start time, target discontinuity, controller effort proxy, authority conflict count, support uptime |
| `BalanceActivation_Validate` | blend complete; no fail condition in previous mode; standing timer reset | enter `BalanceActive_Standing` after contiguous hold completes | any support failure, instability threshold breach, topology change, non-contiguous hold, shell helper use, movement reclaim | shell helper writes, movement-component writes, topology edits on critical/support set, reset writes | balance activation runtime | contiguous hold time, root tilt envelope, peak angular speed by family, contact uptime, COM/support proxy drift |
| `BalanceActive_Standing` | contiguous hold complete | remain active or enter recovery/termination per later mode rules | loss of standing validity, explicit recovery trigger | legacy activation writes that bypass standing mode ownership | balance mode runtime | sustained hold time, ongoing stability metrics |

## Interpretation Rules

- any overlap that is not explicitly named in this matrix is a bug
- shell correction is never a primary truth source
- resets are termination or recovery tools, not hidden continuity helpers
- movement-component ownership must not silently drag or contain the balance-critical chain or support set during balance mode

## Required Runtime Event Counters

The runtime must surface at least:

- authority conflict count
- topology change event count
- reset event count
- shell helper used event count
