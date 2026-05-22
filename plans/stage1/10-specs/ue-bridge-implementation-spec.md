# UE Bridge Implementation Spec

## Purpose

This spec defines the implementation contract for the Stage 1 UE bridge that moves the project toward true balance mode.

It is scoped to the locked architecture:

`PoseSearch -> PHC Policy (NNE/ONNX) -> Physics Control Component -> Chaos Physics -> Renderer`

The bridge is successful only when it can enter `BalanceActive_Standing` and hold honest continuous balance for the V0 acceptance duration defined in [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md).

## Authority

This file defines implementation boundaries and module responsibilities. It does not redefine physical truth.

Authoritative dependencies:

- Runtime architecture and body sets: [continuous_balance_architecture.md](continuous_balance_architecture.md)
- Entry states and outcome vocabulary: [balance-mode-entry-spec.md](balance-mode-entry-spec.md)
- Execution order and data freshness: [engine_execution_contract.md](engine_execution_contract.md)
- Terminal truth and arbitration: [continuous_balance_truth_model.md](continuous_balance_truth_model.md)
- Authority ownership and forbidden writes: [authority_matrix.md](authority_matrix.md)
- Artifact fields and pass/fail thresholds: [instrumentation_and_acceptance.md](instrumentation_and_acceptance.md)
- Physics asset/static plant contract: [physics_asset_contract.md](physics_asset_contract.md)
- Capsule and CMC contract: [character_capsule_contract.md](character_capsule_contract.md)

If this file conflicts with one of those contracts, the more specific contract wins.

## Implementation Goal

The Stage 1 bridge must prove a minimal, honest standing loop:

1. Use PoseSearch to provide the authored standing/locomotion reference source.
2. Pack PHC policy inputs from live UE state and the selected reference.
3. Run the PHC ONNX model through UE5 NNE/ONNX Runtime.
4. Publish low-level actuation only through `UPhysicsControlComponent`.
5. Let Chaos own simulation truth for the balance-critical chain and support set.
6. Emit artifacts that can prove whether the attempt honestly balanced or failed.

The bridge must not create a second controller, bypass Physics Control, depend on TensorRT, author assets through a custom Python pipeline, or hide balance failures behind shell assistance.

## V0 Runtime Slice

V0 is intentionally narrow:

- Manny/Quinn skeleton only.
- Flat ground.
- Idle standing reference.
- Balance-critical chain continuously physical:
  - `pelvis`
  - `spine_01`
  - `spine_02`
  - `spine_03`
  - `thigh_l`
  - `thigh_r`
- Support truth from:
  - `foot_l`
  - `foot_r`
  - `ball_l`
  - `ball_r`
- Calf contact monitored as contamination, not valid support.
- No locomotion authority during the first true-balance acceptance path.
- No perturbation requirement before the 3.0s standing hold is green.

## Runtime State Contract

The bridge implementation must expose and obey the balance-first state names from [balance-mode-entry-spec.md](balance-mode-entry-spec.md):

1. `BalanceActivation_Ready`
2. `BalanceActivation_BlendIn`
3. `BalanceActivation_Validate`
4. `BalanceActive_Standing`
5. `BalanceActive_Recovery`
6. `SafeDenied` / `Failed`

Only `BalanceActive_Standing` is a successful V0 outcome.

`SafeDenied` may be useful diagnostic behavior, but it is not product success. Legacy `BridgeActive`, flip-handoff states, and grace-based proof states must not count as true balance-mode success.

## Module Responsibilities

### PoseSearch

PoseSearch owns motion/reference selection. The bridge may query or consume PoseSearch results, but it must not replace PoseSearch with custom motion matching logic.

Required implementation behavior:

- Validate that the configured PoseSearch database/schema assets are available before activation.
- Treat the first valid PoseSearch result as a startup prerequisite.
- Keep selected reference identity observable in the attempt artifact.
- Do not tune PoseSearch content inside balance-control implementation tasks unless a graph node explicitly scopes that work.

### PHC Policy Through NNE/ONNX

PHC policy inference belongs to UE5 NNE with ONNX Runtime.

Required implementation behavior:

- Use the locked Stage 1 tensor contract:
  - `self_obs = 358`
  - `mimic_target_poses = 6495`
  - `terrain = 256`
  - output `actions = 69`
- Reject model assets or tensor descriptors that violate the contract.
- Treat inference failure as startup/runtime failure, not as a reason to substitute scripted actions.
- Keep offline training/export outside UE runtime code.
- Do not introduce TensorRT.

### Observation Packing

Observation packing is bridge logic, but it must be deterministic and test-backed.

Required implementation behavior:

- Read source pose/reference data before physics publication.
- Read live physical truth only from the correct freshness point defined in [engine_execution_contract.md](engine_execution_contract.md).
- Keep unit conversions explicit.
- Keep SMPL-to-Manny/Quinn mapping assumptions isolated and testable.
- Log enough per-attempt metadata to connect observation shape, reference identity, model identity, and action output.

### Physics Control

Physics Control owns low-level actuation.

Required implementation behavior:

- Create and update controls through `UPhysicsControlComponent`.
- Publish target and gain changes during the PrePhysics bridge update path.
- Blend controller authority gradually onto an already-physical balance-critical chain.
- Keep gains/damping observable in the attempt artifact.
- Never bypass Physics Control with direct Chaos body forces for the V0 bridge.

### Chaos Truth

Chaos owns physical truth. The bridge may observe Chaos state and classify it, but it must not redefine success using bookkeeping state.

Required implementation behavior:

- Validate continuity for the balance-critical chain and support set.
- Accumulate support truth at substep cadence when that implementation layer exists.
- Reduce support patches and support hulls according to [engine_execution_contract.md](engine_execution_contract.md).
- Let raw body state win over modifier/control bookkeeping.

### Renderer

Renderer output is not an authority source for V0 truth.

Required implementation behavior:

- Visualization and debug overlays may mirror artifacts.
- No rendering-side state may influence pass/fail classification.

## Forbidden Authority Paths

The bridge must not:

- Use CharacterMovement or capsule correction as hidden standing assistance during V0 balance activation.
- Depend on global mesh kinematic updates for balance success.
- Treat shell containment, reanchoring, or broad body-locking as a successful standing proof.
- Count calf, thigh, pelvis, spine, or upper-body world bracing as support truth.
- Introduce custom Chaos force control instead of Physics Control.
- Tune PHC, action scale, support thresholds, locomotion, CMC, capsule, shell assist, or PoseSearch content unless the active graph node explicitly authorizes it.

## Acceptance Path

A V0 implementation slice can claim true-balance progress only when it moves one of these measurable gates forward:

1. More complete artifact truth for balance activation.
2. More continuous simulation of the balance-critical chain/support set.
3. Cleaner controller authority rollout onto an already-physical chain.
4. Fewer hidden authority conflicts.
5. Longer truthful hold duration in `BalanceActive_Standing`.

The first product-level V0 pass remains:

- `terminal_reason == nullptr`
- `hold_duration_sec >= 3.0`
- physical continuity validator passed
- primary thresholds stayed within limits
- final runtime outcome is `BalanceActive_Standing`

## TDD Requirements

Every deterministic bridge rule must have a test before or with implementation.

Required test classes by implementation area:

- Tensor contract validation tests for NNE model descriptors.
- Mapping/packing tests for observation vectors and action outputs.
- State-transition tests for balance-entry outcomes.
- Pure support truth tests for hull construction, side counting, proxy classification, and terminal reason arbitration.
- Authority conflict tests for forbidden CMC/shell/capsule/helper writes.
- Functional PIE smoke tests only after deterministic logic has unit coverage.

Exploratory spikes may create temporary evidence, but they are not complete until converted into deterministic tests and no permanent skip-by-design test remains.

## Evidence Requirements

Every balance-mode implementation node must leave evidence that states:

- graph node id/title
- command(s) run
- exact test names
- log/artifact paths
- final runtime outcome
- terminal reason, if any
- hold duration
- support mode and support hull status
- known limitations of the run

Evidence may support a safe denial or a failure diagnosis, but only a truthful `BalanceActive_Standing` hold can close a success claim.

## Migration Rule

The old activation/handoff path may remain as compatibility scaffolding while the rewrite is incomplete, but it is no longer the target design. New work should reduce dependency on flip-based ownership transitions and move toward continuous physical ownership of the balance-critical chain.
