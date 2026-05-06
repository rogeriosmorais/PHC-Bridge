# S2-IMPL-V0-RAW-SIM-THIGH-HIP-ISOLATION-01

## Purpose

Isolate whether thigh/hip amplification comes from target discontinuity, excessive thigh angular strength, hip constraint setup, or passive hip/thigh raw physics.

## Context

Continuing from `S2-IMPL-V0-RAW-SIM-GROUP-C-STABILITY-01`.
Diagnostics show:
- PHC is disabled.
- action samples are zero.
- explicit control target writes are zero.
- passive raw-sim/contact/constraint motion starts immediately.
- torso-zero and support-zero still fail early.
- thigh-zero delays the major spine spike past the 0.3s zero window and extends support duration.

## Goal

Isolate the source of thigh/hip amplification by testing restore variants and detailed logging.

### Thigh Restore Variants
- abrupt restore to 0.20 after 0.30s
- ramp restore from 0.00 to 0.20 over 0.30s
- restore to 0.05 after 0.30s
- keep thigh controls zero for the full run as a control case

### Logging at Restore
- thigh_l/thigh_r target orientation delta
- thigh_l/thigh_r angular strength/damping
- pelvis, thigh, spine_01, spine_02, spine_03 velocities
- support hull and active sides
- currentPoseTargetsSeeded / cached target state
- first body to exceed linear/angular thresholds after restore

## Acceptance Criteria

- explain whether the thigh/hip problem is target discontinuity, gain magnitude, hip constraint setup, or passive physics
- no PHC/policy tuning
- no support grace changes
- no hidden CMC/capsule/shell assist

## Allowed Files

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.PhysicsTuning.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-V0-RAW-SIM-THIGH-HIP-ISOLATION-01.md`
- `plans/stage1/20-execution/evidence/S2-IMPL-V0-RAW-SIM-THIGH-HIP-ISOLATION-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Required Tests

- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1`
- `powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1 -Test PhysAnim.ActivatedStanding.ThighHipIsolation`
