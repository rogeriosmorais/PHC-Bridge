# Truth Model For Continuous Balance

## Purpose

This document replaces the handoff-centered truth model for the new architecture.

Core rule:

- success is sustained physical stability under continuous simulation
- no phase completion, shell status, or compatibility label can substitute for that

## Primary Truth Sources

The primary truth sources for continuous balance are:

1. root pose and root tilt
2. COM behavior or support proxy
3. contact persistence
4. body angular and linear stability
5. sustained hold duration

Shell metrics may explain failure, but cannot certify balance.

## Secondary Diagnostics

These may explain why the primary truth sources failed:

- shell bookkeeping state
- shell influence metrics
- modifier-record bookkeeping
- controller effort proxies
- authority conflict events

None of these secondary diagnostics can certify success by themselves.

## Control-First Failure Taxonomy

Every failure in the new mode must be classified first as one of:

- target discontinuity
- gain or damping instability
- contact or support failure
- pose or reference mismatch
- authority conflict

Only after that may the runtime add secondary context such as shell influence or bookkeeping disagreement.

## Legacy Mapping

The old labels remain available only for migration and comparison.

| Legacy label | New primary meaning |
| :--- | :--- |
| `RootOn spike` | controller instability, target discontinuity, or authority conflict |
| `Settle instability` | sustained-balance failure under continuous physics |
| `phase3_material_shell_correction` | shell influence material, but still secondary to primary physical truth |
| `topology regressed` | topology change event on the balance-critical chain |

## Success Rule

A run is successful only when:

- the balance-critical chain remained continuously simulated
- the controller remained applied without forbidden authority conflict
- standing stability held continuously for the configured duration

No secondary diagnostic may override that rule.
