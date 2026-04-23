# Policy-Step Trace Alignment Plan

## Direction Check

- Keep going in the broader training/runtime alignment direction.
- Do not use the new trace system to justify another lower-limb heuristic tweak yet.
- First align the trace sampling policy with the actual bridge policy cadence.

## Why This Pass

- The bridge trace design says `frames.csv` should contain one row per sampled bridge policy step.
- The current movement trace shows thousands of non-policy rows with blank input summaries and empty movement phases.
- Those rows dominate naive analysis and make the new trace summaries much less useful than intended.

## Evidence From The Latest Movement Trace

- `<blank>` movement phase rows dominate the trace.
- The largest outlier counts are heavily mixed into those blank rows.
- Only a minority of rows carry the input summaries added in the last pass, because those summaries are only valid on policy-update ticks.

## Sources To Re-check Before Coding

- Official UE docs:
  - `UPhysicsControlComponent`
  - `UCharacterMovementComponent`
- Local UE 5.7 source:
  - bridge tick and trace write path in `PhysAnimComponent.cpp`
  - trace writer schema in `PhysAnimBridgeTrace.cpp`
- ProtoMotions docs/code:
  - configuration guide
  - active checkpoint config
  - observation packing paths that already operate at the policy/control cadence

## What To Change

1. Write trace frame rows only for sampled policy steps during the active bridge runtime.
2. Keep sparse events as the source of truth for startup, shutdown, and state transitions.
3. Preserve the current per-policy-step input summaries:
   - `self_obs`
   - `mimic_target_poses`
   - `terrain`
   - movement smoke phase
   - distal composition mode
4. Strengthen `PhysAnim.PIE.MovementTraceSmoke` so it verifies:
   - all frame rows are sampled policy-step rows
   - no blank movement phases remain in the trace rows

## Scope Limits

- No locomotion control tuning changes.
- No policy remapping changes.
- No asset authoring changes.
- No trace schema expansion in this pass.

## Success Criteria

- Build passes.
- `PhysAnim.Bridge` passes.
- `PhysAnim.Component` passes.
- `PhysAnim.PIE.MovementTraceSmoke` passes.
- `PhysAnim.PIE.MovementSmoke` still passes.
- The new trace session contains only policy-step frame rows.

## Expected Outcome

- This should be a keepable observability-quality pass.
- After it lands, the next locomotion alignment decision can be made from a much cleaner trace artifact.
