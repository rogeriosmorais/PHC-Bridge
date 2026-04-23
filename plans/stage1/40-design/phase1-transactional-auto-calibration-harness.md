# Phase 1 Transactional Auto-Calibration Harness

## Legacy Filename Note

This filename is retained for compatibility.

The harness is no longer defined around passing `Prepare`, `LateValidate`, `RootOn`, or `Settle`. It now searches for stable balance-first activation onto a continuously physical chain.

## Purpose

This document defines a dev-only search harness for truthful balance-activation calibration.

It exists to replace ad hoc one-knob-at-a-time tuning with deterministic trial search over a bounded parameter space while keeping the activation contract authoritative.

## Scope

This milestone optimizes for:

1. continuous physical ownership of the balance-critical chain
2. stable controller blend-in
3. benchmark verification that the resulting run reaches `BalanceActive_Standing` and holds it continuously for `3.0` seconds

It does not tune:

1. the success benchmark itself
2. failure taxonomy as a substitute for physical progress
3. grace-window growth as a substitute for physical progress
4. locomotion

## Architectural Fit

The harness must not create a second runtime balance implementation.

The system under test remains the existing runtime path.

The harness is only:

1. a world-scoped orchestrator
2. a transactional snapshot and restore layer
3. a bounded candidate generator
4. a scorer and artifact writer

## Required Properties

Every candidate trial must:

1. start from the exact same captured baseline
2. apply only transient calibration overrides
3. use the normal balance-activation start path
4. continue until the run either holds `BalanceActive_Standing` for `3.0` continuous seconds, `Failed`, `SafeDenied`, or times out
5. restore the same baseline after the trial

The harness must reject candidates that appear calm only because diagnostics or grace rules hid real instability.

## Runtime Shape

Existing dev-only naming may remain in code for compatibility.

The harness responsibilities are:

1. resolve target component by filter
2. capture and restore baseline
3. generate candidate queue
4. run determinism preflight
5. execute candidate trials
6. score candidates
7. write `trials.csv`, `summary.json`, and `pareto.json`

Artifacts write under:

`test-results/phase1-autocalib/<timestamp>/`

## Snapshot Contract

The baseline snapshot must be sufficient for deterministic replay of a balance-activation trial.

The snapshot must include:

1. owner actor transform and velocity
2. skeletal mesh transform
3. required body transforms
4. required body linear and angular velocities
5. required body sleep or awake state
6. body-modifier state required to reproduce ownership and simulation behavior
7. control target and blend state
8. policy and shell accumulators used by activation
9. pending reset bookkeeping
10. current runtime state and related activation bookkeeping that affects the next trial

## Trial Success And Scoring

Trial results must record:

1. candidate id and parameters
2. terminal class
3. truthful blocker or deny reason
4. pass or fail gates
5. score breakdown
6. reproducibility summary

Score ordering must prioritize:

1. contract pass
2. determinism pass
3. continuous physical ownership preserved
4. stable controller blend
5. `BalanceActive_Standing` reached
6. `BalanceActive_Standing` hold seconds
7. worst physical instability metrics
8. shell influence metrics

Ordering rule:

1. reject any candidate that fails contract, safe-denies, times out, loses balance-critical continuity, fails to reach `BalanceActive_Standing`, fails to hold that state for `3.0` continuous seconds, or restores non-deterministically
2. rank survivors by the metric order listed above
3. use a derived scalar only as a stable tiebreak and export convenience

If all candidates fail, the report must still name:

1. the best near-pass
2. the truthful blocker distribution
3. the furthest progressed failure when any rejected trial reaches standing validation or even `BalanceActive_Standing` before failing the hold benchmark
4. the best reproducible failed candidate

## Testing Requirements

Add deterministic tests for:

1. score ordering and gate rejection
2. benchmark thresholds remaining outside the override surface
3. activation snapshot export and import
4. component baseline snapshot round-trip
5. reproducibility of identical candidate plus identical baseline

Add a PIE smoke that:

1. runs the harness on a fixed actor
2. executes at least two candidates
3. verifies identical reruns produce the same terminal result after restore
4. verifies the artifact files are written

## Acceptance

This milestone is complete only when:

1. the harness is dev-only and inactive by default
2. it uses the existing activation runtime path rather than a parallel solver
3. identical candidate plus identical baseline is deterministic after restore
4. no contract-failing candidate outranks a contract-passing candidate
5. all-fail runs still produce truthful output
6. passing runs report the best reproducible candidate, not a one-off winner
7. no report treats truthful safe-deny or legacy phase progress as a passing result
