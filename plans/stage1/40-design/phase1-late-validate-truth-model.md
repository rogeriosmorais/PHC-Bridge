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

## 3. Gating and Timing Rules

### Confirmation Semantics
- **Next-frame confirmation**: Raw body state is provisional on the same frame as an intent change. Ownership-violation diagnostics *must* require next-frame confirmation before triggerring a reset.
- **Same-frame mismatch**: A same-frame mismatch between intent and raw state is *insufficient evidence* for failure.

### Latching vs Recomputed
- **Latched**: Frozen topology, upper-body hold mode (must be frozen from Phase 1 contract, not live readiness).
- **Recomputed live**: Root validity, target continuity, max sim-body speeds, shell/reference deltas.

## 4. Failure Classification

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

### Telemetry-Only Observation
Findings that inform debugging but do not independently trigger resets.
- Individual bone drift below the cumulative gate threshold.
- Sub-threshold target discontinuity.

## 5. Current Empirically Proven Findings

- **Auth writes**: Broad `"All"` writes are not authoritative enough for topology-critical bones; explicit per-bone writes are required.
- **Upper-body hold**: must be frozen from the Phase 1 contract, not live readiness.
- **Distal suppression**: `BridgeActive` distal re-promotion must be explicitly suppressed to prevent ownership thrash.
- **Confirmation timing**: Same-frame ownership mismatch is not sufficient evidence of failure.

## 6. Current Leading Open Blocker

- **Blunder**: `phase1_late_validate_upper_body_instability`
- **Context**: Convergence failures in `LateValidationKinematicHold` often caused by `PendingBodyModifierCachedResetNames` or stale state-machine inputs.
