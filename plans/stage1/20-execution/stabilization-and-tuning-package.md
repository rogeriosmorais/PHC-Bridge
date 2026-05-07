# Stabilization and Tuning Package

## Purpose

This package defines how Stage 1 stabilizes true balance mode without weakening the physical proof.

Tuning is allowed only after the relevant diagnostic surface can explain the failure. A run that becomes visually calmer through hidden assistance is a failed run, not progress.

## Stabilization Order

Work in this order:

1. **Continuity first**: every truth-set body must remain a valid simulating body across the attempt.
2. **Authority second**: eliminate shell helper, CMC, global blend, mesh-wide kinematic, excluded-body bracing, and locomotion writes.
3. **Support truth third**: ensure foot/ball contact, support hull, proxy drift, support gap, and churn are measured from live physics.
4. **Target fidelity fourth**: remove target discontinuities, action scaling errors, frame-conversion errors, and excessive pose mismatch.
5. **Controller tuning last**: change gain, damping, ramp, clamp, smoothing, or per-body strength only after the earlier layers are observable and passing.

Do not tune around a missing artifact field.

## Allowed Tuning Surfaces

Allowed:

- Physics Control strength and damping values
- blend/ramp duration
- PHC action scale and clamp
- action smoothing
- target step limit
- body-set inclusion when tracked by the plant contract
- per-body diagnostic variants when they emit enough evidence to compare outcomes

Not allowed:

- CMC assistance during balance proof
- shell locking or root freezing beyond the contract
- world bracing through excluded bodies
- mesh-wide kinematic rescue
- silently relaxing support/contact truth
- declaring safe denial as success
- changing architecture away from NNE/ONNX plus Physics Control

## Evidence Ladder

Each stabilization attempt must produce one of these evidence outcomes:

| Outcome | Required evidence |
|---|---|
| Diagnostic progress | New or clearer artifact/log fields identify first failure body, time, threshold, or command path. |
| Stability progress | Hold duration, root tilt, peak angular speed, support churn, or proxy drift improves without new authority contamination. |
| Contract failure | Terminal reason is canonical and reconstructible from artifact fields. |
| Product success | `BalanceActive_Standing` held continuously for `3.0` seconds with `terminal_reason = nullptr`. |

Diagnostic progress may close diagnostic tasks. It may not close product-success tasks.

## Minimum Metrics To Compare Tuning Runs

Capture these fields before judging a tuning change:

- `hold_duration_sec`
- `terminal_reason`
- `terminal_substep_timestamp`
- `max_root_tilt_deg`
- `peak_angular_speed`
- `support_mode`
- `support_gap_timer_ms`
- `support_churn_hz`
- `proxy_inside_hull`
- `proxy_outside_hull_duration_ms`
- `controller_gain_scale`
- `controller_damping_ratio`
- `target_discontinuity_deg`
- `rms_mismatch_deg`
- `max_body_mismatch_deg`
- first threshold body/time for linear and angular instability when available

## Required Test Gates

Use the narrowest deterministic gate first, then runtime evidence only when claiming runtime behavior:

```text
.\scripts\build.ps1
.\scripts\build.ps1 -Test PhysAnim.Validators.ControllerStability.ValidateControllerStability
.\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.StabilityMetrics
.\scripts\build.ps1 -Test PhysAnim.Diagnostics.ThighRestore
.\scripts\build.ps1 -Test PhysAnim.PIE.BalanceModeSmoke
```

Read runtime logs from:

```text
F:\NewEngine-AgentB\PhysAnimUE5\Saved\Logs
```

## Tuning Stop Rules

Stop tuning and return to contract/instrumentation work when:

- a failure cannot be reconstructed from artifact fields
- `terminal_reason` is non-canonical
- a body stops simulating without `activation_continuous_simulation_lost`
- CMC, shell helper, global blend, or kinematic mesh assist appears
- calf/excluded-body world contact contributes to apparent standing
- a diagnostic run reads stale logs from a different checkout

## Current Diagnostic Interpretation

`PhysAnim.Diagnostics.ThighRestore` is useful for comparing thigh restore timing, ramping, kinetic-gate behavior, and first threshold offenders.

It is not evidence of true balance success unless paired with a passing balance run that reaches `BalanceActive_Standing` and holds for `3.0` seconds.

## Package Output

The next implementation tasks should use this package to choose changes that either:

- improve observability for the first real balance blocker, or
- improve stability while preserving every authority and truth constraint.
