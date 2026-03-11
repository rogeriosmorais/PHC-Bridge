# Phase 1 Policy Stabilization Plan

## Purpose

This document froze the policy-phase stabilization work for the Stage 1 bridge.

That work started from a proven baseline where:

- startup succeeded
- handoff succeeded
- staged non-root bring-up succeeded
- final hand-group simulation entry succeeded
- final hand-group control-only entry succeeded

The remaining blocker at that point began only when live policy targets started affecting the runtime.

## Final Root-Cause Boundary

The policy-phase stabilization work converged on three concrete faults:

- large first-frame target jumps during the first live policy phase
- stale explicit targets being reinterpreted as offsets when controls switched into `bUseSkeletalAnimation` mode
- an incorrect SMPL->UE quaternion basis conversion that violated the frozen `R_ue = B * R_smpl * B^T` rule

Current falsified hypotheses:

- `direct hand policy targets alone are the remaining root cause`
  - false: deferring direct `hand_l` and `hand_r` policy targets did not remove the instability
- `upper-limb policy targets alone are the remaining root cause`
  - false: deferring the clavicle-to-hand chains still did not remove the instability

So the active fault surface is now:

- the first live policy-target phase as a whole
- target continuity between seeded/current-pose control targets and policy-driven targets
- policy target scope and target-step semantics

Final decisive evidence from the last smoke passes on `March 11, 2026`:

- after continuity fixes, first written target deltas were already small, but raw offsets stayed around `120-144deg`
- that pointed to representation, not timing
- after correcting the bridge rotation conversion, first-policy-frame raw offsets collapsed to about `0-2deg`
- at full policy influence, raw offsets stayed around low double digits instead of three digits
- the `10` second PIE smoke completed without catastrophic per-body spikes

## Frozen Baseline

Until this plan is explicitly reopened, these behaviors are frozen:

- keep the staged bring-up order at `5` groups
- keep the kinematic root/pelvis strategy
- keep deferred cached-target reset at simulation entry
- keep the final hand-group control settle window
- keep the delayed policy ramp after final-group control settle
- do not reopen ONNX, PoseSearch startup, runtime ownership, or earlier handoff mechanics unless new evidence forces it

## Policy-Phase Questions

Each pass in this phase must answer exactly one of these questions:

1. `Policy onset`
   - is the first instability caused by policy turning on at all, even at low alpha?
2. `Policy target scope`
   - is the first instability tied to a particular body group receiving policy targets?
3. `Policy target continuity`
   - is the first instability caused by a large first-step jump from seeded current-pose targets to policy targets?
4. `Policy target representation`
   - is the first instability caused by the policy target orientation itself, not merely the timing or scope?

Do not mix these questions in one pass.

## Required Observability

Before or alongside policy-phase passes, the runtime must expose enough evidence to answer the active question.

Required policy-phase evidence:

- `policyInfluenceAlpha`
- first policy-enabled frame after the control-only baseline
- max offender bone on the first policy-enabled frame
- per-body max linear/angular speeds on the first policy-enabled frame
- target-step diagnostics for the first policy-enabled frame:
  - max target delta bone
  - max target delta angle in degrees
  - whether the max target delta bone matches the first unstable offender

If a pass cannot be interpreted from logs, add observability before changing behavior again.

## Execution Order

Policy-phase work must proceed in this order.

### Step 1: Prove Policy-Onset Baseline

Goal:

- verify exactly when the first policy-written frame occurs
- verify whether the first instability begins on that same frame or one frame later

Allowed changes:

- logging only
- no behavior changes

Success criterion:

- the first policy-enabled frame is identifiable in logs
- the first unstable frame is identifiable in logs

### Step 2: Split Policy By Body Region

Goal:

- determine whether policy onset is stable for lower body and spine before arms/head are included

Required order:

1. lower body + spine only
2. head/neck only
3. one arm chain only
4. both arm chains

Rules:

- one region addition per pass
- no gain retuning during this step
- keep seeded control targets on excluded regions

Success criterion:

- logs prove which first included region reintroduces instability

### Step 3: Measure First Policy Target Delta

Goal:

- compare seeded/current control targets against first policy-driven targets

Required measurements:

- per-bone angular delta from `PreviousControlTargetRotations`
- max target delta bone
- average target delta magnitude

Rules:

- instrumentation before behavior change
- do not assume the unstable bone is the same as the largest target delta without log proof

Success criterion:

- either:
  - a large first-frame target jump is confirmed, or
  - target jumps are small enough that continuity is no longer the lead explanation

### Step 4: Apply One Continuity Fix

Only after Step 3.

Allowed continuity fixes:

- per-bone target-delta clamp for policy-written targets
- separate policy ramp duration by body group
- policy target blending from seeded/current targets into policy targets

Rules:

- choose one continuity mechanism per pass
- do not simultaneously retune gains

Success criterion:

- the first policy-enabled frame no longer shows catastrophic spikes

### Step 5: Reopen Gain Tuning Only If Needed

Only after:

- policy target scope is characterized
- target continuity has been characterized

Allowed gain work:

- body-group-specific angular strength cap
- body-group-specific damping multiplier

Rules:

- gains are not the first explanation anymore
- use gain changes only after target semantics are better understood

## Pass Template

Every policy-phase pass must produce:

1. one dominant policy-phase hypothesis
2. one explicit success criterion
3. one code change class:
   - observability
   - policy scope
   - continuity
   - gain
4. full verification:
   - build
   - `PhysAnim.Component`
   - `run-pie-smoke.ps1`
5. documentation updates

## Final Result

The active policy-phase stabilization work is complete for the current smoke target.

Frozen final decisions from this phase:

- keep the staged bring-up order at `5` groups
- keep the final hand-group control settle window
- switch controls into skeletal-animation target mode once live policy influence begins
- clear all control offsets on that representation switch
- keep the corrected quaternion basis-change implementation in the bridge
- keep the target-delta and raw-offset diagnostics as regression evidence

## Success Gates

### Gate P1

- the first policy-enabled frame is identifiable in logs
- the first unstable frame is identifiable in logs

### Gate P2

- policy can be enabled for at least one non-arm region without catastrophic spikes

### Gate P3

- the first policy-enabled frame no longer produces catastrophic spikes after the chosen continuity fix

### Gate P4

- full-body policy can run for the required smoke window without catastrophic per-body spikes
- status: passed on `March 11, 2026` via `run-pie-smoke.ps1`

## Stop / Escalation Rules

Continue while passes do at least one of:

- isolate a narrower policy scope
- disprove a policy-phase hypothesis
- improve the first policy-enabled frame
- improve observability enough to justify the next pass

Escalate if:

- policy-region isolation and continuity instrumentation both fail to explain the onset behavior
- the next move would require reopening the bridge contract, action representation, or model contract

Current status:

- no escalation is needed for the current `10` second smoke target
- follow-up work is now longer-duration validation, broader policy-scope re-expansion, or G2-readiness work rather than emergency stabilization

## Related Documents

- [Phase 1 Implementation Package](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md)
- [Stage 1 Stabilization Loop](/F:/NewEngine/.agents/workflows/stabilization-loop.md)
- [ELI5 Manual Verification](/F:/NewEngine/plans/stage1/60-user/manual-verification.md)
