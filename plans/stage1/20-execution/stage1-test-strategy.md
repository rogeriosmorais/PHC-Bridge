# Stage 1 Test Strategy

## Purpose

This document defines the repo-level test strategy for Stage 1 true balance mode.

It complements the detailed TDD strategy and matrix by defining which suites must protect each contract surface, how evidence is produced, and which outcomes are allowed to close graph tasks.

## Test Pyramid

Stage 1 uses this pyramid, from fastest and most deterministic to slowest and most evidentiary:

1. **C++ deterministic automation tests** protect bridge contracts, state classification, validators, support truth, action conditioning, and SMPL/UE mapping.
2. **Python offline tests** protect training/runtime contract compatibility, ONNX export shape and naming, IsaacLab compatibility shims, and retargeting assumptions.
3. **UE integration automation tests** protect runtime component behavior that needs UE objects but does not require a full PIE run.
4. **PIE functional smoke tests** produce evidence for real runtime outcomes, especially `PhysAnim.PIE.BalanceModeSmoke`, `PhysAnim.Diagnostics.ThighRestore`, and activated-standing diagnostics.
5. **Manual or captured evidence review** is allowed only as supporting context. It never replaces deterministic tests or automation logs.

The lower layer must exist before relying on a higher layer for a task. Exploratory spikes must be converted into deterministic checks before closure.

## Command Gates

Use PowerShell from the repo root.

| Gate | Command | Use |
|---|---|---|
| Compile | `.\scripts\build.ps1` | Required after UE C++ changes. |
| Targeted UE automation | `.\scripts\build.ps1 -Test <TestName>` | Required for the specific changed contract surface. |
| Balance mode PIE smoke | `.\scripts\build.ps1 -Test PhysAnim.PIE.BalanceModeSmoke` or `.\scripts\run-pie-balance-mode.ps1` | Required when claiming balance activation progress. |
| Thigh restore diagnostics | `.\scripts\build.ps1 -Test PhysAnim.Diagnostics.ThighRestore` | Required when closing thigh Physics Control mitigation diagnostics. |
| Python offline contract | `pytest Training/tests -v` or a focused `pytest Training/tests/<file>.py -v` | Required after training/export/retargeting contract changes. |

If smoke scripts are used, read logs immediately after the run. The evidence source must be the current repo log under `PhysAnimUE5\Saved\Logs\`, not a stale sibling checkout.

## Required UE Test Families

| Surface | Representative tests | Required before closure |
|---|---|---|
| Bridge tensor and mapping contracts | `PhysAnim.Bridge.TensorIndexMap`, `PhysAnim.Bridge.InputDescriptorContract`, `PhysAnim.Bridge.ActionOutputDescriptorContract`, `PhysAnim.Bridge.SmplOrderContract`, `PhysAnim.Bridge.FrameConversion`, `PhysAnim.Bridge.ActionToBoneMappingContract` | Any observation, action, ONNX, or retargeting change. |
| Support truth | `PhysAnim.SupportTruth.*`, `PhysAnim.RuntimeAdapter.Support*` | Any contact, support region, proxy, churn, or support reduction change. |
| Validators and terminal truth | `PhysAnim.Validators.*`, `PhysAnim.Validators.ArtifactArbitration` | Any continuity, capsule, plant, authority, controller-stability, or artifact change. |
| Balance state and activation | `PhysAnim.Component.BalanceModeSmokeOutcome`, `PhysAnim.Component.BalanceStateClassification`, `PhysAnim.Component.RuntimeStateOwnership`, `PhysAnim.StateMachine.Phase1Entry`, `PhysAnim.StateMachine.Phase2Standing` | Any activation-state or success-condition change. |
| Runtime standing evidence | `PhysAnim.PIE.BalanceModeSmoke`, `PhysAnim.ActivatedStanding.StabilityMetrics`, `PhysAnim.Diagnostics.ThighRestore` | Any claim about physical balance progress. |

## Balance Acceptance Rule

The only passing product benchmark is:

- terminal or published runtime state is `BalanceActive_Standing`
- standing hold is continuous for at least `3.0` seconds
- `terminal_reason` is empty or equivalent to no failure
- support/contact truth is emitted from live physics
- hidden assistance is absent according to the authority matrix

The following are not passing balance outcomes:

- `BridgeActive`
- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Recovery`
- safe denial
- a timeout with truthful diagnostics
- a pass caused by shell locking, mesh-wide kinematic help, locomotion authority, or world bracing

## Evidence Rules

Every graph task that claims balance progress must include:

- exact command run
- exact UE automation test name or script
- current-repo log path
- key log lines or artifact fields that prove the expected state
- failure reason when the run does not reach `BalanceActive_Standing`

Diagnostic tests may close diagnostic tasks when they prove observability, command coverage, and failure attribution. They may not close physical viability tasks unless they also prove the balance acceptance rule.

## No Permanent Skip Rule

No new permanent skip-by-design tests may be added to the main suite. If an environment dependency is missing, the task must either:

- use an already accepted environment-conditional test pattern for external tooling, or
- create a deterministic contract test that does not require that dependency.

Skip markers in historical Python tests are not authority to add more skips for Stage 1 balance runtime work.

## Graph Closure Rule

Before `finish_task`, run the narrowest sufficient test gate and record the artifact path in the graph result.

For implementation tasks, the expected rhythm is:

1. write or update the failing deterministic test
2. implement the smallest production change
3. run the focused gate
4. run `.\scripts\build.ps1` when UE code changed
5. collect smoke evidence only when the task claims runtime behavior
6. validate acceptance criteria through mcp-graph
7. finish the graph task

Do not close a task on documentation, tuning notes, or manual observation when a deterministic test could be written.
