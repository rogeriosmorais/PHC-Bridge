# Refactor Code Ownership Map

Status: Working ownership note  
Scope: High-level runtime ownership for Stage 1 balance-entry refactor

## Ownership map

| File | Responsibility |
| :--- | :--- |
| `PhysAnimComponent.Core.cpp` | Component tick entry, runtime-state gating, bridge lifecycle, and frame-level orchestration around transition execution. |
| `PhysAnimComponent.Balance.cpp` | Top-level balance-mode entry points and bridge-facing balance control handoff into the transition runtime. |
| `PhysAnimBalanceReadyTransition.cpp` | Thin transition translation unit and shared compile unit anchor for the split `BalanceReadyTransition` implementation. |
| `PhysAnimBalanceReadyTransition.Core.cpp` | Main Phase 1 / RootOn / Settle phase machine flow, timers, phase changes, and terminal routing. |
| `PhysAnimBalanceReadyTransition.Readiness.cpp` | Readiness gates and continuity checks that decide whether Phase 1 can advance, RootOn can begin, and Settle can remain valid. |
| `PhysAnimBalanceReadyTransition.Certification.cpp` | Certified handoff snapshot building, frozen-topology interpretation, and Phase 1 to RootOn certification results. |
| `PhysAnimBalanceReadyTransition.LateValidation.cpp` | LateValidate and pre-RootOn proof validation rules, including explicit RootOn readiness proof checks. |
| `PhysAnimBalanceReadyTransition.PolicyAndShell.cpp` | Ownership suppression rules for policy, resets, shell influence, move-smoke suppression, and per-bone kinematic/sim routing during transition phases. |
| `PhysAnimBalanceReadyTransition.Diagnostics.cpp` | Failure classification, audit logging, phase-return diagnostics, and transition-facing reason reporting. |
| `PhysAnimBalanceReadyTransition.Helpers.cpp` | Shared helper logic used by multiple transition phases where ownership is not phase-defining on its own. |

## Transition concern mapping

- `Phase 1` is primarily owned by `PhysAnimBalanceReadyTransition.Core.cpp`, with certification in `PhysAnimBalanceReadyTransition.Certification.cpp`, readiness gating in `PhysAnimBalanceReadyTransition.Readiness.cpp`, and suppression rules in `PhysAnimBalanceReadyTransition.PolicyAndShell.cpp`.
- `RootOn` is primarily owned by `PhysAnimBalanceReadyTransition.Core.cpp`, with readiness and precondition truth in `PhysAnimBalanceReadyTransition.Readiness.cpp`, certification/truth-source interpretation in `PhysAnimBalanceReadyTransition.Certification.cpp`, and guard-window suppression in `PhysAnimBalanceReadyTransition.PolicyAndShell.cpp`.
- `Settle` is primarily owned by `PhysAnimBalanceReadyTransition.Core.cpp` and `PhysAnimBalanceReadyTransition.Readiness.cpp`, with failure truthfulness and reporting in `PhysAnimBalanceReadyTransition.Diagnostics.cpp`.

## Interpretation rule

This map reflects the current split already present in the runtime source tree. It is an ownership reference for review and refactor navigation, not a proposal to change the architecture or move behavior between subsystems.
