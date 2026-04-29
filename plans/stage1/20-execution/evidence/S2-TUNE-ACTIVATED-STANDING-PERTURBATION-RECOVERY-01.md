# S2-TUNE-ACTIVATED-STANDING-PERTURBATION-RECOVERY-01 Evidence

Base: `8dd28d0`
Head: `working tree`
Commit: `pending`
Build: `SUCCESS`

PhysAnim.ActivatedStanding.Perturbation: `PASS`
PhysAnim.ActivatedStanding.StabilityMetrics: `PASS`
PhysAnim.StandingProof.Live: `PASS`
PhysAnim.StandingProof.NegativeSupport: `PASS`
PhysAnim.ActivationPath.Wiring: `PASS`
PhysAnim.PIE.G2Presentation: `PASS`
PhysAnim.RuntimeTermination: `PASS`

Tuning change:
- `PhysAnimComponent.h`: increased `BalanceActiveExtraDampingMultiplier` from `1.1` to `1.2`

Baseline perturbation metrics:
- samples: `252`
- activation duration: `2.10 s`
- root drift: `2.44 cm`
- vertical drift: `1.90 cm`
- angular drift: `1.21 deg`
- support hull area min/mean/max: `1030.29 / 1030.29 / 1030.29 cm2`
- active support side count min/mean/max: `2.00 / 2.00 / 2.00`
- terminal reason: `None`

Tuned perturbation metrics:
- samples: `253`
- activation duration: `2.11 s`
- root drift: `2.40 cm`
- vertical drift: `1.89 cm`
- angular drift: `1.20 deg`
- recovery duration: `10.00 s`
- runtime state after perturbation: `BalanceActive_Standing`
- support hull area min/mean/max: `1030.29 / 1030.29 / 1030.29 cm2`
- active support side count min/mean/max: `2.00 / 2.00 / 2.00`
- max body linear speed: `0.00 cm/s`
- max body angular speed: `0.00 deg/s`
- terminal reason: `None`

Files changed:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`

Forbidden files touched: `none`
