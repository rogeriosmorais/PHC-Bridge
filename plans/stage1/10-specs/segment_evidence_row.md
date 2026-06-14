# Contract: Segment Evidence Row

## Overview
Defines the per-segment evidence row shared by PoseSearch, PHC Policy, Physics Control, Chaos, and renderer-facing motion.

Each attempt should report one row per architecture segment.

## Required Shape
The evidence row must preserve a common row envelope with the following shape:

- **`segment_name`** (string): The name of the architecture segment (e.g., `PoseSearch`, `PHC Policy`, `Physics Control`, `Chaos`, `Renderer`).
- **`state`** (string): The reached/activity state. Must be one of exactly three allowed states:
  - `NotReached`: The segment was never evaluated during the attempt.
  - `ReachedButInactive`: The segment was evaluated but did not take control or contribute to the final output.
  - `Active`: The segment took control or actively contributed to the output.
- **`required_metrics`** (object): Segment-specific metrics. Preserves a common row envelope but allows flexible key-value pairs depending on the segment.
- **`missing_required_fields`** (array of strings): Explicitly reports any expected metrics or fields that were missing or unparseable. Distinguishable from false/zero values.
- **`diagnostic_notes`** (string, optional): Human-readable notes or warnings for troubleshooting.
- **`provenance`** (string): Source provenance for values derived from terminal artifact, summary artifact, or log correlation.

## Acceptance Criteria
1. All five architecture segments use the same state vocabulary (`NotReached`, `ReachedButInactive`, `Active`).
2. Required metrics are segment-specific but preserve a common row envelope.
3. Missing required fields are reported explicitly via `missing_required_fields`.
