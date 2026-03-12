# Lower-Limb Contact Exclusion Alignment Plan

## Decision

It is still worth continuing in the Stage 1 training/runtime alignment direction.

It is not worth spending the next pass on more lower-limb target-step or response-magnitude retuning. Recent passes improved some samples but did not move the dominant remaining seam enough.

## Why This Is The Next Seam

ProtoMotions' SMPL humanoid asset explicitly excludes several lower-limb body collision pairs:

- `R_Knee <-> R_Toe`
- `R_Knee <-> L_Ankle`
- `R_Knee <-> L_Toe`
- `L_Knee <-> L_Toe`
- `L_Knee <-> R_Ankle`
- `L_Knee <-> R_Toe`

For Manny Stage 1 runtime, the closest body mapping is:

- `R_Knee -> calf_r`
- `L_Knee -> calf_l`
- `R_Ankle -> foot_r`
- `L_Ankle -> foot_l`
- `R_Toe -> ball_r`
- `L_Toe -> ball_l`

UE PhysicsControl only disables collision for the constrained parent/child pair on each control. It does not mirror a broader MJCF contact-exclude table automatically.

UE physics assets do support explicit body-pair disables through `UPhysicsAsset::DisableCollision(...)` and `CollisionDisableTable`.

So this is a real training/runtime mismatch candidate.

## Constraints

- Do not touch `asset-authored-physics-tuning-design.md`.
- Do not mutate the shared authored Manny physics asset in content.
- Keep the experiment reversible and runtime-only.
- Preserve the current verified baseline if this is not a clean win.

## Implementation Plan

1. Add a Stage 1 stabilization flag for training-aligned lower-limb collision exclusions.
2. Duplicate Manny's physics asset transiently at bridge activation.
3. Apply only the ProtoMotions-aligned lower-limb collision-disable pairs to the transient clone.
4. Swap the skeletal mesh to that transient physics asset only while the bridge owns runtime physics.
5. Restore the original authored physics asset on bridge teardown.
6. Add a component test that proves the helper disables the expected calf/foot/ball body pairs on a transient clone.
7. Run:
   - `PhysAnim.Component`
   - `PhysAnim.PIE.MovementSmoke`

## Success Criteria

- No fail-stop regression.
- Movement smoke still completes.
- Lower-limb outlier spikes improve enough to justify keeping the runtime exclusion policy.

## Failure Criteria

- No meaningful improvement in movement outliers.
- New regressions in forward/backward/strafe samples.
- Runtime complexity increases without a measurable locomotion benefit.

If it fails, restore the last safe baseline and record the result as a falsified alignment seam.

## Outcome

- March 12, 2026:
  - implemented as a transient runtime physics-asset clone during `BridgeActive`
  - verified with UE build, `PhysAnim.Component`, and `PhysAnim.PIE.MovementSmoke`
  - result:
    - Manny already overlapped most of the relevant lower-limb excludes
    - only `2 / 6` mapped pairs were newly disabled by the runtime clone
    - movement smoke stayed stable, but locomotion did not show a clean new win
  - decision:
    - keep the plan as a recorded falsified seam
    - do not keep the runtime collision-exclusion clone in the baseline
