# PRD: AI Activation (The "Wake Up" Task) — EVIDENCE BASED

## Visao Geral
The goal of this task is to transition the character from a "static" physics-verified state to an "active" neural-controlled state. This is not merely a configuration toggle; it is a **verifiable physical transition** where progress is proved through durable artifacts and non-contradictory telemetry.

## Problem Definition
- **Current State:** `Physically-Verified Statue`. The PHC Bridge is active, but the `PhcPolicy` segment is in `ReachedButInactive` state.
- **The Gap:** We have "Actuation Readiness" but zero "Neural Intent" evidence.
- **Goal State:** `Active Neural Control`. The character must move its limbs based on neural inference, verified by an authoritative `evidence_summary.json`.

## Epic: Product Objectives (Evidence Pillars)

### Requirement: Durable Proof
Every test run MUST generate a uniquely identified (`AttemptUuid`) set of artifacts: `.log`, `_terminal.json`, and `_evidence_summary.json`.
**Tamanho:** S
**Prioridade:** 1

### Requirement: Segment Promotion
Successfully promote the `PhcPolicy` segment from `ReachedButInactive` to `Active` using strict classification rules.
**Tamanho:** S
**Prioridade:** 1

### Requirement: Truth Arbitration
Detect and report any contradictions between the AI's "intended action" and the "observed physics" (e.g., AI thinks it's moving, but sensors show zero displacement).
**Tamanho:** S
**Prioridade:** 1

## Epic: Evidence Requirements

### Requirement: policy_inference_success_count
The policy inference must run synchronously on the fixed physics/post-physics tick. The success count is normalized against actual simulation ticks (e.g., ratio of `policy_inference_success_count / expected_simulation_ticks` must be $\ge 0.95$).
**Tamanho:** S
**Prioridade:** 1

### Requirement: action_magnitude_variance
Must be > 0. A "frozen" action output (variance = 0) constitutes a failure of intent, even if the success count is high.
**Tamanho:** S
**Prioridade:** 1

### Requirement: input_buffer_integrity
`input_buffers_finite` must be `True`.
**Tamanho:** S
**Prioridade:** 1

### Requirement: input_buffer_warm_start
When transitioning from `ReachedButInactive` to `Active`, input buffers must be populated with historical telemetry from the preceding 10 frames of the physics-verified state to prevent startup joint torque spikes.
**Tamanho:** S
**Prioridade:** 1

### Requirement: control_target_normal_writes
Must increment in sync with inference successes.
**Tamanho:** S
**Prioridade:** 1

### Requirement: thigh_work_accumulator
We must observe active neural control work. To exclude gravity-driven settling forces, a warm-up baseline work measured from `0.0s to 0.05s` must be subtracted from the accumulator. The resulting net cumulative absolute work ($|positiveWork| + |negativeWork|$) in the `0.05s–0.30s` activation window must exceed a minimum work threshold of $E_{threshold} = 0.5 \text{ Joules}$.
**Tamanho:** S
**Prioridade:** 1

### Requirement: assistance_truth_clean
No external kinematic target forces, skeletal translation overrides, or velocity impulses were used to move the limbs. The standard `PhysicsControlComponent` must only apply joint forces parameterized as PD controllers targeting rotations derived directly from the neural policy's action output.
**Tamanho:** S
**Prioridade:** 1

### Requirement: simulation_truth_clean
The Chaos physics simulation remained stable and continuous. If the engine exits prematurely or crashes, the run status is marked as `FATAL_CRASH` and `simulation_truth_clean` is set to `False`.
**Tamanho:** S
**Prioridade:** 1

### Requirement: artifact_log_contradiction
The `.log` verdict must match the `_terminal.json` verdict. If the `_terminal.json` is missing or corrupt, it is treated as a contradiction/failure.
**Tamanho:** S
**Prioridade:** 1

## Success Criteria
A run achieves **Evidence-Based Success** ONLY if:
**Criterios de aceite:**
- The `PhysAnimEvidenceCollector` reports a `PRODUCT_SUCCESS_CANDIDATE` for the `Standing` or `Reaching` task.
- `PhcPolicy` state is `Active`.
- `action_magnitude_mean` > 0 AND `control_target_normal_writes` > 0.
- `Contradictions` is none in the collector report.

## Epic: Safety & Fallback State
If the verification run fails or triggers a `Kinetic Gate` safety threshold (e.g., extreme joint velocities indicating instability):
- The system automatically triggers the `SafetyGrip` fallback state.
- Policy inference is immediately disabled (`bEnablePolicyInference=false`).
- Joint controllers revert to holding the last known-good pose with high damping coefficients to prevent a chaotic "ragdoll" collapse.
- The state transitions back to `ReachedButInactive`.

## Epic: Implementation Sequence

### Task: Phase 1: Infrastructure Hardening (In-Progress)
Silence verbose tick-spam to prevent "Telemetry Pollution" during high-frequency inference.
**Tamanho:** S
**Prioridade:** 1

### Task: Phase 2: AI Engagement
Toggle `bEnablePolicyInference=true` and load the `MM_Idle` or `MM_Reach` reference animation.
**Tamanho:** S
**Prioridade:** 2

### Task: Phase 3: Artifact Verification
Execute `.\scripts\collect_evidence.py` and manually inspect the `ActionMagnitude` and `ThighWork` fields to ensure "Life-like" physical engagement.
**Tamanho:** S
**Prioridade:** 2

## Riscos
### Risk: Telemetry Inflation
High success counts but zero physical effect.
Probabilidade: Media. Impacto: Alto.
Mitigacao: Truth arbitration rules will mark this as INSUFFICIENT_EVIDENCE.

### Risk: Jitter Discontinuity
AI micro-corrections triggering the `Kinetic Gate`.
Probabilidade: Media. Impacto: Medio.
Mitigacao: We will monitor `gate_release_count` to ensure the AI isn't "fighting" the safety systems.

## Restricoes
### Constraint: Actuator Control Standard
The standard `PhysicsControlComponent` must only apply joint forces parameterized as PD controllers targeting rotations derived directly from the neural policy's action output.
