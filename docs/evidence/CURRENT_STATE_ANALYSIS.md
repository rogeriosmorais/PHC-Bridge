# Current Product State

## Status

The current causal-standing product behavior is **FAIL** in a development run.
A clean committed milestone bundle has not yet been executed for this revision.
The project is not blocked by a signer, receipt, another machine, or a sibling
checkout.

The active product contract is `product-gates/causal-standing.v1.json`. It
requires Manny to reach and remain in `BalanceActive_Standing`, execute the
imported PHC ONNX policy, apply and read back Physics Control targets, simulate
the required Chaos bodies without CharacterMovement or capsule assistance,
recover from a fixed pelvis impulse, and outperform zero actions. Dropped
control dispatch and forced support loss must be rejected for their intended
reasons.

## Latest Observed Attempt

On 2026-07-11, a renderer-enabled Normal development attempt produced a valid
`PRODUCT_RUN` manifest, 1,194 game-tick physics samples over exactly 10 seconds,
and a UE scene capture with 263,307 nonblank pixels. The evaluator returned
`FAIL`, not `INVALID` or `BLOCKED`.

Observed failures:

- runtime state remained `BalanceSafeDeny` during the scored window
- Phase 1 repeatedly reset on `phase1_late_validate_upper_body_instability`
- pelvis and support bodies did not have the required simulation ownership
- no policy samples occurred in the standing window
- no control target readback evidence occurred in the standing window
- recovery could not be demonstrated
- the pre-fix attempt also exposed active CharacterMovement and capsule
  collision; standing defaults now disable those helpers

The development artifact was produced from a dirty working tree and is ignored
under `test-results/product-runs/`. It is diagnostic evidence, not a milestone
claim.

## What Is Implemented

- A locked, append-only causal-standing v1 protocol.
- An evaluator that distinguishes `PASS`, behavioral `FAIL`, malformed
  `INVALID`, and environment `BLOCKED`.
- Real UE PIE observation streams for physics ticks and policy steps.
- Direct body simulation masks, pelvis height, root tilt, pose mismatch,
  inference/action values, attempted control writes, and Physics Control
  readback comparison.
- A renderer-backed scene capture, not a synthetic image fixture.
- Negative controls for zero actions, dropped Physics Control dispatch, and
  forced support loss.
- Standing defaults that do not preserve CharacterMovement, capsule collision,
  or bridge-owned gameplay-shell translation. Movement smoke tests retain an
  explicit opt-in.

These capabilities improve the experiment. They do not make the humanoid
stand.

## Current Technical Blocker

The first product blocker is now concrete: the runtime cannot complete Phase 1
because upper-body motion repeatedly violates the late-validation quiet gate.
It safe-denies before the causal standing window starts. Work should focus on
the physical/control cause of that instability while keeping the v1 protocol
unchanged.

The imported model is available at
`PhysAnimUE5/Content/NNEModels/phc_policy.onnx`. Its training checkpoint is not
available in this repository, so model retraining or checkpoint-level diagnosis
is outside the current scope. The runtime artifact can still be evaluated
honestly as-is.

## Test Tiers

- `npm run test:fast`: deterministic Python protocol/evaluator/runner tests.
- `npm run test:runtime`: one real renderer-enabled Normal PIE attempt. A
  behavioral `FAIL` is recorded and does not make the harness command fail.
- `npm run test:product`: the complete clean milestone bundle with all locked
  repetitions and negative controls. Its process exit code is the product
  verdict.
- `npm test`: fast, runtime, then product.

The local Volta `npm` installation is currently broken independently of this
repository. The underlying PowerShell and Python commands remain directly
executable.

## Next Product Work

1. Reproduce the Phase 1 upper-body instability with raw body/control traces.
2. Identify whether the cause is target-space mismatch, initial target
   discontinuity, body ownership sequencing, or unsuitable controller gains.
3. Add a deterministic regression test for the diagnosed mechanism before the
   runtime fix.
4. Rerun a Normal development attempt without changing causal-standing v1.
5. Run the complete clean milestone bundle only after Normal reaches the scored
   window reliably.
