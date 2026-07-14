# Stage 1 Investigation Log

## E2 — Active-standing target-root alignment (2026-07-14)

**Hypothesis.** The approximately 31° target-relative root mismatch at the first active-standing policy inference causally drives the `RealOnnxPolicy` collapse.

**Baseline configuration and result.** Commit `47a1bfc517a620f76c3de74e1170bc03fb3b79e1`; locked standing-plant v2 protocol; `RealOnnxPolicy`; 1/60 s fixed step; 10 s window; unchanged default seed, initial pose, and no perturbation; root-alignment override off. Result: `FAIL` on body linear speed, pelvis height, and root tilt; first active root mismatch 31.037°; 26 position and 25 rotation values above 5σ; readback 1.0.

**Experimental configuration and result.** Identical configuration and commit with only `-PhysAnimExperimentalStandingTargetRootAlignment` enabled. The first future canonical root was rigidly aligned once to the live root and frozen. Result: `FAIL` on root angular speed, body linear speed, pelvis height, root tilt, and support gap; root mismatch 0°; no position or rotation values above 5σ; maximum root tilt 133.221°; maximum body linear speed 1599.647 cm/s; support gap 416.667 ms; readback 1.0.

**Supported or falsified.** Falsified. The intervention corrected the measured semantic mismatch but materially worsened locked runtime behavior.

**What was learned.** The 31° mismatch is not by itself the causal explanation for collapse. It may be a meaningful recovery-error signal, while an independent action-decoding or Manny constraint-frame mismatch remains a stronger explanation. The rejected override was removed; the active-standing snapshot and rigid-transform invariant test were retained.

**Next experiment selected.** Instrument the first active-standing action path per joint: raw Proto action triplet → decoded SMPL local rotation → mapped Manny constraint-frame target → authored axes/limits → applied/read-back target. Change no behavior until that trace identifies a specific frame or sign mismatch.

Machine-readable record: `experiments/stage1/active-standing-root-frame-alignment.e2.json`.
