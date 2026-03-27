# Phase 1 Transactional Auto-Calibration Harness

## Purpose

This document defines a dev-only Phase 1 search harness for truthful balance-entry calibration.

It exists to replace ad hoc one-knob-at-a-time tuning with deterministic trial search over a bounded solver parameter space while keeping the existing Phase 1 contract authoritative.

## Scope

This first milestone is Phase 1 only:

1. static geometry fit during Prepare and LateValidate
2. no-coupling proof through the first successful `BRT_Phase2_RootOn` entry tick

It does not tune:

1. readiness thresholds
2. LateValidate proof durations
3. no-coupling proof durations
4. deny taxonomy
5. Phase 2 RootOn behavior
6. Phase 3 Settle behavior
7. locomotion

## Architectural fit

The harness must not create a second runtime balance implementation.

The system under test remains:

1. `UPhysAnimComponent`
2. `FPhysAnimBalanceReadyTransition`
3. the existing Phase 1 pelvis-coupling and late-validation path

The harness is only:

1. a world-scoped orchestrator
2. a transactional snapshot/restore layer
3. a bounded candidate generator
4. a scorer and artifact writer

## Required properties

Every candidate trial must:

1. start from the exact same captured baseline
2. apply only transient Phase 1 solver-side overrides
3. use the normal balance-entry start path
4. stop at the first `BRT_Phase2_RootOn` tick, `Failed`, `SafeDenied`, or timeout
5. restore the same baseline after the trial

The harness must reject candidates that appear calm by violating the existing Phase 1 contract.

## Search space

The searchable candidate type is `FPhase1AutoCalibParams`.

It contains only:

1. source preset
2. seed-family preset
3. spine interpolation alpha
4. worst-thigh interpolation alpha
5. focused-delta scale
6. uprightness-weight scale
7. clamp-strength scale
8. pelvis pitch bias degrees
9. pelvis roll bias degrees

The fixed v1 discrete presets are:

1. `CurrentDefault`
2. `SpineBiased`
3. `WorstThighBiased`
4. `BalancedCoupled`
5. `SpineThenWorstThigh`
6. `RescueOnly`

These overrides are solver hints only. They must not alter Phase 1 contract thresholds or durations.

## Runtime shape

Add a dev-only `UPhysAnimPhase1AutoCalibSubsystem`.

Responsibilities:

1. resolve target component by filter
2. capture and restore baseline
3. generate candidate queue
4. run determinism preflight
5. execute Stage A, Stage B, and Stage C
6. score candidates
7. write `trials.csv`, `summary.json`, and `pareto.json`

Add debug commands:

1. `pa.RunPhase1AutoCalib [ownerFilter] [seed] [maxTrials] [outputSubfolder]`
2. `pa.StopPhase1AutoCalib [ownerFilter]`

Artifacts write under:

`test-results/phase1-autocalib/<timestamp>/`

## Snapshot contract

The baseline snapshot must be sufficient for deterministic replay of a Phase 1 trial.

`FPhase1AutoCalibBaselineSnapshot` must include:

1. owner actor transform and velocity
2. skeletal mesh transform
3. required body transforms
4. required body linear and angular velocities
5. required body sleep or awake state
6. body-modifier state required to reproduce ownership and kinematic-vs-sim behavior
7. control target state required to reproduce held and previous targets
8. policy and shell accumulators used by Phase 1
9. pending reset bookkeeping
10. `SafePhase1ConvergenceSnapshot`
11. serialized `FPhysAnimBalanceReadyTransition` state
12. current runtime state and related Phase 1 bookkeeping that affects the next trial

The balance-transition snapshot must explicitly export and import:

1. current and previous phase
2. timers and counters
3. no-coupling proof state
4. certified handoff snapshot
5. certified late-validation result
6. diagnostics
7. failure and safe-deny reasons

## Trial flow

### Preflight

1. capture baseline
2. restore baseline
3. capture metrics
4. restore baseline again
5. capture metrics again
6. abort the run if the two restored metric bundles differ

### Stage A

For each of the six discrete presets:

1. generate `24` deterministic Latin-hypercube samples across the continuous dimensions
2. execute each candidate from the restored baseline
3. keep full trial results

### Stage B

1. keep the best `8` Stage A candidates by score ordering
2. run `3` shrinking coordinate-sweep rounds around each retained candidate
3. evaluate each refined candidate from restored baseline

### Stage C

1. keep the best `5` refined candidates
2. rerun each candidate `5` times from restored baseline
3. mark the candidate reproducible only if terminal class, blocker string, and score breakdown stay within epsilon on all five runs

## Trial success and scoring

`FPhase1AutoCalibTrialResult` must record:

1. candidate id and parameters
2. terminal class
3. truthful blocker or deny reason
4. pass or fail gates
5. score breakdown
6. reproducibility summary

`FPhase1AutoCalibScore` must store the raw ordering metrics:

1. contract pass
2. timeout flag
3. safe-deny flag
4. determinism-pass flag
5. root-on reached flag
6. no-coupling proof satisfied flag
7. worst direct-link angular error
8. mean target delta
9. max target delta
10. thigh asymmetry
11. peak root tilt
12. shell offset delta
13. shell velocity delta
14. peak root linear speed
15. peak root angular speed

Ordering rule:

1. reject any candidate that fails contract, safe-denies, times out, misses required proof, or restores non-deterministically
2. rank survivors by the metric order listed above
3. use a derived scalar only as a stable tiebreak and export convenience

If all candidates fail, the report must still name:

1. the best near-pass
2. the truthful blocker distribution
3. the best reproducible failed candidate

## Testing requirements

Add deterministic tests for:

1. score ordering and gate rejection
2. contract thresholds remaining outside the override surface
3. balance-transition snapshot export and import
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
2. it uses the existing Phase 1 runtime path rather than a parallel solver
3. identical candidate plus identical baseline is deterministic after restore
4. no contract-failing candidate outranks a contract-passing candidate
5. all-fail runs still produce truthful output
6. passing runs report the best `5/5` reproducible candidate, not a one-off winner
