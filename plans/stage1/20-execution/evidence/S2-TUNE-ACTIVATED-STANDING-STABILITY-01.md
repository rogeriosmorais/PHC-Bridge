# S2-TUNE-ACTIVATED-STANDING-STABILITY-01 Evidence

Base: `0b9ddd9`
Head: `working tree`
Commit: `pending`
Build: `SUCCESS`

PhysAnim.ActivatedStanding.StabilityMetrics: `PASS`
PhysAnim.StandingProof.Live: `PASS`
PhysAnim.StandingProof.NegativeSupport: `PASS`
PhysAnim.ActivationPath.Wiring: `PASS`
PhysAnim.PIE.G2Presentation: `PASS`
PhysAnim.RuntimeTermination: `PASS`

Tuning change:
- `BalanceActiveExtraDampingMultiplier` changed from `1.0f` to `1.1f`

Before metrics:
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

After metrics:
- samples: `3600`
- activation duration: `30.01 s`
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

Forbidden files touched: `none`
