# A1-A7 Evidence Summary: Thigh-Hip Isolation

## Hypothesis
Instability in the V0 10-body raw-sim setup is driven by active thigh control torque rather than passive physics or torso/support control.

## Diagnostic Matrix (A1-A7)

| Variant | Control Applied | Result | Findings |
|---------|-----------------|--------|----------|
| **A1** | Passive Only | **Stable (4.0s)** | No spikes. Passive physics is stable. |
| **A5** | Torso Only | **Stable (1.18s)** | No spikes. Failed on drift, but no explosion. |
| **A6** | Thigh Only (Strength 0.2) | **Exploded (0.18s)** | **Immediate Spine Spike.** Identifies thighs as the cause. |
| **A7** | Support Only | **Stable (4.0s)** | No spikes. Support controls are stable. |

## Causal Pathway
`Thigh PhysicsControl Torque -> Pelvis Coupling -> Spine Spike -> Catastrophic Disintegration`

## Conclusion
The instability is definitively triggered by the activation of thigh controls. The mitigation strategy must focus on the thigh control activation envelope (strength sweep, ramping, and kinetic gating).

## Log References (from graph history)
- `PhysAnimUE5-backup-2026.05.06-20.19.49.log`
- `PhysAnimUE5-backup-2026.05.06-20.21.02.log`
- `THIGH_TICK` logs from A6 showed rapid energy gain before spike.
