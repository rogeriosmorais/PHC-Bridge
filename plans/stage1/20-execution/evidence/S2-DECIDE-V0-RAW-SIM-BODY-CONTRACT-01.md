# S2-DECIDE-V0-RAW-SIM-BODY-CONTRACT-01 Evidence

Base: `a42e8f2`

## Decision

V0 activated-standing proof requires raw simulation for exactly these standing truth bodies:

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

The remaining bodies in `GetRequiredBodyModifierBoneNames()` are not V0 standing truth bodies. They must be explicitly classified as excluded/distal/non-V0, isolated from world bracing, and prevented from injecting energy into the V0 critical/support set.

## Non-Goals

- Do not add staged runtime activation.
- Do not accept `simMax=0`.
- Do not force all 22 required body modifiers raw-simulating in V0.

## Rationale

The raw-sim bisect showed that Group C, the 10-body V0 set, has the required critical/support coverage but still loses support quickly. The full 22-body set adds a catastrophic energy spike and is not the V0 proof target.

Next implementation target: make Group C stable enough to hold while preserving the final contract: 10 named bodies raw-simulating together, PHC/control live, nonzero but bounded motion, support valid, no actor offset, no CMC, no shell/global assist, and excluded bodies non-contaminating.
