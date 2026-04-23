# Legacy Handoff Contract

## Purpose

This document archives the old handoff-centered design so it stops contaminating the new docs.

It is not the primary architecture.

## Legacy Canonical Sequence

- `Phase1_Prepare`
- `Phase1_LateValidate`
- `Phase2_RootOn`
- `Phase2_ReadyForPhase3`
- `Phase3_Settle`
- `BalancePerturbationActive`

## Legacy Characteristics

- shell locks and shell continuity played a large truth role
- per-phase retries and guards shaped outcomes heavily
- the system optimized for proving the handoff was safe

## Migration Rule

Use this document only for:

- code migration
- compatibility labels
- historical comparison

Do not use it as the architectural center for new design work.
