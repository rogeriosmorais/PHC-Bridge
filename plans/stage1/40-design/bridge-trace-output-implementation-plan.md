# Bridge Trace Output Implementation Plan

## Goal

Implement the bridge trace output described in [bridge-trace-output-design.md](/F:/NewEngine/plans/stage1/40-design/bridge-trace-output-design.md) as an opt-in Stage 1 diagnostic path.

The goal is not to build a generic telemetry framework.

The goal is to produce one structured, session-scoped artifact that makes bridge runtime behavior inspectable and comparable across runs.

## Why We Should Do This

The current bridge already exposes useful logs and diagnostics, but they are spread across:

- startup logs
- periodic action diagnostics
- instability logs
- runtime state transitions

That is enough for local debugging.

It is not enough for:

- repeated A/B tuning
- G2 supporting evidence
- regression checking after refactors
- clean hand-off from one PIE run to later analysis

Bridge trace output closes that gap with a contained amount of code.

## Scope

### In Scope

- one trace folder per session
- one session metadata file
- one sampled frame CSV
- one sparse event JSONL
- stage timings for the major bridge phases
- reuse of existing action, control-target, and instability diagnostics
- automation coverage for formatting and trace gating logic

### Out Of Scope

- full tensor dumps
- per-body full-table dumps every frame
- Unreal Insights integration
- automatic charting or diff tooling
- multiplayer-specific semantics

## Proposed File Changes

### New Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridgeTrace.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridgeTrace.cpp`

These files should own:

- trace session metadata structs
- frame row structs
- event structs
- serialization helpers
- buffered writer logic

### Existing Files To Update

- [PhysAnimComponent.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h)
- [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp)
- [PhysAnimBridgeTests.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridgeTests.cpp)

## Design Constraints

The implementation must respect these rules.

### 1. Opt-In Only

Tracing must be disabled by default.

Recommended controls:

- component setting:
  - `bEnableBridgeTraceOutput`
- optional cvar override:
  - `physanim.TraceOutput`

Suggested mode values:

- `0` = off
- `1` = session metadata + events only
- `2` = session metadata + events + frame CSV

### 2. No Per-Frame File Open/Close

Do not write each row by reopening the file every tick.

Use:

- a long-lived writer
- in-memory line buffers
- periodic flushes

### 3. Trace Failure Must Be Non-Fatal

If the trace writer fails:

- log a warning
- disable further trace writes for that session
- do not fail-stop the bridge

### 4. Reuse Existing Diagnostics

Do not create redundant computations if existing diagnostics already provide the data.

Reuse:

- `FPhysAnimActionDiagnostics`
- `FPhysAnimControlTargetDiagnostics`
- `FPhysAnimRuntimeInstabilityDiagnostics`

## Implementation Phases

## Phase 1. Lock The Data Model

Add trace-only structs in the new private helper files.

Recommended structs:

- `FPhysAnimBridgeTraceSessionMetadata`
- `FPhysAnimBridgeTraceFrame`
- `FPhysAnimBridgeTraceEvent`
- `FPhysAnimBridgeTraceWriter`

The writer should own:

- trace folder path
- session id
- buffered strings or builders
- file handles or archives
- enabled mode
- flush cadence
- failure state

Deliverable:

- helper layer compiles with no component integration yet

## Phase 2. Add TDD Coverage For Serialization And Gating

Before integrating the runtime path, add tests for the new helper layer.

Suggested automation tests:

- `PhysAnim.Trace.SessionMetadataSerialization`
  - required fields serialize
- `PhysAnim.Trace.FrameCsvHeader`
  - header order is stable
- `PhysAnim.Trace.FrameCsvRowSerialization`
  - representative frame row formats correctly
- `PhysAnim.Trace.EventJsonSerialization`
  - event JSONL line is valid and stable
- `PhysAnim.Trace.ModeGating`
  - mode `0` writes nothing
  - mode `1` writes metadata and events only
  - mode `2` writes frame rows
- `PhysAnim.Trace.WriterFailureDisablesFurtherWrites`
  - failure path degrades safely

These tests belong in [PhysAnimBridgeTests.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridgeTests.cpp) unless they grow large enough to justify a dedicated trace test file.

## Phase 3. Add Trace Settings To `UPhysAnimComponent`

Add a small set of configuration fields to [PhysAnimComponent.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h):

- `bEnableBridgeTraceOutput`
- `BridgeTraceOutputMode`
- `BridgeTraceFlushIntervalSeconds`
- `BridgeTraceSampleEveryNthFrame`

Keep defaults conservative:

- tracing off
- flush interval around `0.5` to `1.0` seconds
- sample every frame when tracing is on

Also add runtime members:

- current session id
- trace frame counter
- trace writer instance
- last flush time

## Phase 4. Emit Session Metadata And Sparse Events

Integrate session lifecycle events first.

Emit metadata at trace start:

- map
- actor
- runtime name if already known
- model asset path
- PoseSearch database path if already loaded
- key stabilization settings snapshot

Emit events at these points:

- trace start
- startup success
- startup blocked
- runtime state transition
- simulation handoff complete
- fail-stop
- trace stop

Likely integration seams:

- `StartBridge()`
- `FailStop(...)`
- `TransitionRuntimeState(...)`
- the existing simulation-handoff branch in [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L775)
- component end play / destruction path

This phase already provides value even before frame CSV exists.

## Phase 5. Add Stage Timing Capture

Add lightweight timing capture around the major bridge stages.

Recommended measured stages:

- PoseSearch query
- future pose sampling
- body sample gather
- observation pack
- inference
- action conditioning
- control-target write
- `UpdateControls`
- instability check
- total bridge tick

Implementation guidance:

- use `FPlatformTime::Seconds()` or equivalent
- keep timing ownership local to the bridge tick
- do not allocate timing objects every frame

Preferred shape:

- one small local timing struct built inside the tick
- copied into the trace frame row if tracing is enabled

## Phase 6. Emit `frames.csv`

Once timings and diagnostics are both available, add per-frame row emission.

Each row should combine:

- session/time fields
- runtime path fields
- timing fields
- action diagnostics
- control-target diagnostics
- instability diagnostics

Recommended integration point:

- after `UpdateControls(...)`
- after instability diagnostics are finalized
- before any early return that would lose the frame's final state

Important rule:

- if a fail-stop occurs this tick, still try to emit the final frame row before the trace shuts down, if the required data is already available

## Phase 7. Buffered Flush And Shutdown Semantics

The writer should flush:

- periodically by elapsed time
- on fail-stop
- on trace stop
- on component destruction or end play

Do not depend on a graceful editor shutdown for data safety.

The design should tolerate:

- PIE stop
- fail-stop mid-run
- map reload

## Phase 8. Manual Verification

After automation passes, validate the workflow with one short PIE run.

Acceptance checks:

1. enabling trace creates a session folder under `Saved/PhysAnim/Traces`
2. `session.json` exists and contains runtime/model metadata
3. `events.jsonl` records startup and state transitions
4. `frames.csv` has rows for the active bridge window
5. the last frame before fail-stop is preserved when a fail-stop occurs
6. disabling trace produces no trace folder

## Recommended Row Schema

The first implementation should write these fields exactly once in a stable header order.

### Required Header Groups

- session/time
- runtime path
- stage timings
- action diagnostics
- control-target diagnostics
- instability diagnostics

Do not add optional per-bone families to the first header.

If more fields are needed later, version the schema rather than silently reordering columns.

## Runtime Flow Integration

The intended runtime sequence is:

1. trace mode resolves from component settings plus cvar overrides
2. when tracing begins, create the session folder and write `session.json`
3. emit startup and runtime-state events as they occur
4. on each sampled bridge tick:
   - gather timings
   - gather already-computed diagnostics
   - serialize one CSV row into the buffer
5. flush periodically
6. on fail-stop or shutdown:
   - emit final event
   - flush remaining buffers
   - close writer

## Suggested Testing Order

Implement in this exact order to keep debugging tight:

1. helper serialization tests
2. writer gating tests
3. writer failure handling tests
4. session metadata emission
5. event emission
6. stage timing capture
7. frame CSV emission
8. manual PIE verification

This order matters because it isolates formatting bugs before live runtime bugs.

## Risks

### Risk 1. Too Much Data

If the trace is too noisy, it will stop being useful.

Mitigation:

- keep version 1 flat and narrow
- no raw tensors
- no full per-body tables every frame

### Risk 2. Trace Overhead Distorts The Run

If tracing changes the behavior we are diagnosing, it fails its purpose.

Mitigation:

- opt-in only
- buffered writes
- cheap serialization
- no repeated file open/close

### Risk 3. Schema Churn

If the CSV header keeps changing, historical comparison gets worse.

Mitigation:

- freeze a first header
- add `trace_version`
- append fields later only with intent

### Risk 4. Component Complexity Grows Further

`UPhysAnimComponent` is already large.

Mitigation:

- keep writer logic in dedicated helper files
- keep component responsibility limited to:
  - collect runtime data
  - hand it to the writer

## Completion Criteria

This implementation is complete when:

1. the bridge can emit a trace session folder on demand
2. the trace survives normal PIE stop and fail-stop cases
3. the frame CSV records timings plus existing diagnostics
4. automation covers serialization and mode gating
5. one real locomotion session can be inspected without relying on plain logs alone

## Next Step After Implementation

If the first version proves useful, the next reasonable follow-up is not "more fields."

The next reasonable follow-up is:

- one small analysis helper or notebook that plots:
  - target delta
  - lower-limb limit occupancy
  - instability accumulation
  - inference time

against time for a single run.

That should only happen after the trace artifact itself is stable and clearly useful.
