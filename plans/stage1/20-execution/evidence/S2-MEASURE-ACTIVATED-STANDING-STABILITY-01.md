# S2-MEASURE-ACTIVATED-STANDING-STABILITY-01 Evidence

Base: `0b9ddd9`
Head: `working tree`
Commit: `pending`
Build: `SUCCESS`

PhysAnim.ActivatedStanding.StabilityMetrics: `PASS`
PhysAnim.StandingProof.Live: `PASS`
PhysAnim.StandingProof.NegativeSupport: `PASS`
PhysAnim.ActivationPath.Wiring: `PASS`
PhysAnim.PIE.G2Presentation: `PASS`

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
- metrics finite: `yes`
- metrics non-empty: `yes`

Live proof signals:
- capsule valid: `yes`
- continuity valid: `yes`
- activation reached standing: `yes`
- activation terminal reason: `None`

Files changed:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Core.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`

Forbidden files touched: `none`
