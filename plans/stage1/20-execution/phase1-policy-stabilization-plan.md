# Phase 1 Policy Stabilization Plan

## Purpose

This document freezes the next stabilization phase for the Stage 1 bridge.

It begins from the current proven baseline:

- startup succeeds
- handoff succeeds
- staged non-root bring-up succeeds
- final hand-group simulation entry succeeds
- final hand-group control-only entry succeeds

The current blocker begins only when live policy targets start affecting the runtime.

This plan exists to stop ad hoc policy-phase fixes and replace them with a concrete, testable execution sequence.

## Current Root-Cause Boundary

Current evidence from PIE smoke shows:

- the first catastrophic spike no longer happens during bridge activation
- the first catastrophic spike no longer happens during simulation entry
- the first catastrophic spike no longer happens during final hand-group control entry
- the first catastrophic spike begins only after policy influence starts

Current falsified hypotheses:

- `direct hand policy targets alone are the remaining root cause`
  - false: deferring direct `hand_l` and `hand_r` policy targets did not remove the instability
- `upper-limb policy targets alone are the remaining root cause`
  - false: deferring the clavicle-to-hand chains still did not remove the instability

So the active fault surface is now:

- the first live policy-target phase as a whole
- target continuity between seeded/current-pose control targets and policy-driven targets
- policy target scope and target-step semantics

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

## Current Active Next Pass

The next pass should target `policy target continuity`, not gain tuning.

Specifically:

- add first-policy-frame target-delta instrumentation
- log the max target delta bone and angle
- compare it to the first unstable offender bone in the same run

Do not change control gains in that pass.

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

## Stop / Escalation Rules

Continue while passes do at least one of:

- isolate a narrower policy scope
- disprove a policy-phase hypothesis
- improve the first policy-enabled frame
- improve observability enough to justify the next pass

Escalate if:

- policy-region isolation and continuity instrumentation both fail to explain the onset behavior
- the next move would require reopening the bridge contract, action representation, or model contract

## Related Documents

- [Phase 1 Implementation Package](/F:/NewEngine/plans/stage1/20-execution/phase1-implementation-package.md)
- [Stage 1 Stabilization Loop](/F:/NewEngine/.agents/workflows/stabilization-loop.md)
- [ELI5 Manual Verification](/F:/NewEngine/plans/stage1/60-user/manual-verification.md)
