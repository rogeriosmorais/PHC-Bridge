# Evidence: S2-FIX-LIVE-SUPPORT-HULL-AREA-01

## Task Summary
Fix live support evidence gathering so that `StandingProof.Live` no longer reaches the Settle phase with `SupportHullAreaCm2 = 0.0` when the character has real ground support.

## Mechanical Proof
- **Build**: SUCCESS
- **Test**: `PhysAnim.StandingProof.Live`
- **Result**: PASSED
- **Hull Area**: 1029.3 cm2 (previously 0.0)
- **Sample Count**: 5 hits per foot (10 total)
- **Standing Logic**: Terminated states resolve to `FailStopped` (Commit 5d4712d).

## Diagnostic Logs
```text
[PhysAnimBalance] SAMPLES_OK body=foot_l count=5
[PhysAnimBalance] SAMPLES_OK body=foot_r count=5
[PhysAnimBalance] HULL_AREA_AUDIT area=1029.3 hits=10
```

## Commit Summary
- `5d4712d`: Fix Phase 2 logic: terminated standing states resolve to FailStopped
- `44cca3c`: Fix S2-FIX-LIVE-SUPPORT-HULL-AREA-01: implement 5-point foot sampling
- `691edc2`: Update execution log: S2-FIX-LIVE-SUPPORT-HULL-AREA-01 complete
