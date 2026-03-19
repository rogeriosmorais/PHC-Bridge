# Balance Perturbation Mode Design

Status: Authoritative feature overview
Scope: Stage 1 balance-only diagnostic mode
Audience: runtime, controls, debugging, validation, and transition work for PhysAnim bridge

## 1. Purpose

Balance Perturbation Mode is a standing-balance diagnostic mode for Stage 1.

Its job is to measure **true PHC-driven recovery of the articulated body** after controlled perturbations, without conflating that recovery with:

- locomotion assistance
- shell/world translation assistance
- capsule dragging
- startup handoff assistance
- hidden movement-system correction
- transition-only shaping that exists only during entry

This mode is not a locomotion mode and not a shell-motion test.

It is a standing-balance test mode that becomes valid only after the runtime completes the documented entry pipeline.

## 2. Authoritative document split

This file is the feature-level overview only.

The normative runtime contracts live in:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`
- `plans/stage1/40-design/balance_mode_phase2.md`

Interpretation rule:

- this file defines what the feature is for
- the entry spec defines how the mode is entered
- the Phase 1 spec defines the pre-root-on stabilization contract
- the Phase 2 spec defines root-on, guard-window, and handoff behavior

If any rule in this file conflicts with those three specs, the three specs win.

## 3. Activation rule

Balance Perturbation Mode is active only after the entry transition succeeds.

That means:

- not while queued
- not during preflight
- not during Phase 1 stabilization
- not during Phase 2 root-on
- not during Phase 3 settle

The mode is active only after the runtime has completed the documented entry sequence and has explicitly switched into active balance mode.

## 4. Core active-mode requirements

While Balance Perturbation Mode is active, the runtime must enforce all of the following:

- standing balance only
- no locomotion entry
- no bridge-owned movement drive
- no CharacterMovement walking or custom locomotion authority
- no shell/world translation as a recovery mechanism
- no perturbation firing without an active quiet proof
- no hidden active-mode assistance that changes the interpretation of recovery

This mode is intended to test the articulated body’s recovery behavior, not gameplay movement support.

## 5. Perturbation method

The canonical perturbation method for the initial framework is:

- target body: `pelvis`
- type: single instantaneous linear impulse
- space: world space
- directions: `+X`, `-X`, `+Y`, `-Y`

This is the only authoritative perturbation path for the first-pass framework.

The implementation may add more perturbation methods later, but they are not part of the baseline contract unless this document is revised.

## 6. Active-mode ownership

When the mode becomes active, the runtime must have an explicit active-mode owner.

Required owner:

- `BalanceModeRuntimeOwner`

This owner is responsible for:

- holding locomotion-disabled state while active
- holding shell-assist-disabled state while active
- holding perturbation scheduler enable state while active
- invalidating runs if forbidden owners become active
- handing authority back only on explicit mode exit

## 7. Quiet proof requirement

A perturbation may fire only when active-mode quiet proof is valid.

Required proof object:

- `ActiveModeQuietProof`

Required owner:

- `BalanceQuietProofAccumulator`

This owner is responsible for:

- starting accumulation when active-mode quiet conditions become true
- resetting accumulation when they become false
- invalidating the proof on contamination, locomotion entry, or fail-stop
- reporting actual accumulated duration
- exposing validity to the perturbation scheduler

Default named requirement:

- `BalanceModeQuietRequiredSeconds = 1.0`

## 8. Scenario controller

A single owner must control perturbation scenarios.

Required owner:

- `BalanceScenarioController`

It owns:

- scenario selection
- scenario arming
- fire-once behavior
- cooldown start
- timeout start
- scenario completion classification

The initial scenario matrix is deterministic and fixed.

## 9. Run recording and classification

A single owner must aggregate run truth.

Required owners:

- `BalanceRunRecorder`
- `BalanceRecoveryEvaluator`
- `BalanceContaminationEvaluator`

Together they are responsible for:

- run start snapshot
- perturbation proof
- recovery-window metric accumulation
- contamination flags
- pass/fail inputs
- final run classification

## 10. What counts as a valid recovery result

A run counts as recovered only if all are true:

- perturbation was valid
- the pelvis body showed measurable response
- no fail-stop or instability termination occurred
- pelvis/root tilt returned below recovery threshold
- pelvis linear velocity returned below recovery threshold
- pelvis height returned to acceptable standing band
- stable condition was sustained for required hold duration
- no locomotion entry occurred during the balance-only test
- the run was not contaminated by shell/world correction

A run counts as contaminated if shell/world or movement-system correction contributed material corrective displacement during the recovery window.

## 11. Trustworthiness rule

The framework is trustworthy only if it can demonstrate all of the following:

- perturbation clearly reaches the pelvis body
- response is measurable
- response scales with perturbation tier
- shell/world correction is absent or explicitly flagged
- recovery metrics are stable and repeatable
- if PHC/control authority is intentionally weakened, recovery quality worsens

Required owner for negative tests:

- `BalanceValidationHarness`

## 12. Final summary

Balance Perturbation Mode is a standing-balance diagnostic mode.

It is entered only through the documented transition pipeline.

Once active, it tests articulated recovery using pelvis impulses while locomotion and shell-assisted recovery remain disabled or explicitly classified as contamination.

This file is the feature overview only.

The normative runtime behavior is defined by:

- `balance_mode_entry_transition_spec.md`
- `balance_mode_phase1_stabilization_spec.md`
- `balance_mode_phase2.md`