# S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01 Evidence

Base: `7c10274040f4cbc87a96da9e92c88c295a873439`
Head: `6c47cddc74afffa9a58c869b918dc620b6618795`
Commit: `6c47cddc74afffa9a58c869b918dc620b6618795`
Build: `SUCCESS`

PhysAnim.ActivatedStanding.LocomotionRequestState: `PASS`
PhysAnim.ActivatedStanding.LocomotionGate: `PASS`
PhysAnim.ActivatedStanding.LocomotionReadiness: `PASS`
PhysAnim.ActivatedStanding.Perturbation: `PASS`
PhysAnim.ActivatedStanding.StabilityMetrics: `PASS`
PhysAnim.StandingProof.Live: `PASS`
PhysAnim.StandingProof.NegativeSupport: `PASS`
PhysAnim.ActivationPath.Wiring: `PASS`
PhysAnim.PIE.G2Presentation: `PASS`

Request-state summary:
- no movement intent: `denied`
- short movement intent pulse: `denied`
- stable movement intent after standing activation: `requested`
- negative support case: `denied`
- terminal reason present: `denied`
- invalid capsule: `denied`
- invalid continuity: `denied`
- request-state logs emitted: `yes`

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
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Locomotion.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnim.SmokeTests.cpp`
- `plans/stage1/20-execution/task-packets/S2-IMPL-ACTIVATED-STANDING-LOCOMOTION-REQUEST-STATE-01.md`

Forbidden files touched: `none`
