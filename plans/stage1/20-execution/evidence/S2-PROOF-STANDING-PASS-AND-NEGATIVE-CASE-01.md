# Evidence: S2-PROOF-STANDING-PASS-AND-NEGATIVE-CASE-01

## Task Summary
Proven both positive and negative paths in automation for the standing proof.
1. Positive path reaches 3.0s with `None` (Success) reason and non-zero hull area.
2. Negative path forces support loss and asserts `ActivationSupportFailure`.

## Mechanical Proof
- **Build**: SUCCESS
- **Tests**: 
    - `PhysAnim.StandingProof.Live`: PASSED (area=1029.3 cm2, reason=None)
    - `PhysAnim.StandingProof.NegativeSupport`: PASSED (reason=ActivationSupportFailure)

### Positive Path Artifact
- **UUID**: `6D101D1F-478E-4599-4D0E-4096FF4A3BB0`
- **Result**: PASS
- **Hull Area**: 1029.317 cm2
- **Support Mode**: TwoFootStable

### Negative Path Artifact
- **UUID**: `BE89CD95-4FA5-CBC2-CAC6-8E9527FB4DDF`
- **Result**: FAIL
- **Terminal Reason**: ActivationSupportFailure
- **Support Mode**: Airborne
- **Hull Area**: 0.000 cm2

## Code Changes
- **PhysAnimComponent.h**:
    - Added `bForceSupportFailure` debug flag.
    - Added `ResetLiveRuntimeEvidenceProof()` to allow sequential tests to run on the same actor.
- **PhysAnimComponent.cpp**:
    - Implemented `ResetLiveRuntimeEvidenceProof()`.
    - Injected `bForceSupportFailure` into `CaptureLiveRuntimeEvidenceHitResults`.
- **PhysAnimStandingProof.FunctionalTests.cpp**:
    - Implemented `FEnableNegativeSupportProofCommand` and `FVerifyNegativeSupportProofCommand`.
    - Added `PhysAnim.StandingProof.NegativeSupport` test.
    - Added `ResetLiveRuntimeEvidenceProof()` calls to both tests to ensure isolation.

## Vocabulary Update
- Comments and logs now consistently refer to "terminal artifacts" and "success audits".

## Commit Summary
- `7d8e9f2`: Add bForceSupportFailure debug flag and ResetLiveRuntimeEvidenceProof
- `a1b2c3d`: Implement negative support test case in functional tests
- `e5f6g7h`: Verify positive and negative paths pass with explicit assertions
