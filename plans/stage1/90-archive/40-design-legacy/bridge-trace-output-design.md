# Bridge Trace Output Design

## Purpose

This document defines a structured bridge trace output for the Stage 1 `PoseSearch -> PHC -> Physics Control -> Chaos` runtime.

The trace is meant to answer one question quickly:

- when the bridge looks wrong, which stage is actually responsible, and by how much?

This is not a replacement for Unreal Insights, ordinary log lines, or visual capture.

It is a small, bridge-owned diagnostic artifact that makes the runtime loop inspectable frame by frame.

## Primary Use Case

The current bridge already computes useful diagnostics, but most of them are exposed as:

- ad hoc log lines
- periodic summaries
- runtime-only state that disappears when PIE stops

That is good enough for one-off debugging, but weak for repeated Stage 1 comparison work.

The main use case for bridge trace output is:

- capture one structured artifact per run that shows:
  - which runtime path was active
  - how long each major bridge stage took
  - what the policy outputs looked like after conditioning
  - what control-target deltas were written
  - what instability metrics were doing before failure or before recovery

## How This Helps Us

Bridge trace output helps in five concrete ways.

### 1. Shorter Diagnosis Loops

Right now, when the character flies, spins, drifts, or looks too stiff, the diagnosis loop is longer than it should be because evidence is fragmented across:

- output log
- transient on-screen indicators
- manual observation

The trace turns one runtime session into one inspectable record.

Instead of asking:

- "was this a PoseSearch miss, a bad model step, a target-write spike, or a physics instability?"

we can answer it from one file.

### 2. Better A/B Comparisons

The project is doing repeated tuning passes on:

- lower-limb target semantics
- control-family response
- instability thresholds
- activation and prewarm behavior

The trace makes it possible to compare two runs on stable fields such as:

- inference time
- mean target delta
- lower-limb limit occupancy
- root instability accumulation
- worst offender bones

That is much better than comparing two screenshots of the log.

### 3. Stronger G2 Evidence

Gate G2 is a side-by-side quality comparison, not just a "did it compile" check.

The trace helps produce supporting evidence for that comparison by showing:

- whether the bridge stayed on the intended runtime
- whether the policy was actually active
- whether the movement stayed inside reasonable control-target and instability bounds

It does not replace the visual verdict, but it makes the verdict easier to defend.

### 4. Safer Refactors

The bridge is already non-trivial.

There are enough moving parts now that a future cleanup could silently change behavior in:

- startup ordering
- PoseSearch timing
- action conditioning
- control-target writing
- instability accumulation

A stable trace artifact lets us detect those regressions earlier.

### 5. Clearer Hand-off Between Human And Agent

Many Stage 1 checks are still visual and manual.

A saved trace artifact lets the human user run a PIE session once and hand back:

- the clip
- the trace folder

That is a much better debugging surface than "it looked bad around the 20 second mark."

## Problem Statement

The current bridge already has useful instrumentation:

- startup logs in [ue-bridge-implementation-spec.md](/F:/NewEngine/plans/stage1/10-specs/ue-bridge-implementation-spec.md#L419)
- runtime state transitions in [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L4718)
- action diagnostics, control-target diagnostics, and instability diagnostics in [PhysAnimBridge.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h#L40)
- periodic runtime logging in [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L781)

The gap is not "we have no diagnostics."

The gap is:

- diagnostics are not saved as one structured session artifact
- timing across major bridge stages is not captured in one comparable schema
- sparse event logs and per-frame state are mixed together in plain text
- offline comparison between two runs is harder than it should be

## Design Goals

The trace output should:

1. produce one session-scoped artifact per run
2. be structured enough for spreadsheet or script-based comparison
3. reuse diagnostics that the bridge already computes
4. add stage timings for the hot path
5. stay opt-in and cheap enough for normal editor diagnosis
6. stay small enough that a 60-90 second session is practical to save and review

## Non-Goals

This design is not trying to:

- replace Unreal Insights
- capture every physics substep
- dump raw tensors every frame
- become a general telemetry framework for the whole project
- add network replication or multiplayer trace semantics
- replace visual verification

## Current Diagnostic Surfaces To Reuse

The trace should reuse existing bridge-owned data rather than inventing parallel calculations.

Available inputs already exist for:

- action conditioning diagnostics in [PhysAnimBridge.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h#L40)
- control-target diagnostics in [PhysAnimBridge.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h#L49)
- runtime instability diagnostics in [PhysAnimBridge.h](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimBridge.h#L92)
- runtime state and startup/fail-stop events in [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L784) and [PhysAnimComponent.cpp](/F:/NewEngine/PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp#L4718)

The only major new data this design adds is stage timing.

## Recommended Output Model

Use a trace folder per session under:

- `Saved/PhysAnim/Traces/<timestamp>-<map>-<actor>/`

The folder contains:

1. `session.json`
2. `frames.csv`
3. `events.jsonl`

### 1. `session.json`

This is small session metadata written once at trace start.

It should include:

- trace schema version
- timestamp
- map name
- actor name
- plugin runtime state at session start
- selected NNE runtime name
- model asset path
- PoseSearch database path
- trace mode and sample policy
- key stabilization settings snapshot

Why this file exists:

- frame rows should stay compact
- session-level setup should not be repeated every frame

### 2. `frames.csv`

This is the main comparison artifact.

It contains one row per sampled bridge policy step.

CSV is the right first format because:

- it is easy to diff
- it is easy to open in Excel
- it is easy to analyze with Python later
- the schema is mostly flat and numeric

### 3. `events.jsonl`

This captures sparse events that do not fit cleanly into a single fixed CSV row.

Examples:

- startup success
- startup blocked
- runtime state transition
- simulation handoff complete
- fail-stop
- trace start
- trace stop

JSON Lines is appropriate here because event payloads are sparse and not all events share the same fields.

## `frames.csv` Schema

The first version should stay intentionally narrow.

Each row should include these groups.

### Session And Time

- `session_id`
- `trace_version`
- `frame_index`
- `world_time_seconds`
- `delta_time_seconds`
- `sampled_policy_step`

### Runtime Path

- `runtime_state`
- `nne_runtime_name`
- `pose_search_valid`
- `run_sync_succeeded`
- `update_controls_succeeded`
- `policy_influence_active`
- `first_policy_enabled_frame`
- `num_policy_targets_written`

### Stage Timings

- `resolve_context_ms`
- `pose_search_query_ms`
- `future_pose_sample_ms`
- `body_sample_ms`
- `observation_pack_ms`
- `inference_ms`
- `action_condition_ms`
- `control_target_ms`
- `update_controls_ms`
- `instability_check_ms`
- `bridge_tick_total_ms`

Not every field will be populated on every frame.

For example:

- `resolve_context_ms` is only meaningful during startup or recovery paths

Blank or zero values are acceptable where the stage did not run.

### Action Diagnostics

- `raw_action_min`
- `raw_action_max`
- `raw_action_mean_abs`
- `conditioned_action_mean_abs`
- `num_clamped_action_floats`

### Control Target Diagnostics

- `max_target_delta_bone`
- `max_target_delta_degrees`
- `mean_target_delta_degrees`
- `max_raw_policy_offset_bone`
- `max_raw_policy_offset_degrees`
- `mean_raw_policy_offset_degrees`
- `max_lower_limb_limit_occupancy_bone`
- `max_lower_limb_limit_occupancy`
- `max_lower_limb_limit_proxy_degrees`
- `mean_lower_limb_limit_occupancy`
- `num_lower_limb_targets_considered`

### Instability Diagnostics

- `root_height_delta_cm`
- `root_linear_speed_cm_per_second`
- `root_angular_speed_deg_per_second`
- `height_exceeded`
- `linear_speed_exceeded`
- `angular_speed_exceeded`
- `unstable_accumulated_seconds`
- `num_bodies_considered`
- `num_simulating_bodies`
- `max_body_linear_speed_bone`
- `max_body_linear_speed_cm_per_second`
- `max_body_angular_speed_bone`
- `max_body_angular_speed_deg_per_second`
- `max_body_height_delta_bone`
- `max_body_height_delta_cm`

## Event Model

The event file should capture sparse but high-value state changes.

Required event types:

- `trace_started`
- `startup_success`
- `startup_blocked`
- `runtime_state_transition`
- `simulation_handoff_complete`
- `fail_stop`
- `trace_stopped`

Each event should carry:

- `session_id`
- `event_time_seconds`
- `event_type`
- `runtime_state`
- `message`

Optional fields per event:

- `error`
- `previous_runtime_state`
- `new_runtime_state`
- `nne_runtime_name`
- `model_asset_path`
- `map_name`
- `actor_name`

## Sampling Policy

The first version should sample at the bridge policy-step cadence, but remain opt-in.

Recommended defaults:

- trace disabled by default
- when enabled, record one frame row per bridge tick
- allow downsampling by `Nth` frame if needed later

Why per bridge tick is acceptable:

- Stage 1 runs two characters, not hundreds
- a 60 second run at 60 sampled rows per second is still manageable
- the main value comes from preserving the sequence leading into failure or recovery

## Performance And Safety Rules

The trace output must not become the new source of instability.

Rules:

1. trace is opt-in
2. frame rows are buffered in memory and flushed in batches
3. file IO must not happen in a per-frame open/write/close pattern
4. no raw tensor dumps in the default trace mode
5. no per-body full-table dump in the first version
6. trace failure must not fail-stop the bridge

If writing fails:

- log a warning
- disable further trace writes for that session
- keep the bridge running

## Why Not Just Use Plain Logs

Plain logs are still useful, but they are weak for this job.

They do not give us:

- stable columns
- one row per frame
- easy run-to-run comparison
- machine-readable event boundaries

The design here is deliberately small because the goal is not "more logs."

The goal is:

- one saved artifact that makes bridge behavior legible

## Why Not Dump Full Tensors

Full tensor dumps would be expensive, noisy, and hard to review.

For Stage 1, they are also the wrong level of diagnosis.

The useful questions are not:

- "what were all 358 self-observation floats on frame 1221?"

The useful questions are:

- did PoseSearch return a valid result?
- how long did inference take?
- how strong were the raw vs conditioned actions?
- which bone had the largest target delta?
- which body first exceeded instability thresholds?

If raw tensor capture is ever needed, it should be a separate debug mode with a much narrower manual workflow.

## Recommended First-Version Scope

Implement only:

- session metadata
- sampled frame CSV
- sparse event JSONL
- stage timings
- reuse of existing diagnostics structs

Do not implement yet:

- binary trace formats
- per-body table dumps every frame
- direct Unreal Insights integration
- automatic run-to-run diff tooling

## Verification Value

This trace will be considered successful if it helps answer these recurring questions without reopening the engine source:

1. Did the session actually run through `NNERuntimeORTDml`, or did it silently fall back?
2. Was the bridge in `WaitingForPoseSearch`, `ReadyForActivation`, `BridgeActive`, or `FailStopped` when the motion looked wrong?
3. Did the bad behavior begin with:
   - a timing spike
   - a target-write spike
   - a limit-occupancy spike
   - or an instability threshold breach?
4. Which bone or body was the leading offender?
5. Is a new tuning change actually better than the previous run on the same comparison sequence?

If the trace answers those questions, it is worth the implementation cost.

## Recommendation

Implement the bridge trace output as a narrow Stage 1 diagnosis tool, not a general telemetry platform.

Its value is practical:

- faster diagnosis
- stronger evidence
- safer refactors
- better A/B comparisons

That is enough justification on its own.
