# S2-PROOF-ACTIVATED-STANDING-PERTURBATION-01 Evidence

Base: `d9a0441f7c7dc1b2b38f67545fdf2e83ba7ff406`
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

Perturbation application:
- applied once: `yes`
- source: `actor offset`
- direction: `forward`
- magnitude: `small`
- offset: `2.0 cm forward`
- runtime state before perturbation: `BalanceActive_Standing`

Baseline metrics:
- samples: `252`
- activation duration: `2.10 s`
- root drift: `2.44 cm`
- vertical drift: `1.90 cm`
- angular drift: `0.00 deg`
- support hull area min/mean/max: `1030.29 / 1030.29 / 1030.29 cm2`
- active support side count min/mean/max: `2.00 / 2.00 / 2.00`
- terminal reason: `None`

Recovery metrics:
- recovery duration: `10.00 s`
- samples: `1453`
- runtime state after perturbation: `BalanceActive_Standing`
- root drift: `2.44 cm`
- vertical drift: `1.90 cm`
- angular drift: `1.21 deg`
- support hull area min/mean/max: `1030.29 / 1030.29 / 1030.29 cm2`
- active support side count min/mean/max: `2.00 / 2.00 / 2.00`
- max body linear speed: `0.00 cm/s`
- max body angular speed: `0.00 deg/s`
- terminal reason: `None`

Audit artifact:
- terminal artifact uuid: `45A119D0-49E6-EB50-0A57-479B046A7B6B`
- terminal artifact path: `PhysAnimUE5/Saved/PhysAnim/ProofArtifacts/45A119D0-49E6-EB50-0A57-479B046A7B6B_terminal.json`
- artifact terminal reason: `None`
- artifact support mode: `TwoFootStable`
- artifact support hull area: `1030.295 cm2`
- artifact written: `yes`

Files changed:
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimComponent.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimStandingProof.FunctionalTests.cpp`

Forbidden files touched: `none`
