# Locomotion Trace Summary Plan

## Direction Check

- Keep going in the broader training/runtime alignment direction.
- Do not continue with another blind lower-limb heuristic tweak as the next primary move.
- The remaining locomotion mismatch is now subtle enough that the next useful pass is observability, not another guessed runtime adjustment.

## Why This Pass

- We now have a working bridge trace session writer.
- We have already corrected several objective contract seams:
  - policy cadence
  - future target time packing
  - current/future mimic target reference origin
  - terrain-relative self observation
  - terrain input packing
- Movement smoke is still only partially improved.
- The next useful decision should come from packed runtime evidence, not another heuristic response split.

## Sources To Re-check Before Coding

- Official UE docs:
  - `UPhysicsControlComponent`
  - `UCharacterMovementComponent`
- Local UE 5.7 source:
  - `PhysicsControlComponentImpl.cpp`
  - current bridge trace writer
- ProtoMotions docs/code:
  - configuration guide
  - `humanoid_obs.py`
  - `mimic_obs.py`
  - `terrain_obs.py`
  - active checkpoint `config.yaml`

## What To Add

Add per-frame trace summaries for the packed policy inputs and runtime phase:

1. `self_obs`
   - root height scalar
   - mean absolute value
2. `mimic_target_poses`
   - mean absolute value
   - min/max future time channel
3. `terrain`
   - mean
   - min
   - max
   - center sample
4. movement/runtime context
   - movement smoke phase name
   - whether distal locomotion composition mode is active

## Scope Limits

- No runtime control-tuning changes in this pass.
- No policy/action remapping changes.
- No asset authoring changes.
- No changes to the active locomotion baseline beyond trace observability.

## Success Criteria

- Build passes.
- `PhysAnim.Component` passes.
- `PhysAnim.PIE.MovementSmoke` passes.
- Trace CSV rows contain the new summary fields.
- The pass leaves the current locomotion baseline behavior unchanged.

## Expected Outcome

- This should be a keepable observability pass.
- It should make the next alignment decision evidence-based.
- After this pass, the next runtime change should be chosen from actual traced input/output behavior during locomotion spikes.
