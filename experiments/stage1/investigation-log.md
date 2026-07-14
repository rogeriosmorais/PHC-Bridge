# Stage 1 Investigation Log

## E2 — Active-standing target-root alignment (2026-07-14)

**Hypothesis.** The approximately 31° target-relative root mismatch at the first active-standing policy inference causally drives the `RealOnnxPolicy` collapse.

**Baseline configuration and result.** Commit `47a1bfc517a620f76c3de74e1170bc03fb3b79e1`; locked standing-plant v2 protocol; `RealOnnxPolicy`; 1/60 s fixed step; 10 s window; unchanged default seed, initial pose, and no perturbation; root-alignment override off. Result: `FAIL` on body linear speed, pelvis height, and root tilt; first active root mismatch 31.037°; 26 position and 25 rotation values above 5σ; readback 1.0.

**Experimental configuration and result.** Identical configuration and commit with only `-PhysAnimExperimentalStandingTargetRootAlignment` enabled. The first future canonical root was rigidly aligned once to the live root and frozen. Result: `FAIL` on root angular speed, body linear speed, pelvis height, root tilt, and support gap; root mismatch 0°; no position or rotation values above 5σ; maximum root tilt 133.221°; maximum body linear speed 1599.647 cm/s; support gap 416.667 ms; readback 1.0.

**Supported or falsified.** Falsified. The intervention corrected the measured semantic mismatch but materially worsened locked runtime behavior.

**What was learned.** The 31° mismatch is not by itself the causal explanation for collapse. It may be a meaningful recovery-error signal, while an independent action-decoding or Manny constraint-frame mismatch remains a stronger explanation. The rejected override was removed; the active-standing snapshot and rigid-transform invariant test were retained.

**Next experiment selected.** Instrument the first active-standing action path per joint: raw Proto action triplet → decoded SMPL local rotation → mapped Manny constraint-frame target → authored axes/limits → applied/read-back target. Change no behavior until that trace identifies a specific frame or sign mismatch.

Machine-readable record: `experiments/stage1/active-standing-root-frame-alignment.e2.json`.

## E3 — First active-standing action semantics (2026-07-14)

**Hypothesis.** The first active-standing action path materially distorts valid ProtoMotions absolute joint targets during Manny neutral/bind composition or constraint adaptation before Physics Control publication.

**Baseline configuration and result.** Commit `8640197929b33842f58be3483456d5edd5ea07ee`; locked standing-plant v2 protocol; `RealOnnxPolicy`; 1/60 s fixed step; 10 s window; unchanged default seed, initial pose, and no perturbation; trace off. Result: `FAIL` on body linear speed, pelvis height, and root tilt; minimum pelvis ratio 0.153366; maximum root tilt 105.401°; maximum body linear speed 1119.794 cm/s; readback 1.0.

**Experimental configuration and result.** Identical configuration, commit, binary, and evaluator with only `-PhysAnimActionSemanticTrace` enabled. Result: the physics, policy, and both observation-snapshot streams were bit-identical to baseline; evaluator `FAIL` was identical. The valid trace captured 23 ordered action joints and 21 targets. Constraint-range remapping changed 14 controls, averaged 27.157° across the eight lower-body controls, and changed `foot_l` by 47.093°; the following projection reached 8.859° on `calf_r`; publication/readback error was 0°.

**Supported or falsified.** Supported as a localization hypothesis, not yet as causal proof. The dominant systematic action transformation is constraint-range remapping, while downstream publication and readback are exact.

**What was learned.** Raw action decoding preserves the checkpoint's 23-joint order, all 21 targets are written/read back, and neither optional range/distal scaling nor blending/publication explains the collapse. The Manny range mapper is now the highest-information causal candidate; the retained trace is behavior-neutral.

**Next experiment selected.** Behind a development-only runtime flag, bypass only `MapProtoPolicyTargetToMannyConstraintRange` during active standing while retaining `AdaptParentRelativeTarget` safety projection. Run a fresh same-commit trace-enabled baseline beside it under the unchanged locked protocol.

Machine-readable record: `experiments/stage1/active-standing-action-semantics.e3.json`.

## E4 — Active-standing constraint-range remap bypass (2026-07-14)

**Hypothesis.** Proto's full-range action is already an absolute joint target, so Manny constraint-range remapping is a causal distortion; bypassing only that remap while retaining safety projection should reduce the locked failure set.

**Baseline configuration and result.** Commit `98433c4d8fb963403223e6631b6fc364ad40d382`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s fixed step; 10 s; unchanged default seed, initial pose, and no perturbation; action trace on; remap enabled. Result: `FAIL` on body linear speed, pelvis height, and root tilt; maximum root tilt 103.503°; maximum body linear speed 1183.699 cm/s; readback 1.0.

**Experimental configuration and result.** Identical declared configuration and commit with only `-PhysAnimExperimentalConstraintRangeRemapBypass` added. Trace confirmed remap delta fell from 19.091° mean to numerical zero while safety-projection maximum rose from 8.856° to 67.033° and 21/21 targets still read back. Result: `FAIL` on root angular speed, body linear speed, body angular speed, pelvis height, root tilt, and support gap; maximum body linear speed 1888.966 cm/s; maximum body angular speed 9404.083°/s; support gap 450 ms; readback 1.0.

**Supported or falsified.** Neither; `INVALID`. The first policy-input snapshot already differed before the active-standing-only intervention could execute: self-observation maximum absolute difference 0.002321 and first-action maximum absolute difference 0.008537. The required pre-intervention equivalence was not established.

**What was learned.** The override and retained projection behaved exactly as designed, and the observed direction strongly favors the competing explanation that Manny range mapping is necessary. That direction is not a causal verdict because launch-to-launch input variation confounded the pair. No production behavior is promoted; the override remains test-only for one controlled rerun.

**Next experiment selected.** Run three consecutive unchanged, trace-enabled, remap-enabled `RealOnnxPolicy` baselines on the same clean commit/binary. Compare exact first-policy snapshot hashes, first actions, active-standing snapshots, and metric spread. If variation reproduces, instrument its earliest observation source; if all three are identical, classify this baseline as malformed and rerun E4 with exact pre-intervention equality required.

Machine-readable record: `experiments/stage1/active-standing-constraint-range-remap-bypass.e4.json`.

## E5 — RealOnnxPolicy pre-intervention determinism control (2026-07-14)

**Hypothesis.** The standing-plant fixture has launch-to-launch variation before active-standing intervention, so nominally identical runs can enter different first-policy input modes and invalidate semantic A/B comparisons.

**Baseline configuration and result.** Commit `5de9aeb47c52b663b65ac8768a7ed3372dc9889f`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s fixed step; 10 s; unchanged default seed, initial pose, no perturbation, action trace on, range remap enabled, and no behavior override. Repetitions 1 and 2 were byte-identical across first and active snapshots, semantic trace, physics, and policy streams. Both evaluator results were `FAIL` on body linear speed, pelvis height, and root tilt with readback 1.0.

**Experimental configuration and result.** The third repetition used the identical binary, model, runtime flags, evaluator, and physics configuration; only artifact identity/repetition metadata changed, which targeted source inspection confirmed is not used for runtime seeding, pose, policy, or physics. All three repetition-3 attempts produced the same raw hashes, but one wrapper timeout and two identical `CaptureRender` access violations left every full run `INVALID` without a manifest/evaluator result. Before that downstream failure, all three captured 601 physics samples and the same complete semantic trace. Their first-policy hash differed from repetitions 1/2, with maximum differences of 0.002321 in self observation, 0.002570 in mimic targets, 0.000044 in terrain, and 0.008537 in the first action. The two hashes exactly reproduce E4's baseline and experimental pre-intervention modes.

**Supported or falsified.** Supported by the pre-intervention raw evidence. The full repetition-3 behavioral runs remain `INVALID`, and no standing or product pass is claimed.

**What was learned.** E4's mismatch was neither isolated nor caused by its active-standing remap-bypass branch. The fixture has at least two reproducible startup modes before NNE/action intervention. The alternate malformed-run raw stream was standing-like, but that is diagnostic only because render/manifest publication failed and no negative-control product protocol ran.

**Next experiment selected.** Add a development-only, behavior-neutral first-policy provenance trace covering live actor/component/root and per-body observation transforms, PoseSearch selection/time and target root, previous action, and terrain sample origin/raw height before tensor packing. First prove trace neutrality; then compare the earliest source field across the two known snapshot hashes. If all sources are equal, move to tensor assembly or memory ownership.

Machine-readable record: `experiments/stage1/real-onnx-preintervention-determinism.e5.json`.
