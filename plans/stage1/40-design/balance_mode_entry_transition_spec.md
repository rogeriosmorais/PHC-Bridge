# Balance Mode Entry / Transition Spec

Status: Authoritative implementation spec
Scope: Stage 1 runtime entry path into Balance Perturbation Mode
Audience: runtime, controls, debugging, and validation work for PhysAnim bridge

## 1. Purpose

This document specifies the entry and transition contract for Balance Perturbation Mode.

It defines:

- request handling
- queueing
- state machine
- preflight
- ownership rules
- transition phases
- failure and recovery
- activation boundary

This document is authoritative for how the runtime gets from `BridgeActive` into active Balance Perturbation Mode.

The companion documents are:

- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`
- `plans/stage1/40-design/balance_mode_phase2.md`

## 2. Feature relationship

The feature-level purpose of balance mode is defined in:

- `plans/stage1/40-design/balance-perturbation-mode-design.md`

This document defines the runtime contract for entering that feature.

## 3. Core rule

Balance Perturbation Mode is not active until the transition succeeds.

A queued request is not active mode.
Phase 1 is not active mode.
Phase 2 is not active mode.
Phase 3 is not active mode.

The runtime may claim active balance mode only after transition success and explicit activation.

## 4. Required runtime states

Minimum authoritative state set:

- `BridgeActive`
- `BalanceStartQueued`
- `BalanceTransition_Preflight`
- `BalanceTransition_Phase1_Prepare`
- `BalanceTransition_Phase1_LateValidate`
- `BalanceTransition_Phase2_RootOn`
- `BalanceTransition_Phase3_Settle`
- `BalancePerturbationActive`
- `BalanceTransitionFailed`

## 5. Request handling contract

A balance request may result in exactly one of these outcomes:

1. accepted and queued
2. rejected as invalid context
3. transition begins
4. transition fails and returns to `BridgeActive`
5. transition succeeds and mode becomes active

Not allowed:

- silent drop
- endless queue/reject oscillation
- implicit transition without authoritative state change
- claiming success while pelvis/root is still kinematic

## 6. Queueing rules

A request must be queued, not rejected, when the runtime is in valid bridge context but temporarily not eligible.

Queue-worthy blockers include:

- final-group control ramp inactive
- policy influence below required threshold
- startup handoff incomplete
- control-authority ramp incomplete

These are temporary blockers, not failures.

A queued request remains pending until one of these occurs:

- transition starts
- runtime exits `BridgeActive`
- request is cancelled
- a hard invalidation occurs

Only one queued balance request may exist at once.

## 7. Preflight contract

Preflight begins only after queue gates are satisfied.

Preflight must answer:

- is the source runtime state valid
- are required controls and modifiers present
- is the runtime in a topology that can converge
- is there a valid owned path to Phase 1 and Phase 2
- is the handoff path coherent

Preflight is a single evaluation step, not a passive polling loop.

## 8. Ownership rule

Every transition condition must be one of:

- observed-only
- transition-owned
- external-owned

If a condition is transition-owned, preflight must not reject forever merely because that condition is currently false.

## 9. Owner map

Minimum owner map:

- `finalGroupRampActive` owner = BridgeActive bring-up controller
- `policyInfluenceAtThreshold` owner = BridgeActive policy influence ramp controller
- `transitionTopologyAchieved` owner = Phase 1 body-modifier topology shaping
- `policySuppressionAppliedToTransitionSet` owner = Phase 1 transition policy-routing logic
- `pendingCachedResetsDischargedOrPrevented` owner = Phase 1 reset suppression logic
- `upperBodyOwnershipModeStabilized` owner = Phase 1 upper-body ownership controller
- `TransitionOwnedShellLocked` active = balance-entry shell-authority transfer lifecycle
- `shellReferenceReanchoredBeforeProof` = balance-entry shell-authority transfer lifecycle
- `startupUnlockSuppressedDuringEntry` = startup/gameplay shell-lock arbitration logic
- `PreRootOnShellSafetyProof` inputs become valid = Phase 1 topology shaping plus shell-authority transfer and shell-lock maintenance
- `pelvisBodySimulating` = Phase 2 root-on body-modifier flip
- `postRootOnTopologyPreserved` = Phase 2 body-modifier/runtime-mode enforcement
- `postRootOnShellAuthorityPreserved` = Phase 2 and Phase 3 shell-lock maintenance
- active-mode ownership handoff = Phase 3 settle logic

## 10. Phase 1 boundary

Phase 1 exists to make root-on safe.

This document does not define the exact Phase 1 shaping procedure.
That contract is defined in:

- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`

This document only defines that Phase 1 must emit a certified handoff payload before Phase 2 may begin.

## 11. Certified handoff payload

Phase 1 must emit a certified handoff payload containing at minimum:

- handoff topology classification
- `simCount`
- `proximalSimCount`
- `distalSimCount`
- policy suppression state
- control-authority settled state
- max target delta
- mean target delta
- quiet proof duration
- late-validation sustain duration
- upper-body ownership mode
- upper-body stability summary
- shell authority mode
- whether shell reference was re-anchored
- whether shell reference was reseeded after lock
- whether startup/gameplay ownership stayed suppressed

Phase 2 must consume this payload rather than infer readiness from time alone.

## 12. Valid handoff classifications

The documented handoff classes are:

- `UpperOnlySafeDenyHandoff`
- `RootCoupledReadyHandoff`

Interpretation rule:

- `UpperOnlySafeDenyHandoff` is valid for safe denial
- `RootCoupledReadyHandoff` is the first topology class that may truthfully permit root-on

## 13. Handoff invalidation

Phase 1 readiness is revocable.

If any certified handoff field regresses before or during Phase 2 entry, the runtime must:

- invalidate readiness
- log the invalidation reason
- deny Phase 2 or return to Phase 1 depending on ownership

Invalidating regressions include:

- topology no longer matches certified topology
- sim coverage regresses
- policy suppression no longer holds
- target continuity exceeds certified bounds
- cached reset or topology flip becomes pending
- shell lock is released or reseeded
- gameplay shell/capsule authority returns

## 14. Phase 2 boundary

Phase 2 governs root-on and the immediate guard window.

The exact Phase 2 procedure is defined in:

- `plans/stage1/40-design/balance_mode_phase2.md`

This document only defines that Phase 2 may begin only from a still-valid certified handoff and that Phase 2 may deny safely before root-on.

## 15. Safe denial rule

The transition pipeline must support a safe denial path.

If the runtime reaches a valid Phase 1 success state that is not root-on-ready, or if the root-on proof is absent or invalid, the runtime must deny safely rather than attempt root-on.

Denial is valid.

A denial is not a root-on failure.

## 16. Phase 3 boundary

Phase 3 is the bounded settle window after root-on.

Phase 3 must prove:

- pelvis/root remains simulating
- topology remains preserved
- shell authority remains coherent
- no abort threshold is exceeded
- active-mode ownership handoff is coherent

Only after Phase 3 success may the runtime activate balance mode.

## 17. Activation contract

Activation must do all of the following:

- explicit authoritative state change to `BalancePerturbationActive`
- explicit “mode active” log
- enable perturbation scheduler
- enable active-mode diagnostics
- hand off authority to the active-mode owner
- stop transition-only behavior

The mode is not active before this point.

## 18. Recovery contract

When transition fails, recovery must:

- stop perturbation scheduling
- clear transition-local suppression
- clear transition-local timers
- clear transition-local cached reset state
- restore intended `BridgeActive` topology
- clear transient hazard flags
- return to one coherent state only:
  - `BridgeActive`
  - or `BalanceTransitionFailed`

Recovery is incomplete unless the runtime is coherent again.

## 19. Automatic retry rule

Automatic retry is permitted only if all are true:

- failure class is retryable
- recovery completed
- recovery restored coherent `BridgeActive`
- something material changed
- fresh quiet proof occurred after recovery
- retry cooldown elapsed
- retry budget is not exceeded

If these are not true, the runtime must not automatically loop.

## 20. Logging contract

Required one-shot logs:

- request queued + reason
- queue gate satisfied
- preflight begin
- preflight reject + reason
- transition phase changes
- Phase 1 handoff summary
- Phase 2 deny / root-on summary
- Phase 3 settle success or failure
- activation
- transition cleanup summary
- shell-authority transfer begin / success / failure

Required retry-loop logs:

- why retry is allowed or denied
- what changed
- whether fresh quiet proof was re-established
- remaining retry budget

## 21. Invariants

These must always hold:

- queued requests must have a real future satisfaction path
- transition-owned conditions must not be treated as permanent rejection reasons
- balance mode cannot be marked active while pelvis/root is kinematic
- failed transition must restore coherent `BridgeActive`
- repeated Phase 2 spikes must not brute-force through retries without new evidence
- shell/capsule authority must be owned explicitly at each phase boundary

## 22. Acceptance criteria

This spec is satisfied only when:

- queued requests converge
- requests do not loop forever
- transition ownership is explicit
- safe denial works
- active mode is entered only after real transition success
- logs are compact and decisive