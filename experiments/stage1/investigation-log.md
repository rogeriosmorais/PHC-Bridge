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

## E6 — First-policy input provenance (2026-07-14)

**Hypothesis.** The two E5 first-policy input modes already differ in a shared pre-flattening source: live canonical body state, PoseSearch target/reference state, or terrain root/ground state.

**Baseline configuration and result.** Commit `cd767f0ac4e227b9c56655e9406a42acfc3dafd4`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged default seed, initial pose, no perturbation, action trace on, and provenance trace off. Result: valid development run, Mode A hash `AAAF45E77B0D8BFAFD231C3E32A2F852CD2CF420128B74EFF6651E62FBB316AA`, evaluator `FAIL` on body linear speed, pelvis height, and root tilt, readback 1.0.

**Experimental configuration and result.** Identical configuration with only `-PhysAnimPolicyInputProvenanceTrace` enabled. Two trace-on runs were byte-identical to baseline across the first and active input snapshots, semantic trace, physics, and policy streams; both had the same locked `FAIL` and readback 1.0. Their valid provenance artifacts were also byte-identical (`055E74345E4738065EB6C5B8AE6D4758D91A8A6C4A5E7235EC2C9732F2FFE6B9`). Mode B did not recur.

**Supported or falsified.** The source-location hypothesis was not adjudicated: neither supported nor falsified because only Mode A occurred. The prerequisite claim that the trace is behavior-neutral was supported.

**What was learned.** Mode A reaches first inference on policy tick 1 at world time 0.033333335 s with `MM_Idle` at 5.433333397 s, zero previous action and canonical velocities, and valid counts for every traced source. Uncontrolled relaunches are now low-information; repetitions stopped after the third unchanged behavioral failure.

**Next experiment selected.** Test whether one fixed startup pose-propagation tick causes the mode split. Use a development-only runtime sweep of first-policy delay `{0,1}` ticks with the validated provenance trace and otherwise identical locked harness. Mode B's exact input hash and the earliest provenance delta are the judges; do not change production defaults or thresholds.

Machine-readable record: `experiments/stage1/real-onnx-policy-input-provenance.e6.json`.

## E7 — First-policy fixed-tick delay (2026-07-14)

**Hypothesis.** The E5 startup-mode split is caused by whether first policy inference samples Manny before or after one fixed skeletal and physics pose-propagation tick; delaying Mode A by one fixed tick should reproduce known Mode B.

**Baseline configuration and result.** Commit `817a96a4714e2af9018b14924c3847acfa11a729`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed, pose, no perturbation, action/provenance traces on, delay `0`. The valid run reproduced Mode A hash `AAAF45E77B0D8BFAFD231C3E32A2F852CD2CF420128B74EFF6651E62FBB316AA` at world time 0.033333335 s in `Standing_Preparation`. Evaluator `FAIL`: body linear speed, pelvis height, root tilt; readback 1.0.

**Experimental configuration and result.** Identical clean commit, binary, model, protocol, and harness with only delay changed to `1`; provenance confirmed configured/consumed `1/1`. First inference moved to world time 0.050000003 s in `Standing_FullSimulationActivation`, but produced new hash `CA62E6D543A73A887FC1153FE2C528E4E4845696211C2AA9AB13EF305556C723`, not Mode B `723FD942…`. Active-standing snapshot, semantic trace, physics, policy, and render were byte-identical to baseline. The evaluator and readback were also identical.

**Supported or falsified.** Falsified. One tick later does not reproduce Mode B and has no downstream standing effect.

**What was learned.** The changed source is the live root/body sample: owner/mesh transforms, PoseSearch identity/time, mimic reference frames, future poses, raw terrain heights, and previous actions stayed exact. Delay +1 and historical Mode B have nearly perfect opposite signed deltas for mimic targets (`-0.999997` correlation) and terrain (`-1.0`), selecting an earlier-sample hypothesis. Startup velocities at the delayed boundary also varied between a metadata-invalid diagnostic attempt and the authoritative run, while positions/rotations were reproducible. The rejected override, parser, publication fields, and tests were removed in commits `5e12162` and `b863294`; the general E6 provenance trace remains.

**Next experiment selected.** Add observation-only startup chronology instrumentation before and after the runtime-state-machine update on each fixed tick through first inference. Test whether an existing earlier live-body sample reconstructs Mode B and whether velocity publication timing separates the modes. Do not run extra inference or change production behavior, protocol, or thresholds.

Machine-readable record: `experiments/stage1/real-onnx-first-policy-delay.e7.json`.

## E8 — Observation-only startup chronology (2026-07-14)

**Hypothesis.** Historical Mode B is the live Manny/SMPL body state from one fixed tick earlier than Mode A, with startup velocity publication timing distinguishing the states.

**Baseline configuration and result.** Commit `3a654c8ef1c5cc2bec817a9db4703f50e9a7f3e7`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; action/provenance traces on; startup chronology off. Automation passed. The evaluator `FAIL` remained body linear speed, pelvis height, and root tilt with readback 1.0; first-input hash was Mode A `AAAF45E7…`.

**Experimental configuration and result.** Identical commit, binary, model, protocol, harness, and evaluator with only `-PhysAnimStartupChronologyTrace` enabled. Automation passed; evaluator failure set and readback were identical. First and active input snapshots, semantic trace, provenance, physics, policy, and render were byte-identical to baseline. The valid three-sample chronology occurred entirely at 0.033333335 s: `BridgeActive` before the state update, `Standing_Preparation` after it, and policy tick 1 after inference. All owner/mesh/root/body values were identical across the three samples and exactly matched the Mode-A first-input Manny source; all published body velocities were zero.

**Supported or falsified.** Neither; inconclusive. Trace neutrality was supported, and the same-tick state transition was excluded as the source of Mode B. The registered earlier-fixed-tick hypothesis was not adjudicated because `WaitingForPoseSearch` returns before the trace hooks, so no 0.0166667 s sample was captured.

**What was learned.** Mode A is already fully selected at the first observed `BridgeActive` tick; neither the transition to `Standing_Preparation` nor the first policy update changes its live source. The missing information boundary is the preceding `WaitingForPoseSearch` tick, not damping, friction, contact, drag, thresholds, or another policy delay.

**Next experiment selected.** Extend only the explicit development chronology across the `WaitingForPoseSearch` early-return path, without extra PoseSearch calls, inference, raycasts, physics changes, or dispatch. Compare the newly observed prior-tick per-body signed pose/velocity delta against Mode A and E7 delay-plus-one; keep the same hashes, evaluator, and readback judges.

Machine-readable record: `experiments/stage1/real-onnx-startup-chronology.e8.json`.

## E9 — WaitingForPoseSearch startup chronology (2026-07-14)

**Hypothesis.** The fixed tick immediately before Mode A contains the live body pose in the historical Mode-B direction, and its velocity publication differs from Mode A.

**Baseline configuration and result.** Commit `656e78fd623c396f57776b4ff64e24e9bad46556`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; action/provenance traces on; startup chronology off. Automation passed with no errors. The evaluator `FAIL` remained body linear speed, pelvis height, and root tilt with readback 1.0; first-input hash was Mode A `AAAF45E7…`.

**Experimental configuration and result.** Identical clean commit, binary, model, protocol, harness, and evaluator with only `-PhysAnimStartupChronologyTrace` enabled. Automation passed; the evaluator failure set and readback were identical. First/active input, semantic, provenance, physics, policy, and render artifacts were byte-identical to baseline. The valid six-stage trace exposed a 0.016666668 s `WaitingForPoseSearch → BridgeActive` tick before the existing 0.033333335 s `BridgeActive → Standing_Preparation → policy tick 1` sequence. The prior-tick body position and rotation deltas versus Mode A were near-perfect signed inverses of authoritative E7 delay +1: correlations `-0.9999968702` and `-0.9999981786`, scales `-1.0008553753` and `-1.0001182905`, with maximum inverse residuals `2.93e-6` m and `2.30e-6`. Both prior and Mode-A samples published exactly zero linear and angular velocity.

**Supported or falsified.** Partially supported: the earlier-pose component was supported; the velocity-discriminator component was falsified. Historical Mode B was not yet reconstructed, so this is not causal-standing proof.

**What was learned.** The live pose follows an almost symmetric one-tick-before/Mode-A/one-tick-after sequence, and runtime-state changes within the observed ticks do not alter that source. Historical Mode B differs from Mode A in all 214 self-observation pose slots and none of the 144 all-zero velocity slots. The remaining high-information boundary is observation reconstruction from the prior-tick Manny body source, not more physics tuning.

**Next experiment selected.** Cache and replay only prior-tick `MannyCurrentBodySamples` at first inference before the unchanged Manny-to-canonical adapter; do not override root frames, terrain, PoseSearch, future references, timing, physics, or dispatch. Seek exact historical Mode-B equality for the full hash and the self, mimic, terrain, and actions (model output) sections, with configured/consumed/source-time/source-hash evidence in a separate development-only artifact.

Machine-readable record: `experiments/stage1/real-onnx-waiting-chronology.e9.json`.

## E10 — First-inference prior-body replay (2026-07-14)

**Hypothesis.** Replaying only the cached 0.0166667 s `MannyCurrentBodySamples` at first inference before the unchanged Manny-to-canonical adapter reconstructs historical Mode B exactly.

**Baseline configuration and result.** Commit `37724a3b86094776eecba8413636bd65f54ceb1c`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; action, provenance, chronology, and body-source diagnostics on; replay off. Automation passed. The valid source record was configured/consumed `false/false`, effective equaled live, and the snapshot exactly reproduced Mode A `AAAF45E7…`. Evaluator `FAIL`: body linear speed, pelvis height, root tilt; readback 1.0.

**Experimental configuration and result.** Identical clean commit, binary, model, protocol, harness, and evaluator with only `-PhysAnimExperimentalReplayPriorBodySamplesAtFirstInference` added. Automation passed; source evidence was valid, configured/consumed `true/true`, prior/live records were exactly the same as baseline, and effective equaled prior. The candidate hash was `586CC117…`, not Mode B `723FD942…`. Mimic and terrain matched Mode B exactly. Self observation matched 357/358 values; only root-height index 0 differed by `4.3869019e-5` m. All 69 actions differed from Mode B with maximum `8.2850456e-5` and RMS `2.5648583e-5`. The evaluator failure set and readback were unchanged; active-standing, semantic, physics, policy, render, and chronology artifacts were byte-identical.

**Supported or falsified.** Falsified for exact reconstruction. Prior-body replay is necessary for the Mode-B pose direction but is insufficient without its contemporaneous root-height ground reference. This is not causal-standing proof.

**What was learned.** The prior replay exactly reconstructed all local positions, rotations, velocities, mimic targets, and terrain. `ResolveSelfObservationGroundHeight` paired the prior body-root Z with the live mesh root-world Z, shifting synthetic ground height and preserving Mode-A root height. The prior/live root-world Z delta was `0.00440216` cm, accounting for the remaining Mode-B residual to `1.53e-7` m. The next uncertainty is semantic root/ground-frame ownership, not physics tuning. The rejected replay behavior and runner flag were removed in `5d5a4d7`; the behavior-neutral source diagnostic remains.

**Next experiment selected.** Add development-only observation of the root-height ground-reference decomposition at the prior and first-inference boundaries: body-root Z, root-bone world Z, static ground hit, floor fallback inputs, resolved world ground Z, synthetic ground height, and final root height. First prove behavior neutrality; do not override any value, change production defaults, or tune physics.

Machine-readable record: `experiments/stage1/real-onnx-prior-body-replay.e10.json`.

## E11 — First-policy ground-reference decomposition (2026-07-14)

**Hypothesis.** Historical Mode B pairs the prior Manny body-root Z with its contemporaneous prior root-bone world Z and unchanged resolved world ground; E10 missed root-height index 0 because it paired the prior body root with the live root-world reference.

**Baseline configuration and result.** Commit `d4f81d9ba2fad626acf92e220b4781321215718b`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; action/provenance diagnostics on; startup chronology off. Automation passed. The snapshot was exact Mode A `AAAF45E7…`; evaluator `FAIL`: body linear speed, pelvis height, root tilt; readback 1.0.

**Experimental configuration and result.** Identical clean commit, binary, model, protocol, harness, evaluator, and diagnostics with only `-PhysAnimStartupChronologyTrace` enabled. Automation passed and the new ground-reference artifact was complete and valid. Every shared snapshot, provenance, semantic, physics, policy, and render artifact was byte-identical to baseline. Prior and live static traces both resolved ground Z to exactly 210 cm. Root-bone world Z moved by `0.00440216064453125` cm. The actual prior decomposition produced root height `0.9379022717475891` m (`0x3F701A5D`), exactly Mode B; the live decomposition produced Mode A `0.9379460215568542` m (`0x3F701D3B`). The evaluator failure set and readback remained unchanged.

**Supported or falsified.** Supported. The instrumentation was behavior-neutral, the resolved-ground branch did not change, and the contemporaneous prior root-world reference closed E10's final Mode-B root-height gap exactly.

**What was learned.** The remaining E10 mismatch was temporal incoherence at the root-height reference boundary, not static-ground selection, floor fallback, policy decoding, dispatch, or the physical plant. No physics tuning is implicated. The behavior-neutral diagnostic remains; no runtime behavior or production default was promoted.

**Next experiment selected.** Run a default-off coherent prior-observation replay with three locked arms on one commit/binary: unchanged production control, prior-body/live-ground E10 control, and prior-body/prior-ground candidate. The primary causal comparison changes only the ground-reference epoch with prior body replay held fixed. Exact Mode-B raw/section/action hashes and unchanged unrelated provenance are the judges.

Machine-readable record: `experiments/stage1/real-onnx-ground-reference.e11.json`.

## E12 — Coherent prior observation epoch (2026-07-14)

**Hypothesis.** With prior Manny body samples held fixed, changing only their root-height reference from the live first-policy reference to the contemporaneous prior reference reconstructs historical Mode B exactly.

**Baseline configuration and result.** Commit `6c7df5062b751bfb6e53dbd6f769a3e79d2c7ef7`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; action, provenance, chronology, body-source, and ground-reference diagnostics on. The unchanged live/live control was exact Mode A `AAAF45E7…`. The primary prior-body/live-ground baseline was valid, configured/applied/consumed, and exactly reproduced E10 `586CC117…`. Both evaluator results were `FAIL` on body linear speed, pelvis height, and root tilt with readback 1.0.

**Experimental configuration and result.** Identical commit, binary, model, protocol, harness, and diagnostics; only `ground_reference_epoch` changed from live to prior while the effective body fingerprint remained prior `45E954D382D46B12`. The trace was complete and valid; it applied prior synthetic ground `2.0999999046325684` m and published root height `0.9379022717475891` m (`0x3F701A5D`). The raw snapshot exactly equaled Mode B `723FD942…`; self, mimic, terrain, and all 69 action section hashes were exact. The evaluator still `FAIL`ed on the same three criteria with readback 1.0. Active-standing input, semantic trace, physics, policy, render, chronology, and the general body-source diagnostic were byte-identical across all three arms.

**Supported or falsified.** Supported for exact semantic reconstruction. This is a valid behavioral failure, not causal-standing or product success.

**What was learned.** Historical Mode B is fully explained by a coherent one-tick-prior current-observation epoch; no hidden float, tensor, frame, ground-branch, or provenance dependency remains at this boundary. The startup mode is not causal for standing because all three arms converge to the same active-standing input and byte-identical downstream behavior. The test-only replay behavior will be removed separately; general diagnostics remain.

**Next experiment selected.** Rerun E4's active-standing constraint-range-remap bypass with an exact Mode-A first-input requirement and byte-identical active-standing pre-intervention snapshot. Change only the existing development-only range-remap bypass, retain safety projection, and judge the unchanged locked failure set, physical metrics, semantic-stage deltas, and readback. This resolves E4's former startup-mode confound without returning to broad physics tuning.

Machine-readable record: `experiments/stage1/real-onnx-coherent-prior-epoch.e12.json`.

## E13 — Controlled constraint-range-remap bypass (2026-07-14)

**Hypothesis.** Manny full-range remapping is the causal active-standing distortion; bypassing only `MapProtoPolicyTargetToMannyConstraintRange` while retaining `AdaptParentRelativeTarget` safety projection will strictly improve the locked standing verdict.

**Baseline configuration and result.** Commit `49e0b8c14d2b15ad40dea21d7f5d5f1ef4d9be9f`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; action, provenance, and chronology traces on; remap enabled. The run was exact Mode A `AAAF45E7…`, active-standing input `6FC88062…`, and evaluator `FAIL`ed on body linear speed, pelvis height, and root tilt with readback 1.0. The mapper changed lower-body targets by 27.157° on average; safety projection averaged 0.690° and peaked at 8.859°.

**Experimental configuration and result.** Identical clean commit, binary, model, protocol, harness, evaluator, and diagnostics; only `-PhysAnimExperimentalConstraintRangeRemapBypass` was added. First and active-standing inputs, provenance, and chronology were byte-identical to baseline. The trace verified approximately zero remap delta, retained 21/21 projection/publication/readback, and increased safety projection to 10.334° mean and 67.033° maximum. The evaluator `FAIL` set expanded to root angular speed, body linear speed, body angular speed, pelvis height, root tilt, and support gap. Readback remained 1.0.

**Supported or falsified.** Falsified. This is a valid causal result: E4's previously confounded adverse direction reproduced exactly after passing the pre-intervention equivalence gate.

**What was learned.** The current Manny range mapper is protective under the existing constraint pipeline; deleting it forces the safety projection to absorb much larger targets and materially worsens every affected stability metric. This does not prove that the mapper is the authoritative ProtoMotions transform. The remaining semantic uncertainty is upstream action decoding, neutral/frame meaning, units, ordering, and training-time `map_actions_to_pd_range` preprocessing. The rejected bypass will be removed separately; the general semantic/readback trace remains.

**Next experiment selected.** Add a behavior-neutral authoritative ProtoMotions action-decoding contract trace. Compare each raw 3-value joint action, training-time PD offset/scale and joint frame, resulting SMPL-space target, and current UE decoder output. First prove trace neutrality; a deterministic order, axis/sign, neutral, unit, or range mismatch supports the hypothesis, while exact agreement across all 23 joints falsifies it.

Machine-readable record: `experiments/stage1/active-standing-constraint-range-remap-controlled.e13.json`.

## E14 — Authoritative ProtoMotions action-decoding contract (2026-07-14)

**Hypothesis.** The UE bridge decodes the checkpoint's 69 normalized actions with the wrong scale, order, exponential-map meaning, or handedness before Manny neutral and constraint adaptation.

**Baseline configuration and result.** Commit `ed6ea5a2c6bf146167ac7d6cd38a97e283d7732a`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; action trace off. Automation passed; the evaluator `FAIL`ed on body linear speed, pelvis height, and root tilt. Minimum pelvis ratio was 0.153366, maximum root tilt was 105.401°, maximum body linear speed was 1119.794 cm/s, and readback was 1.0. A preliminary relative-output-path invocation was `INVALID` because it produced no manifest and was not used as evidence.

**Experimental configuration and result.** Identical clean commit, reused binary, model, protocol, harness, evaluator, and thresholds with only `-PhysAnimActionSemanticTrace` enabled. Physics, policy, first-policy input, active-standing input, render, failed criteria, and every locked metric were bit-identical to baseline. The independent ProtoMotions v2.3 evaluator returned `VALID/MATCH`: 0/69 conditioned-scalar mismatches and 0/23 decoded-quaternion mismatches; maximum quaternion-component error was `1.461e-8` and maximum angular error was `1.708e-6°`.

**Supported or falsified.** Falsified. This is a valid behavior-neutral contract result, not causal-standing or product success.

**What was learned.** The checkpoint contract and UE decoder agree on joint order, clamp, zero-offset/π PD scaling, xyz exponential-map decoding, and Isaac-to-UE handedness. The standing failure is downstream of raw action decoding or elsewhere in the observation/policy contract; broad physics tuning remains unjustified.

**Next experiment selected.** Add a behavior-neutral Manny neutral/bind and per-joint local-frame round-trip trace. Test Proto identity, known-axis rotations, multiplication direction, and bilateral frame symmetry before range adaptation, using a fresh same-commit trace-off/trace-on A/B and unchanged locked runtime judges.

Machine-readable record: `experiments/stage1/active-standing-action-decode-contract.e14.json`.

## E15 — Manny local-frame action/observation round trip (2026-07-14)

**Hypothesis.** The action-side Manny neutral/bind-axis composition is not symmetric with the observation-side Manny-bind recovery, so absolute ProtoMotions joint targets do not return as the same canonical joint rotations before constraint-range adaptation.

**Baseline configuration and result.** Commit `59bcf9ba39d2aab8dd101745039355224aa96354`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; local-frame trace off. Automation passed with warnings; the evaluator `FAIL`ed on body linear speed, pelvis height, and root tilt. Minimum pelvis ratio was 0.153366, maximum root tilt was 105.401°, maximum body linear speed was 1119.794 cm/s, and target readback was 1.0.

**Experimental configuration and result.** Identical clean commit, reused binary, model, protocol, harness, evaluator, and thresholds with only `-PhysAnimMannyLocalFrameRoundtripTrace` enabled. Physics, policy, first-policy input, active-standing input, existing action-semantic trace, render, failed criteria, and every locked behavioral metric were exact. The independent evaluator returned `VALID/MISMATCH`: all 19 decisive joints failed identity and actual-target round trips and all 114 decisive axis probes failed. Maximum identity, actual, and probe errors were 35.963°, 106.602°, and 47.155°. Policy neutral differed from bind by up to 35.963°; action and observation bind rotations matched exactly. The parent action axis matched the observation bind frame only after removing a constant 90° component-to-world rotation.

**Supported or falsified.** Supported. This is valid behavior-neutral semantic evidence, not causal-standing or product success.

**What was learned.** The action and observation adapters are not inverses in the live harness. Identity input isolates the captured-neutral-versus-bind offset. Nonzero inputs additionally expose world-frame action conjugation against component-frame observation recovery. The underlying cached bind calibration is coherent; the runtime choice of neutral and frame is not. Broad physics tuning remains unjustified.

**Next experiment selected.** Change only the action conjugation axis behind a development override from cached parent world space to equivalent mesh component space. Keep the captured neutral and every downstream stage unchanged. The locked semantic prediction is that identity errors remain unchanged while actual and axis-probe errors collapse to the existing per-joint identity residuals; rerun the unchanged baseline beside it and report the physical verdict without promotion or threshold changes.

Machine-readable record: `experiments/stage1/active-standing-manny-local-frame-roundtrip.e15.json`.

## E16 attempt 1 — Component-space action axis (INVALID, 2026-07-14)

**Hypothesis.** Cached-world action-axis conjugation causes E15's non-identity round-trip excess; the equivalent mesh-component axis should leave identity residuals unchanged and collapse each actual/probe error to that residual.

**Baseline configuration and result.** Commit `3e6a7d5ebb2d43b09381c65bab9af8f9900973da`; locked standing-plant v2; `RealOnnxPolicy`; 1/60 s; 10 s; unchanged seed and pose; no perturbation; trace on; component-axis override off. The baseline exactly reproduced E15's physics, policy, first/active inputs, action trace, and render. It `FAIL`ed body linear speed, pelvis height, and root tilt; minimum pelvis ratio was 0.153366, maximum root tilt 105.401°, maximum body speed 1119.794 cm/s, and readback 1.0.

**Experimental configuration and result.** Same clean commit, reused binary, model, protocol, harness, evaluator, and thresholds; only `-PhysAnimExperimentalComponentActionAxis` was enabled. Automation completed and readback was 1.0, but the locked semantic evaluator returned `INVALID`: first-active decoded `thigh_l` already differed by 58.4543°. The first-policy input remained exact, while the active-standing input, policy stream, physics, and render diverged. The post-intervention trace algebra collapsed actual/identity and probe/identity residuals to `1.389e-11°` and `2.779e-11°`, but those values are non-authoritative because pre-intervention equivalence failed. The physical run `FAIL`ed root angular speed, body linear/angular speed, pelvis height, root tilt, and support gap.

**Supported or falsified.** Neither. The run was malformed under the preregistered validity gates, so the hypothesis was not evaluated.

**What was learned.** The override was applied during `Standing_Preparation`, altering the closed-loop state before the first active-standing capture. The evaluator prevented a mathematically attractive post-intervention signature from being misreported as causal evidence. No threshold, protocol, or production default changed.

**Next experiment selected.** Rerun E16 with the same hypothesis and judges, but make the development-only override effective only in `BalanceActive_Standing`. Add a contract that preparation remains on the baseline path. Require exact first-policy and first-active input/action equality before interpreting the semantic or physical result.

Machine-readable record: `experiments/stage1/active-standing-action-axis-frame.e16.attempt1.json`.


## E16 attempt 2 — Delayed component-space action axis (INVALID, 2026-07-15)

**Hypothesis.** Cached-world action-axis conjugation causes E15's non-identity round-trip excess; activating the equivalent mesh-component axis only in `BalanceActive_Standing` should preserve all locked pre-intervention state while leaving identity residuals unchanged and collapsing actual/probe excess to those residuals.

**Implementation and tests.** Preregistered at `6d953ec`; behavior commit `46fa0db`. A test-first activation contract deliberately failed with `LNK2019` before implementation, then passed after the override was constrained to `configured && RuntimeState == BalanceActive_Standing`. `PhysAnim.ProductHarness.DropDispatchSwitch` passed, and the focused evaluator suite passed 24/24. Both runtime arms reused the same clean binary, model, protocol, timestep, capture window, pose, seed, perturbation, evaluators, and thresholds.

**Baseline result.** World-axis root `test-results/action-axis-frame/world-axis-46fa0db` exactly reproduced the established baseline physics/policy hashes. The standing evaluator `FAIL`ed body linear speed, pelvis height, and root tilt; minimum pelvis ratio was 0.153366, maximum root tilt 105.401°, maximum body speed 1119.794 cm/s, and readback was 1.0.

**Experimental result.** Delayed component-axis root `test-results/action-axis-frame/component-axis-delayed-46fa0db` completed successfully and readback remained 1.0. The active-standing input and first-active action-semantic trace were byte-identical to baseline. The active-boundary evaluator returned `VALID/SUPPORTED`: identity inputs/targets were exactly equal, maximum identity-error delta was `1.172e-13°`, and actual-minus-identity plus probe-minus-identity residuals collapsed to `1.389e-11°` and `2.779e-11°`. However, the first-policy snapshot differed materially before the intended intervention: 144 self-observation values changed (maximum 41.638), all 69 action values changed (maximum 0.4791), while mimic and terrain remained exact. This violated the preregistered first-policy equality gate. The physical evaluator `FAIL` set expanded to root linear/angular speed, body linear speed, pelvis height, and root tilt; maximum body speed rose to 3066.237 cm/s.

**Supported or falsified.** Neither under the complete preregistration. The run is `INVALID` because the locked first-policy equality gate failed. Separately, the equal active-boundary semantic evidence strongly supports the local frame-algebra mechanism, but it cannot override the broader invalidity rule or establish product success.

**What was learned.** Delayed activation removed attempt 1's active-standing confound and proved that component-space conjugation eliminates the non-identity round-trip excess on identical active input/actions. A separate startup repeatability problem remains at the first-policy capture, and component-axis behavior alone does not produce standing. No production default, protocol, or threshold changed.

**Next experiment selected.** Run a behavior-neutral A/A/B startup-repeatability experiment on one commit/binary. Trace configured/effective axis mode at every target write from first policy through active standing. If A/A also differs while active-boundary hashes converge, redesign the causal physical test around deterministic action replay or a same-state counterfactual. If only B differs while the override is proven ineffective, find the hidden command-line side effect before testing captured-neutral ownership.

Machine-readable record: `experiments/stage1/active-standing-action-axis-frame.e16.attempt2.json`.


## E17 — Startup repeatability and admissible delayed-axis A/B (2026-07-15)

**Hypothesis.** E16 attempt 2's first-policy mismatch was either normal startup nondeterminism or a hidden pre-active side effect of the component-axis flag.

**Configuration.** Three fresh runs on commit `649fe7b5cf27920bf371cd11ec7a6b31e5b2213c`, one reused binary, locked standing-plant v2 protocol/model/timestep/window/pose, no perturbation, action and local-frame traces on. A1 and A2 were identical world-axis controls; B added only the component-axis override, effective solely in `BalanceActive_Standing`.

**Result.** A1 and A2 were byte-identical at first policy, active-standing input, action trace, physics, policy, and render. B was also byte-identical to both controls at first policy and active-standing input. The active-boundary evaluator returned `VALID/SUPPORTED`: identity inputs/targets were exact, maximum identity-error delta was `1.172e-13°`, maximum actual-minus-identity residual was `1.389e-11°`, and maximum probe-minus-identity residual was `2.779e-11°` against the locked `0.001°` threshold.

**Physical result.** A1/A2 reproduced the established failure on body linear speed, pelvis height, and root tilt. B worsened to root linear/angular speed, body linear speed, pelvis height, and root tilt; maximum body speed rose from `1119.794` to `3066.237 cm/s` and maximum root speed from `818.947` to `1712.640 cm/s`. Readback remained 1.0.

**Supported or falsified.** Startup nondeterminism and hidden pre-active flag side effect were both falsified. The fresh delayed-axis A/B is admissible and supports the frame-algebra hypothesis, but the semantically corrected component axis worsens standing.

**What was learned.** E16 attempt 2's earlier first-policy mismatch was an isolated malformed run. World-space conjugation causes the non-identity round-trip excess, yet appears to compensate physically for the remaining captured-neutral-versus-bind identity residual. No production behavior is promoted.

**Next experiment selected.** Run a preregistered 2x2 active-standing intervention over action-axis frame (world/component) and neutral source (captured stable neutral/bind-calibrated neutral). Require exact pre-intervention captures. The component+bind arm should eliminate both identity and non-identity residuals; physical outcomes determine whether captured neutral is compensatory or causal.

Machine-readable record: `experiments/stage1/startup-repeatability.e17.json`.


## E18 — Action-adapter axis/neutral factorial (2026-07-15)

**Hypothesis.** The live action adapter uses both the wrong parent-axis frame and a captured standing neutral that differs from observation bind. Component-space axis plus bind neutral should make action composition and observation recovery exact inverses and may improve standing.

**Configuration.** Four arms on clean commit `b582421aba2891a7aa9a9627ad28be09e2bf546d`, one reused binary, locked standing-plant v2 protocol/model/timestep/window/pose, no perturbation, and unchanged range mapping, safety projection, gains, timing, and readback. The factors began only after the first policy snapshot/output: world versus component parent axis, and captured versus bind neutral.

**Semantic result.** The evaluator returned `VALID/SUPPORTED`; first-policy input and output were byte-identical and readback was 1.0 in every arm. A reproduced 35.963° identity residual and up to 91.364° non-identity excess. B preserved the identity residual but reduced non-identity excess to `2.779e-11°`. C removed identity residual but retained up to 104.016° world-axis error. D produced exactly `0°` identity, actual-action, and every decisive axis-probe error.

**Physical result.** All arms failed. A fell below 80 cm pelvis height at 0.267 s. D entered active standing nearly upright and delayed that crossing to 0.417 s, but still fell below 20 cm by 0.600 s and failed body linear/angular speed, pelvis height, and root tilt. D maximum body speed was 1555.042 cm/s versus A's 1119.794 cm/s; readback remained 1.0.

**Supported or falsified.** The adapter hypothesis is supported semantically, while its sufficiency as the cause of standing failure is falsified. Exact action/observation inversion improves the initial transient but does not produce standing.

**What was learned.** The largest measured downstream semantic distortion is now the unchanged constraint-range mapper: in D it rotates the exact pre-range target by 19.94° on average and up to 58.18°, while subsequent safety projection averages only 0.045°. No production behavior is promoted.

**Next experiment selected.** Hold component+bind fixed and bypass only `MapProtoPolicyTargetToMannyConstraintRange` from first policy, retaining `AdaptParentRelativeTarget`, step limiting, gains, timing, and all locked judges. Require exact first-policy snapshot/output and readback 1.0.

Machine-readable record: `experiments/stage1/action-adapter-factorial.e18.json`.


## E20 — Policy action-family ablation (2026-07-15)

**Hypothesis.** Nonzero ONNX output destabilizes standing through a specific anatomical action family rather than through inference, target dispatch, controls, or the passive plant.

**Configuration.** Five same-binary arms on commit `e45ac4232ef629e7f195ca26283018c893badb45`, with component-space parent-axis composition, captured standing neutral, protective range mapping, safety projection, gains, timing, pose, and no perturbation held fixed. Raw ONNX output remained unchanged; only the post-conditioning retained family differed: zero, lower-only, axial-only, arms-only, or all.

**Validity.** First-policy snapshots were byte-identical. Every conditioned scalar matched the declared anatomical mask over all 300 policy steps. Inference and target dispatch remained live, and readback was 1.0 in every arm.

**Result.** Lower-only passed the full ten-second hold with pelvis ratio 0.960, maximum root tilt 17.664°, maximum body speed 140.146 cm/s, and no support gap. Axial-only failed catastrophically, crossing 80 cm pelvis height at 0.333 s and 20 cm at 0.450 s. Arms-only failed independently, crossing 80 cm at 1.783 s and losing support for 1566.7 ms. All actions failed. The zero arm remained physically upright and bounded; its RealOnnxPolicy evaluation failed only the intentionally required nonzero-action criterion, consistent with the separately passing locked ZeroActions layer.

**Supported or falsified.** Supported with two independently sufficient harmful families. Lower-body policy actions are compatible with standing; axial and arm outputs can each destroy it.

**What was learned.** The failure is not a generic policy-output magnitude problem and is not caused by the lower-body balance commands. The next causal boundary is upper-body target or actuator compatibility. The exact pinned ProtoMotions checkpoint package is now available locally for authoritative comparison.

**Next experiment selected.** Compare checkpoint per-joint PD authority and limits with UE, then subdivide axial actions into torso/spine/chest versus neck/head and arms into proximal versus distal groups.

Machine-readable record: `experiments/stage1/policy-action-family-ablation.e20.json`.


## E22 — Exact checkpoint force-PD response (2026-07-15)

**Hypothesis.** The all-action policy fails because UE uses uniform 8 Hz acceleration-mode drives instead of the pinned ProtoMotions SMPL force-PD response.

**Configuration.** Same-binary A/B on commit `3e13a6968344e6756020f0d8680e095374e309ce`, with exact component-axis plus bind-neutral action mapping, all ONNX actions, unchanged range mapping and safety projection, and intervention only after the first active-standing policy capture.

**Validity.** First-policy input/output, first-active-standing input, action semantic trace, and Manny local-frame trace were byte-identical. The experimental arm transitioned from the existing 8 Hz acceleration drive to 21/21 checkpoint family profiles in force mode. Thigh readback was 450.158 Hz equivalent strength, zero damping ratio, 800000 extra damping units, 5000000 torque units, and force mode. Target readback remained 1.0.

**Result.** The control failed after crossing 80 cm pelvis height at 0.417 s. Checkpoint force-PD delayed that crossing to 1.583 s, but maximum body linear speed rose from 1555.0 to 10797.4 cm/s, maximum body angular speed from 5752.9 to 30499.99 deg/s, maximum root angular speed from 647.2 to 11340.8 deg/s, and support loss reached 883.3 ms. The policy window terminated early.

**Supported or falsified.** Falsified. The exact trained actuator response is not dynamically compatible with the UE Manny plant; the current acceleration-mode controls are protective.

**Next experiment selected.** Return to E20's localized upper-body target failure and subdivide axial and arm action groups until the minimum independently sufficient failing joint subset is identified.

Machine-readable record: `experiments/stage1/checkpoint-force-pd.e22.json`.

## E23 — Upper-action contiguous subset ablation (2026-07-15)

**Hypothesis.** E20's independently harmful axial and arm families can be localized to smaller contiguous ProtoMotions joint subsets while raw inference, observations, the known-stable acceleration plant, and downstream target stages remain unchanged.

**Configuration.** Seven same-binary arms on commit `c01d2d9cd609312c7b844e813aea1eb8ed877ecf`, with component-space parent-axis composition, captured standing neutral, protective range mapping, safety projection, 8 Hz acceleration controls, timing, pose, and no perturbation held fixed. A generic post-conditioning range mask retained zero actions, trunk indices 8–10, neck/head 11–12, left proximal 13–15, left collapsed distal 16–17, right proximal 18–20, or right collapsed distal 21–22.

**Validity.** All first-policy snapshots, including all 69 raw ONNX actions, were byte-identical. Every action triplet outside the declared range was exactly zero. Every arm produced 601 physics samples and 300 policy samples with target readback ratio 1.0. The zero arm remained physically stable and failed only the intentionally absent `real_policy_action` criterion.

**Result.** Trunk indices 8–10 were independently catastrophic: pelvis crossed 80% height at 0.333 s, root tilt crossed 45° at 0.350 s, maximum body linear speed reached 1555.38 cm/s, and maximum body angular speed reached 4617.80°/s. Right proximal indices 18–20 were independently sufficient for delayed collapse, crossing both height and tilt limits at about 7.0 s. Neck/head, both collapsed distal groups, and the full left proximal chain passed ten seconds with maximum tilt below 7.27° and readback 1.0.

**Supported or falsified.** Supported. The axial failure is confined to `Torso/Spine/Chest`; the arm failure is asymmetric and confined at this resolution to the right proximal chain.

**What was learned.** The collapsed wrist/hand mappings are not independently harmful. Left proximal outputs are also safe in isolation. Two distinct remaining regions require individual-joint resolution: Proto joints 8–10 and 18–20.

**Next experiment selected.** Run indices 8, 9, 10, 18, 19, and 20 individually from the same binary with a zero control. If individuals pass, run preregistered contiguous pairs before adding noncontiguous mask support.

Machine-readable record: `experiments/stage1/upper-action-subset-ablation.e23.json`.

## E24 — Individual harmful-joint ablation (2026-07-15)

**Hypothesis.** At least one individual ProtoMotions joint inside each E23-sufficient region is independently sufficient for standing failure on the Manny plant.

**Configuration.** Seven same-binary arms using the E23 range mask: zero, `Torso` index 8, `Spine` 9, `Chest` 10, `R_Thorax` 18, `R_Shoulder` 19, and `R_Elbow` 20. Component-space action axis, captured standing neutral, range mapping, safety projection, 8 Hz acceleration controls, timing, pose, and no perturbation remained fixed.

**Validity.** All first-policy snapshots were byte-identical, all nonselected conditioned triplets were exactly zero, target readback was 1.0, and the zero arm was physically stable with only the intentional nonzero-action criterion absent.

**Result.** `Torso` alone reproduced catastrophic collapse at the same boundary as the full trunk subset: pelvis crossed 80% height at 0.333 s, root tilt crossed 45° at 0.350 s, maximum body linear speed reached 1552.28 cm/s, and maximum body angular speed reached 4097.43°/s. `Spine` and `Chest` passed. Every individual right-proximal joint also passed, with maximum root tilt below 5.60°.

**Supported or falsified.** Partially supported. The trunk failure is completely localized to Proto joint 8. The right-proximal failure is an interaction among otherwise safe individual joints.

**Next experiment selected.** Run right-proximal contiguous pairs 18–19 and 19–20. In parallel, test the already stable lower-only policy under the complete locked causal-standing product bundle.

Machine-readable record: `experiments/stage1/individual-harmful-joint-ablation.e24.json`.

## E25 — Lower-only full causal-standing bundle (2026-07-15)

**Hypothesis.** The E20-proven stable lower-body ONNX action subset supplies causal perturbation recovery while incompatible upper-body outputs are unnecessary for the locked product contract.

**Configuration.** Three `Normal`, three `ZeroActions`, one `DropControlDispatch`, and one `ForcedSupportLoss` PRODUCT_RUN on the locked causal-standing protocol. The Normal candidate retained Proto joints 0–7 with component-space action axis, captured standing neutral, protective range mapping/projection, and the current 8 Hz acceleration plant.

**Validity and absolute result.** All three Normal and all three ZeroActions repetitions passed every absolute criterion with target readback 1.0. Drop dispatch failed only target readback and forced support loss failed support gap, as intended.

**Causal result.** The bundle failed only `causal_recovery_advantage`. Median Normal recovery AUC was 65.6985 versus ZeroActions 41.1418, a ratio of 1.59688 against the locked maximum 0.8. Lower-only therefore stands but is not causal standing.

**What was learned.** The deficit is persistent pose error rather than a missed recovery deadline. Normal post-impulse RMS averaged 8.217° versus zero 5.142°, and `foot_l` was the maximum-error bone in 476 of 481 post-impulse samples. No acceptance threshold changed.

**Next experiment selected.** Screen Proto lower-body joints 0–7 individually under the same product perturbation and evaluator. A viable candidate must pass all absolute gates and have recovery AUC no greater than 32.9134 against the observed deterministic zero reference.

Machine-readable record: `experiments/stage1/lower-only-causal-standing.e25.json`.

## E26 — Individual lower-joint recovery screen (2026-07-15)

**Hypothesis.** One individual lower-body ProtoMotions joint can retain nonzero policy behavior and reduce post-impulse pose-error AUC below the zero-action reference.

**Configuration.** Eight PRODUCT_RUN Normal screens under the locked causal-standing perturbation, each retaining exactly one lower-body Proto joint through the existing range mask. The E25 deterministic ZeroActions median AUC of 41.1418 supplied the screening reference; the locked candidate threshold was 32.9134.

**Result.** No arm crossed the threshold. `R_Ankle` was best at AUC 40.1759. `L_Knee`, `L_Toe`, `R_Hip`, and `R_Toe` were effectively neutral around 41–42. `L_Ankle` and `R_Knee` worsened AUC to 47.2274 and 53.0417. `L_Hip` lost standing and failed recovery with AUC 115.5441.

**Supported or falsified.** Falsified. Static single-joint selection cannot establish causal advantage.

**What was learned.** The useful signal, if present, is not an isolated absolute joint target. The policy carries a steady action bias that changes pose even before perturbation; safe single-joint actions mostly reproduce the passive trajectory rather than improve it.

**Next experiment selected.** Capture the standing policy action as a calibration vector and dispatch only subsequent action deviations, preserving raw inference, timing, action mapping, plant, and locked product evaluator.

Machine-readable record: `experiments/stage1/lower-joint-recovery-screen.e26.json`.

## E27 — First-active policy action baseline residual (2026-07-15)

**Hypothesis.** E25's persistent pose error is caused by a steady absolute policy-action bias; dispatching only current-minus-first-active action deviations should preserve response while removing the incompatible offset.

**Configuration.** Same-binary PRODUCT_RUN arms: lower-only absolute control, lower-only baseline residual, and ZeroActions with the residual flag. Raw ONNX output remained stored separately; the residual vector entered the unchanged conditioning/mask/decode/mapping path. The zero path was explicitly guarded.

**Result.** All arms passed absolute acceptance and target readback 1.0. Control reproduced AUC 65.6985. Residualization reduced AUC to 45.6597, a 20.0388-unit improvement, but remained worse than ZeroActions 41.1418 and above the locked candidate threshold 32.9134. ZeroActions conditioned action magnitude remained exactly zero.

**Supported or falsified.** Partially supported. The steady action bias is causal, but first-active residualization alone is insufficient.

**What was learned.** Before the first active-standing snapshot exists, the flag still allows absolute actions during preparation/policy blend. That can bias the body state used for calibration and explains why the residual controller does not recover to the passive trajectory.

**Next experiment selected.** Hold conditioned policy actions at zero until the first active-standing baseline is captured, reset smoothing at the domain switch, then dispatch residual actions.

Machine-readable record: `experiments/stage1/policy-action-baseline-residual.e27.json`.

## E28 — Zero policy actions until active baseline capture (2026-07-15)

**Hypothesis.** E27 remained above zero because absolute actions contaminated the body before calibration; zeroing actions until the first active-standing baseline should yield a stable calibration state and recovery-beneficial residuals.

**Configuration.** Same-binary PRODUCT_RUN control, lower-only zero-then-residual experimental arm, and ZeroActions guard. The experimental mode zeroed pre-capture conditioning inputs, reset smoothing at residual activation, and left raw inference unchanged.

**Result.** Control passed with AUC 65.6985. The experimental arm worsened to AUC 70.2901 and failed pelvis-height and root-tilt criteria, despite target readback 1.0. ZeroActions remained unchanged at AUC 41.1418 with exactly zero conditioned actions.

**Supported or falsified.** Falsified. Preparation actions are stabilizing rather than the remaining source of causal error.

**Next experiment selected.** Audit perturbation-induced self-observation velocities and heading-frame/basis conversion against the pinned ProtoMotions checkpoint implementation. Test only a source-supported basis or sign correction.

Machine-readable record: `experiments/stage1/policy-action-zero-until-baseline.e28.json`.

## E29 — Physics-body position observation provenance (2026-07-15)

**Hypothesis.** ProtoMotions observes one coherent rigid-body state, while UE appeared to mix skeletal bone positions with physics-body rotations and velocities; selecting physics-body transform origins should improve policy response.

**Configuration.** Same-binary PRODUCT_RUN control with bone positions, Normal with physics-body positions, and ZeroActions guard. Rotation, velocities, lower-only action mask, target mapping/projection, 8 Hz acceleration plant, perturbation, and evaluator were unchanged. A 24-body trace recorded bone and body origins.

**Result.** All arms passed absolute acceptance and target readback. Control AUC was 65.6985, physics-body positions AUC 65.5364, and ZeroActions AUC 41.1418. The largest measured bone-to-body origin delta was only 0.000006512 cm.

**Supported or falsified.** Falsified. For Manny's PhysicsAsset the BodyInstance transform origin is numerically the bone origin, so the apparent source-level mismatch does not exist at runtime.

**Next experiment selected.** Test state-triggered acceleration-mode angular authority under the locked perturbation, leaving observations, actions, target semantics, and acceptance thresholds unchanged.

Machine-readable record: `experiments/stage1/rigid-body-position-observation.e29.json`.

## E30 — Delayed acceleration-strength screen (2026-07-15)

**Hypothesis.** Lower-only policy targets are recovery-beneficial but undertracked by the same 8 Hz authority used for ZeroActions; a moderate post-capture strength increase should reduce actual-to-target error.

**Configuration.** Same-binary PRODUCT_RUN Normal arms at strength factors 1.0, 1.25, 1.5, and 2.0, plus a ZeroActions 1.0 guard. The factor activated only after the first active-standing policy snapshot. Damping ratio, extra damping, torque, targets, observations, actions, mapping, perturbation, and evaluator remained fixed.

**Validity.** Every first-policy snapshot had SHA256 `AAAF45E77B0D8BFAFD231C3E32A2F852CD2CF420128B74EFF6651E62FBB316AA`. Physics evidence showed multiplier 1.0 at capture and the configured factor from 0.0167 s onward. Damping ratio stayed 1.5, extra damping 2.4, torque multiplier 1.0, and target readback 1.0.

**Result.** Control AUC was 65.6985. Factor 1.25 passed at 45.2485. Factor 1.5 passed at 30.3745, below the locked maximum 32.9134 and at 0.7383 of ZeroActions AUC. Factor 2.0 overshot, failing pelvis height and root tilt at AUC 39.1640. ZeroActions remained unchanged at 41.1418.

**Supported or falsified.** Supported. The policy direction is causal once its targets receive sufficient acceleration-mode authority, with a bounded optimum near factor 1.5.

**Next experiment selected.** Run the complete locked 3+3+2 causal-standing bundle at policy-mode strength factor 1.5.

Machine-readable record: `experiments/stage1/acceleration-strength-screen.e30.json`.

## E31 — Complete lower-policy authority candidate bundle (2026-07-15)

**Hypothesis.** Lower-only policy actions with delayed 1.5 acceleration-strength authority satisfy the complete locked causal-standing product contract.

**Configuration.** Three Normal repetitions at lower-only actions and 1.5 policy-mode authority, three ZeroActions repetitions at the unchanged 1.0 plant, plus DropControlDispatch and ForcedSupportLoss controls. All used one binary, the locked protocol, fixed timestep, perturbation, render, and evaluator.

**Result.** Bundle status `PASS`. Every Normal and ZeroActions repetition passed absolute criteria with target readback 1.0. Normal AUC was deterministically 30.3745; ZeroActions was 41.1418; ratio 0.738289 against the locked maximum 0.8. Drop dispatch failed target readback at ratio 0.6190, and forced support loss failed support gap, as intended.

**Supported or falsified.** Supported. This is the first complete causal-standing product-candidate PASS.

**Promotion decision.** The candidate still uses development flags. Promote the lower-body compatibility mask and 1.5 policy-mode authority into explicit production behavior through TDD, then rerun the full bundle without any experimental arguments before claiming production success.

Machine-readable record: `experiments/stage1/lower-policy-authority-bundle.e31.json`.

## E32 — No-flag production causal standing (2026-07-15)

**Hypothesis.** Promoting the E31 lower-body compatibility mask and delayed 1.5 policy authority into production would reproduce the complete locked bundle without experimental command-line flags.

**Configuration.** Production commit `b82131b893ab23b87a037b862093a50057c7058b`, locked PRODUCT_RUN protocol, Normal repetition 1, no experimental arguments, and action-semantic evidence enabled. The full bundle was to continue only if required Normal arms remained viable.

**Validity.** The manifest reports a clean source tree and the process command line contains no experimental flags. Conditioned action scalars 0–23 retain the policy output and every scalar 24–68 is exactly zero. Physics evidence shows angular strength 1.0 at time zero and 1.5 from 0.0167 s onward. Target readback is 1.0.

**Result.** Normal failed pelvis height, root tilt, support gap, and recovery with AUC 145.4728. The required first Normal arm is therefore sufficient to falsify the bundle; additional E32 repetitions were not run.

**Supported or falsified.** Falsified. E31 depended on the component-axis adapter as well as the promoted action mask and strength. With the production-default cached-world action axis, the otherwise correct lower-body targets inject high-energy instability.

**Next experiment selected.** Promote the already validated component-axis action composition into the standing-policy production path through red-green TDD, then rerun the complete eight-run bundle without experimental flags.

Machine-readable record: `experiments/stage1/production-causal-standing.e32.json`.

## E33 — Production component-axis causal standing (2026-07-15)

**Hypothesis.** Promoting the exact E18/E31 component-axis action composition into the standing-policy production path would restore the complete locked bundle without experimental command-line flags.

**TDD.** Red produced exactly three unresolved production helper symbols. Green passed `PhysAnim.ProductHarness.DropDispatchSwitch` and `PhysAnim.Bridge.MannyLocalFrameRoundtripTraceContract`. Production routing is active throughout standing activation and the legacy test helpers now delegate to the production quaternion implementation.

**Configuration.** Source commit `1555e8863d00cd4fbe6fc293cac93966fca9682c`, one binary, locked PRODUCT_RUN protocol SHA256 `75b29360907028d081cbfa43e965a35fef760873c76d71b52f2147e99f54606a`, no `PhysAnimExperimental` arguments, lower-body compatibility mask, delayed 1.5 nonzero-policy authority, and production component-axis target composition.

**Result.** Bundle PASS. Normal x3 each AUC 30.374536874795023 with all absolute gates and readback 1.0. ZeroActions x3 each AUC 41.14179622283463. Median Normal/ZeroActions ratio 0.7382890311905358. DropControlDispatch failed only target readback at 0.6190476190476191. ForcedSupportLoss failed only support gap.

**Validity.** All manifests are clean and match the source/model/protocol. Normal and Zero policy, physics, and input-snapshot streams are exact across repetitions. Normal conditioned scalars 24–68 are always zero while lower-body policy actions remain nonzero. ZeroActions conditioned scalars are all zero. Normal strength transitions 1.0 to 1.5; ZeroActions remains 1.0. No experimental argument appears in the runner or run evidence.

**Supported or falsified.** Supported. Holding the mask and authority fixed, E32 cached-world composition failed at AUC 145.47282397075423; E33 component-space composition passes at AUC 30.374536874795023. Production causal standing is achieved.

**Baseline.** Treat `1555e8863d00cd4fbe6fc293cac93966fca9682c` as immutable before separately preregistered upper-body restoration.

Machine-readable records: `experiments/stage1/production-component-action-axis.e33.json` and `experiments/stage1/production-component-action-axis.e33.audit.json`.

## E34 — Neck/head upper-body restoration candidate (2026-07-15)

**Hypothesis.** Restoring ProtoMotions neck/head joints 11–12 in addition to the production lower-body set preserves the complete locked causal-standing product contract.

**Configuration.** Same committed binary and locked 3+3+2 PRODUCT_RUN bundle as E33, with one development option adding only joints 11–12 to retained joints 0–7. Component-axis composition, neutral, range mapping/projection, 8 Hz acceleration plant, delayed 1.5 Normal authority, perturbation, model, and thresholds were unchanged.

**Validity.** Every manifest used clean source commit `d3582083b04c5874a3cfcfd80d2d1b2a602400e2`, the locked protocol/model hashes, and only the intended `NeckHead` option. Normal traces retained nonzero lower and neck/head actions while joints 8–10 and 13–22 were exactly zero. ZeroActions stayed exactly zero; strength paths and destructive controls remained correct.

**Result.** All three Normal and all three ZeroActions runs passed absolute gates with readback 1.0. Normal median recovery AUC worsened to 35.6079 versus ZeroActions 41.1418, ratio 0.865491 above the locked maximum 0.8. The bundle failed only `causal_recovery_advantage`. Normal also split into two policy/physics trajectories; repetitions 1 and 3 matched, while repetition 2 had a different first-policy input snapshot and AUC 35.7462. ZeroActions remained byte-deterministic.

**Supported or falsified.** Falsified. Neck/head together can stand, but they consume the causal recovery margin and violate deterministic acceptance.

**Next experiment selected.** Keep E33 lower-only production unchanged and localize the pair with separately preregistered single-joint experiments, beginning with Proto joint 11 (`Neck`).

Machine-readable record: `experiments/stage1/upper-body-neck-head-restoration.e34.json`.

## E35 — Neck-only upper-body restoration (2026-07-15)

**Hypothesis.** Restoring only Proto joint 11 (`Neck`) in addition to production joints 0–7 preserves the complete locked causal-standing contract.

**Configuration.** Same locked 3+3+2 PRODUCT_RUN bundle and E33 plant, with only `-PhysAnimExperimentalCausalStandingUpperBody=Neck`. Joint 12 and all other upper-body outputs remained exactly zero.

**Validity.** All manifests used clean source commit `6740a49b81d8a64725de39116890377511f1567a`, locked model/protocol hashes, correct masks and strength paths, and only the intended option. Normal and ZeroActions streams were byte-deterministic. Destructive controls failed only their intended criteria.

**Result.** All Normal and ZeroActions repetitions passed absolute gates and readback 1.0. Neck-only Normal AUC was deterministically 34.7217 versus ZeroActions 41.1418, ratio 0.843951 above the locked maximum 0.8. The bundle failed only `causal_recovery_advantage`.

**Supported or falsified.** Falsified. Neck alone is stable and deterministic but consumes too much of the causal recovery advantage.

**Next experiment selected.** Keep E33 lower-only production unchanged and run separately preregistered Head-only restoration.

Machine-readable record: `experiments/stage1/upper-body-neck-restoration.e35.json`.

## E36 — Head-only upper-body restoration (2026-07-15)

**Hypothesis.** Restoring only Proto joint 12 (`Head`) in addition to production joints 0–7 preserves the complete locked causal-standing contract.

**Configuration.** Same locked 3+3+2 PRODUCT_RUN bundle and E33 plant, with only `-PhysAnimExperimentalCausalStandingUpperBody=Head`. Neck and every other upper-body output remained exactly zero.

**Validity.** All manifests used clean source commit `646cf99990db169a96e79f41c5b5c2b69ef7bd44`, locked model/protocol hashes, correct masks and strength paths, and only the intended option. ZeroActions was byte-deterministic and both destructive controls remained discriminative.

**Result.** The authoritative bundle passed with median Normal AUC 30.5563 versus ZeroActions 41.1418, ratio 0.742708 below the locked 0.8 maximum. Head is therefore recovery-beneficial. Exact product acceptance was not met because Normal split into two policy/input/physics trajectories: repetition 1 AUC 29.7672, repetitions 2–3 AUC 30.5563.

**Supported or falsified.** Partially supported. Head-only is causal and absolutely stable, but not exactly deterministic when active throughout standing preparation.

**Next experiment selected.** Keep Head masked during preparation and enable it only in `BalanceActive_Standing`, testing whether deterministic E33 startup can be retained without losing Head's recovery benefit.

Machine-readable record: `experiments/stage1/upper-body-head-restoration.e36.json`.

## E37 — Head restoration only in active standing (2026-07-15)

**Hypothesis.** Masking Head during preparation/blend and restoring it only in `BalanceActive_Standing` preserves E36's causal benefit while restoring exact repeatability.

**Result.** The locked bundle passed at ratio 0.733380. Normal AUCs were 30.1726, 30.0320, and 30.1726; all absolute and destructive-control gates behaved correctly. Exact determinism still failed with the same two first-active input trajectories.

**Supported or falsified.** Partially supported. Active-state timing improves causal recovery but is still early enough to permit startup bifurcation.

**Next experiment selected.** Restore Head only after `FirstActiveStandingPolicyInferenceSnapshot` is captured, matching the proven post-capture strength transition boundary.

Machine-readable record: `experiments/stage1/upper-body-head-active-only.e37.json`.

## E38 — Head restoration after first active-policy cycle (2026-07-15)

**Hypothesis.** Keeping Head zero through the first active-policy conditioning cycle, then restoring it from the next active inference onward, preserves the causal benefit and eliminates the startup trajectory split.

**Configuration.** Same locked 3+3+2 PRODUCT_RUN bundle and E33 plant, with only `-PhysAnimExperimentalCausalStandingUpperBody=HeadAfterFirstPolicy`. Dedicated evidence captured the first active conditioned action vector before later product sampling.

**Validity.** Every first-active Head triplet was exactly `[0,0,0]` at action width 69, while later Normal policy samples contained nonzero Head behavior. Masks, strengths, model/protocol hashes, clean manifests, target readback, and destructive controls were correct.

**Result.** The bundle passed at median Normal AUC 29.9655 versus ZeroActions 41.1418, ratio 0.728347. Normal repetitions 1–2 matched exactly, but repetition 3 entered the alternate first-policy input trajectory and scored 29.7989. ZeroActions remained byte-deterministic.

**Supported or falsified.** Partially supported. Post-snapshot Head is causally beneficial and its timing is proven, but the upstream first-policy snapshot bifurcation remains and blocks production promotion.

**Next experiment selected.** Keep E33 lower-only production unchanged; preserve Head-after-first-policy as a proven experimental candidate and test Spine+Chest (Proto joints 9–10) separately, excluding catastrophic torso joint 8.

Machine-readable record: `experiments/stage1/upper-body-head-after-first-policy.e38.json`.

## E39 — Spine+Chest upper-body restoration (2026-07-15)

**Hypothesis.** Restoring Proto joints 9–10 (`Spine`, `Chest`) while keeping catastrophic torso joint 8 and all other upper-body actions zero preserves the locked causal-standing contract.

**Configuration.** Same locked 3+3+2 PRODUCT_RUN bundle and E33 plant, with only `-PhysAnimExperimentalCausalStandingUpperBody=SpineChest`.

**Validity.** All manifests used clean source commit `25b68038df230a4cb408a04239deeca496dfdc8b`, locked model/protocol hashes, exact masks and strength paths, and only the intended option. First-active and complete policy evidence showed nonzero joints 9–10, exact-zero joint 8 and joints 11–22. Normal and ZeroActions streams were byte-deterministic.

**Result.** Every Normal and ZeroActions run passed absolute gates and readback 1.0. Normal AUC was deterministically 31.9287 versus ZeroActions 41.1418, ratio 0.776066 below the locked maximum 0.8. Destructive controls failed only their intended criteria.

**Supported or falsified.** Supported. Spine+Chest is the first deterministic upper-body region to pass the complete causal product bundle.

**Next experiment selected.** Promote Spine+Chest into the default production mask through TDD, then rerun the strict no-experimental-flag bundle.

Machine-readable record: `experiments/stage1/upper-body-spine-chest-restoration.e39.json`.

## E40 — Production Spine+Chest promotion (2026-07-15)

**Hypothesis.** Promoting the E39 Spine+Chest mask into default production behavior reproduces the complete locked causal-standing contract without experimental behavior arguments.

**Configuration.** Default standing now retains Proto joints 0–7 and 9–10. Torso joint 8, Neck, Head and all arm actions remain exactly zero. The strict 3+3+2 PRODUCT_RUN bundle used clean source commit `3bbcfc54225293a185b23c8d211f25b853cb91f6` with no `PhysAnimExperimental` arguments.

**Validity.** Runner and evidence contained no experimental arguments. All manifests used the locked model/protocol hashes and clean source commit. First-active and full policy evidence showed the exact production mask. Normal and ZeroActions streams were byte-deterministic; strength paths and destructive controls remained correct.

**Result.** Bundle status `PASS`. Normal AUC was deterministically 31.9287, ZeroActions 41.1418, ratio 0.776066 against the locked maximum 0.8. Drop dispatch failed only target readback and forced support loss failed only support gap.

**Supported or falsified.** Supported. Spine+Chest is now validated production behavior and the new immutable upper-body restoration baseline.

**Next experiment selected.** Separately preregister distal wrist/hand restoration against production commit `3bbcfc54225293a185b23c8d211f25b853cb91f6`.

Machine-readable record: `experiments/stage1/production-spine-chest-restoration.e40.json`.

## E41 — Distal wrist/hand restoration (2026-07-15)

**Hypothesis.** Restoring Proto joints 16–17 and 21–22 (`L_Wrist`, `L_Hand`, `R_Wrist`, `R_Hand`) on top of production Spine+Chest preserves the locked causal-standing contract.

**Configuration.** Same locked 3+3+2 PRODUCT_RUN bundle, with only `-PhysAnimExperimentalCausalStandingUpperBody=DistalHands`. Proximal arms, torso joint 8, Neck and Head remained exactly zero.

**Validity.** All manifests used clean source commit `54ce5b188dd5934818fba8f5330f315084f05c01`, locked model/protocol hashes, exact masks and strength paths, and only the intended option. The four distal source-joint triplets were nonzero in Normal and every conditioned scalar was zero in ZeroActions.

**Result.** The authoritative bundle passed at ratio 0.742277. Normal AUCs were 29.8358, 30.5386 and 30.5386; ZeroActions AUCs were 41.1418, 41.3581 and 41.1418. Destructive controls failed only their intended criteria.

**Supported or falsified.** Partially supported. Distal hands are causally beneficial and absolutely stable, but exact repeatability fails because the recurring two-state first-policy startup snapshot bifurcation also appears in ZeroActions. This is upstream of distal-hand action application.

**Next experiment selected.** Resolve the startup snapshot bifurcation before production promotion, then rerun distal hands from the fixed deterministic baseline.

Machine-readable record: `experiments/stage1/upper-body-distal-hands-restoration.e41.json`.

## E42 — Fixed timestep before PIE startup (2026-07-15)

**Hypothesis.** The recurring first-policy snapshot bifurcation is caused by enabling the fixed 60 Hz timestep only after PIE has already started.

**Configuration.** Harness-only change. A dedicated latent command enables fixed timestep before `FStartPIECommand`; capture retains it through PIE and a post-play command restores the prior clock. The production controller and no-flag E40 joint mask remain unchanged.

**Validity.** The full 3+3+2 PRODUCT_RUN bundle used clean harness commit `64e5a8cb93eaf1bca199a7812c892a2a2419e5cb`, locked model/protocol hashes, no experimental behavior arguments, exact production masks and strengths, and discriminative destructive controls.

**Result.** Bundle PASS at ratio 0.772007. Normal repetitions 1-2 exactly reproduced AUC 31.9287 and snapshot `AAAF45...`; repetition 3 used snapshot `723FD9...` and AUC 30.7547. ZeroActions was internally deterministic at AUC 41.3581 on the `723FD9...` startup family.

**Supported or falsified.** Falsified. Pre-PIE fixed timestep is valid and behavior-neutral, but is not sufficient to eliminate the two startup state families.

**Next experiment selected.** Screen single-threaded engine/Chaos execution. If the bifurcation remains, adopt deterministic same-state replay or an in-process counterfactual for joint promotion rather than relying on exact cross-process startup reproduction.

Machine-readable record: `experiments/stage1/deterministic-pre-pie-fixed-step.e42.json`.

## E43 — Single-threaded deterministic acceptance execution (2026-07-15)

**Hypothesis.** Unreal's documented `-onethread` execution switch removes the asynchronous startup ordering responsible for the recurring first-policy state bifurcation.

**Configuration.** Same fixed-step harness and unchanged E40 production controller, with only `-onethread` added to every PRODUCT_RUN arm. No PhysAnim experimental behavior arguments.

**Validity.** Clean preregistration commit `b4f4f9255da3bf07a74ac7b3e5809c3741a02c08`, locked model/protocol, exact production masks and strengths, and intended destructive controls.

**Result.** Full bundle PASS at ratio 0.756685. Normal AUC was exactly 29.7751 in all three repetitions; ZeroActions was exactly 39.3495 in all three. Policy-input, policy, first-active and physics streams were byte-identical within both variants. Controls failed only their intended criteria.

**Supported or falsified.** Supported. `-onethread` is the deterministic causal-standing acceptance configuration for subsequent upper-body restoration.

**Next experiment selected.** Rerun DistalHands under the deterministic configuration and promote if the complete bundle remains causal and exact.

Machine-readable record: `experiments/stage1/deterministic-single-thread-execution.e43.json`.

## E44 — Deterministic distal wrist/hand restoration (2026-07-15)

**Hypothesis.** E41's DistalHands candidate becomes exactly repeatable under the supported `-onethread` acceptance configuration.

**Configuration.** Restore Proto joints 16–17 and 21–22 on top of production lower body and Spine/Chest; keep proximal arms, torso, Neck and Head masked. Run the locked bundle with `DistalHands` and `-onethread`.

**Validity.** Clean preregistration commit `45c0cfbaff9959c868b43c9a5c2b7bc296904549`, locked model/protocol, exact masks/strengths and intended destructive controls.

**Result.** Bundle PASS at ratio 0.752739. Normal AUC was exactly 29.6199 in all three repetitions; ZeroActions was exactly 39.3495. All streams were byte-identical within each variant.

**Supported or falsified.** Supported. DistalHands is eligible for production promotion.

**Next experiment selected.** Promote DistalHands into the default production mask and rerun with no PhysAnim experimental behavior arguments under `-onethread`.

Machine-readable record: `experiments/stage1/deterministic-distal-hands-restoration.e44.json`.

## E45 — Production distal wrist/hand restoration (2026-07-15)

**Hypothesis.** Making joints 16–17 and 21–22 part of the default mask reproduces E44 without a PhysAnim experimental behavior flag.

**Configuration.** Production lower body + Spine/Chest + both distal wrist/hand pairs; deterministic `-onethread` execution.

**Validity.** Clean production commit `a8ee5efab94f4de43dcba6729137b78cd4d3ab28`, locked model/protocol, no PhysAnim experimental arguments, exact masks/strengths and intended destructive controls.

**Result.** Bundle PASS at ratio 0.752739. Normal AUC exactly 29.6199; ZeroActions exactly 39.3495; all repetitions byte-identical.

**Supported or falsified.** Supported. DistalHands is now production behavior.

**Next experiment selected.** Head restoration after the first active policy snapshot.

Machine-readable record: `experiments/stage1/production-distal-hands-restoration.e45.json`.

## E46 — Deterministic Head after first active policy cycle (2026-07-15)

**Hypothesis.** Head joint 12 can be restored from the second active policy inference onward under deterministic execution.

**Configuration.** Production lower body + Spine/Chest + distal hands; Head delayed one active-policy cycle; Neck, torso and proximal arms masked; `-onethread`.

**Validity.** Clean preregistration commit `718607ef5773691a95493163bed2078c9417e694`, locked model/protocol, exact timing/mask/strength contracts and intended destructive controls.

**Result.** Bundle PASS at ratio 0.762566. Normal AUC exactly 30.0066; ZeroActions exactly 39.3495; all streams byte-identical. First active Head triplet is exactly zero and later Head is nonzero.

**Supported or falsified.** Supported. Delayed Head is eligible for production promotion.

**Next experiment selected.** Production-safe first-active-cycle latch and no-flag Head promotion.

Machine-readable record: `experiments/stage1/deterministic-head-after-first-policy.e46.json`.

## E47 — Production delayed Head restoration (2026-07-15)

**Hypothesis.** A production runtime latch can reproduce E46 without a PhysAnim experimental flag.

**Configuration.** Production lower body + Spine/Chest + distal hands; Head enabled only after one completed active-standing policy inference; `-onethread`.

**Validity.** Clean production commit `a88f786c18a77b2e5f48cfad3b3f25247240d108`, locked model/protocol, no PhysAnim experimental arguments, exact timing/mask/strength contracts and intended destructive controls.

**Result.** Bundle PASS at ratio 0.762566. Normal AUC exactly 30.0066; ZeroActions exactly 39.3495; all repetitions byte-identical. First active Head is zero and later Head is nonzero.

**Supported or falsified.** Supported. Delayed Head is now production behavior.

**Next experiment selected.** A reusable scaled, delayed restoration framework for Neck, left proximal arm, right proximal arm and Torso.

Machine-readable record: `experiments/stage1/production-head-after-first-policy.e47.json`.

## E48 — Scaled delayed restoration framework (2026-07-15)

**Purpose.** Add reusable per-region scaling for the four remaining masked regions without changing production behavior.

**Configuration.** Region selector for Torso, Neck, LeftProximal or RightProximal; scale clamped to [0,1]; activation only after one completed active-standing policy inference.

**Validity.** Exact parser/scaling/legacy-overload tests passed. A clean no-option Normal run from commit `aea5d7c3796bb4c5734563614fe04822cc2e1c93` is byte-identical to E47 across policy input, first-active actions, policy and physics streams.

**Supported or falsified.** Supported. The framework is behavior-neutral without a selector and ready for isolated region restoration.

**Next experiment selected.** Neck at a reduced delayed scale.

Machine-readable record: `experiments/stage1/scaled-delayed-restoration-framework.e48.json`.

## E49 — Neck at 50% after first active policy cycle (2026-07-15)

**Hypothesis.** Neck joint 11 at 0.5 delayed scale preserves the locked causal-standing contract.

**Result.** Valid deterministic bundle, but FAIL only on causal recovery. Normal AUC was exactly 31.9974, ZeroActions exactly 39.3495, ratio 0.813160. Absolute standing and destructive controls remained correct; first active Neck was zero and later Neck was nonzero.

**Supported or falsified.** Falsified at scale 0.5.

**Next experiment selected.** Neck at 0.25 with identical timing and execution.

Machine-readable record: `experiments/stage1/scaled-delayed-neck.e49.json`.

## E50 — Neck at 25% after first active policy cycle (2026-07-15)

**Hypothesis.** Neck joint 11 at 0.25 delayed scale preserves the locked causal-standing contract.

**Result.** Supported. The deterministic bundle passed with Normal AUC exactly 30.2616, ZeroActions exactly 39.3495 and ratio 0.769048. First-active Neck stayed zero, later Neck became nonzero, other unselected remaining regions stayed zero, and both destructive controls retained their intended failures.

**Next experiment selected.** Promote delayed Neck scale 0.25 into production and rerun the no-flag deterministic bundle.

Machine-readable record: `experiments/stage1/scaled-delayed-neck.e50.json`.

## E51 — Production Neck at 25% after first active policy cycle (2026-07-15)

**Hypothesis.** Production can reproduce E50 without any PhysAnim experimental argument.

**Result.** Supported. The no-flag deterministic bundle exactly reproduced E50: Normal AUC 30.2616, ZeroActions 39.3495, ratio 0.769048. First-active Neck remained zero, later Neck was nonzero, and destructive controls remained discriminative.

**Next experiment selected.** Delayed scaled restoration of the left proximal arm chain.

Machine-readable record: `experiments/stage1/production-scaled-delayed-neck.e51.json`.
