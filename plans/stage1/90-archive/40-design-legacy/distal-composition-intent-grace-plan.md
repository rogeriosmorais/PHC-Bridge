# Distal Composition Intent-Grace Plan

## Goal

Reduce remaining within-phase dropouts of the training-aligned distal locomotion composition mode without reopening the already-falsified lower-limb tuning branches.

## Why This Pass

The first intent-latch pass proved the explicit-only distal composition mode is still net-positive during locomotion, but the policy-step movement trace still showed a small number of mid-phase `true -> false` transitions while scripted movement was still active.

That pointed to a narrow seam:

- the mode should not exit immediately when speed dips briefly
- a short recent-intent grace window is cheaper and more defensible than another broad lower-limb retune

## Evidence Before Coding

- `APawn::GetPendingMovementInputVector()` and `APawn::GetLastMovementInputVector()` in local UE source confirm there is a real distinction between fresh input intent and the most recently consumed intent
- `UCharacterMovementComponent::GetCurrentAcceleration()` gives a third movement-intent surface on the gameplay side
- ProtoMotions has no direct training-side counterpart to the UE-only distal composition toggle, so reducing runtime heuristic churn is preferable to adding more lower-limb response complexity

## Proposed Runtime Change

Keep the committed intent-aware latch, but add a short grace window after active movement intent disappears.

Behavior:

1. resolve live movement intent from:
   - pending movement input
   - last movement input
   - current movement acceleration
2. when intent is active:
   - keep distal composition active
   - reset the recent-intent timer
3. when intent is inactive:
   - keep distal composition active while `time_since_active_intent <= grace_seconds`
   - only allow normal hysteresis exit after that window expires

## Initial Policy

- `DistalLocomotionCompositionPolicyIntentGraceSeconds = 0.20`

## Success Criteria

- build stays green
- `PhysAnim.Component` stays green
- `PhysAnim.PIE.MovementSmoke` stays green
- `PhysAnim.PIE.MovementTraceSmoke` stays green
- within-phase `distal_locomotion_composition_mode_active` flips drop relative to the prior intent-latch baseline
- locomotion-phase max or p95 lower-limb angular spikes improve or remain neutral

## Failure Criteria

- no measurable reduction in within-phase flips
- forward/backward/strafe maxima regress materially
- late post-movement settling gets worse again

## Expected Read

If this works:

- the remaining mode churn is mostly about transient intent gaps, not the validity of the distal composition policy itself

If this fails:

- the next seam is not more stickiness
- we should move to another locomotion-time representation contract rather than keep tuning exit behavior
