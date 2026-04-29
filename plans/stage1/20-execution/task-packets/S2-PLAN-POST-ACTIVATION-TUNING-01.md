# S2-PLAN-POST-ACTIVATION-TUNING-01 — Post-Activation Tuning Plan

## Purpose

Define the next tuning sequence only after the activation proof baseline is locked. This task does not implement tuning and does not change runtime behavior.

## Classification

Planning / governance.

This is not:
- runtime implementation
- tuning implementation
- validator or adapter refactoring
- activation rewrite
- support-truth rewrite
- failure-arbitration rewrite
- asset authoring

## Locked Baseline Snapshot

```text
StandingProof.Live: PASS
StandingProof.NegativeSupport: PASS with expected ActivationSupportFailure
ActivationPath.Wiring: PASS
PIE.G2Presentation: PASS
proof infrastructure cleanup: complete
capsule validation: live
continuity validation: live
activation bypass: closed
```

## Allowed Files

- `plans/stage1/20-execution/task-packets/S2-PLAN-POST-ACTIVATION-TUNING-01.md`
- `plans/stage1/20-execution/execution-log.md`

## Forbidden Files

- all source files
- all test files
- all runtime adapter files
- all runtime orchestrator files
- all runtime termination files
- all validators files
- all support-truth files
- all failure-arbitration files
- all assets
- all ONNX files
- all control tuning files
- all locomotion tuning files
- all PoseSearch tuning files
- all mass tuning files
- all PhysicsControl redesign files
- all workflow/process files other than `execution-log.md`

## Required Work

1. Freeze the current proof baseline as the prerequisite for any future tuning.
2. Define the next tuning sequence in this order:
   A. visual stability / jitter measurement
   B. support-preserving control tuning
   C. perturbation tolerance
   D. locomotion transition readiness
   E. broader runtime integration
3. For each future tuning phase, define:
   - objective
   - allowed future files
   - metrics
   - pass/fail criteria
   - regression tests
   - stop conditions
4. Keep the plan architecture-conforming and small.
5. Update `execution-log.md` so the next task is this plan packet and the status is `runnable`.

## Tuning Phases

### A. Visual Stability / Jitter Measurement

- Objective: measure standing-mode jitter before any tuning changes.
- Allowed future files: control tuning files, mass tuning files, PhysicsControl redesign files, and any later evidence artifacts needed to record the measurement baseline.
- Metrics: root translation jitter, pelvis angular jitter, camera-relative sway, and support-hull variance.
- Pass/fail criteria: pass when the baseline is measured reproducibly and the jitter metrics are available for comparison; fail if the measurement is not repeatable or requires changing locked proof logic.
- Regression tests: `PhysAnim.StandingProof.Live`, `PhysAnim.PIE.G2Presentation`, `PhysAnim.ActivationPath.Wiring`.
- Stop conditions: stop if the only path to lower jitter is to weaken proof gating, touch validators/adapters, or alter activation semantics.

### B. Support-Preserving Control Tuning

- Objective: improve control smoothness while preserving support-state correctness.
- Allowed future files: control tuning files, mass tuning files, PhysicsControl redesign files.
- Metrics: support-hull area stability, contact churn, shell displacement, and controller stability windows.
- Pass/fail criteria: pass when control smoothness improves without reducing support-hull validity or proof-chain pass rates; fail if support metrics regress.
- Regression tests: `PhysAnim.StandingProof.Live`, `PhysAnim.StandingProof.NegativeSupport`, `PhysAnim.PIE.G2Presentation`.
- Stop conditions: stop if the tuning requires proof bypass, runtime state-machine edits, or support-truth changes.

### C. Perturbation Tolerance

- Objective: increase recovery quality under pushes and other controlled disturbances.
- Allowed future files: control tuning files, mass tuning files, PhysicsControl redesign files, later perturbation-test files.
- Metrics: recovery time, recovery distance, peak tilt, peak actor displacement, and terminal reason distribution.
- Pass/fail criteria: pass when recovery improves across the perturbation set without destabilizing the locked standing baseline; fail if disturbance handling becomes less deterministic.
- Regression tests: `PhysAnim.RuntimeTermination`, `PhysAnim.StandingProof.NegativeSupport`, `PhysAnim.StateMachine.Phase2Standing`.
- Stop conditions: stop if recovery depends on changing validators, arbitration, or termination semantics.

### D. Locomotion Transition Readiness

- Objective: prepare the standing bridge for locomotion transitions without breaking the current proof baseline.
- Allowed future files: locomotion tuning files, PoseSearch tuning files, control tuning files, and later transition-specific test files.
- Metrics: transition success rate, pose-search stability, startup latency, and transition rollback rate.
- Pass/fail criteria: pass when locomotion readiness improves while the standing proof chain remains green; fail if transition work causes standing regressions.
- Regression tests: `PhysAnim.ActivationPath.Wiring`, `PhysAnim.StandingProof.Live`, `PhysAnim.StateMachine.Phase1Entry`.
- Stop conditions: stop if the work requires bridge activation rewrites, support-truth changes, or runtime adapter redesign.

### E. Broader Runtime Integration

- Objective: widen runtime integration only after the standing proof and transition readiness are stable.
- Allowed future files: later runtime-integration packets, runtime-adjacent tuning files, and any narrow proof/telemetry files explicitly named by those future packets.
- Metrics: cross-system latency, artifact consistency, runtime-state agreement, and regression-free integration breadth.
- Pass/fail criteria: pass when broader runtime integration adds capability without regressing the locked proof baseline; fail if integration broadens faster than the proof chain can hold.
- Regression tests: the locked proof tests plus the future integration packet's own deterministic tests.
- Stop conditions: stop if the integration path violates the architecture lock or forces a custom runtime system.

## Required Tests

- not applicable for this planning task

## Required Build

- not applicable

## Required Scope Check

- `.\\scripts\\check_task_scope.ps1 -TaskPacket plans/stage1/20-execution/task-packets/S2-PLAN-POST-ACTIVATION-TUNING-01.md -WorkingTree -AllowExecutionLog`

## Required Workflow Check

- `.\\scripts\\check_workflow_state.ps1 -Mode status -Strict`

## Definition Of Done

- locked proof baseline is summarized
- next tuning sequence is defined in five phases
- each phase has objective, allowed future files, metrics, pass/fail criteria, regression tests, and stop conditions
- no runtime, tuning, asset, or validator code is edited
- `execution-log.md` points to this packet and marks it runnable
- scope check passes
- strict workflow check passes

## Stop Conditions

Stop immediately if:
- any C++ file needs to be edited
- any asset or ONNX file needs to be edited
- any tuning value needs to be changed in this task
- any validator, adapter, pipeline, arbitration, or support-truth logic needs to be changed
- the plan cannot keep the architecture lock intact
- the plan cannot define measurable pass/fail criteria for a phase
- the same conceptual uncertainty repeats twice

## Required Handoff

```text
Summary:
Task: S2-PLAN-POST-ACTIVATION-TUNING-01
Base:
Head:
Commit:
Build: not applicable
Tests: not applicable
Scope:
Ledger impact:
Execution log:
Files changed:
Forbidden files touched:
Working tree:
Next task:
```
