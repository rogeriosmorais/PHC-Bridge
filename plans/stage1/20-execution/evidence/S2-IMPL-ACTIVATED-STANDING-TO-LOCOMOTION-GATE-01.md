# S2-IMPL-ACTIVATED-STANDING-TO-LOCOMOTION-GATE-01 Evidence

Base: `d6329cc5230105764e99f6c2090f1f25e434b656`
Head: `2dc3c6ff6de63370016dc585fbea5e6dd9147f3a`
Commit: `2dc3c6ff6de63370016dc585fbea5e6dd9147f3a`
Build: `SUCCESS`

PhysAnim.ActivatedStanding.LocomotionGate: `PASS`
PhysAnim.ActivatedStanding.LocomotionReadiness: `PASS`
PhysAnim.ActivatedStanding.Perturbation: `PASS`
PhysAnim.ActivatedStanding.StabilityMetrics: `PASS`
PhysAnim.StandingProof.Live: `PASS`
PhysAnim.StandingProof.NegativeSupport: `PASS`
PhysAnim.ActivationPath.Wiring: `PASS`
PhysAnim.PIE.G2Presentation: `PASS`

Gate summary:
- no movement intent: `denied`
- short movement intent pulse: `denied`
- stable movement intent after standing activation: `allowed`
- negative support case: `denied`
- terminal reason present: `denied`
- locomotion gate logs emitted: `yes`

Standing metrics summary:
- samples: `1559`
- activation duration: `30.03 s`
- root drift: `0.00 cm`
- vertical drift: `0.00 cm`
- angular drift: `0.00 deg`
- max body linear speed: `0.00 cm/s`
- max body angular speed: `0.00 deg/s`
- support hull area min/mean/max: `1030.29 / 1030.29 / 1030.29 cm2`
- active support side count min/mean/max: `2.00 / 2.00 / 2.00`
- terminal reason: `None`
- fail-stop count: `0`

Files changed:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Locomotion.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-TO-LOCOMOTION-GATE-01.md`

Forbidden files touched: `none`
