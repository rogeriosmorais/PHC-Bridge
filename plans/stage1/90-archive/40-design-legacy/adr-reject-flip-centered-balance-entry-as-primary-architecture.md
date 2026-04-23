# ADR: Reject Flip-Centered Balance Entry As The Primary Architecture

## Status

Accepted

## Decision

Reject flip-centered balance entry as the primary Stage 1 architecture.

The old canonical sequence:

- `Phase1_Prepare`
- `Phase1_LateValidate`
- `Phase2_RootOn`
- `Phase2_ReadyForPhase3`
- `Phase3_Settle`
- `BalancePerturbationActive`

is now legacy compatibility behavior, not the main design center.

## Why

- it kept the architecture centered on proving a handoff was safe
- it made it too easy to hide controller weakness behind transition explanations
- it gave shell-state and phase-completion concepts too much architectural weight
- it encouraged drift back into a flip-centered system

## Consequences

- the continuous-balance rewrite is a new primary architecture
- the old handoff system is frozen as legacy compatibility behavior
- success is defined by sustained physical standing, not by phase completion
