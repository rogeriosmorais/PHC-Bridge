# Task Packet S1-P1-A2

## Task ID

`S1-P1-A2`

## Goal

Turn the first end-to-end UE runtime success into a stabilization-ready one-character package, then prepare the G2 comparison only after the runtime is stable enough to judge fairly.

## Frozen Inputs

- accepted `S1-P1-A1` handoff
- current Phase 1 runtime state:
  - startup succeeds in UE
  - runtime log confirms `NNERuntimeORTDml`
  - the character currently flies / spins uncontrollably after startup
- [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/phase1-implementation-package.md)
- [phase1-ue-bridge-bringup-runbook.md](/F:/NewEngine/plans/stage1/phase1-ue-bridge-bringup-runbook.md)
- [g2-evaluation.md](/F:/NewEngine/plans/stage1/g2-evaluation.md)
- [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md)
- [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- current [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)

## Writable Paths

- [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/phase1-implementation-package.md)
- [phase1-ue-bridge-bringup-runbook.md](/F:/NewEngine/plans/stage1/phase1-ue-bridge-bringup-runbook.md)
- [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md)
- [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- [g2-evaluation.md](/F:/NewEngine/plans/stage1/g2-evaluation.md)
- [execution-log.md](/F:/NewEngine/plans/stage1/execution-log.md)
- [assumption-ledger.md](/F:/NewEngine/plans/stage1/assumption-ledger.md)

## Required Work

1. Record the current runtime truth explicitly:
   - startup success is proven
   - NNE runtime/model loading is no longer the blocker
   - uncontrolled post-startup flight / spin is the active Phase 1 blocker
2. Freeze one manual stabilization checkpoint for the current one-character runtime.
3. Freeze the tuning order so implementation does not improvise:
   - keep the working asset/model/startup path fixed
   - reduce action influence before changing mapping assumptions
   - adjust fixed Physics Control gains/damping next
   - inspect mapping / frame assumptions only if low-influence tuning still produces pathological motion
4. Freeze an explicit runtime state machine so startup and fail-stop ownership semantics are no longer implicit:
   - `Uninitialized`
   - `RuntimeReady`
   - `WaitingForPoseSearch`
   - `ReadyForActivation`
   - `BridgeActive`
   - `FailStopped`
   - only `BridgeActive` may own bridge physics
5. Define exactly what counts as `stable enough for G2`.
6. Only after the stabilization checkpoint is explicit, update the G2 package so it cannot be run on an obviously unstable build.

## Execution Order

1. Accept the current runtime truth as frozen:
   - startup succeeds
   - the runtime is unstable after startup
   - do not reopen model-import or asset-path discovery unless startup stops working again
2. Use [manual-verification.md](/F:/NewEngine/plans/stage1/manual-verification.md) to classify the current runtime with `MV-P1-01`.
3. Treat the first implementation pass as a stabilization pass, not a quality pass.
4. Keep the working startup path fixed while testing the first stabilization adjustments.
5. Keep startup and fail-stop behavior inside the frozen runtime state machine:
   - startup prerequisites may advance to `RuntimeReady`
   - the bridge must wait in `WaitingForPoseSearch` until the first valid `MotionMatch(...)` result exists
   - with zero-action safe mode enabled, the runtime must enter `ReadyForActivation` first
   - only after actions are explicitly enabled may the runtime enter `BridgeActive`
   - any startup/runtime fault must end in `FailStopped`, with bridge-owned physics released
6. Apply tuning changes only in the frozen order from [phase1-implementation-package.md](/F:/NewEngine/plans/stage1/phase1-implementation-package.md):
   - action influence first
   - fixed Physics Control gains and damping next
   - mapping / frame assumptions only after the simpler causes are ruled out
   - use the live runtime knobs first:
     - `physanim.ForceZeroActions`
     - `physanim.ActionScale`
     - `physanim.ActionClampAbs`
     - `physanim.ActionSmoothingAlpha`
     - `physanim.StartupRampSeconds`
     - `physanim.MaxAngularStepDegPerSec`
     - `physanim.AngularStrengthMultiplier`
     - `physanim.AngularDampingRatioMultiplier`
     - `physanim.AngularExtraDampingMultiplier`
     - `physanim.EnableInstabilityFailStop`
     - `physanim.MaxRootHeightDeltaCm`
     - `physanim.MaxRootLinearSpeedCmPerSec`
     - `physanim.MaxRootAngularSpeedDegPerSec`
     - `physanim.InstabilityGracePeriodSeconds`
7. Re-run `MV-P1-01` after each meaningful stabilization pass and record whether the result moved from `fail` toward `pass`.
8. Do not ask the user to run `G2` until `MV-P1-01` is explicitly recorded as `pass`.

## Definition Of Done

- the current Phase 1 blocker is documented precisely
- the stabilization checkpoint can support `pass`, `fail`, or `blocked`
- the tuning order is frozen tightly enough to avoid ad hoc runtime debugging
- the bridge exposes objective instability evidence instead of relying only on visual judgment
- G2 remains blocked until the stabilization checkpoint passes

## Escalate To User When

- the runtime is so unstable that no fair stabilization checkpoint can be written
- the required tuning work would change the locked bridge contract or widen scope materially

## Verification

- review against [acceptance-thresholds.md](/F:/NewEngine/plans/stage1/acceptance-thresholds.md)
- handoff must use [handoff-format.md](/F:/NewEngine/plans/stage1/handoff-format.md)

## Next Consumer

- implementation owner for Phase 1 stabilization work
- then user for `G2` only after stabilization is accepted
