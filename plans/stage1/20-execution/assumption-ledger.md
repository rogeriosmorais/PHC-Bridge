# Stage 1 Assumption Ledger

## Purpose

This is the orchestrator-owned control document for Stage 1. It tracks the assumptions that can kill the project, the signals that would prove those assumptions wrong, and the fallback or stop decision tied to each one.

This is not a passive risk list. The orchestrator uses it to decide whether downstream work is still safe.

## Status Scale

- `green`: assumption currently acceptable for downstream work
- `yellow`: assumption still plausible, but under active watch
- `red`: assumption is likely false or insufficiently supported; downstream work depending on it should stop
- `resolved`: assumption is no longer an open uncertainty because it was confirmed, rejected, or made irrelevant

## How To Use This Ledger

For each major Stage 1 assumption, track:

- what must be true
- why it matters
- how we will test it
- what evidence would falsify it
- what fallback exists
- whether the project should stop if it fails

Only the orchestrator updates status.

## Critical Assumptions

| ID | Assumption | Why It Matters | Current Status | How It Will Be Tested | Falsification Signal | Fallback | Stop If False |
|---|---|---|---|---|---|---|---|
| A-01 | Pretrained policy motion in training looks convincingly alive enough to justify Stage 1 | Stage 1 is pointless if the starting policy already looks robotic | green | G1 training visualization and manual check `MV-G1-01` | motion looks stiff, unstable, or puppet-like in training | narrow to locomotion-only test or stop before deeper integration | yes |
| A-02 | The selected ProtoMotions/PHC config can be mapped cleanly into a UE5 bridge contract | the plugin cannot be built safely without a stable observation/action contract | green | `bridge-spec.md` confirmed in Phase 0 | tensor fields, ordering, or representation stay unclear | narrow the chosen config or pause until a workable config is selected | yes |
| A-03 | SMPL <-> UE5 retargeting can be made stable enough for Manny | wrong transforms or mirroring will invalidate the entire bridge | green | `retargeting-spec.md` plus G1 Manny smoke test | wrong limbs move, mirroring appears, or instability comes from mapping errors | revise mapping and transform layer | yes |
| A-04 | `UPhysicsControlComponent` can express the policy intent well enough for Stage 1 | the entire low-custom-code thesis depends on it | green | control-path prototype and Manny response checks | targets are ignored, unstable, or too limited to drive plausible motion | explicit fallback to raw torque control if the project chooses it | yes |
| A-05 | Chaos substepping at 120-240 Hz is stable enough for the articulated body | unstable simulation breaks any quality comparison | green | Phase 0 substep stability check | persistent instability despite tuning and solver adjustments | try async physics, then reassess Stage 1 viability | yes |
| A-06 | NNE + ONNX Runtime can run the exported policy fast enough and compatibly in UE5 | inference must fit inside Stage 1 without adding a new runtime stack | green | dummy-model check, then real ONNX load using `onnx-export-spec.md` | model fails to load, runtime creation fails, or runtime cost is unacceptable | shrink/re-export model, reassess viability | yes |
| A-07 | PoseSearch output is sufficient as the upstream motion-intent source for Stage 1 | the bridge assumes PoseSearch can supply the needed target motion intent | yellow | bridge-spec confirmation and one-character integration | required target features are unavailable or unusable | narrow conditioning assumptions rather than inventing a new architecture | maybe |
| A-07b | The available pretrained policy covers enough broad locomotion to make pretrained-first worthwhile | if not, we should switch to fine-tuning or training earlier | yellow | pretrained evaluation on the locked locomotion set | pretrained result is obviously not useful even for locomotion / general motion | go straight to fine-tuning on the locked locomotion set | no |
| A-07c | The locked Stage 1 locomotion-only motion set is sourceable without major scope creep | undefined or missing motions create silent planning drift | yellow | motion-source review against `motion-set.md` | key locomotion motions are missing or only available through major extra pipeline work | replace or remove missing motions explicitly | maybe |
| A-08 | Physics-driven motion will look noticeably better than kinematic playback | this is the thesis tested by G2 | yellow | G2 side-by-side evaluation | user judges difference as negligible or worse | ship only as experiment documentation, do not continue to Stage 2 | yes |
| A-09 | The final Stage 1 locomotion showcase will be compelling enough to justify Stage 2 | Stage 2 is optional and should not start on weak evidence | yellow | G3 observer evaluation | observers find the result unconvincing | stop at Stage 1 | no |

## Latest Orchestrator Review

- `Review commit`: `5465b23` plus local uncommitted G2 comparison harness changes
- `Reviewed artifacts`:
  - `plans/stage1/10-specs/bridge-spec.md`
  - `plans/stage1/10-specs/retargeting-spec.md`
  - `plans/stage1/10-specs/test-strategy.md`
  - `plans/stage1/50-content/motion-set.md`
  - `plans/stage1/50-content/motion-source-map.md`
  - `plans/stage1/50-content/motion-source-lock-table.md`
  - `plans/stage1/20-execution/phase0-execution-package.md`
  - `PhysAnimUE5/PhysAnimUE5.uproject`
  - `PhysAnimUE5/Config/DefaultEngine.ini`
  - `PhysAnimUE5/Saved/Logs/PhysAnimUE5.log`
  - `PhysAnimUE5/Saved/Logs/PhysAnimUE5_2.log`
- `Conclusion`:
  - the Phase 0 planning contract is now concrete enough to execute on this exact Windows machine without more setup replanning
  - the selected `motion_tracker/smpl` checkpoint is now the preferred Stage 1 runtime target because its inference contract is materially simpler than the deferred MaskedMimic path
  - no G1-critical assumption beyond `A-02` moves out of `yellow` yet because `g1-evidence.md` still has no training-side motion evidence or UE-side manual evidence
  - `A-02` now moves to `green` because the local runtime input set, action shape, representation path, and fixed-gain policy are explicit in `bridge-spec.md`
  - partial setup evidence now confirms the planned UE install root and `UE5_PATH`, but it does not yet reduce any G1-critical risk
  - updated setup evidence now also confirms UE `5.7.3` and the presence of the `F:\NewEngine\PhysAnimUE5` scaffold with Manny content paths available
  - ProtoMotions `v2.3.2`, the selected `motion_tracker/smpl` checkpoint, and a Python `3.11` Windows-native environment are now local and consistent with the Isaac Sim `5.x` requirement
  - Isaac Sim `5.1.0.0` and Isaac Lab `2.3.2.post1` are now installed in the locked env and headless `SimulationApp` startup succeeded locally
  - the current Windows path also required a small local ProtoMotions compatibility patch for Python `3.11` dataclass defaults plus a single-device Fabric override to bypass the default DDP / NCCL path
  - the visual IsaacLab path additionally required local compatibility shims for this machine and package set: preloading `h5py` before `AppLauncher`, excluding broken RTX sensor extensions from Kit launch, constructing `Se2Keyboard` with `Se2KeyboardCfg`, and supporting MoviePy `2.x` without `moviepy.editor`
  - the earlier Vulkan startup crash was consistent with an external graphics hook conflict rather than a PHC or IsaacLab logic failure; future visual checks on this machine should start with overlays and capture hooks disabled
  - Isaac sensor DLL warnings for `generic_mo_io.dll`, lidar, radar, and `isaacsim.sensors.rtx` are still present, but the current evidence suggests they are non-blocking noise for the locomotion-only `MV-G1-01` visual path
  - user evidence on March 10, 2026 now confirms the saved clip `F:\NewEngine\Training\ProtoMotions\output\renderings\phase0_eval_visual-2026-03-10-10-15-07.mp4`, so the `MV-G1-01` recording path itself is no longer in doubt
  - user evidence on March 10, 2026 also judges `MV-G1-01` as `pass`, so `A-01` now moves from `yellow` to `green`
  - the Phase 0 eval command, retargeting validation set, and evidence paths are now frozen for `S1-P0-A2`
  - the current UE scaffold review is stronger now: `PhysAnimUE5.uproject` enables `PoseSearch` and `PhysicsControl`, Manny assets are present under `Content/Characters/Mannequins`, editor logs show `NNERuntimeORT` runtime initialization, and a PIE session launched successfully on March 10, 2026
  - this stronger scaffold evidence reduces setup ambiguity, but it does not by itself satisfy `MV-G1-03` or the substep-stability threshold because those still require user-observed motion behavior
  - user evidence on March 10, 2026 confirms visible left-elbow / left-arm motion after running `PhysAnim.MVG102.Start` in `/Game/ThirdPerson/Lvl_ThirdPerson`
  - the orchestrator narrowed `MV-G1-02` to a stationary proof only, so later movement-induced shoulder artifacts are recorded as out of scope for that checkpoint rather than as a failure of the basic command path
  - `MV-G1-03` now has a frozen UE runtime path on March 10, 2026: `/Game/ThirdPerson/Lvl_ThirdPerson` plus `UPhysAnimMvG103Subsystem` and the `PhysAnim.MVG103.Start` smoke harness for the explicit `isolated left elbow flexion` validation case
  - user evidence on March 10, 2026 now confirms the frozen `120 Hz` synchronous-substep settings (`Tick Physics Async = false`, `Substepping = true`, `Max Substep Delta Time = 0.008333`, `Max Substeps = 4`) remained stable, with no jitter or wobble dominating the run
  - `A-03` now moves from `yellow` to `green` because the Manny smoke-test mapping check completed without leaving an open mapping-failure blocker
  - `A-04` now moves from `yellow` to `green` because the Physics Control command path and Manny response checks both passed
  - `A-05` now moves from `yellow` to `green` because the documented synchronous-substep path stayed controllable on this machine
  - the selected pretrained `motion_tracker/smpl` checkpoint now exports successfully to `F:\NewEngine\Training\output\phc_policy.onnx` through `Training\scripts\export_onnx.py`, with accepted opset `17` and offline `onnxruntime 1.24.3` parity max abs diff `1.64e-7`
  - user evidence on March 10, 2026 now confirms a real UE startup-success line through `NNERuntimeORTDml`, so `A-06` now moves from `yellow` to `green`
  - March 11, 2026 policy-phase stabilization passes then removed the remaining live-policy blow-up:
    - first-policy-frame continuity is now bounded
    - stale explicit targets are cleared when switching into skeletal-animation target mode
    - the bridge quaternion basis conversion now preserves identity local rotations correctly
  - the latest `run-pie-smoke.ps1` evidence shows first-policy-frame raw offsets collapsed from roughly `120-144 deg` to about `0-2 deg`, later full-policy raw offsets stayed around low double digits, and the passive smoke window now also passes at `65` seconds without drift, collapse, delayed fail-stop, or late root instability
- March 11, 2026 movement-smoke work then added a deterministic `WASD`-equivalent PIE harness behind test-only controls
- March 11, 2026 runtime integration then relaxed the earlier smoke-only assumption:
  - `BridgeActive` can now preserve capsule collision and `CharacterMovement` in normal editor/runtime sessions through `physanim.AllowCharacterMovementInBridgeActive`
  - movement smoke still exists for deterministic automation, but it is no longer the only path that keeps the gameplay shell alive
  - the passive smoke path remained green after that harness was added
  - the first valid movement-smoke run initially failed because runtime fail-stop still judged root instability in world space from the activation frame
  - after fixing instability evaluation to use gameplay-shell-relative root/body motion when the shell is preserved:
    - the deterministic `WASD`-equivalent movement smoke now completes without `BridgeActive -> FailStopped`
    - passive smoke remains green
    - the first movement-stability milestone is now considered passed
  - the longer deterministic locomotion soak is now also green
  - manual real-`WASD` now works during `BridgeActive` with no visible immediate problems reported by the user
  - the next Phase 1 risk is no longer “can the bridge move at all?”; it is now fair G2 capture readiness and whether the side-by-side comparison actually demonstrates a noticeable quality win
  - a live side-by-side G2 harness now exists:
    - current player Manny remains `Physics-Driven`
    - one duplicate Manny is spawned as the `Kinematic` baseline
    - the preferred comparison path is now one PIE session with both actors visible at once
  - the preferred G2 judgment path is now the scripted presentation harness, not free-walk comparison:
    - `PhysAnim.G2.StartPresentation` locks camera and inputs
    - both actors run the same deterministic sequence
    - manual side-by-side remains only as a fallback sanity check
  - one remaining manual-runtime startup edge case was then isolated to lower-limb bring-up order rather than policy onset or user timing:
    - the old staged order split each leg across `thigh -> calf -> foot/ball`
    - ProtoMotions locomotion control assumes a coherent hip-knee-ankle-toe chain, so that split was a bad runtime match
    - failing logs showed the first large spike in `calf_r` / `ball_r` while `policyInfluenceAlpha = 0.00`
  - the staged bring-up order is now corrected for lower legs:
    - calves, feet, and balls unlock together before the upper-arm chain
    - after this fix, the passive PIE smoke stays bounded through startup and policy activation with no early lower-body fail-stop
  - the stabilization stress-test assumption is now better constrained:
    - a live runtime ramp can now reduce the three angular stabilization multipliers uniformly from `1.0 -> 0.0`
    - first idle stress-test evidence stayed stable all the way to `multiplier=0.00`
    - therefore the current G2 perturbation readability problem is not best explained by “the stabilization override cannot be relaxed enough”
  - the stress-test question sheet is now effectively answered for both idle and simple deterministic movement:
    - idle remains robust through full uniform relaxation
    - movement sharply narrows the usable relaxation envelope and keeps the lower body as the main bottleneck
    - damping ratio remains the most sensitive single stabilization lever
  - the perturbation hypothesis has now changed again:
    - full temporary relaxation is possible during idle without collapsing the bridge
    - removing the shell shove removes the sideways slide
    - but the remaining body-only contact perturbation still produces only modest body-level motion
  - current best inference:
    - the G2 perturbation limitation is now more about root anchoring / contact coupling than about PD gains being “too strong”
  - new evidence after the stress matrix narrows that further:
    - letting the root simulate during the perturbation does make the push visually obvious
    - but every tested root-unlock variant immediately becomes unstable, even after:
      - gentler pusher settings
      - shorter override windows
      - normal gains instead of relaxed gains
      - temporary policy suspension
  - updated working assumption:
    - the current idle standing push is not a viable final G2 perturbation format under the present bridge contract
    - future perturbation work should pivot to a different scenario, most likely a locomotion-coupled disturbance, rather than keep tuning the same standing push
  - that locomotion-coupled perturbation pivot is now implemented in the live G2 harness:
    - the scripted presentation starts with a short walking perturbation phase
    - both actors follow the same scripted locomotion
    - only the `Physics-Driven` actor receives the extra contact disturbance
    - the shell-level shove remains disabled
  - current evidence on that new path:
    - no fail-stop in the automated G2 presentation run
    - measurable divergence exists between the actors during the perturbation window
    - but the visible quality gap may still be subtle, so `A-08` remains `yellow` until user judgment says the comparison is clearly better
  - March 12, 2026 refinement note:
    - the perturbation stabilization override was found to be incorrectly wired as a no-op in live code
    - that is now corrected with movement-safe multipliers (`0.72 / 0.78 / 0.74`)
    - a lower-body, lead-leg-biased locomotion perturbation profile is also now in place
    - even after those fixes, the perturbation remains stable but only modestly divergent, so the core risk for `A-08` is still readability of the quality win, not runtime failure
  - March 12, 2026 training/runtime alignment note:
    - the first alignment pass is now implemented and verified
    - policy inference and target writes are locked to the pretrained ProtoMotions control cadence (`30 Hz`) instead of free-running at game/render tick rate
    - PhysicsControl and runtime instability checks still run every tick, so the solver path stays stable while policy targets are held between control steps
    - current interpretation:
      - cadence mismatch was real and is now reduced
      - the next alignment work should move to joint-limit and action-range inventory before mass or PD-family retuning
  - March 12, 2026 joint-limit inventory note:
    - the first direct Manny constraint audit is now explicit and repeatable through `PhysAnim.Component.MannyConstraintInventory`
    - `17 / 21` bridge controls map to direct Manny constraint pairs
    - `neck_01`, `head`, `clavicle_l`, and `clavicle_r` do not currently expose direct one-to-one Manny pairs in the audit path
    - the resulting comparison sheet is now saved in:
      - `plans/stage1/40-design/smpl-to-manny-limit-table.md`
    - current alignment risk:
      - Manny's lower body, mid/upper spine, shoulders, and elbows are substantially tighter than the broad ProtoMotions SMPL training ranges
      - that means UE action-range semantics are still narrower and partially indirect relative to training
    - current orchestrator read:
      - cadence alignment is now green enough to continue
      - limit/range mismatch remains a real `yellow` alignment risk until explicit Stage 1 operating limits are chosen
  - March 12, 2026 mass-distribution inventory note:
    - the first Manny mass inventory is now explicit and repeatable through `PhysAnim.Component.MannyMassInventory`
    - the family-level comparison sheet is now saved in:
      - `plans/stage1/40-design/smpl-to-manny-mass-table.md`
    - current alignment risk:
      - Manny is currently torso-heavy and upper-body-heavy relative to the ProtoMotions SMPL asset
      - Manny is currently leg-light relative to the ProtoMotions SMPL asset
      - `spine_01` is effectively massless in the current Manny audit path, so torso mass sits higher in the chain than the training body
    - current orchestrator read:
      - the mass mismatch is now concrete enough to justify a family-level mass policy
      - that work should still be validated mainly against movement and perturbation, because idle already tolerates large stabilization relaxation
  - March 12, 2026 operating-limit policy note:
    - the first explicit Stage 1 operating-limit policy is now saved in:
      - `plans/stage1/40-design/stage1-operating-limit-policy.md`
    - current orchestrator read:
      - hard-limit edits are now deliberately deferred
      - the next alignment pass should change family-level mass distribution before changing broad Manny constraint ranges
  - March 12, 2026 family mass-policy note:
    - that family-level mass distribution pass is now implemented in the bridge runtime
    - the bridge now applies training-aligned Manny family mass scales on activation and restores original scales on teardown
    - current evidence:
      - component, idle smoke, movement smoke, and G2 presentation all remain green
    - current orchestrator read:
      - the family mass policy is now safe enough to keep as the active Stage 1 baseline
      - the next open alignment question is PD-family response fitting, not whether mass alignment is feasible at all
  - March 12, 2026 PD-family response-fit note:
    - the first control-family response-fit sweep under deterministic movement is now complete
    - current evidence:
      - `blend=0.50` is the best measured nonzero fit from the first sweep
      - `blend=0.25` is not the right baseline because it introduces a worse late angular outlier
      - `blend=1.00` is still stable, but it does not outperform `0.50`
    - implication:
      - Stage 1 should keep the training-aligned control-family profile enabled at `0.50` unless a later targeted fit disproves it
  - March 12, 2026 toe-family refinement note:
    - moving `ball_*` onto the locomotion-leg family baseline is safe and mildly beneficial
    - current evidence:
      - toe angular peaks improve slightly
      - root linear peaks improve
      - peak calf linear spikes do not improve
    - implication:
      - the remaining fit problem is now more local than family-wide
      - next fit should target toe-specific damping response before changing broader lower-limb families again
  - March 12, 2026 toe-local fit follow-up:
    - the first two isolated toe-local fits were both worse than the committed toe-family mapping
    - current evidence:
      - toe-only extra damping is not the right next lever
      - toe-only strength increase is also not the right next lever
    - implication:
      - the remaining lower-limb mismatch is probably not solved by simple isolated `ball_*` gain retuning
      - the next alignment pass should move away from toe-only tuning and inspect a different lower-limb mismatch surface
  - March 12, 2026 toe-constraint authoring audit note:
    - the manual `ball_*` constraints now have a dedicated structural audit
    - current evidence:
      - direct constraints exist on both sides
      - left/right limits match
      - left/right frames are symmetric
      - toe axes are normalized and non-degenerate
    - implication:
      - the current problem does not look like a gross manual toe-constraint creation mistake
      - the next pass should focus on permissive toe operating limits or another lower-limb mismatch surface instead of basic authoring correctness
  - March 12, 2026 toe operating-limit policy note:
    - the first runtime toe operating-limit sweep is complete
    - current evidence:
      - a full toe-limit blend is too aggressive
      - a `0.50` toe-limit blend is the first measured operating-limit setting that improves both peak body linear and peak body angular movement spikes versus the committed toe-family baseline
    - implication:
      - the toe limit mismatch is real
      - the next baseline should keep the toe operating-limit policy enabled at `0.50` and then be judged in the normal regression suite
  - March 12, 2026 ankle-constraint audit result:
    - the direct Manny ankle constraints are structurally sane
    - current evidence:
      - direct constraints exist on both sides
      - left/right motions and limit angles match
      - frames are symmetric
      - axes are normalized and non-degenerate
      - angular rotation offsets are zero
    - implication:
      - the next lower-limb problem is not gross ankle authoring
      - the next pass should investigate lower-limb target-range and limit-occupancy behavior under movement
  - March 12, 2026 lower-limb occupancy result:
    - the new runtime diagnostics show repeated lower-limb over-occupancy during locomotion
    - current evidence:
      - first policy-enabled frame starts at `0.00x`
      - once movement starts, `calf_*` repeatedly reaches about `1.7x - 2.6x` against the current `5.0deg` tightest-limit proxy
    - implication:
      - the next lower-limb problem is target-range semantics under movement, not malformed toe or ankle authoring
      - the next runtime fit should start with a calf-family target-range policy before touching broader authoring or hard limits again
  - March 12, 2026 ankle-chain follow-up:
    - the next lower-limb mismatch surface is the ankle chain, not more isolated toe retuning
    - reason:
      - after the verified `0.50` toe-limit baseline, the strongest remaining movement peaks are still anchored in `calf_l(sim)` body linear speed and `ball_l(sim)` body angular speed
    - implication:
      - the next question is whether `foot_* <- calf_*` is structurally sane or merely sensitive under movement
  - March 12, 2026 knee/ankle target-range result:
    - the training-side lower-limb review is now specific:
      - this SMPL asset uses the `3-DoF` symmetric `1.2x` action-range expansion for knee/ankle/toe joints
      - not the `1-DoF` extend-past-limit branch
    - current evidence:
      - a first runtime knee/ankle-chain target-range policy is implemented
      - `calf_*` target scale `0.50`
      - `foot_*` target scale `0.75`
      - deterministic movement smoke still passes with no fail-stop
      - lower-limb occupancy drops materially versus the previous baseline, commonly into the `~0.7x - 1.2x` range
    - implication:
      - target-range mismatch was a real part of the problem
      - the next lower-limb pass should inspect distal coupling / representation, not go back to basic authoring checks
  - March 12, 2026 distal lower-limb target-range follow-up:
    - extending the lower-limb target-range policy into the distal chain is safe:
      - `calf_* = 0.50`
      - `foot_* = 0.50`
      - `ball_* = 0.35`
      - deterministic movement smoke still passes with no fail-stop
    - measured effect:
      - the first forward burst is materially less explosive than the previous knee/ankle-only baseline
      - later backward/strafe bursts still produce large `ball_*` angular spikes
    - implication:
      - scalar distal range shaping helps, but does not finish the lower-limb problem
      - the next lower-limb pass should inspect explicit distal target representation under locomotion
  - March 12, 2026 movement-only distal representation follow-up:
    - added a locomotion-time distal attenuation policy:
      - activates above `50 cm/s` planar owner speed
      - `foot_*` scale `0.75`
      - `ball_*` scale `0.50`
    - current evidence:
      - deterministic movement smoke still passes with no fail-stop
      - early forward movement spikes improve somewhat versus the distal-range baseline
      - backward and strafe phases still keep large distal `ball_*` spikes
    - implication:
      - simple locomotion-time distal attenuation is not enough by itself
      - the next lower-limb pass should move to structural distal target construction, not more scalar attenuation
  - March 12, 2026 distal locomotion target-composition follow-up:
    - current evidence:
      - local UE 5.7 PhysicsControl source confirms explicit targets are applied in the space of the skeletal target transform
      - ProtoMotions PD targets are authored in simulator joint space, not as additive animation offsets
    - first runtime experiment:
      - above `50 cm/s`, force `foot_*` and `ball_*` into explicit-only target mode
      - keep the rest of the body on the current policy-active skeletal-target composition path
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - backward distal spikes improve materially versus the locomotion-time attenuation baseline
      - strafe remains mixed, with some peaks shifting up the lower-limb chain
    - implication:
      - target composition mode is a real part of the remaining lower-limb mismatch
      - distal-only composition switching is not yet sufficient for a final baseline
      - the next lower-limb pass should test whether explicit-only composition needs to expand to the full knee/ankle/toe chain or whether locomotion-transition handling is the deeper remaining issue
  - March 12, 2026 full lower-limb locomotion composition follow-up:
    - widened the same locomotion-time explicit-only target mode to `calf_*`, `foot_*`, and `ball_*`
    - current evidence:
      - deterministic movement smoke still passes with no fail-stop
      - no locomotion-start discontinuity appears
      - forward and backward remain mixed
      - one forward sample worsens materially with a `ball_l` angular spike around `10104 deg/s`
      - late backward still shows large distal spikes, including `ball_r` around `9176 deg/s`
    - implication:
      - lower-limb composition mode remains a real mismatch surface
      - but simply widening explicit-only switching to the full knee/ankle/toe chain is not a clean baseline
      - the next lower-limb pass should target locomotion transition handling or more proximal lower-limb composition, not more distal-set widening by itself
  - March 12, 2026 lower-limb composition transition-policy follow-up:
    - current evidence:
      - local UE 5.7 PhysicsControl source confirms the locomotion-time representation switch is binary
      - ProtoMotions training does not perform an equivalent runtime mode flip
    - runtime experiment:
      - reverted the explicit-only set back to `foot_*` and `ball_*`
      - added stateful speed hysteresis and dwell:
        - enter `50 cm/s`
        - exit `100 cm/s`
        - enter hold `0.20 s`
        - exit hold `0.20 s`
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - no locomotion-start discontinuity appears
      - forward and backward peaks are materially lower than the failed full-chain composition pass
      - remaining spikes often migrate proximally into `thigh_*` or other non-distal bodies
    - implication:
      - abrupt locomotion-time representation switching was part of the remaining lower-limb mismatch
      - the stateful transition policy is now the best measured locomotion-composition baseline
      - the next lower-limb pass should inspect more proximal composition or target-velocity handling, not widen the distal explicit-only set again
  - March 12, 2026 proximal lower-limb composition follow-up:
    - runtime experiment:
      - kept the current hysteresis+dwell baseline
      - added `thigh_*` to the explicit-only locomotion composition set
      - left `calf_*` on the composed path
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - but forward and backward both regress materially
      - repeated `ball_r` / `foot_r` / `calf_r` peaks become much larger than the transition-policy baseline
    - implication:
      - proximal widening is not the right next baseline
      - the code should stay on the transition-policy baseline
      - the next lower-limb pass should move to another locomotion-time mismatch surface, most likely target write timing or another policy-side transition seam
  - March 12, 2026 distal explicit target-velocity follow-up:
    - runtime experiment:
      - kept the committed `foot_*` / `ball_*` hysteresis+dwell baseline
      - zeroed explicit target angular-velocity synthesis only for those distal controls while the mode is active
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - first forward distal spikes drop materially into the low-thousands range
      - lower-limb occupancy stays mostly around `~0.9x - 1.1x`
      - some later peaks still migrate proximally into `calf_*`, `thigh_*`, and occasional `foot_*`
    - implication:
      - synthesized explicit target angular velocity was a real remaining mismatch surface
      - this pass is a keepable improvement to the locomotion-time distal baseline
      - the next lower-limb pass should inspect per-bone target-write smoothing or another proximal locomotion seam, not re-open whole-chain explicit-only switching
  - March 12, 2026 lower-limb composed target-velocity follow-up:
    - runtime experiment:
      - kept the committed `foot_*` / `ball_*` hysteresis+dwell baseline
      - widened locomotion-time angular-velocity suppression to the whole lower-limb chain:
        - `thigh_*`
        - `calf_*`
        - `foot_*`
        - `ball_*`
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - forward is mixed, with some distal improvement but continued large proximal spikes
      - backward regresses, with large `ball_*` spikes still present
      - late idle/strafe also show new `ball_*` outliers
    - implication:
      - broad lower-limb angular-velocity suppression is not a clean new baseline
      - the code should stay on the narrower distal-only suppression baseline
      - the next lower-limb pass should move to per-bone target-write smoothing or another proximal locomotion transition seam
  - March 12, 2026 locomotion-time proximal response-fit follow-up:
    - runtime experiment:
      - kept the current distal locomotion baseline unchanged
      - added a proximal lower-limb response profile only for `thigh_*` / `calf_*`
      - damping ratio scale `1.20`
      - extra damping scale `1.35`
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - forward remains mixed
      - backward stays difficult but does not reopen the old high-spike regime
      - strafe and late idle improve materially relative to the distal-only suppression baseline
    - implication:
      - locomotion-time proximal response fitting is a keepable improvement on top of the current distal baseline
      - the next likely useful experiment is a more selective per-bone proximal profile, not another broad target-semantics change
  - March 12, 2026 locomotion-time thigh de-intensification follow-up:
    - runtime experiment:
      - reduced `thigh_*` to `1.05 / 1.10`
      - kept `calf_*` at `1.20 / 1.35`
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - some strafe samples improve
      - but forward does not materially beat the shared proximal baseline
      - backward and late idle reopen larger outliers
    - implication:
      - selective thigh de-intensification is not a clean new baseline
      - the shared proximal-response profile remains the best measured response-fit baseline so far
  - March 12, 2026 corrected locomotion shell-coupling follow-up:
    - the first shell-coupling alarm was caused by a reference-frame bug in the telemetry:
      - shell offset/velocity deltas were computed from root diagnostics that were already shell-relative
    - after fixing that bug and rerunning deterministic movement smoke:
      - shell planar offset delta is only modest, typically around `~10 - 16 cm` at peak
      - shell/root planar velocity mismatch is only modest, typically around `~2 - 36 cm/s`
      - planar velocity alignment remains near `1.00` through the movement phases
      - movement smoke still passes without fail-stop
    - implication:
      - gameplay-shell coupling is not the dominant remaining locomotion risk
      - it is still worth monitoring, but it should not displace lower-limb alignment as the main direction
  - March 12, 2026 lower-limb target-step policy follow-up:
    - runtime experiment:
      - tightened locomotion-time per-step target caps for `thigh_*`, `calf_*`, `foot_*`, and `ball_*`
      - recorded explicit lower-limb target-step occupancy during the run
    - measured result:
      - deterministic movement smoke still passes with no fail-stop
      - but forward regresses materially and backward still keeps large lower-limb outliers
      - target-step occupancy stays only moderate rather than saturated
    - implication:
      - lower-limb target-step smoothing was worth falsifying, but it is not the dominant remaining locomotion seam
      - the next lower-limb pass should move to another representation seam, not more step-cap tuning
- `Phase 0 critical assumptions`:
  - `A-01`
  - `A-02`
  - `A-03`
  - `A-04`
  - `A-05`
  - `A-06`
  - `A-07`
  - `A-07b`
  - `A-07c`

## Reassessment Triggers

The orchestrator must revisit ledger status when:

- a planning spec is locked
- a gate package is prepared
- user evidence arrives
- a worker reports a blocker
- a fallback path becomes more likely than the primary path

## Downstream Safety Rules

- `green`: downstream tasks may proceed normally
- `yellow`: downstream tasks may proceed only if they do not hard-code the uncertain assumption in irreversible ways
- `red`: downstream tasks that depend on the assumption must stop

## Immediate Orchestrator Duties

Before starting the next execution-planning pass, the orchestrator should:

1. record whether the current commit's review of `bridge-spec.md`, `retargeting-spec.md`, and `test-strategy.md` is complete
2. update each assumption above from `yellow` only if evidence justifies it
3. note which assumptions block the Phase 0 execution package

## First Expected Updates

The next meaningful ledger updates should come from:

- confirming the PHC observation/action contract from the frozen local ProtoMotions sources
- capturing Manny bridge smoke-test evidence for `MV-G1-03`
- defining the exact evidence required to call `UPhysicsControlComponent` viable for Stage 1

## Stage 1 Runtime Alignment Notes

- March 12, 2026:
  - the broader training/runtime alignment direction remains worthwhile
  - lower-limb contact-exclusion alignment was tested and did not become the new dominant lever
  - ProtoMotions-style lower-limb excludes mostly already overlap Manny's current runtime setup
  - the remaining locomotion problem still looks more like lower-limb target semantics / distal coupling than missing contact disables
- March 12, 2026:
  - PhysicsControl startup cache warmup/order was a real UE-side seam
  - the bridge now explicitly prewarms the skeletal pose before the first manual activation-time cache update
  - the unused per-tick `GetCachedBoneTransforms(...)` call was removed
  - fresh deterministic movement smoke dropped startup PhysicsControl warning counts from `130 / 42` to `0 / 0`
  - implication:
    - it is still worth continuing in the broader training/runtime alignment direction
    - but this pass reinforces that the remaining locomotion issue is not generic startup cache invalidity
    - the next useful pass should return to locomotion-time lower-limb target semantics / representation, not more startup warning cleanup
- March 12, 2026:
  - family-weighted lower-limb target-write smoothing was tested as a locomotion-time representation pass
  - it stayed stable, but it was not a clean win
  - strafe regressed materially enough that the runtime code was restored to the previous safe baseline
  - implication:
    - it is still worth continuing in the broader training/runtime alignment direction
    - it is not worth continuing the lower-limb write-smoothing sub-direction as the primary next lever
- March 12, 2026:
  - the bridge self-observation structure matches ProtoMotions `max_coords` much more closely than the recent lower-limb heuristic branches suggested
  - the caller-level `ground_height = 0.0f` assumption was still a real contract violation relative to ProtoMotions' terrain-relative root-height observation
  - the bridge now resolves a real walkable-floor world `Z` and feeds a corrected synthetic ground-height value into the existing self-observation packer
  - implication:
    - it is still worth continuing in the broader training/runtime alignment direction
    - it is increasingly worth prioritizing clear observation/representation contract fixes over additional lower-limb heuristic retuning
## 2026-03-12 - Proto runtime world-frame contract

- Previous assumption:
  - the old SMPL local-joint frame remap was also appropriate for Proto runtime world-space observations and future-target world transforms
- Status:
  - falsified
- Evidence:
  - ProtoMotions runtime world is `z-up`
  - local Proto simulator/common-state conversion reorders quaternions and bodies but does not perform a world-axis remap
  - UE movement smoke stayed stable after splitting runtime world conversion from local action conversion
- Current working assumption:
  - world-space observation/future-target data should use a dedicated Proto-runtime-world conversion path
  - local action/joint rotation conversion should remain on the existing SMPL authoring-frame helpers unless separately disproven
## 2026-03-12 - Future target time-channel semantics

- Previous assumption:
  - emitting the nominal future sample offset was close enough for the `with_time` channel in `mimic_target_poses`
- Status:
  - falsified
- Evidence:
  - active ProtoMotions checkpoint has `mimic_target_pose.with_time = true`
  - local ProtoMotions `mimic_obs.py` clips future motion times before appending the time feature
  - UE bridge was clamping the sampled animation time but still writing the original requested offset
- Current working assumption:
  - the bridge should always emit the effective clamped future time delta for the time channel
  - treat this as a direct target-packing contract requirement, even when the locomotion impact is modest
## 2026-03-12 - Mimic current-reference terrain normalization

- Previous assumption:
  - using raw world-space current body samples as the `mimic_target_poses` current reference was close enough
- Status:
  - falsified
- Evidence:
  - local ProtoMotions `mimic_obs.py` subtracts terrain height from all current body positions before building `mimic_target_poses`
  - the UE bridge was still feeding raw world-space current body samples into `BuildMimicTargetPoses(...)`
  - after switching the current reference to terrain-relative `Z`, component tests and deterministic movement smoke both stayed green
- Current working assumption:
  - the current-state reference used for `mimic_target_poses` should be terrain-relative in `Z`
  - full XY data-origin / respawn-offset alignment is still a separate future seam and should not be invented in Stage 1 without direct evidence
## 2026-03-12 - Mimic target data-origin contract

- Previous assumption:
  - feeding world-origin dependent future target samples into `mimic_target_poses` was close enough
- Status:
  - falsified
- Evidence:
  - local ProtoMotions `mimic_obs.py` builds target poses in the motion-data frame
  - local ProtoMotions normalizes the current reference by subtracting both terrain height and `respawn_offset_relative_to_data`
  - UE PoseSearch sampling with `RootTransformOrigin = SkeletalMesh->GetComponentTransform()` makes future targets world-origin dependent
  - switching future target sampling to identity origin and subtracting a matching XY data-origin proxy from the current reference kept component tests and deterministic movement smoke green
- Current working assumption:
  - `mimic_target_poses` should use data-origin-aligned future targets plus a similarly normalized current reference
  - the current Stage 1 XY offset is a proxy derived from the selected current animation root, not a full replacement for Proto's respawn system

## 2026-03-12 - Terrain observation contract

- Previous assumption:
  - feeding a zeroed terrain tensor was close enough on the flat Stage 1 map
- Status:
  - falsified
- Evidence:
  - active ProtoMotions checkpoint has `terrain: true`
  - active checkpoint expects `terrain_obs_num_samples = 256`
  - local ProtoMotions terrain code builds a yaw-rotated `16 x 16` height grid and encodes `root_height - sampled_ground_height`
  - the UE bridge was still emitting `BuildZeroTerrain(...)`
  - replacing the zero tensor with a real static-world terrain sampler kept component tests and deterministic movement smoke green
- Current working assumption:
  - Stage 1 should emit a real terrain tensor, even on flat ground
  - the next observation/representation passes should assume terrain input is now part of the live runtime contract

## 2026-03-12 - Locomotion alignment observability

- Previous assumption:
  - downstream control-target and instability diagnostics were enough to choose the next locomotion-time alignment pass
- Status:
  - falsified
- Evidence:
  - multiple locomotion-time lower-limb branches produced mixed results without isolating which packed input channel was still most mismatched
  - the bridge trace writer existed but did not summarize the actual policy inputs per frame
  - adding per-frame summaries for `self_obs`, `mimic_target_poses`, `terrain`, movement smoke phase, and distal composition mode kept build/tests/movement smoke green
- Current working assumption:
  - the next locomotion-time alignment decision should be chosen from trace summaries of the packed inputs, not only from downstream lower-limb spike metrics
  - broader training/runtime alignment remains worth continuing, but the next primary lever should be evidence from the trace rather than another guessed lower-limb heuristic

## 2026-03-12 - Trace row cadence

- Previous assumption:
  - writing trace rows every bridge tick was close enough, even though the packed-input summaries only updated on policy steps
- Status:
  - falsified
- Evidence:
  - the first movement trace contained thousands of blank-phase rows and only a minority of rows with meaningful packed-input summaries
  - the trace design and implementation docs both define `frames.csv` as one row per sampled bridge policy step
  - after switching frame emission to policy-step cadence, the latest movement trace had `0` blank phases and `0` non-policy rows
- Current working assumption:
  - `frames.csv` should be treated as a policy-step artifact, with sparse events carrying startup/shutdown/state-transition information
  - the next locomotion-time alignment pass should be chosen from this cleaned trace, not from mixed tick-rate rows
## 2026-03-12 - Distal composition mode is still net-positive

- Assumption tested:
  - after the recent alignment fixes, the explicit-only distal locomotion composition heuristic might have become unnecessary and could now be hurting more than helping
- Result:
  - false
- Evidence:
  - disabling the policy removed all `distal_locomotion_composition_mode_active=true` trace rows
  - but movement-trace maxima regressed across all major locomotion phases
- Current assumption:
  - distal composition mode is still net-positive in the current runtime baseline
  - the next seam should be how the mode behaves while active or during transitions, not simple removal
## 2026-03-12 - CSV trace and Insights are complementary

- Assumption:
  - the bridge CSV trace is enough by itself to understand where runtime spikes sit relative to animation, physics, and frame timing
- Result:
  - false
- Updated assumption:
  - the CSV trace explains bridge-local inputs and decisions
  - Unreal Insights explains where those bridge phases sit relative to game thread, animation, physics, and render timing
  - both are required for the next locomotion-time investigations
## 2026-03-12 - Distal composition should be sticky under live intent

- Assumption tested:
  - distal composition mode should not be allowed to exit purely on a transient speed dip while locomotion intent is still active
- Result:
  - supported
- Evidence:
  - intent-aware latching materially reduced locomotion-phase maxima and late settling noise
  - but it did not fully eliminate within-phase deactivations
- Updated assumption:
  - distal composition mode needs stickier transition handling than a pure speed hysteresis
  - the next remaining seam is not simple on/off removal, but why mode still drops out in a few live locomotion windows
## 2026-03-12 - Distal composition needs a short recent-intent grace window

- Assumption tested:
  - the remaining within-phase distal-composition dropouts are partly caused by brief gaps between active locomotion intent and the current speed/acceleration surfaces
- Result:
  - supported
- Evidence:
  - after adding a `0.20s` recent-intent grace window, within-phase flips dropped from `6 -> 4`
  - `Forward`, `StrafeLeft`, `StrafeRight`, and `Complete` all improved on max angular speed
  - `Backward` max regressed, but remained stable and the phase p95 stayed acceptable
- Updated assumption:
  - the distal composition policy should stay sticky for a short recent-intent window
  - the remaining issue is narrower than before and should be investigated from the cleaned movement trace, not by reopening broad lower-limb heuristics
## 2026-03-12 - Distal composition should not bypass enter hold purely on live intent

- Assumption tested:
  - the remaining phase-start false rows are mostly caused by the normal enter hold, so live locomotion intent should be allowed to bypass that hold
- Result:
  - falsified
- Evidence:
  - an intent-aware zero-second enter hold removed most phase-start false rows
  - but it materially worsened active locomotion maxima across every major movement phase
- Updated assumption:
  - the remaining seam is not just delayed activation
  - the distal composition policy still needs restraint on entry
  - the next useful pass should target a narrower transition or representation mismatch, not more aggressive composition-mode activation
