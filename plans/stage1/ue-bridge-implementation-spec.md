# Stage 1 UE Bridge Implementation Spec

## Purpose

This document is the Unreal-specific implementation spec for the Stage 1 bridge.

It exists because [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md) locks the model-facing contract, but does not by itself freeze the exact UE `5.7.3` classes, functions, ownership model, and tick/update flow needed to build the runtime safely.

Do not resume bridge code changes beyond narrow smoke-test fixes until this document is filled in and reviewed.

## Status

- `Current status`: draft
- `Target phase`: Phase 1
- `Applies to`: one-character Stage 1 runtime only

## Scope

This spec must freeze:

- the runtime owner of the bridge in UE
- the exact PoseSearch integration point
- the exact NNE model/session lifecycle
- the exact `UPhysicsControlComponent` control/modifier lifecycle
- the exact tick/update ordering
- the exact asset ownership and content paths
- the exact reset/failure behavior
- the exact test seams and runtime evidence required

This spec must not:

- change the locked Stage 1 architecture
- widen scope into Stage 2 systems
- introduce a new inference runtime
- reopen the bridge tensor contract already locked in [bridge-spec.md](/F:/NewEngine/plans/stage1/bridge-spec.md)

## Required Freeze Points

Before implementation resumes, this document must explicitly answer all of the following.

### 1. Runtime Owner

- Which UE class owns the live bridge loop:
  - subsystem
  - actor component
  - anim instance
  - other explicit class
- Why that owner is correct for Stage 1
- Which class owns setup versus per-frame execution versus teardown

### 2. Character And Asset Binding

- Which character class is the single Stage 1 runtime target
- Which skeletal mesh asset is the default target
- Which physics asset is required
- Which content paths are frozen for:
  - character mesh
  - PoseSearch database
  - ONNX / `UNNEModelData`
  - any bridge config assets

### 3. PoseSearch Integration

- Which exact UE class or API supplies the current target pose
- What data is read from PoseSearch each render tick
- How the target pose is converted into the bridge input representation
- What the fallback is if the active PoseSearch result is invalid or unavailable

### 4. NNE Integration

- Which runtime is used by default on this machine:
  - `NNERuntimeORTDml`
  - `NNERuntimeORTCpu` fallback
- Which UE classes create and own the model/session objects
- When model loading occurs
- How tensor shapes are validated
- How the keyed runtime inputs are marshaled each tick
- What runtime logs count as successful model/session initialization

### 5. Physics Control Integration

- Which exact component owns the controls
- When controls and body modifiers are created
- Which target space is used for each control type
- Which functions are called for:
  - orientation targets
  - gain updates
  - reset/stop
- Which bodies are allowed to simulate
- What preconditions must be true before reporting bridge startup success

### 6. Tick And Threading Model

- Which tick group owns bridge evaluation
- Whether inference runs once per render tick or another cadence
- How substeps reuse the latest outputs
- What data must be stable before physics runs
- What assumptions are made about game thread versus physics thread interaction

### 7. Failure Handling

- What happens if:
  - PoseSearch data is missing
  - NNE runtime/model creation fails
  - tensor shapes do not match the locked contract
  - required physics bodies are missing
  - controls fail to initialize
  - motion becomes unstable enough to invalidate the comparison
- Which failures are `blocked` versus `fail` versus recoverable runtime stop conditions

### 8. Test Seams

- What can be unit-tested outside UE
- What needs UE automation or scripted runtime coverage
- What still requires manual verification
- Which logs or captures are the minimum evidence for:
  - PoseSearch path alive
  - NNE path alive
  - retargeting path alive
  - Physics Control path alive
  - full one-character loop alive

## Required API Mapping Table

This document is not complete until it includes a concrete table with at least:

| Responsibility | UE class | UE function(s) | Notes |
|---|---|---|---|
| runtime owner | `TBD` | `TBD` | |
| PoseSearch target read | `TBD` | `TBD` | |
| model load | `TBD` | `TBD` | |
| session creation | `TBD` | `TBD` | |
| input shape validation | `TBD` | `TBD` | |
| inference execution | `TBD` | `TBD` | |
| control creation | `TBD` | `TBD` | |
| control target write | `TBD` | `TBD` | |
| gain write | `TBD` | `TBD` | |
| reset / teardown | `TBD` | `TBD` | |

## Acceptance Criteria

This spec is implementation-ready only when:

- every required freeze point above is explicitly answered
- the API mapping table is populated with exact UE `5.7.3` classes and functions
- the runtime owner and tick model are no longer ambiguous
- the startup, reset, and failure paths are explicit
- the test seams are defined well enough that Phase 1 can proceed without fresh UE API discovery work

If those conditions are not met, the bridge is not fully specced and code changes should remain paused apart from isolated diagnostic fixes explicitly required to unblock the spec work.
