# Instrumentation And Acceptance

## Purpose

This document defines the minimum instrumentation, run artifacts, and pass/fail rules for the continuous-balance rewrite.

Build observability before behavior.

## Minimum Run Artifact

Every continuous-balance run must produce or be able to produce a report artifact containing:

- sustained-hold time
- root tilt envelope
- peak angular speed by family
- contact uptime
- control effort proxy
- number of authority conflicts
- any topology change events

Recommended additions when available:

- COM or support proxy drift
- worst-family oscillation summary
- shell influence event count

## Forbidden Metrics

These metrics are not allowed to stand in for success:

- phase-completion counters
- shell-lock or shell-reference status by itself
- “clean transition” counters
- retry counts that do not end in sustained standing

## Acceptance Gates

### Milestone 1

- honest continuous-physics diagnostics exist
- run artifact exists
- failure can be explained without leaning on legacy phase completion

### Milestone 2

- `1.0` second stable hold exists under the continuous-balance mode

### Milestone 3

- `3.0` second stable hold exists under the continuous-balance mode

### Milestone 4

- small perturbation recovery is demonstrated after Milestone 3 is real

## Regression Gates

The rewrite branch is acceptable if it delivers:

- more honest failure
- less hidden assistance
- stronger observability

even when early visual behavior looks worse than the legacy path.

## Expected Early Regressions That Are Acceptable

- worse-looking early stance behavior
- earlier failure under continuous physics
- higher visible oscillation because grace logic is no longer hiding it
- clearer controller weakness that used to be misread as a transition problem

These are acceptable during the rewrite if instrumentation quality improves and hidden assistance decreases.
