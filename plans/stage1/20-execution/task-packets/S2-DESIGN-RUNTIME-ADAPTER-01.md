# S2-DESIGN-RUNTIME-ADAPTER-01 — Runtime Adapter Snapshot Task Design

## Purpose

Design the next Slice 2 task packets for the runtime-adapter snapshot frontier before any runtime implementation begins.

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-01.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-02.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-03.md`
- `plans/stage1/20-execution/task-packets/S2-IMPL-RUNTIME-ADAPTER-04.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- production C++ files
- test C++ files
- runtime state-machine files
- bridge activation files
- PhysicsControl setup files
- capsule/CMC behavior files
- artifact emission files
- workflow/process files other than `execution-log.md`

## Required Inputs

- `AGENTS.md`
- `plans/stage1/20-execution/execution-log.md`
- this task packet
- `plans/stage1/20-execution/balance_first_tdd_strategy.md`
- `plans/stage1/20-execution/balance_first_test_matrix.md`
- `plans/stage1/20-execution/balance_first_rollout_protocol.md`
- `plans/stage1/10-specs/instrumentation_and_acceptance.md`
- `plans/stage1/10-specs/continuous_balance_truth_model.md`
- `plans/stage1/10-specs/authority_matrix.md`
- `plans/stage1/20-execution/balance_first_refactor_plan.md`

Broader context is explicitly allowed for this design task because the output is task-packet design, not implementation.

## Required Work

1. Design the smallest Slice 2 packet chain that advances from `Pure logic tests` to `Runtime adapter snapshot only`.
2. Keep runtime enforcement out of Slice 2 unless a later packet explicitly permits it.
3. Define packet-level allowed files, forbidden files, required tests, required build, scope check, and stop conditions.
4. Preserve the TDD rule: no production runtime code before a failing deterministic test exists.
5. Ensure each generated implementation packet has one owning surface and one measurable proof.
6. Do not create a custom harness or workflow engine.
7. Do not edit production or test C++ in this design task.
8. Update `execution-log.md` only after the packet chain is created and validated.

## Required Packet Chain

Create exactly these initial Slice 2 packets unless a contract gap is found:

1. `S2-IMPL-RUNTIME-ADAPTER-01.md`
   - introduce value-only runtime snapshot structs needed by adapter-fed validators
   - no live runtime reads yet
   - compile/build proof only, unless pure tests are needed for struct defaults

2. `S2-IMPL-RUNTIME-ADAPTER-02.md`
   - add deterministic tests for continuity snapshot semantics
   - no live `UWorld`, no `UObject`, no `FBodyInstance`

3. `S2-IMPL-RUNTIME-ADAPTER-03.md`
   - implement pure continuity validator over value snapshots
   - cover `VALID-01A` through `VALID-01D`

4. `S2-IMPL-RUNTIME-ADAPTER-04.md`
   - add first runtime adapter snapshot shell that converts live runtime state into the value snapshot without enforcing outcomes
   - must remain snapshot-only
   - no terminal routing
   - no runtime state-machine rewrite

## Required Tests

- not applicable; this is a task-packet design task

## Required Build

- not applicable unless packet creation tooling requires it

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-DESIGN-RUNTIME-ADAPTER-01.md -WorkingTree -AllowExecutionLog`

## Definition Of Done

- all four Slice 2 implementation packets exist
- each packet is small, TDD-driven, and has explicit allowed/forbidden files
- no production/test C++ files changed
- scope check passes
- one design commit is created
- `execution-log.md` points to `S2-IMPL-RUNTIME-ADAPTER-01`
- strict workflow validation passes

## Stop Conditions

Stop immediately if:
- the next implementation surface cannot be named precisely
- a packet would require broad runtime state-machine edits
- a packet cannot define a failing deterministic test before implementation
- the design requires a custom harness/workflow engine
- the design requires human/manual editor intervention
- the same conceptual uncertainty repeats twice

If stopped, write a pivot memo and set the execution log to blocked.

## Required Handoff

```text
Summary:
Task: S2-DESIGN-RUNTIME-ADAPTER-01
Base:
Head:
Commit:
Build: not applicable
Tests: not applicable
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task: S2-IMPL-RUNTIME-ADAPTER-01 or blocked
```
