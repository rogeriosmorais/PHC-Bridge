# Failure Mode Branch Map - Stage 1 (Standing Stability)

This document maps known and anticipated **BalanceActive** failure modes to explicit graph branches. It prevents agents from improvising when regressions occur.

## 1. Branch: Phase 3 Settlement Instability
- **Evidence**: `terminal_reason: phase3_post_root_on_instability`
- **First Diagnostic**: Inspect `PeakAngularSpeedDegPerSec` and `MaxRootTiltDeg` during the 40-tick grace window.
- **Allowed Direction**: Tighten `RootExpansionLimit`, extend grace window slightly, or dampen initial joint velocities.
- **Forbidden Shortcut**: Bypassing `ValidatePhase3Continuity` or disabling the instability check.
- **Next Graph Action**: Execute `node_78833e2a534a` (Phase 3 Decision).

## 2. Branch: Physical Continuity Failure
- **Evidence**: `physical_continuity_validator_passed: false` or `terminal_reason: ActivationContinuousSimulationLost`.
- **First Diagnostic**: Check `TopologyChangeCount` and `PelvisSleepDurationMs`.
- **Allowed Direction**: Improve collision filtering, enforce joint limits, or reduce substep delta.
- **Forbidden Shortcut**: Manually snapping actor transforms or forcing `bTerminated = false`.
- **Next Graph Action**: Create **S1-INSTRUMENT-CONTINUITY-SPIKE-01** to isolate the jump.

## 3. Branch: Authority Contamination
- **Evidence**: `authority_conflict_count > 0` or `shell_helper_used_count > 0`.
- **First Diagnostic**: Compare `CapsuleWorldPosCm` vs. `ShellRootTransform` in the terminal artifact.
- **Allowed Direction**: Tighten kinematic shell locks, synchronize CMC mode changes, or improve arbitration priority.
- **Forbidden Shortcut**: Ignoring non-zero counters to claim product success.
- **Next Graph Action**: Create **S1-SHELL-AUTHORITY-HARDENING-01**.

## 4. Branch: Controller Instability
- **Evidence**: `terminal_reason: ActivationInstabilityThresholdBreach` or high `RmsMismatchDeg`.
- **First Diagnostic**: Inspect `ControllerStabilityFailureField` in the artifact.
- **Allowed Direction**: Tune PID gains (with TDD proof), apply gain scheduling, or implement low-pass filtering on targets.
- **Forbidden Shortcut**: Tuning gains for a single test seed without multi-seed validation.
- **Next Graph Action**: Create **S1-CONTROLLER-TUNING-ITERATION-01**.

## 5. Branch: Evidence Insufficiency (G2 Block)
- **Evidence**: Missing fields in `terminal_artifact.json` or schema mismatch.
- **First Diagnostic**: Compare `PhysAnimProofArtifactEmitter.cpp` against `docs/artifact-schema-check-s1.md`.
- **Allowed Direction**: Extend `FPhysAnimRunArtifactSnapshot` struct and update serialization logic.
- **Forbidden Shortcut**: Using manual log excerpts as a substitute for structured JSON evidence.
- **Next Graph Action**: Create **S1-SCHEMA-INSTRUMENTATION-TASK-01**.

## 6. Hard Stop Rules
Agents MUST stop and escalate to the USER if any of the following occur:
- **Rule 6.1**: Two consecutive attempts yield the same terminal reason without evidence of logic change.
- **Rule 6.2**: Two different fix hypotheses fail to move the stability duration by >10%.
- **Rule 6.3**: An implementation requires changing the **Architecture Lock** defined in `AGENTS.md`.

> [!WARNING]
> Escalation is mandatory when a Hard Stop Rule is triggered. Failure to escalate is a breach of the branch-based execution policy.
