# Single-Character Implementation Package

## Purpose

This package defines the current single-character Stage 1 implementation scope for true balance mode.

It supersedes the archived Phase 1 staged-bringup package for current implementation planning. It does not supersede `STAGE1_PLAN.md` or the active `10-specs` contracts.

## Target

One Manny/Quinn-derived runtime character must run the locked architecture:

```text
PoseSearch -> PHC Policy (NNE/ONNX) -> Physics Control Component -> Chaos Physics -> Renderer
```

The implementation target is not visual plausibility and not safe denial. The target is `BalanceActive_Standing` held continuously for `3.0` seconds under live physics truth.

## Active Inputs

- `STAGE1_PLAN.md`
- `plans/stage1/10-specs/balance-mode-entry-spec.md`
- `plans/stage1/10-specs/continuous_balance_architecture.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/10-specs/authority_matrix.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`
- `plans/stage1/10-specs/smpl-ue5-retargeting-spec.md`
- `plans/stage1/20-execution/stage1-test-strategy.md`
- `plans/stage1/20-execution/balance_first_refactor_plan.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `plans/stage1/20-execution/assumption-ledger.md`

Historical packages under `plans/stage1/90-archive` are context only.

## Implementation Scope

In scope:

- continuously physical balance-critical proximal chain
- flat-ground idle stance
- live support/contact truth
- PHC action and observation contract checks
- Physics Control targets and gains through UE built-ins
- failure reasons and artifacts sufficient to explain every terminal outcome
- targeted diagnostics for body-level instability, including thigh/hip evidence

Out of scope until standing is honest:

- locomotion handoff
- perturbation recovery beyond small diagnostic probes
- broad motion set expansion
- visual polish
- hidden world bracing, shell locking, or mesh-wide kinematic assistance
- custom training or UE asset-authoring pipelines

## Required Test Gates

Use PowerShell from repo root.

| Change type | Minimum gate |
|---|---|
| UE C++ compile impact | `.\scripts\build.ps1` |
| Bridge/retargeting contract | `.\scripts\build.ps1 -Test PhysAnim.Bridge.SmplOrderContract` and relevant `PhysAnim.Bridge.*` test |
| Support/contact truth | focused `PhysAnim.SupportTruth.*` or `PhysAnim.RuntimeAdapter.Support*` test |
| Validator/failure truth | focused `PhysAnim.Validators.*` test |
| Balance-state or success rule | `.\scripts\build.ps1 -Test PhysAnim.Component.BalanceModeSmokeOutcome` plus relevant state-machine test |
| Runtime balance claim | `.\scripts\build.ps1 -Test PhysAnim.PIE.BalanceModeSmoke` |
| Thigh/hip diagnostic claim | `.\scripts\build.ps1 -Test PhysAnim.Diagnostics.ThighRestore` |

Runtime evidence must be read from `F:\NewEngine-AgentB\PhysAnimUE5\Saved\Logs`.

## Success Rule

A single-character implementation slice counts as balance progress only if the evidence improves one of these:

- live physical ownership continuity
- support/contact truth
- PHC-to-Physics-Control target fidelity
- controller stability
- failure attribution quality
- continuous standing hold duration

A slice counts as product success only when the run reaches:

```text
BalanceActive_Standing
```

and holds it for:

```text
3.0 continuous seconds
```

## Current Risks

- `A-06`: thigh restore diagnostics are not physical viability proof.
- `A-08`: evidence helpers may read stale sibling-checkout logs; current-repo log provenance is required.
- `A-09`: runtime ONNX assets exist, but original offline checkpoint provenance is not confirmed in this checkout.

## Next Implementation Preference

Prefer the next graph task that directly increases observability or stability of true balance mode over historical Phase 1 packaging work.

If the graph offers stale staged-bringup tasks, reconcile them into balance-first equivalents before implementation.
