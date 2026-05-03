# Balance-First Rollout Protocol

## Purpose

This document prevents in-engine balance debugging from becoming visual tuning.

Every runtime run must be treated as a controlled falsification experiment.

## Rollout Ladder

1. Pure logic tests
2. Runtime adapter snapshot only
3. Shadow validation
4. Fail-only enforcement
5. Limited activation
6. Full standing target

## Run Classification

Every run must end as exactly one of:

1. PASS
2. EXPLAINED FAIL
3. INSTRUMENTATION FAIL
4. CONTRACT GAP
5. FORBIDDEN EDIT

## Progress Rule

Visual improvement does not count as progress.

Progress means one of:
- more mapped tests green
- more failures explained by canonical terminal reasons
- more required artifact fields populated
- fewer unknown failure classes
- narrower failing subsystem
- better reproducibility

## Stop Rules

Stop immediately if:
- an engine failure has no canonical terminal_reason
- terminal_reason exists but required forensic fields are missing
- the same failure is being tuned for the third time
- a fix touches forbidden files
- a visual improvement is not reflected in artifacts
- a threshold is changed before the metric is validated
- a fallback/rescue path is added to make a run pass
- a failure disappears without explanation

## One-Hypothesis Rule

Every fix must answer:

1. What exact failure was observed?
2. Which artifact field proved it?
3. Which contract rule did it violate?
4. Which single code surface owns that rule?
5. Which test fails before the fix?
6. Which test passes after the fix?

If those cannot be answered, do not code.
