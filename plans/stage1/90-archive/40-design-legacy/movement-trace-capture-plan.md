# Movement Trace Capture Plan

## Direction Check

- Keep going in the broader training/runtime alignment direction.
- Do not spend the next pass on another guessed lower-limb response tweak.
- Close the workflow gap between trace-capable runtime code and the standard movement regression harness.

## Why This Pass

- The bridge now writes per-frame summaries for the packed policy inputs.
- Standard `PhysAnim.PIE.MovementSmoke` runs still do not produce trace sessions by default.
- Without a deterministic trace-producing regression path, the new observability is not actionable.

## Sources To Re-check Before Coding

- Official UE docs:
  - `UPhysicsControlComponent`
  - `UCharacterMovementComponent`
- Local UE 5.7 source:
  - `PhysAnimComponent.cpp`
  - `PhysAnimBridgeTrace.cpp`
  - automation movement smoke test path
- ProtoMotions docs/code:
  - configuration guide
  - `humanoid_obs.py`
  - `mimic_obs.py`
  - `terrain_obs.py`

## What To Add

1. A dedicated PIE automation test that:
   - enables full bridge trace output
   - runs the deterministic movement smoke
   - verifies a new trace session folder was created
   - verifies `session.json`, `events.jsonl`, and `frames.csv` exist
   - verifies `frames.csv` contains the new packed-input summary columns
   - verifies at least one named movement smoke phase appears in the frame rows
2. A startup log line with the resolved trace session folder path for manual inspection.

## Scope Limits

- No runtime locomotion control tuning changes.
- No policy remapping changes.
- No asset authoring changes.
- No new lower-limb heuristic fitting in this pass.

## Success Criteria

- Build passes.
- `PhysAnim.Bridge` passes.
- `PhysAnim.Component` passes.
- `PhysAnim.PIE.MovementTraceSmoke` passes.
- Existing `PhysAnim.PIE.MovementSmoke` still passes.

## Expected Outcome

- This should be a keepable observability workflow pass.
- After this pass, the next locomotion-time alignment decision can be made from deterministic trace output produced by a standard automation run.
