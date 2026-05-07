# Weak-Agent BalanceActive Protocol

## Purpose

This document is the current handoff protocol for less capable agents working toward first truthful `BalanceActive_Standing >= 3.0s`.

It is deliberately conservative. It does not authorize an agent to work all the way to stability in one run. It defines the route, the branch points, and the conditions that force the agent to stop and return to mcp-graph.

## Current Rule

A weak executor may take only the next bounded graph task. It may not continue through a newly discovered blocker unless the graph already contains the next task and that task is selected by the normal pull flow.

Immediate pull target:

- `node_aac1a5d7dd3f` - `S1-PIVOT-GATE-AUDIT-ACTIVE-WIP-01`

The immediate route is not runtime implementation. It is workflow cleanup and route validation:

1. `node_aac1a5d7dd3f` - active WIP and pivot gate audit
2. `node_4470920e0349` - stability definition lock
3. `node_44709b2cdbca` - this weak-agent protocol
4. `node_2f73465e52b0` - lesser-agent execution packet
5. `node_b9ce2f9d69e8` - BalanceActive failure-mode branch map
6. `node_dbddc8ca345d` - design-change escalation gate
7. `node_a674c51a2ddb` - first-BalanceActive contract coverage audit
8. `node_9f67b82b1a67` - first-BalanceActive gap analysis

Only after that route says the path is executable should a weak agent receive the Phase 3 implementation lane:

- `node_78833e2a534a` - Phase 3 instability decision
- `node_fe74d8d94e21` - pre-entry plant/capsule audit
- `node_deea5e3bca44` - runtime truth arbitration check
- `node_04eead4961bc` - artifact schema acceptance check
- `node_41078a2fd9cf` - narrow Phase 3 post-RootOn instability fix
- `node_fbd95aec7ed4` - first BalanceActive product-success gate
- `node_8d94b0d7e2a3` - standing stability regression/soak

## Product Success Definition

The only first-stability success is:

- runtime state reaches `BalanceActive_Standing`
- standing hold is continuous for at least `3.0` seconds
- `terminal_reason` is null / none
- `physical_continuity_validator_passed` is true
- support/contact truth comes from live physics
- all primary metrics remain within the active thresholds
- no hidden authority assistance is present

These are not product success:

- `BridgeActive`
- `BalanceActivation_Ready`
- `BalanceActivation_BlendIn`
- `BalanceActivation_Validate`
- `BalanceActive_Recovery`
- `SafeDenied`
- diagnostic smoke success
- visually calmer motion without artifact proof

## Authoritative Gates

Use these graph nodes and contracts when judging work:

- `node_fbd95aec7ed4` - first BalanceActive product-success gate
- `node_828bafe36d66` - authority matrix
- `node_afc6c72a4054` - balance mode entry vocabulary
- `node_b895d7c254ff` - continuous balance truth model
- `node_c6e2c62a4bc8` - instrumentation and acceptance
- `node_b2b00018cb16` - physics asset contract
- `node_cf01194d0393` - character capsule contract
- `node_d3629903781c` - standing positive scenario
- `node_70ee06beb532` - standing negative support scenario

If a generic duplicate contract appears in context, use the authoritative detailed node instead. Generic nodes are pointers, not closure authority.

## Branch Rules

### BalanceActive Success

If smoke reaches `BalanceActive_Standing >= 3.0s`:

1. Do not declare done immediately.
2. Run the product-success gate `node_fbd95aec7ed4`.
3. If the gate passes, route to soak `node_8d94b0d7e2a3`.
4. If the gate fails, mark the current task blocked and name the failed gate.

### Same Terminal Reason Repeats

If the same terminal reason appears after two focused attempts:

1. Stop implementation.
2. Record tested hypotheses and artifact paths.
3. Mark the task blocked.
4. Route to `node_8281119123bb` if the repeated reason is Phase 3 related.

### New Truthful Terminal Reason

If a new terminal reason appears:

1. Verify it is canonical through `node_deea5e3bca44`.
2. If canonical and reconstructible, create or identify the next narrow graph task.
3. If non-canonical or not reconstructible, block on truth/arbitration or artifact schema work.

### Missing Artifact Fields

If required artifact fields are missing:

1. Do not infer from visuals or stale logs.
2. Route to `node_04eead4961bc`.
3. If the missing field is plant or capsule related, route to `node_fe74d8d94e21`.

### Build Or Test Failure

If build or required test fails:

1. Do not widen scope.
2. Fix only if the failure is inside the current task's allowed surface.
3. If the fix needs another surface, mark blocked and create or identify the next graph node.

### Architecture-Lock Pressure

If the evidence suggests changing PoseSearch ownership, bypassing NNE/ONNX, moving actuation out of Physics Control, replacing Chaos semantics, or changing the V0 proof target:

1. Stop.
2. Route to `node_dbddc8ca345d`.
3. Do not implement the design change inside the current task.

## Forbidden Moves

Never do these in a weak-agent implementation task:

- declare `SafeDenied` as success
- accept threshold-only success
- add hidden CMC assistance
- use shell locking or kinematic rescue to pass
- count excluded-body or calf bracing as support truth
- relax support/contact truth to make a run pass
- introduce TensorRT
- create a custom Python pipeline for UE5 asset authoring
- start broad demo or G3 work before G2 evidence is unblocked
- continue after two unchanged terminal reasons

## Required Handoff

Every weak-agent handoff must include:

```text
Graph task:
Command(s) run:
Current artifact/log path:
Observed terminal reason:
Product success gate status: pass/fail/not-run
Branch taken:
Next graph node:
Ledger impact: none/updated/blocked
```

If the agent cannot fill this out, the task is not ready for a weak executor.
