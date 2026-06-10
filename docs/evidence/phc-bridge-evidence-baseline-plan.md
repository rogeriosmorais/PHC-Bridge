# PHC-Bridge Evidence Baseline Plan

## Summary

Build a durable "water meter" for the existing architecture:

`PoseSearch -> PHC Policy -> Physics Control -> Chaos -> Renderer-facing motion`

The goal is not to prove success yet. The goal is to produce a harsh, falsifiable baseline showing exactly which segments are reached, how much activity each segment produces, and where truth quality fails.

## Key Changes

- Add a versioned evidence contract defining three segment verdicts: `NotReached`, `ReachedButInactive`, and `Active`.
- Track separate truth-quality flags for assistance, continuity, support, simulation, terminal failure, and artifact/log contradiction.
- Add a per-attempt evidence summary artifact, separate from the existing terminal proof JSON.
- Write evidence summaries under `PhysAnimUE5/Saved/PhysAnim/EvidenceSummaries/`.
- Add a repository evidence collector that reads latest logs and artifacts and emits a compact harsh report.
- Keep the existing terminal proof artifact as the source of truth for terminal pass/fail fields.

## Evidence Segments

Each attempt should report one row per architecture segment.

### PoseSearch

Required evidence:

- Query attempts.
- Valid result count.
- Selected database, asset, or animation identifier when available.
- Selected time or pose sample when available.
- Consecutive invalid-frame count.

Segment classification:

- `NotReached`: no query attempted.
- `ReachedButInactive`: query attempted but no valid result.
- `Active`: at least one valid result selected.

### PHC Policy

Required evidence:

- Model loaded.
- Runtime name, such as ORT DML or ORT CPU.
- Input buffers finite.
- Inference attempts.
- Inference successes.
- Inference failures.
- Action count.
- Action min, max, mean, and norm.
- Inference latency if available.

Segment classification:

- `NotReached`: no model loaded or no inference attempted.
- `ReachedButInactive`: inference attempted but failed, or action output is structurally empty.
- `Active`: successful inference with finite, non-empty action output.

### Physics Control

Required evidence:

- Physics Control component available.
- Controlled body count.
- Control target sample count.
- Normal target writes.
- Total target writes.
- Max and mean target delta degrees.
- Max and mean raw policy offset degrees.

Segment classification:

- `NotReached`: no Physics Control component or no target path reached.
- `ReachedButInactive`: target path reached but zero normal writes.
- `Active`: normal target writes occurred against controlled bodies.

### Chaos

Required evidence:

- Runtime simulating body count.
- Critical body simulating count or mask.
- Support body simulating count or mask.
- Pelvis awake state.
- Support mode.
- Proxy inside support hull.
- Physical continuity validator result.
- Max body linear velocity.
- Max body angular velocity.

Segment classification:

- `NotReached`: no runtime body simulation sampled.
- `ReachedButInactive`: bodies exist but required critical/support simulation or awake state is absent.
- `Active`: required bodies are simulating, continuity is evaluated, and support truth is populated.

### Renderer-Facing Motion

Required evidence:

- Actor displacement.
- Mesh displacement.
- Root yaw delta.
- Pose-change proxy, such as max body transform delta.
- Whether the run used `NullRHI`.

Segment classification:

- `NotReached`: no renderer-facing pose or transform sampled.
- `ReachedButInactive`: samples exist but no measurable motion or pose change occurred.
- `Active`: measurable motion or pose change occurred.

Important limitation:

- `NullRHI` cannot prove visual rendering quality. Under `NullRHI`, this segment is only renderer-facing motion proxy evidence.

## Artifact Rules

- Artifact fields beat human-readable logs.
- Logs only provide correlation and diagnostic context.
- A log-level `PASS` with contradictory JSON fields must classify as `CONTRADICTORY`.
- Missing evidence fields must classify as `Missing`, never as pass.
- A 3-second hold with zero policy inference or zero control target writes is `DIAGNOSTIC`, not product success.
- A 3-second hold with `proxy_inside_hull=false` is `DIAGNOSTIC` or `BLOCKED`, not product success.
- Product success requires all architecture segments to be active and all strict truth-quality flags to be clean.

## Overall Verdicts

Use these final attempt verdicts:

- `PRODUCT_SUCCESS_CANDIDATE`: all segments active, hold duration meets threshold, no terminal failure, physical continuity passes, support truth passes, and no hidden assistance is detected.
- `DIAGNOSTIC`: at least one meaningful segment is active, but product success is not proven.
- `BLOCKED`: the attempt reaches a specific segment and fails there with a clear terminal reason or truth-quality violation.
- `CONTRADICTORY`: log-level and artifact-level claims disagree.
- `INSUFFICIENT_EVIDENCE`: required fields are missing or no durable artifact was produced.

## Implementation Tasks

### Task 1: Evidence Contract and Classifier

- Define the segment states, truth-quality flags, and final classification rules.
- Add deterministic tests for classifier behavior.
- Ensure artifact/log disagreement produces `CONTRADICTORY`.
- Ensure partial success cannot be promoted to product success.

### Task 2: Evidence Summary Artifact

- Add a new evidence summary struct and emitter.
- Do not overload the existing terminal proof artifact.
- Include attempt UUID, test name, map, timestamp, command metadata when available, segment metrics, quality flags, terminal reason, and strict verdict.
- Emit one summary per attempt.

### Task 3: Runtime Metric Capture

- Reuse existing counters where possible:
  - Policy inference count.
  - Control target writes.
  - Runtime simulating body count.
  - Support fields.
  - Continuity fields.
- Add missing PoseSearch and renderer-facing motion counters only where current telemetry cannot answer the segment question.
- Avoid adding new gameplay behavior while instrumenting.

### Task 4: Evidence Collector

- Parse the latest terminal JSON.
- Parse the latest evidence summary JSON.
- Parse relevant `PhysAnim` log lines only for correlation.
- Produce one human-readable report with sections:
  - `Actual Evidence`
  - `Weak Evidence`
  - `Contradictions`
  - `Missing Evidence`
  - `Next Blocking Segment`

### Task 5: Baseline Execution

- Run no more than three test commands total.
- Recommended order:
  1. `.\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics`
  2. `python .\scripts\read_logs.py`
  3. Evidence collector against latest artifacts
- Only run Stage2A walk/turn after baseline standing evidence is classified.

## Test Plan

### Classifier Unit Tests

- 3-second hold plus terminal none plus zero policy/control activity classifies as `DIAGNOSTIC`.
- Terminal failure with active upstream segments classifies as `BLOCKED` at the failing segment.
- Log says pass but artifact says failure classifies as `CONTRADICTORY`.
- Missing required fields classify as `INSUFFICIENT_EVIDENCE`.
- All segments active plus clean strict truth flags classifies as `PRODUCT_SUCCESS_CANDIDATE`.

### Serialization Tests

- Evidence summary preserves every segment field.
- Missing optional fields remain distinguishable from false or zero values.
- Existing terminal proof artifact remains backward compatible.

### Integration Smoke

- One standing proof run generates both the existing terminal artifact and the new evidence summary.
- The collector identifies the latest attempt.
- The collector prints segment-by-segment status.
- The collector refuses to call the run product success when policy/control/Chaos truth is inactive or contradictory.

## Acceptance Criteria

- A single command can produce a current evidence report from the latest run artifacts.
- The report identifies how far execution reached through the architecture.
- The report distinguishes activity from truthful physical success.
- The report downgrades misleading pass-like artifacts to diagnostic evidence.
- The report names the next blocking segment.
- No permanent skip-by-design or fail-by-design tests are added.

## Assumptions

- First priority is a baseline map, not forcing a strict pass.
- Existing terminal proof artifacts remain intact for backward compatibility.
- Evidence summaries may live in `Saved/PhysAnim` initially.
- Sanitized evidence reports can be checked into `docs/evidence` when needed.
- `NullRHI` evidence is accepted only as renderer-facing motion proxy evidence, not visual proof.
