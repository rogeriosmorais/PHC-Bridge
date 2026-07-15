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
