# Balance Mode Entry Spec

## Purpose

This document is a shallow entry point for the Stage 1 balance-activation contract. It defines the canonical state names and maps legacy concepts to the new architecture.

**This file is NOT an authority surface for timing, ownership, or physical truth.**

## Authoritative Documentation

The balance-activation contract is defined by the following authoritative documents:

- **Timing and Cadence**: [engine_execution_contract.md](engine_execution_contract.md)
- **Failure Truth and Arbitration**: [continuous_balance_truth_model.md](continuous_balance_truth_model.md)
- **Subsystem Ownership and States**: [authority_matrix.md](authority_matrix.md)
- **Instrumentation and Acceptance Gates**: [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)
- **Structural Body Sets**: [continuous_balance_architecture.md](continuous_balance_architecture.md)

## Canonical Runtime States

The authoritative runtime states for balance activation are:

1. `BalanceActivation_Ready`
2. `BalanceActivation_BlendIn`
3. `BalanceActivation_Validate`
4. `BalanceActive_Standing`
5. `BalanceActive_Recovery`
6. `SafeDenied` / `Failed` (Terminal Outcomes)

The detailed entry preconditions, exit conditions, and failure reasons for each mode are defined exclusively in the [Authority Matrix](authority_matrix.md) and the [Truth Model](continuous_balance_truth_model.md).

**Note on Plant Audit**: Passing the full physical plant audit (See [physics_asset_contract.md](physics_asset_contract.md)) is a **prerequisite** for entering the balance state machine; it does not occur during `BalanceActivation_Ready`.

## Authoritative State Mapping

Legacy filenames and code symbols (Phase 1/2/3, LateValidate, RootOn, Settle) are mapped to the new flow as follows:

| Legacy Concept | New Architectural State |
| :--- | :--- |
| `Prepare` / `LateValidate` | `BalanceActivation_Ready` |
| `RootOn` | (Superseded ownership-flip concept; not part of target design) |
| `Settle` | `BalanceActivation_Validate` |
| `BalanceActive` | `BalanceActive_Standing` |

No implementation detail is allowed that relies on logic from this demoted spec that is not present in the authoritative documents listed above.
