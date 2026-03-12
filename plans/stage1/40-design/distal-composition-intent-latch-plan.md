# Distal Composition Intent-Latch Plan

## Why this pass

The cleaned policy-step movement trace and the distal-composition ablation together gave a specific answer:

- disabling distal composition mode makes locomotion worse
- but the current speed-only exit logic allows the mode to fall out during live movement when speed dips transiently
- those mode transitions line up with some of the worst locomotion-time outlier frames

So the next seam is not "remove the mode." It is "stop letting it churn while movement intent is still active."

## Evidence

From the latest clean trace:

- `Forward` transitioned from `true -> false` mid-phase and produced a `~4644 deg/s` frame
- `StrafeLeft` also transitioned off mid-phase
- `StrafeRight` and `Backward` spent most of the phase in distal mode, which still means the mode is needed, but it also means mid-phase transitions are not helping

## Docs/source checked

### Unreal Engine

- `APawn::GetPendingMovementInputVector`
  - official docs: pending movement input is the most up-to-date input vector before consumption
- `APawn::GetLastMovementInputVector`
  - official docs: last processed movement input reflects the input that actually affected movement
- `UCharacterMovementComponent::GetCurrentAcceleration`
  - local engine source: returns the live acceleration used by movement

### PhysicsControl

- `UpdateControls` remains our manual-tick bridge point.
- There is no engine-side concept that requires distal composition mode to switch off purely because planar speed dipped for a frame.

### ProtoMotions

- There is no training-side counterpart to this UE-only distal-composition toggle.
- That makes aggressive heuristic churn more suspicious than a sticky latch.

## Plan

1. Keep distal composition mode enabled by default.
2. Change the exit path so that once the mode is active, it stays active while there is live locomotion intent:
   - pending movement input, or
   - last processed movement input, or
   - current movement acceleration
3. Preserve the existing speed threshold and exit hold for true locomotion shutdown.
4. Add a component test that proves:
   - active mode stays latched through a low-speed dip while movement intent is present
   - active mode can still deactivate when intent is gone and the low-speed hold completes
5. Rebuild and rerun:
   - `PhysAnim.Component`
   - `PhysAnim.PIE.MovementSmoke`
   - `PhysAnim.PIE.MovementTraceSmoke`
6. Compare:
   - number of within-phase mode transitions
   - locomotion-phase max/p95 angular speed
   - lower-limb occupancy

## Success criteria

- no regressions in smoke stability
- fewer or zero within-phase distal-composition deactivations
- materially lower outlier frames on the affected phases

## Failure criteria

- no change in transition churn
- locomotion maxima regress
- mode stays active too long and worsens late post-move settling
