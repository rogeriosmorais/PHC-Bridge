# Slice 1 Definition: Support Logic Core

## 1. Scope

This is the first coding slice of the balance-first rewrite. It focuses exclusively on the pure geometric and temporal logic of the support pipeline.

**Targets**: See the authoritative [Slice 1 Commit Plan](balance_first_refactor_plan.md#8-slice-1-commit-plan).

## 2. Why This Slice First

- **Pure Logic**: This logic can be implemented and tested as pure functions with zero Unreal Engine dependencies.
- **High Leverage**: This forms the foundation of the support truth-set, which is the most critical part of the new architecture.
- **TDD Discipline**: Provides a narrow, deterministic surface to establish the red-green-refactor workflow before touching runtime state.

## 3. Implementation Rules

- **Zero Runtime Surgery**: Do not touch existing bridge state-machine or handoff logic.
- **Pure Functions**: Implementation must use pure inputs (structs/arrays) and produce pure outputs.
- **Contract Alignment**: Use the exact field names and types defined in the 10-spec suite.
- **Repo-Local Paths**: Follow the plugin's existing layout: `Public/PhysAnimTruthTypes.h`, `Public/PhysAnimSupportTruth.h`, `Private/PhysAnimSupportTruth.cpp`, and `Private/PhysAnimSupportTruth.Tests.cpp`.

## 4. Definition of Done

- **Tests First**: Every logic path has a corresponding failing unit test before implementation.
- **Green Suite**: All Layer 1 unit tests pass.
- **No Side Effects**: The code produces no side effects and is decoupled from the `PhysAnimBridge` runtime.
- **Review Ready**: The logic is forensically verifiable against the `engine_execution_contract.md`.

## 5. Next Step

Once Slice 1 is green, the next slice will be **Validator Extraction** to begin isolating the contract audits.
