# Stage 1 Execution Log

## Purpose

This file is the orchestrator-owned live task-state board for Stage 1.

Use it to track:

- what is active
- what is blocked
- what is waiting on the user
- what frozen inputs are in effect
- what handoffs were accepted

## Current State

- `Current phase`: Phase 1 / `S1-P1-A1` accepted / `S1-P1-A2` stabilization passes completed for the current smoke target
- `Overall status`: UE install, project scaffold, ProtoMotions checkout, pretrained checkpoint, Python `3.11` environment, and the Isaac Sim / Isaac Lab runtime are confirmed locally; Gate G1 is explicitly `pass`; the selected Phase 1 runtime model is the pretrained `motion_tracker/smpl` checkpoint; the full UE startup path succeeds through `NNERuntimeORTDml`; the one-character bridge now completes a `65` second passive idle PIE smoke window without catastrophic post-startup instability, drift, collapse, or delayed fail-stop; deterministic movement smoke and longer locomotion soak are both green; preserved-gameplay-shell manual `WASD` also works in `BridgeActive`; the next active Phase 1 task is G2 comparison packaging, not more blind stabilization
- `Last planning milestone`: the frozen policy-phase stabilization plan converged on March 11, 2026 after continuity fixes, representation-switch cleanup, and the corrected SMPL->UE quaternion basis conversion removed the remaining live-policy blow-up; the passive idle validation window then extended from `30s` to `65s` and stayed green; movement-stability then passed through deterministic smoke, longer locomotion soak, and gameplay-shell-relative fail-stop evaluation; on March 12, 2026 the first training/runtime alignment pass then locked policy updates to the pretrained ProtoMotions control cadence (`30 Hz`) while leaving Chaos/PhysicsControl on their existing per-tick update path; the follow-up direct Manny constraint inventory is now frozen in [smpl-to-manny-limit-table.md](/F:/NewEngine/plans/stage1/40-design/smpl-to-manny-limit-table.md); the next frozen alignment milestone is operating-limit and mass-distribution policy, not ad hoc mass/gain copying

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as of commit `0a9bf13` | `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | `plans/stage1/10-specs/environment-spec.md`, `plans/stage1/60-user/user-interventions.md`, `plans/stage1/60-user/user-return-template.md` | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | `plans/stage1/50-content/ue-project-scaffold.md`, `plans/stage1/60-user/user-interventions.md`, `plans/stage1/60-user/user-return-template.md` | UE editor setup | none |
| S1-P0-A1 | AI | completed | `plans/stage1/40-tasks/task-packet-s1-p0-a1.md` plus frozen Phase 0 inputs | `plans/stage1/20-execution/phase0-execution-package.md`, `plans/stage1/10-specs/bridge-spec.md`, `plans/stage1/10-specs/retargeting-spec.md`, `plans/stage1/20-execution/assumption-ledger.md`, `plans/stage1/20-execution/execution-log.md` | none |
| S1-P0-A2 | AI + User | completed | `plans/stage1/20-execution/phase0-execution-package.md`, `plans/stage1/60-user/manual-verification.md`, `plans/stage1/10-specs/acceptance-thresholds.md`, `plans/stage1/30-evidence/g1-evidence.md` | `plans/stage1/30-evidence/g1-evidence.md`, `plans/stage1/20-execution/assumption-ledger.md`, `plans/stage1/20-execution/execution-log.md` | none |
| S1-P1-A1 | AI | completed | `plans/stage1/40-tasks/task-packet-s1-p1-a1.md`, `plans/stage1/20-execution/phase1-implementation-package.md`, `plans/stage1/10-specs/onnx-export-spec.md`, `plans/stage1/10-specs/ue-bridge-implementation-spec.md` | `Training/scripts/export_onnx.py`, `Training/physanim/export_onnx.py`, `Training/tests/test_onnx_export.py`, `plans/stage1/20-execution/phase1-implementation-package.md`, `plans/stage1/10-specs/dependency-lock.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P1-A2 | AI | in_progress | accepted `S1-P1-A1` handoff, `phase1-implementation-package.md`, `manual-verification.md`, `acceptance-thresholds.md`, `phase1-ue-bridge-bringup-runbook.md` | `plans/stage1/40-tasks/task-packet-s1-p1-a2.md`, `plans/stage1/60-user/manual-verification.md`, `plans/stage1/10-specs/acceptance-thresholds.md`, `plans/stage1/30-evidence/g2-evaluation.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none; current work is G2 comparison packaging and user-side judgment setup |

## Frozen Inputs For Phase 0 Preparation

- `Freeze point`: commit `0a9bf13`
- `Frozen docs`:
  - `AGENTS.md`
  - `ENGINEERING_PLAN.md`
  - `plans/stage1/10-specs/bridge-spec.md`
  - `plans/stage1/10-specs/retargeting-spec.md`
  - `plans/stage1/10-specs/test-strategy.md`
  - `plans/stage1/60-user/manual-verification.md`
  - `plans/stage1/20-execution/assumption-ledger.md`
  - `plans/stage1/10-specs/acceptance-thresholds.md`
  - `plans/stage1/50-content/pretrained-model-selection.md`
  - `plans/stage1/50-content/pretrained-checkpoint-retrieval.md`
  - `plans/stage1/10-specs/environment-spec.md`
  - `plans/stage1/50-content/motion-set.md`
  - `plans/stage1/50-content/motion-source-map.md`
  - `plans/stage1/50-content/motion-source-lock-table.md`
  - `plans/stage1/50-content/ue-project-scaffold.md`
  - `plans/stage1/20-execution/phase0-execution-package.md`
- `Unfreeze rule`: only unfreeze if the user returns setup evidence that changes a planned value or if an assumption moves materially in the ledger

## Next Runnable Tasks

| Priority | Task ID | Why Runnable / Not Runnable Yet |
|---|---|---|
| 1 | S1-P1-A2 | runnable now; runtime stability and first movement milestones are green, so the next safe task is G2 side-by-side packaging and evidence capture |
| 2 | G2 evidence capture | runnable once the frozen comparison sequence and live side-by-side harness are accepted |
| 3 | S1-P2-A1 | not runnable until G2 is explicitly passed |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| none | no Phase 0 evidence remains outstanding |

## Latest Phase 0 Evidence Progress

- the selected local `motion_tracker/smpl` checkpoint contract is now written down in `bridge-spec.md`
- G1 Criterion 2 now has concrete evidence in `g1-evidence.md` and scores `pass`
- the motion-source review in `g1-evidence.md` now scores `pass`
- the UE scaffold is now verified more concretely: `PhysAnimUE5.uproject` lists `PoseSearch` and `PhysicsControl`, Manny content exists under `Content/Characters/Mannequins`, editor logs show `NNERuntimeORT` runtime availability, and PIE launched successfully
- March 10, 2026 IsaacLab debug work identified one real launch hazard and several compatibility mismatches in the local ProtoMotions path: overlay/capture hooks could crash Vulkan startup, `h5py` had to be imported before Isaac Lab app launch on this setup, the local Isaac Lab package requires `Se2KeyboardCfg`, and the installed MoviePy package uses the new root-level `ImageSequenceClip` export instead of `moviepy.editor`
- the current visual eval path now renders and records locally with those compatibility fixes in place; noisy RTX sensor DLL errors still appear in the console, but they are not currently blocking `MV-G1-01`
- user evidence now confirms `F:\NewEngine\Training\ProtoMotions\output\renderings\phase0_eval_visual-2026-03-10-10-15-07.mp4` was saved successfully for `MV-G1-01`
- user evidence now also confirms a `pass` verdict for `MV-G1-01`
- `MV-G1-02` is now frozen to `/Game/ThirdPerson/Lvl_ThirdPerson` plus the plugin runtime harness command `PhysAnim.MVG102.Start`
- Visual Studio Build Tools 2022 with MSVC v143 and Windows SDK `22621` were installed on March 10, 2026, and Unreal built `PhysAnimPlugin` successfully on this machine
- the UE `MV-G1-02` harness was debugged through missing visible motion, a Live Coding linker failure, and one Live Coding class-reload crash; a normal closed-editor rebuild then succeeded and user evidence on March 10, 2026 confirmed visible left-elbow movement in the frozen test path
- later March 10, 2026 discussion narrowed `MV-G1-02` to an explicitly stationary proof; movement-induced shoulder artifacts were judged out of scope for this checkpoint and deferred to later integrated UE checks
- the user explicitly accepted screenshot-only evidence for `MV-G1-02`, so that checkpoint now scores `pass`
- `MV-G1-03` is now frozen to `/Game/ThirdPerson/Lvl_ThirdPerson` plus the dedicated mapped-joint smoke harness command `PhysAnim.MVG103.Start`, using the explicit validation case `isolated left elbow flexion`
- the user later confirmed `MV-G1-03` completed; the Manny smoke-test mapping check is now accepted as `pass`, with no remaining open blocker at the isolated left-elbow smoke-harness stage
- later March 10, 2026 Phase 1 bring-up work reached the UE model-loading gate and confirmed the next blocker is not Unreal wiring but missing export output: `F:\NewEngine\Training\output\phc_policy.onnx` does not exist, `Training\scripts\export_onnx.py` does not exist, the ONNX validation test is still a skip-if-placeholder, and no accepted ONNX export handoff exists yet
- the selected pretrained checkpoint still exists at `F:\NewEngine\Training\ProtoMotions\data\pretrained_models\motion_tracker\smpl\last.ckpt`, so the missing ONNX file is an orchestration/process gap rather than a missing model-source artifact
- user evidence on March 10, 2026 now confirms the frozen `120 Hz` synchronous-substep configuration (`Tick Physics Async = false`, `Substepping = true`, `Max Substep Delta Time = 0.008333`, `Max Substeps = 4`) stayed controllable without jitter or wobble dominating the run
- Gate G1 now passes; Phase 0 evidence capture is complete and Phase 1 is unblocked
- `Training\scripts\export_onnx.py` now exists and exports the selected pretrained `motion_tracker/smpl` actor path through the locked three-input Stage 1 contract
- `Training\tests\test_onnx_export.py` now validates the real input/output names and numeric parity requirement instead of skipping behind a placeholder contract
- March 10, 2026 worker validation exported `F:\NewEngine\Training\output\phc_policy.onnx` with accepted opset `17` and `onnxruntime 1.24.3` parity max abs diff `1.64e-7`
- the exported ONNX was copied to `F:\NewEngine\PhysAnimUE5\Content\NNEModels\phc_policy.onnx`, so the next runtime step is UE import / NNE model-creation validation rather than more offline export work
- user evidence on March 10, 2026 now confirms the startup-success line:
  - `[PhysAnim] Startup success. Runtime=NNERuntimeORTDml Model=/Game/NNEModels/phc_policy.phc_policy`
- the active Phase 1 blocker has changed:
  - model loading is proven
  - the current failure mode is uncontrolled post-startup flight / spinning
  - the next required planning artifact is a stabilization/tuning package, not a G2 comparison handoff
  - the current implementation now includes objective instability monitoring on the root body, so the next stabilization loop can use fail-stop metrics and runtime diagnostics instead of relying only on visual judgment

## Accepted Handoffs

| Task ID | Artifact | Accepted? | Notes |
|---|---|---|---|
| S1-PLAN-01 | Stage 1 planning bundle | yes | foundational planning artifacts in place |
| S1-PLAN-02 | `bridge-spec.md` | yes | planning-level contract defined and now updated with the selected local runtime contract |
| S1-PLAN-03 | `retargeting-spec.md` | yes | planning-level mapping defined, awaits runtime validation |
| S1-PLAN-04 | `test-strategy.md` | yes | verification split defined |
| S1-PLAN-05 | environment / pretrained / scaffold / threshold bundle | yes | execution-planning gaps materially reduced |
| S1-PLAN-06 | task packets / lock sheets / user return path | yes | re-entry into Phase 0 is now operationally defined |
| S1-PLAN-07 | retrieval / export / comparison lock bundle | yes | remaining Phase 0-1 planning gaps materially reduced |
| S1-P0-A1 | Phase 0 machine-specific execution package | yes | Windows-native Isaac Sim / Isaac Lab path is frozen and Phase 0 can advance without more setup replanning |
| S1-P1-A1 | single-character implementation package freeze + ONNX export path | yes | startup now succeeds through `NNERuntimeORTDml`, so export/import discovery is no longer on the critical path |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| G2 | readying | runtime stability blockers are cleared for the current scope; remaining work is fair comparison packaging and user judgment |
| S1-P2-A1 | blocked | depends on G2 pass |
| S1-P2-A2 | blocked | depends on Phase 2 result |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks
- March 11, 2026:
  - normal manual runtime now preserves capsule collision and `CharacterMovement` during `BridgeActive` through `physanim.AllowCharacterMovementInBridgeActive = 1`
  - the deterministic movement smoke harness remains valid, but gameplay-shell preservation is no longer smoke-only
  - manual/runtime visibility now also includes an always-visible on-screen bridge state indicator controlled by `physanim.ShowBridgeStatusIndicator`
  - movement-triggered fail-stop false positives were traced to world-space root instability checks after gameplay-shell preservation
  - fix: when the gameplay shell is preserved, runtime instability now evaluates root/body translation relative to the owning actor shell instead of the original world-space activation frame
  - verification: `run-pie-movement-smoke.ps1` now completes without `BridgeActive -> FailStopped`
  - first movement-stability milestone is now treated as `pass`
  - next validation pass is a longer deterministic locomotion soak over repeated scripted movement cycles plus short manual real-`WASD` confirmation
  - the longer deterministic locomotion soak is now also green
  - manual real-`WASD` in `BridgeActive` now works with preserved `CharacterMovement`
  - a live side-by-side G2 harness now exists through `PhysAnim.G2.StartSideBySide` / `PhysAnim.G2.StopSideBySide`
  - the preferred G2 format is now one PIE session with a `Physics-Driven` Manny and a spawned `Kinematic` Manny, not two separate recordings unless the live harness is unavailable
  - a scripted G2 presentation harness now exists through `PhysAnim.G2.StartPresentation`
    - it freezes player move/look input
    - it drives both actors through the same short sequence
    - it uses a fixed tracking comparison camera
  - manual side-by-side remains available as a weaker fallback through `PhysAnim.G2.StartSideBySide`
  - a later March 11, 2026 startup-stability pass found one remaining early lower-body edge case in manual runtime:
    - staged bring-up was promoting thighs first, calves second, and feet/balls third
    - that created a mixed simulated/kinematic lower-limb chain during startup
    - the failing log showed the first bad spike in `calf_r` and `ball_r` before policy influence mattered
  - fix:
    - staged bring-up now unlocks calves, feet, and balls together as one lower-leg group
    - the arm chain still remains separate from the final hand-only group
  - verification after that lower-leg staging fix:
    - `PhysAnim.Component` passed
    - `run-pie-smoke.ps1` passed
    - the fresh smoke log no longer shows the old early `calf_r/ball_r` blow-up at group `2/5`
    - startup, policy activation, and the rest of the smoke window stayed bounded without `Fail-stop`
  - a CVar-driven stabilization stress-test now exists in the live bridge runtime:
    - `pa.StabilizationStressTest 1`
    - `pa.StabilizationStressTestRampSeconds 45`
    - it linearly ramps the three angular stabilization multipliers from `1.0 -> 0.0` after the bridge is fully settled
    - the current runtime diagnostics now log `stressTest[enabled active multiplier elapsed]`
    - the harness now also supports:
      - recovery profile (`pa.StabilizationStressTestProfile 1`)
      - target floor / hold / ramp-up controls
      - per-parameter sweep mode
      - first spike / first instability markers
      - local pose drift metrics for spine, head, and feet
  - first stress-test result:
    - `PhysAnim.PIE.Smoke` stayed stable through the full idle ramp down to `multiplier=0.00`
    - no `Fail-stop` occurred during that run
    - conclusion: the current G2 perturbation problem is not simply “stabilization still too strong to relax”
  - extended stress-test matrix results:
    - idle answers are now concrete enough to answer the full question sheet:
      - lower body remains the first weak link
      - idle never collapses, even at `multiplier=0.00`
      - recovery succeeds
      - damping ratio is the most critical single lever
    - movement materially changes those answers:
      - first instability in the uniform movement sweep now appears around `multiplier=0.44`
      - first angular spike shifts to `ball_r`
      - first linear spike shifts to `foot_l`
      - local foot drift grows into the `80-90 cm` range by the end of the movement sweeps
      - the `ActionScale 0.0` proxy is not enough to claim “PD alone is safe” under locomotion
  - perturbation debugging resumed after the stress matrix:
    - shell-level shove was removed because it only produced sideways actor sliding
    - the presentation harness now uses body-level contact push plus a temporary full low-gain perturbation override
    - latest automated G2 evidence shows:
      - no actor-shell slide (`actorDelta = 0.0 cm`)
      - modest articulated response (`localHead ~= 2.3 cm`, `localFoot ~= 3.0-4.5 cm`)
    - conclusion:
      - the remaining perturbation readability problem is not “insufficient stabilization relaxation”
      - it is now most likely rooted in the kinematic root / gameplay-shell anchoring and how contact couples into the articulated chain
  - follow-up perturbation experiments then tested the opposite extreme:
    - temporarily allowing the root body modifier to simulate during the G2 perturbation window
    - keeping the contact push body-only
    - trying:
      - low gains
      - normal gains
      - gentler pusher parameters
      - shorter root-unlock windows
      - temporary policy suspension during the shove
  - all of those root-unlock variants failed in the same direction:
    - the perturbation became clearly visible
    - but the bridge immediately crossed fail-stop thresholds
    - the shortest / gentlest root-unlock runs still failed in about `0.25-0.35s`
  - current conclusion:
    - the standing external-push perturbation is now well-explored under the current Stage 1 bridge contract
    - root-kine path is too subtle
    - root-sim path is unstable
    - the next perturbation attempt should change scenario, not just retune the same idle push again
  - perturbation work then pivoted to that next scenario:
    - the G2 presentation now starts with a locomotion-coupled perturbation instead of a standing shove
    - both actors begin the same short scripted walk
    - only the `Physics-Driven` actor receives the extra contact disturbance
    - the kinematic comparison actor stays on the same scripted locomotion path without that extra contact
    - the presentation shell-shove path remains disabled
  - latest locomotion-coupled perturbation result:
    - the presentation stays stable with no `Fail-stop`
    - the perturbation now produces measurable divergence during the walk instead of the old standing twitch-only result
    - current limitation: the difference is still likely subtle to the eye, so this improves the G2 setup but does not yet guarantee a clear `pass`
  - a follow-up March 12, 2026 refinement pass then fixed one real implementation gap in that locomotion-coupled path:
    - the presentation perturbation stabilization override had accidentally been left as a no-op (`1.0 / 1.0 / 1.0`)
    - it now applies real movement-safe angular relaxation during the perturbation window
    - current frozen override multipliers:
      - strength `0.72`
      - damping ratio `0.78`
      - extra damping `0.74`
  - the same pass also tightened the perturbation profile toward the lower body:
    - slower initial perturbation walk
    - lower / narrower pusher volume
    - lead-leg-biased pusher placement
  - result after those refinements:
    - automated G2 presentation still stays stable with no `Fail-stop`
    - but the measured source-vs-kinematic local gap remains modest
    - current best reading is that the locomotion-coupled scenario is now correctly implemented and safer, but still presentation-limited rather than obviously persuasive
  - March 12, 2026 alignment follow-up:
    - `PhysAnim.Component.MannyConstraintInventory` now audits every Stage 1 bridge control against the Manny physics asset
    - `17 / 21` bridge controls have direct Manny constraint pairs
    - the four missing direct Manny pairs are:
      - `neck_01 <- spine_03`
      - `head <- neck_01`
      - `clavicle_l <- spine_03`
      - `clavicle_r <- spine_03`
    - the explicit SMPL-vs-Manny inventory is now recorded in:
      - `plans/stage1/40-design/smpl-to-manny-limit-table.md`
    - current high-confidence mismatch:
      - Manny lower-body joints, mid/upper spine, shoulders, and elbows are materially tighter than the broad SMPL training-side target ranges
    - implication:
      - current UE action-range semantics are narrower and less transparent than training
      - next alignment work should define deliberate Stage 1 operating limits and then move to mass-distribution auditing, not blindly widen constraints
  - March 12, 2026 mass-distribution audit note:
    - `PhysAnim.Component.MannyMassInventory` now records the current Manny per-body mass distribution from the physics asset path
    - the explicit family-level comparison is now saved in:
      - `plans/stage1/40-design/smpl-to-manny-mass-table.md`
    - current high-confidence mismatch:
      - Manny is torso-heavy and upper-body-heavy relative to the ProtoMotions SMPL training asset
      - Manny is leg-light relative to the ProtoMotions SMPL training asset
      - `spine_01` is effectively massless in the current inventory path, so torso mass is concentrated higher in the chain than training
    - implication:
      - the next alignment pass should define a family-level Stage 1 mass-adjustment policy together with operating limits
      - mass alignment should be judged primarily against movement and perturbation behavior, not just idle
  - March 12, 2026 operating-limit policy note:
    - the first explicit Stage 1 operating-limit policy is now saved in:
      - `plans/stage1/40-design/stage1-operating-limit-policy.md`
    - frozen decision:
      - do not broaden Manny hard limits blindly
      - keep current Manny constraints as the Stage 1 hard safety envelope
      - move next to family-level mass adjustment before any broad limit retuning
  - March 12, 2026 family mass-policy implementation note:
    - the first family-level Manny mass-adjustment policy is now implemented in the live bridge runtime
    - current policy:
      - pelvis scaled down
      - full leg chains scaled up
      - spine, neck/head, and shoulder/arm/hand families scaled down
    - the bridge now applies those training-aligned family mass scales on bridge activation and restores original body mass scales on bridge teardown
    - verification:
      - `PhysAnim.Component` passed
      - `PhysAnim.PIE.Smoke` passed
      - `PhysAnim.PIE.MovementSmoke` passed
      - `PhysAnim.PIE.G2Presentation` passed
    - current runtime read:
      - the mass-alignment pass is mechanically correct and regression-safe for the current smoke scope
      - the next alignment decision is whether to keep this family policy as the Stage 1 baseline and move to PD-family response fitting
  - March 12, 2026 PD-family fitting note:
    - the first training-aligned control-family response fit is now enabled as the Stage 1 baseline
    - first measured movement-smoke sweep:
      - `blend=0.00`
      - `blend=0.25`
      - `blend=0.50`
      - `blend=1.00`
    - current measured result:
      - all four blends passed `PhysAnim.PIE.MovementSmoke`
      - `blend=0.50` produced the best nonzero movement fit in the first sweep:
        - lower forward peak body linear speed than `0.00`, `0.25`, and `1.00`
        - lower forward peak body angular speed than `0.00`, `0.25`, and `1.00`
      - `blend=0.25` introduced a worse late angular outlier than the other tested blends
    - frozen baseline:
      - keep `bApplyTrainingAlignedControlFamilyProfile = true`
      - keep `TrainingAlignedControlFamilyProfileBlend = 0.50`
  - March 12, 2026 toe-family refinement note:
    - `ball_l` and `ball_r` remain the most frequent peak angular offenders during deterministic movement
    - first targeted fit:
      - move `ball_*` out of the weaker mid-tier family and onto the locomotion leg family baseline
    - measured result versus the previous `0.50` default profile:
      - still no fail-stop
      - lower root linear peak during movement
      - slightly lower toe angular peak
      - slight increase in peak lower-leg linear spike
    - current runtime read:
      - this is a modest improvement, not a decisive fix
      - the next fit should be more targeted, likely toe-specific extra damping rather than another broad family reassignment
  - March 12, 2026 toe-local follow-up note:
    - tested toe-specific follow-ups after the toe-family reassignment:
      - raise `ball_*` extra damping above the leg baseline
      - raise `ball_*` strength slightly above the leg baseline
    - measured result:
      - both variants remained stable
      - both variants were worse than the committed toe-family mapping baseline
      - toe-only extra damping increased both root and toe peaks
      - toe-only strength increase was the worst result, with a large backward toe angular spike
    - current runtime read:
      - the committed toe-family reassignment remains the best measured toe-focused fit so far
      - the next pass should stop retuning isolated toe gains and instead inspect a different mismatch surface
  - March 12, 2026 toe-constraint authoring audit note:
    - added `PhysAnim.Component.MannyToeConstraintAuthoring`
    - current audit result:
      - `ball_l <- foot_l` and `ball_r <- foot_r` both exist as direct Manny constraints
      - left/right toe motions and limit angles match exactly
      - left/right reference frames are symmetric by magnitude
      - toe axes are normalized and non-degenerate
      - angular rotation offsets are zero on both sides
    - current runtime read:
      - there is no gross sign that the manually created `ball_*` constraints were authored incorrectly
      - the more plausible remaining issue is that the toe constraints are permissive and sensitive, not malformed
      - the next lower-limb pass should inspect toe operating-limit policy or another non-gain mismatch surface, not keep assuming bad manual authoring
  - March 12, 2026 toe operating-limit policy note:
    - added a temporary runtime toe operating-limit policy for:
      - `ball_l <- foot_l`
      - `ball_r <- foot_r`
    - measured sweep:
      - `blend=1.00`:
        - lower peak lower-leg linear speed
        - worse toe angular spike
        - worse root linear peak
      - `blend=0.50`:
        - lower peak body linear speed than the committed toe-family baseline
        - lower peak body angular speed than the committed toe-family baseline
        - slightly higher root linear peak than the committed toe-family baseline, but much better than `blend=1.00`
    - frozen baseline candidate:
      - keep `bApplyTrainingAlignedToeLimitPolicy = true`
      - keep `TrainingAlignedToeLimitPolicyBlend = 0.50`
  - March 12, 2026 ankle-constraint authoring audit plan:
    - next lower-limb pass moves one level up the chain:
      - `foot_l <- calf_l`
      - `foot_r <- calf_r`
    - reason:
      - after the verified `0.50` toe-limit baseline, deterministic movement still peaks at `calf_l(sim)` for body linear speed and `ball_l(sim)` for body angular speed
    - immediate goal:
      - prove whether the direct Manny ankle constraints are malformed or merely sensitive before making any broader lower-limb operating-limit change
  - March 12, 2026 ankle-constraint authoring audit result:
    - added `PhysAnim.Component.MannyAnkleConstraintAuthoring`
    - current audit result:
      - `foot_l <- calf_l` and `foot_r <- calf_r` both exist as direct Manny constraints
      - left/right ankle motions and limit angles match exactly
      - left/right reference frames are symmetric by magnitude
      - ankle axes are normalized and non-degenerate
      - angular rotation offsets are zero on both sides
    - current runtime read:
      - there is no gross sign that the ankle chain was authored incorrectly
      - the next lower-limb mismatch surface is no longer manual ankle authoring
      - the next pass should inspect lower-limb target-range / limit-occupancy behavior under movement
  - March 12, 2026 lower-limb limit-occupancy instrumentation result:
    - runtime diagnostics now log:
      - `lowerLimbLimitOccupancy=<bone>:<ratio>x proxy=<deg>`
    - measured result:
      - first policy-enabled frame starts at `0.00x`
      - once movement begins, `calf_l` / `calf_r` repeatedly climb above `1.0x`
      - common measured range is about `1.7x - 2.6x`
      - the proxy is consistently `5.0deg`, matching Manny's tightest direct calf/knee limited axis
    - current runtime read:
      - the remaining lower-limb mismatch is target-range / limit-occupancy, not malformed toe or ankle authoring
      - the next pass should test a temporary lower-limb target-range policy, starting with `calf_*`
  - March 12, 2026 lower-limb target-range policy note:
    - reviewed ProtoMotions lower-limb action mapping again:
      - the relevant SMPL lower-limb path is the `3-DoF` symmetric `1.2x` expansion
      - not the `1-DoF` extend-past-limit path
    - implemented the first runtime target-range policy on the knee/ankle chain:
      - `calf_*` target scale `0.50`
      - `foot_*` target scale `0.75`
      - `ball_*` unchanged in this pass
    - measured result in deterministic movement smoke:
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
      - lower-limb occupancy falls materially from the old `~1.7x - 2.6x` common range to roughly `~0.7x - 1.2x` for most active samples
      - first forward movement samples now sit near `~0.95x - 1.03x`
    - current runtime read:
      - lower-limb target-range mismatch is confirmed as real
      - this knee/ankle-chain target-range policy is the new baseline candidate
      - remaining distal foot/toe spikes mean the next pass should inspect lower-limb representation / distal coupling, not more authoring tweaks
  - March 12, 2026 distal lower-limb target-range note:
    - extended the same training-aligned lower-limb target-range policy into the distal chain:
      - `calf_* = 0.50`
      - `foot_* = 0.50`
      - `ball_* = 0.35`
    - deterministic movement smoke stayed green:
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - first forward movement spikes improve materially versus the previous knee/ankle-only baseline
      - but later backward/strafe phases still produce large `ball_*` angular spikes
      - some bursts shift the peak angular offender up the chain instead of removing the distal problem
    - current runtime read:
      - scalar distal range shaping helps, but does not finish the lower-limb problem
      - the next pass should inspect explicit distal target representation under locomotion
  - March 12, 2026 movement-only distal representation note:
    - implemented the first locomotion-time distal representation policy:
      - activates above `50 cm/s` planar owner speed
      - `foot_*` locomotion representation scale `0.75`
      - `ball_*` locomotion representation scale `0.50`
    - deterministic movement smoke stayed green:
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - early forward movement spikes improve somewhat relative to the distal-range baseline
      - backward and strafe phases still produce large distal `ball_*` spikes
    - current runtime read:
      - this locomotion-time distal attenuation is not a clean new baseline
      - the next pass should move to more structural distal target construction under locomotion, not another scalar attenuation
  - March 12, 2026 distal locomotion target-composition note:
    - reviewed the next lower-limb mismatch surface against:
      - official `UPhysicsControlComponent` / `FPhysicsControlData` docs
      - local UE 5.7 PhysicsControl source showing explicit targets are applied in the space of the skeletal target transform
      - ProtoMotions code confirming PD targets are authored in simulator joint space
    - first runtime experiment:
      - above `50 cm/s`, force `foot_*` and `ball_*` controls into explicit-only target mode
      - leave the rest of the body on the current policy-active skeletal-target composition path
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - backward distal spikes improve materially versus the locomotion-time attenuation baseline
      - strafe remains mixed, with some peaks shifting up into `foot_*`, `calf_*`, or `thigh_*`
    - current runtime read:
      - target composition mode is now confirmed as a real lower-limb mismatch surface
      - distal-only composition switching is promising, but not yet a clean final baseline
      - the next pass should test whether the explicit-only composition policy must expand to the full knee/ankle/toe chain or whether locomotion-transition handling is the remaining problem
  - March 12, 2026 full lower-limb locomotion composition note:
    - re-checked:
      - official UE PhysicsControl docs
      - local UE 5.7 PhysicsControl source
      - ProtoMotions local control mapping code and online docs
    - implemented the next narrow representation experiment:
      - above `50 cm/s`, `calf_*`, `foot_*`, and `ball_*` all force `bUseSkeletalAnimation = false`
      - the rest of the body stays on the current policy-active skeletal-target composition path
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - no locomotion-start discontinuity
      - movement completion remains green
      - forward and backward remain mixed rather than clearly improving
      - one forward sample worsens materially, with `ball_l` peaking around `10104 deg/s`
      - late backward still contains large distal spikes, including `ball_r` around `9176 deg/s`
    - current runtime read:
      - whole-chain explicit-only switching is not a clean new baseline
      - the remaining mismatch is likely locomotion transition handling or more proximal lower-limb composition, not more distal-set widening alone
  - March 12, 2026 lower-limb composition transition-policy note:
    - re-checked:
      - official `UPhysicsControlComponent` and `SetControlsUseSkeletalAnimation` docs
      - local UE 5.7 PhysicsControl source confirming the runtime mode switch is binary
      - ProtoMotions simulator/control mapping code confirming there is no equivalent locomotion-time representation flip in training
    - implemented the next transition-handling experiment:
      - reverted the explicit-only affected set back to `foot_*` and `ball_*`
      - added locomotion-speed hysteresis and dwell:
        - enter `50 cm/s`
        - exit `100 cm/s`
        - enter hold `0.20 s`
        - exit hold `0.20 s`
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - no locomotion-start discontinuity
      - forward and backward peaks are materially lower than the failed full-chain composition pass
      - remaining spikes often migrate proximally into `thigh_*` or other non-distal bodies
    - current runtime read:
      - abrupt representation switching was part of the locomotion mismatch
      - this stateful transition policy is the best measured locomotion-composition baseline so far
      - the next pass should inspect more proximal lower-limb composition or target-velocity handling, not widen the distal explicit-only set again
  - March 12, 2026 proximal lower-limb composition note:
    - re-checked:
      - official UE PhysicsControl docs
      - local UE 5.7 PhysicsControl source
      - ProtoMotions simulator/control mapping code
    - implemented one narrow structural experiment:
      - kept the current locomotion-speed hysteresis+dwell baseline
      - added `thigh_*` to the explicit-only locomotion composition set
      - kept `calf_*` on the composed path
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - the pass regressed materially versus the transition-policy baseline
      - forward samples rose into repeated `ball_r` spikes around `~6177 - 7009 deg/s`
      - backward samples rose into repeated `foot_r` / `ball_r` spikes around `~8700 - 11979 deg/s`
      - repeated `calf_r` linear spikes also rose into the `~2200 - 3080 cm/s` range
    - current runtime read:
      - adding `thigh_*` to the explicit-only locomotion composition set is not the right new baseline
      - the code has been restored to the last safe transition-policy baseline
      - the next pass should target another locomotion-time mismatch surface, most likely lower-limb target write timing or another policy-side transition seam
  - March 12, 2026 distal explicit target-velocity note:
    - re-checked:
      - official `UPhysicsControlComponent`, `SetControlsUseSkeletalAnimation`, and `UpdateControls` docs
      - local UE 5.7 PhysicsControl source showing `SetControlTargetOrientation(...)` synthesizes explicit target angular velocity from orientation deltas unless `AngularVelocityDeltaTime == 0`
      - ProtoMotions simulator PD-target path showing torque from target position and current velocity, without an equivalent explicit target angular velocity term
    - implemented one narrow locomotion-time experiment:
      - kept the committed `foot_*` / `ball_*` hysteresis+dwell baseline
      - zeroed only the explicit target angular-velocity delta time for those distal controls while the mode is active
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - first forward distal spikes drop materially into the low-thousands range
      - lower-limb occupancy stays mostly around `~0.9x - 1.1x`
      - some later peaks still migrate proximally into `calf_*`, `thigh_*`, and occasional `foot_*`
    - current runtime read:
      - synthesized explicit target angular velocity was a real locomotion-time mismatch surface
      - this is a keepable improvement to the current distal locomotion baseline
      - the next pass should inspect per-bone write smoothing or another proximal lower-limb seam, not re-open whole-chain explicit-only switching
  - March 12, 2026 lower-limb composed target-velocity note:
    - re-checked:
      - official UE PhysicsControl docs
      - local UE 5.7 source confirming explicit target angular velocity is added even when skeletal-animation targets are used
      - ProtoMotions PD-target path confirming there is no directly equivalent explicit target angular-velocity term
    - implemented one narrow locomotion-time experiment:
      - kept the committed `foot_*` / `ball_*` hysteresis+dwell baseline
      - widened angular-velocity suppression to `thigh_*`, `calf_*`, `foot_*`, and `ball_*`
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - forward is mixed, with some distal improvement but continued large proximal spikes
      - backward regresses, with large `ball_*` spikes still present
      - late idle/strafe also show new `ball_*` outliers
    - current runtime read:
      - broad lower-limb explicit target angular-velocity suppression is not a clean new baseline
      - the code has been restored to the narrower distal-only suppression baseline
      - the next pass should target per-bone target-write smoothing or another proximal locomotion transition seam
  - March 12, 2026 locomotion-time proximal lower-limb response-fit note:
    - re-checked:
      - official UE PhysicsControl docs
      - local UE 5.7 PhysicsControl source
      - ProtoMotions docs and local control-mapping code
    - implemented one narrow locomotion-time response experiment:
      - kept the current distal locomotion baseline unchanged
      - added a proximal lower-limb response profile only for `thigh_*` / `calf_*`
      - damping ratio scale `1.20`
      - extra damping scale `1.35`
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - forward remains mixed
      - backward stays difficult but does not reopen the old high-spike regime
      - strafe and late idle improve materially relative to the distal-only suppression baseline
    - current runtime read:
      - this is a keepable improvement on top of the current distal locomotion baseline
      - the next pass should stay in locomotion-time response fitting, most likely with a more selective per-bone proximal profile
  - March 12, 2026 locomotion-time thigh de-intensification note:
    - re-used:
      - official UE PhysicsControl docs
      - local UE 5.7 PhysicsControl source
      - ProtoMotions docs and local control code
    - implemented one narrow selective-response experiment:
      - `thigh_*` reduced to damping ratio `1.05`, extra damping `1.10`
      - `calf_*` stayed at damping ratio `1.20`, extra damping `1.35`
    - verification:
      - UE build passes
      - `PhysAnim.Component` passes
      - `PhysAnim.PIE.MovementSmoke` passes
      - no fail-stop
    - measured runtime result:
      - some strafe samples improve
      - forward does not materially beat the shared proximal-response baseline
      - backward and late idle re-open larger outliers than the shared baseline
    - current runtime read:
      - thigh de-intensification is not a clean new baseline
      - runtime code has been restored to the shared proximal-response profile
  - March 12, 2026 corrected locomotion shell-coupling audit note:
    - the first shell/body drift read turned out to be invalid:
      - shell telemetry was accidentally derived from an already shell-relative root diagnostic
      - that double-counted shell/body separation and made the drift look much worse than it was
    - after fixing the reference-frame bug and rerunning deterministic movement smoke:
      - shell planar offset delta is modest, commonly around `~10 - 16 cm` at peak
      - shell/root planar velocity mismatch is modest, commonly around `~2 - 36 cm/s`
      - shell/root planar velocity alignment stays near `1.00` during active movement
      - no fail-stop appears in the corrected movement smoke
    - current runtime read:
      - the shell-coupling audit was worth doing because it ruled out the wrong pivot
      - gameplay-shell coupling is not the dominant remaining locomotion blocker
      - the next useful work should stay on lower-limb locomotion-time representation / response
