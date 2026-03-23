# Phase 1 / LateValidate Truth Model

Status: Authoritative design contract  
Scope: Truth sources and gating rules for `Prepare` and `LateValidate`

## 1. Frozen Phase 1 Topology Contract

When Phase 1 is accepted, the following topology is frozen as the authoritative contract:

- **Root (pelvis)**: `Kinematic`
- **Proximal set**: `Simulated`
- **Distal set**: `Kinematic`
- **Upper-body set**: `Kinematic`

### Expected sim counts (Stage 1)
- `simCount = 5` (proximal only)
- `proximalSimCount = 5`
- `distalSimCount = 0`
- `upperBodySimCount = 0`

## 2. Ownership Source Priority

Decision gates must evaluate ownership by consulting sources in this strict priority order:

1. **Frozen Topology Contract**: The absolute source of truth for what *should* be happening during this attempt.
2. **Intended Ownership**: The high-level state machine's current request.
3. **Modifier-Record Ownership**: What `PhysicsControl` believes it has applied.
4. **Raw Body State**: The actual simulation state observed from the physics engine.

## 3. LateValidate Gate Definitions

These gates define the transition from Phase 1 (`Prepare`/`LateValidate`) to Phase 2 (`RootOn`).

### [bringUp] - Control Authority Settlement
- **Condition**: `FinalBringUpGroupControlAuthorityAlpha >= 1.0` (with `KINDA_SMALL_NUMBER` epsilon).
- **Source**: `UPhysAnimComponent::CalculateBringUpGroupControlAuthorityAlpha` (reads `HighestUnlockedBringUpGroupIndex` and `MaxAutoUnlockBringUpGroup`).
- **Logic**: All bring-up groups (including the final stabilization group) must be fully unlocked. This ensures no hidden "movement smoke" or transition-shaping forces are active during proof.

### [shellSafety] - Pre-Root-On Proof
- **Condition**: A multi-vector proof that the shell is stable and authority is correctly transitioned.
  - `RootOnReadinessShellProofDurationSeconds >= BalancePhase2PreRootOnShellProofRequiredSeconds`
  - `ShellOffsetDelta <= BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm`
  - `ShellVelocityDelta <= BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond`
  - `ShellOffsetGrowth <= BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm`
  - `ShellVelocityGrowth <= BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond`
  - **Not Actively Affecting**: `!bShellCorrectionActivelyAffecting` (metrics must be below significant thresholds: 0.1cm / 1.0cm/s).
  - **Lock/Anchor**: Shell must be `Locked` by the transition and `Reanchored` to the current state.
- **Source**: `FPhysAnimBalanceReadyTransition::BuildCertifiedHandoffSnapshot` (aggregating metrics from `FPhase1AcceptedConvergenceSnapshot` and `UPhysAnimComponent`).
- **Logic**: This gate proves that the shell authority transfer is not just logically correct but physically stable. Stale `shellCorrectionActive` states (owner active but metric below threshold) are logged but do not block the proof. Active metrics block the proof.

### [expectedRelease] - Upper-Body Release Readiness
- **Condition**: Both the LateValidate clock and the ShellHold clock must satisfy their required durations.
  - `LateValidationAccumulatedSeconds >= BalancePhase1LateValidateRequiredSeconds`
  - `RootOnReadinessShellHoldAccumulatedSeconds >= BalancePhase2RequiredShellHoldDuration`
- **Source**: `FPhysAnimBalanceReadyTransition::Tick` (comparing `LateValidationAccumulatedSeconds` and `RootOnReadinessShellHoldAccumulatedSeconds` against settings).
- **Logic**: This gate allows the upper body to transition from `LateValidationKinematicHold` to `None` (Normal ownership). It is the primary protection against "release instability" where the upper body sim-set collapses immediately upon RootOn.

### [readyProven] - Aggregate Readiness
- **Source**: `FPhysAnimBalanceReadyTransition::ValidateRootOnReadinessSnapshot` (aggregating the results of `bringUp`, `shellSafety`, and `expectedRelease`).
- **Logic**: All primary readiness signals must be `True`. This is the final "go/no-go" signal for Phase 2 entry.
  - Classification must be `RootCoupledReady` (proximal simulation count == 5).
  - Classification `UpperOnlySafeDeny` (proximal simulation count == 0) results in a safe denial instead of Phase 2 entry.

## 4. Operational Transition Rules

### Confirmation Semantics
- **Next-frame confirmation**: Raw body state is provisional on the same frame as an intent change. Ownership-violation diagnostics *must* require next-frame confirmation before triggerring a reset.
- **Same-frame mismatch**: A same-frame mismatch between intent and raw state is *insufficient evidence* for failure.

### Latching vs Recomputed
- **Latched**: Frozen topology, upper-body hold mode (must be frozen from Phase 1 contract, not live readiness).
- **Recomputed live**: Root validity, target continuity, max sim-body speeds, shell/reference deltas.

### Stale Latched State (Shell)
- **Informational**: `bShellCorrectionOwnerActive` without significant metric influence is logged as a "stale latch" but does not block the safety proof.
- **Blocking**: Only *active* influence (`bShellCorrectionActivelyAffecting`) blocks the `shellSafety` proof.
- **Source**: `FPhysAnimBalanceReadyTransition::BuildCertifiedHandoffSnapshot`.

### Pending-Reset Leakage
- **Global Block**: `bHasPendingResets` blocks admission to `LateValidate`.
- **Upper-Body Violation**: During `LateValidationKinematicHold`, any pending reset on an upper-body bone is a terminal instability violation.
- **Stale State**: Resets must be drained/applied before the quiet window can advance. This prevents "latent discontinuities" from passing the quiet window gate only to explode during LateValidate.
- **Source**: `UPhysAnimComponent::GetPendingBodyModifierCachedResetNames`.

## 5. Failure Classification

### Contract Failure
The runtime failed to uphold the logic of the bridge.
- Topology mismatch or wrong frozen capture.
- Illegal write leaks to simulated bones.
- `BridgeActive` distal re-promotion suppression failure.
- Stale or wrong convergence source.

### Physical-Viability Failure
The contract was upheld, but the physical world was too unstable to satisfy the gates.
- Accepted sim set (proximal) is dynamically unstable.
- Entry quietness collapses under contact/tuning.
- Body-motion instability after contract-correct admission.

### Stale-State / Reset-Leak Failure
The contract logic is correct, but the system state contains "latent" or "stale" data that invalidates the proof.
- Pending body-modifier resets leaked into LateValidate.
- Stale shell-correction flags held after metric quietness.
- Unapplied target caches from a previous attempt.

### Telemetry-Only Observation
Findings that inform debugging but do not independently trigger resets.
- Individual bone drift below the cumulative gate threshold.
- Sub-threshold target discontinuity.
- `Proxy` bone promotion confirmed by next-frame check.

## 6. Current Empirically Proven Findings

- **Auth writes**: Broad `"All"` writes are not authoritative enough for topology-critical bones; explicit per-bone writes are required.
- **Upper-body hold**: must be frozen from the Phase 1 contract, not live readiness.
- **Distal suppression**: `BridgeActive` distal re-promotion must be explicitly suppressed to prevent ownership thrash.
- **Confirmation timing**: Same-frame ownership mismatch is not sufficient evidence of failure.

## 7. Last Confirmed Blocker

- **Blunder**: `phase1_late_validate_upper_body_instability`
- **Context**: Convergence failures in `LateValidationKinematicHold`.
- **Confirmed Producer**: `ClassifyLateValidationFailureReason` had a parallel stale path; now consolidated to the authoritative observed-violation gate.
- **Note**: This section identifies the last confirmed blocker as of the latest smoke test and is expected to change frequently as the investigation progresses.
