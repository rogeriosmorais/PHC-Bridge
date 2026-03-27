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

- `Current phase`: Phase 1 / truthful RootOn-readiness investigation.
- `Overall status`: UE startup is stable and the latest truthful balance smoke reaches explicit safe denial with `phase1_root_on_readiness_pelvis_thigh_margin_insufficient`.
- `Last planning milestone`: contract and ownership drift are largely cleaned up; the newest Phase 1 solver step keeps the thigh follow-through path truthful by forbidding it from re-breaking recovered spine readiness.
- `Latest runtime forensics`: the final solver path still runs `spine_interp` and a constrained `worst_thigh_interp_thigh_r`, but the new acceptance guard limits that follow-through to a spine-safe micro-step (`a0.01`) instead of the earlier spine-breaking `a0.05`; the live result now keeps the spine inside readiness at `pelvisSpine01Angular=17.91` while only partially improving the thigh blocker (`pelvisThighLAngular=31.97`, `pelvisThighRAngular=34.19`), which narrows the next work to finding a stronger thigh improvement that preserves the recovered spine margin.

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as initially frozen | `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | environment and setup specs | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | scaffold and user setup docs | UE editor setup | none |
| S1-P0-A1 | AI | completed | frozen Phase 0 inputs | Phase 0 package + spec paths | none |
| S1-P0-A2 | AI + User | completed | Phase 0 execution package + G1 evidence paths | evidence + log paths | none |
| S1-P1-A1 | AI | completed | Phase 1 implementation package, ONNX/export specs, UE bridge implementation spec | ONNX export/runtime bridge paths | none |
| S1-P1-A2 | AI | completed | accepted `S1-P1-A1` handoff, Phase 1 implementation package, manual verification, acceptance thresholds, bring-up runbook, newer balance design docs | `plans/stage1/40-design/*.md`, `plans/stage1/30-evidence/g2-evaluation.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P1-A3 | AI | in_progress | Distal Kinematic experiment success, read-only telemetry, authoritative modifier writes | `plans/stage1/20-execution/execution-log.md` | none |

## Frozen Inputs For Phase 0 Preparation

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
| 1 | S1-P1-A2 | runnable now; runtime is stable enough for design-led transition work, G2 packaging, and balance transition validation |
| 2 | Balance transition validation | runnable now; the repo now has enough entry/transition instrumentation to validate explicit Phase 1 / Phase 2 contracts |
| 3 | G2 evidence capture | runnable once the frozen comparison sequence and presentation path are accepted for the current runtime baseline |
| 4 | S1-P2-A1 | not runnable until G2 is explicitly passed |

## Waiting On User

| Item | Expected Evidence |
|---|---|
| none | no Phase 0 evidence remains outstanding |

## Latest Phase 0 Evidence Progress

- the selected local `motion_tracker/smpl` checkpoint contract remains written down in `bridge-spec.md`
- G1 Criterion 2 remains `pass`
- the motion-source review remains `pass`
- the UE scaffold remains concretely verified:
  - `PhysAnimUE5.uproject` lists `PoseSearch` and `PhysicsControl`
  - Manny content exists under `Content/Characters/Mannequins`
  - editor logs show `NNERuntimeORT` runtime availability
  - PIE launched successfully
- the current visual eval path still renders and records locally with the earlier compatibility fixes in place
- user evidence previously confirmed successful save and `pass` verdicts for the frozen `MV-G1-*` checks
- Visual Studio Build Tools 2022 with MSVC v143 and Windows SDK `22621` remain installed and usable
- the UE startup-success path remains proven:
  - `[PhysAnim] Startup success. Runtime=NNERuntimeORTDml Model=/Game/NNEModels/phc_policy.phc_policy`
- Gate G1 remains `pass`
- Phase 0 evidence capture remains complete
- Phase 1 remains unblocked

## Accepted Handoffs

| Task ID | Artifact | Accepted? | Notes |
|---|---|---|---|
| S1-PLAN-01 | Stage 1 planning bundle | yes | foundational planning artifacts in place |
| S1-PLAN-02 | `bridge-spec.md` | yes | planning-level contract defined and updated with selected local runtime contract |
| S1-PLAN-03 | `retargeting-spec.md` | yes | planning-level mapping defined; runtime validation work followed |
| S1-PLAN-04 | `test-strategy.md` | yes | verification split defined |
| S1-PLAN-05 | environment / pretrained / scaffold / threshold bundle | yes | execution-planning gaps materially reduced |
| S1-PLAN-06 | task packets / lock sheets / user return path | yes | re-entry into Phase 0 operationally defined |
| S1-PLAN-07 | retrieval / export / comparison lock bundle | yes | Phase 0-1 planning gaps materially reduced |
| S1-P0-A1 | Phase 0 machine-specific execution package | yes | Windows-native Isaac Sim / Isaac Lab path frozen |
| S1-P1-A1 | single-character implementation package freeze + ONNX export path | yes | startup now succeeds through `NNERuntimeORTDml`; export/import discovery no longer critical path |

## Blocked / Deferred

| Task ID | Status | Reason |
|---|---|---|
| G2 | readying | baseline runtime is stable enough, but comparison packaging still needs to reflect the current locomotion/balance state honestly |
| S1-P2-A1 | blocked | depends on G2 pass |
| S1-P2-A2 | blocked | depends on Phase 2 result |
| Ramp / slope locomotion fidelity | deferred | flat-ground locomotion playback and truthful balance transition remain higher priority |
| Broad perturbation tuning by guesswork | deferred | explicit transition design and objective phase diagnostics are now preferred over ad hoc tuning |

## Ledger Sync Note

Whenever new setup or gate evidence arrives:

1. update `assumption-ledger.md`
2. update this execution log
3. only then issue or advance worker tasks

---

## 2026-03-11 — Gameplay-shell preservation and movement-stability milestone

- normal manual runtime now preserves capsule collision and `CharacterMovement` during `BridgeActive`
- the deterministic movement smoke harness remained valid, and gameplay-shell preservation became more than a smoke-only path
- an always-visible on-screen bridge state indicator was added for easier runtime inspection
- movement-triggered fail-stop false positives were traced to world-space root instability checks after gameplay-shell preservation
- fix:
  - when the gameplay shell is preserved, runtime instability now evaluates root/body translation relative to the owning actor shell instead of the original world-space activation frame
- verification:
  - movement smoke completed without `BridgeActive -> FailStopped`
- result:
  - first movement-stability milestone was treated as `pass`
  - longer deterministic locomotion soak also went green
  - manual real-`WASD` in `BridgeActive` also worked

## 2026-03-11 — G2 comparison harnesses and perturbation exploration

- a live side-by-side G2 harness was added through a pair of start/stop commands
- the preferred G2 format became one PIE session with:
  - one `Physics-Driven` Manny
  - one spawned `Kinematic` Manny
- a scripted G2 presentation harness was also added:
  - freezes player move/look input
  - drives both actors through the same short sequence
  - uses a fixed comparison camera
- perturbation work continued through multiple scenario variants:
  - shell-level shove
  - body-level contact push
  - low-gain perturbation override
  - temporary root-unlock variants
  - policy-suspension variants
- result:
  - shell-level shove was rejected because it mostly produced sideways actor sliding
  - body-level perturbation gave modest articulated response without obvious shell slide
  - root-unlock variants made the perturbation visible but quickly crossed fail-stop thresholds
- practical conclusion:
  - the standing external-push perturbation path was well explored under the current Stage 1 contract
  - root-kine perturbation was too subtle
  - root-sim perturbation was unstable
  - the next useful perturbation attempt should change scenario, not keep retuning the same standing shove

## 2026-03-11 to 2026-03-12 — Locomotion-coupled perturbation pivot

- perturbation work pivoted to a locomotion-coupled scenario:
  - both actors begin the same scripted walk
  - only the `Physics-Driven` actor receives the extra disturbance
  - the kinematic actor remains on the same scripted locomotion path without the extra contact
- result:
  - the presentation remained stable with no fail-stop
  - measurable divergence during the walk appeared
  - the difference was still likely subtle to the eye
- later refinement:
  - the perturbation stabilization override had accidentally been left as a no-op
  - it was corrected to apply real movement-safe angular relaxation during the perturbation window
  - the profile was also pushed lower in the body and made more locomotion-focused
- result:
  - implementation correctness improved
  - the scenario remained stable
  - but it still did not guarantee an obviously persuasive G2 `pass`

## 2026-03-12 — Training/runtime alignment pass

A very large alignment pass focused on reducing mismatch between ProtoMotions training semantics and the live Unreal runtime.

Major work items included:

- Manny constraint inventory and explicit SMPL-vs-Manny limit documentation
- family-level Manny mass audit and family-level mass-adjustment runtime policy
- training-aligned control-family response fitting
- toe-family refinement and toe/ankle authoring audits
- lower-limb limit-occupancy instrumentation
- lower-limb target-range policy for the knee/ankle/toe chain
- locomotion-time distal representation experiments
- distal explicit-only composition experiments
- locomotion-time composition hysteresis / dwell / intent latching / intent grace
- distal explicit target-angular-velocity suppression
- proximal lower-limb locomotion-time response fitting
- shell-coupling telemetry correction
- lower-limb target-step policy experiments
- lower-limb contact-exclusion alignment experiments
- PhysicsControl cache prewarm
- lower-limb target-write smoothing experiments
- self-observation root-height alignment
- Proto runtime world-frame alignment
- future target time-channel alignment
- mimic-target current-reference ground alignment
- mimic-target data-origin alignment
- terrain input alignment
- locomotion trace input summaries
- policy-step trace alignment
- Unreal Insights instrumentation in the bridge runtime

Practical conclusion from this whole pass:

- several objective contract seams were corrected and are worth keeping
- many falsifiable locomotion heuristics were tested and either kept or reverted
- the bridge became better instrumented and better aligned with training-side semantics
- locomotion improved in bounded ways, but the work did **not** produce a final “walking solved” verdict
- the repo ended this stretch with a stronger runtime baseline and far better diagnostics, not with locomotion fully solved

## 2026-03-14 — Flat-ground-first locomotion decision

This stretch focused on:

1. cleaning up temporary T-pose / identity-check scaffolding,
2. stabilizing startup into a reliable idle baseline,
3. isolating locomotion ownership away from `CharacterMovement`,
4. starting bridge-owned trajectory / motion-matching integration,
5. deciding to defer ramps / floor behavior until flat-ground walk playback is correct.

Important outcomes:

- a good idle-only baseline was reached and kept
- bridge-owned deterministic locomotion was accepted as the intended direction
- shell movement alone was shown to be insufficient
- trajectory/motion-matching integration began in earnest
- flat-ground-first was adopted as the next priority:
  - stop worrying about ramps and floor handling
  - make visible walking animation playback work correctly on a flat plane first

Current practical read from that period:

- startup is stable
- some non-idle clips can be selected in logs
- but the character still does not present a convincing visible walk
- sliding remains the dominant visible result
- ramps / floor-following stay deferred until flat-ground walking is correct

## 2026-03-15 — Balance Perturbation Mode design and first implementation push

- the explicit Balance Perturbation Mode design was added
- the first runtime implementation pass for that mode followed
- balance mode was no longer treated as an improvised side path; it became a named diagnostic mode with:
  - dedicated purpose
  - perturbation method
  - contamination rules
  - pass/fail framing
- early runtime behavior was still unstable, but this was the point where the work stopped being “just another tuning branch” and became a first-class subsystem

## 2026-03-16 — Hardening the old balance-ready path

This block of work focused on making balance entry less fragile before the later full transition-spec refactor.

Key changes included:

- startup fly-away behavior on balance startup was reduced
- logical simulation permission was decoupled from physical-state verification to avoid deadlocks
- readiness reasons became more granular
- promotion-during-settle detection was strengthened
- promotions after policy start were prohibited
- late-reset violations were turned into explicit state-machine issues
- pelvis resets in Balance Mode were banned
- entry preflight checks were introduced
- a dedicated controller was extracted for pelvis/root handoff
- a weak damped posture-hold layer was added during Balance Mode pre-entry
- promotion / readiness / diagnostics were consolidated and made more explicit

Practical outcome:

- the old balance-ready path became much better instrumented and less ad hoc
- but the design still lacked a sufficiently explicit phase-owned entry contract
- this set the stage for the later full transition-state-machine rewrite

## 2026-03-18 — Balance entry transition state machine refactor

- the balance mode entry path was refactored into distinct stages with truthful state management
- the explicit Balance Mode Entry Transition Spec was then implemented in code
- manual-trigger cancellation race behavior was fixed
- transition settings and gates were unified
- defaults were relaxed to avoid unnecessary queue blocking

This was the key architectural shift:

- balance entry stopped being a loose mix of readiness checks and ad hoc promotion
- it became an explicit multi-phase transition with:
  - queueing
  - preflight
  - Phase 1 prepare
  - Phase 2 root-on
  - Phase 3 settle
  - failure/recovery handling

## 2026-03-18 — New observed balance failure mode

Latest runtime diagnosis changed the main read of the problem.

Old dominant failure pattern:
- invalid entry / pre-entry rejection loops
- balance requests unable to progress meaningfully

New dominant failure pattern:
- requests can queue and begin transition
- Phase 1 can now sometimes reach a quiet-looking pre-root-on state
- the deterministic failure now appears at Phase 2 root-on as a repeatable spike

Observed behavior in the latest logs:

- requests queue normally under bring-up gating
- transition invocation now occurs from a meaningful queued path
- Phase 1 can capture a hold reference and declare readiness for root-on
- immediately after root-on, Phase 2 aborts with a repeatable spike
- failed attempts then tend to recycle unless recovery/retry policy is made stricter

Current interpretation:

- the main problem is no longer “Phase 1 never starts” or “entry is permanently invalid”
- the main problem is now “root-on itself is not yet a safe, deterministic choreography”
- this is progress, because the failure is narrower and more truthful than the old blended failure modes

## 2026-03-18 — New design-doc direction from the latest failures

The design work responded to the new runtime evidence by splitting the transition problem into more explicit documents.

New design state:

- the entry-transition spec now acts as the overall contract/state-machine document
- Phase 1 stabilization has been recognized as a distinct implementation-design problem
- a dedicated Phase 1 stabilization spec was drafted to define:
  - transition-critical sets
  - target topology
  - posture-hold rules
  - quiet-window proof
  - failure classes
  - retry conditions
- the latest logs then showed that Phase 2 also needs its own narrow design doc
- a dedicated Phase 2 root-on spec was drafted to define:
  - exact frame ordering for root-on
  - authority suppression during the flip
  - post-root-on guard window
  - root-on spike thresholds
  - recovery and retry rules

Practical meaning:

- balance work is now design-led instead of guess-led
- the repo is no longer just chasing runtime symptoms
- it is converging on a layered contract:
  - overall entry/transition contract
  - explicit Phase 1 stabilization contract
  - explicit Phase 2 root-on contract

---

## 2026-03-19 — Phase 2 Root-On Hardening (Shell Safety)

- Implemented the "Shell Safety Proof" requirement: Phase 2 root-on is now denied if the owning actor shell cannot be proven stable/safe in the pre-root-on window.
- Refined Phase 2 authority gating and readiness classification to distinguish between different classes of root-on denial.
- Fixed late validation snapshot ordering to ensure authoritative state capture before the root-on transition.
- Separated diagnostics for late validation and root-on readiness, providing clearer reasons for admission failures.

## 2026-03-20 — Phase 1 Policy Diagnostic Separation

- Refactored policy diagnostics to strictly separate Phase 1 held-pose activity from normal policy target writes.
- Renamed and split the `NumPolicyTargetsWritten` counter into categories (normal, held, total) to verify that normal policy writes are correctly suppressed during `Prepare` and `LateValidate`.
- Validated through smoke tests that `policyActive` remains false and `written=0` during Phase 1, with only `holdWritten` targets active.
- Refactored balance entry logic to consolidate `UPhysAnimComponent` as the sole authority for phase-owned state transitions.
- Gated `BalanceEntry_LateValidate` on sim-body quietness to avoid entering validation from a jittery startup state.

## 2026-03-21 — Root Tilt calculation fix

- Corrected `RootTilt` calculation in `UPhysAnimComponent::TickComponent`.
- Switched from mesh root bone quaternion to a more reliable source to avoid false positives in Phase 1 tilt-high errors.

## 2026-03-22 — Phase 1 Hardening (Diagnostics and Guards)

- Restricted Phase 1 body-motion diagnostics to only consider simulated bodies, preventing kinematic upper-body hold bones from contaminating stability metrics.
- Added a body-motion-instability gate to the `Prepare` phase to prevent repeated `LateValidate` retries from unstable states.
- Added a dynamic-stability margin gate before `LateValidate` to reduce immediate safe-denies and improve startup success.
- Refined the Phase 1 startup freeze lifetime to ensure precise acquisition upon acceptance and release only upon terminal outcomes.

## 2026-03-22 — Simplified Phase 1 Admission (Pelvis-Gate Removal)

- Removed `pelvis_not_simulating` as a terminal prerequisite for Phase 1 entry.
- Corrected the admission logic to allow the root/pelvis to remain kinematic during the `Prepare` phase, aligning with the accepted topology contract.
- Implemented `Prepare` blocking escalation to terminate non-viable entry attempts early, reducing log spam from persistent blocking conditions.

## 2026-03-23 — Distal Kinematic Authoritative Control Milestone

- **Milestone: Distal Kinematic Experiment.** Pivoted to enforcing `distal=kin` as the authoritative Phase 1 topology.
- Eliminated ownership thrash between global `SetAllKinematic` and per-bone modifier syncs in `BridgeActive`.
- Fixed a bug where `PhysicsControl` modifiers were being overwritten to `Simulated` for distal bones despite raw body state being `Kinematic`.
- Ensured distal bones stop flipping ownership through explicit per-bone authoritative modifier writes and stricter topology enforcement.

## 2026-03-23 — Read-Only Telemetry and State Maintenance Refinement

- Refined `TickComponent` telemetry to be strictly read-only, moving all state mutation and repair to the authoritative control path.
- Investigated and resolved stale modifier records that were keeping bones in a `Simulated` state even after raw body state became `Kinematic`.
- Validated that distal tracked bones stop oscillating ownership and reach a coherent stable state at balance entry.

## Current practical conclusion

Repository baseline:
- Keep the current stable idle and movement-smoke runtime baseline.
- Keep the truthful phased balance-transition direction.
- Enforce the "Distal Kinematic" topology as the source of truth for Phase 1.

Current truthful smoke read as of 2026-03-27:
- `PhysAnim.PIE.BalanceModeSmoke` now reaches explicit safe denial truthfully instead of stalling ambiguously.
- the latest observed terminal reason is `phase1_root_on_readiness_pelvis_thigh_margin_insufficient`
- that means the current blocking surface is Phase 1 RootOn-readiness thigh margin proof, not a live Phase 2 RootOn spike in this run

Active engineering problem:
1. keep the truthful safe-deny / terminal-state contract intact
2. preserve read-only telemetry and phase-correct failure labeling
3. isolate why the pelvis-thigh readiness margin remains insufficient at LateValidate even after the spine margin has been recovered

## Notes

Known important reference points from this work:
- The shift from broad stabilization to explicit phase-owned balance transition design is complete.
- The latest engineering focus is on "truthful" topology enforcement and avoiding ownership thrash.
- Bridge-owned locomotion remains the intended direction for `BridgeActive`.
- The current honest failure point in the latest smoke is `phase1_root_on_readiness_pelvis_thigh_margin_insufficient`, not the older invalid-entry rejection loop.

## 2026-03-27 — Phase 1 Spine-Safe Worst-Thigh Follow-Through

- Added deterministic TDD coverage for a spine-safe worst-thigh margin-sweep acceptance rule, allowing small preserved-spine trade within recovered Phase 1 readiness while still rejecting candidates that spend too much spine margin.
- Added a new post-`worst_thigh_interp` local sweep that only accepts candidates improving the current worst thigh while keeping the spine inside readiness.
- Verified with `.\scripts\build.ps1 -Test PhysAnim.Component`, `.\scripts\build.ps1 -Test PhysAnim.PIE.BalanceModeSmoke`, and `python .\scripts\read_logs.py`.
- The new runtime path now lands on `..._spine_interp_a0.10_worst_thigh_interp_thigh_r_a0.01_worst_thigh_margin_sweep_y-0.50_p0.05_r-0.50`.
- Live Phase 1 metrics improved to `pelvisThighLAngular=31.49`, `pelvisThighRAngular=33.89`, and `pelvisSpine01Angular=17.98`.
- The truthful blocker remains `phase1_root_on_readiness_pelvis_thigh_margin_insufficient`, which narrows the remaining work to stronger thigh recovery/viability rather than a missing spine-safe thigh follow-through surface.

## 2026-03-27 — Focused Thigh Blend-Sample Search

- Added deterministic TDD coverage for focused-sample relevance so blend candidates that explicitly include the blocked thigh in their source tagging are now eligible for the spine-safe worst-thigh focused-delta pass instead of restricting that pass to the first direct thigh constraint sample.
- Broadened `worst_thigh_focus_delta` to iterate all thigh-relevant valid constraint samples, including weighted blend candidates, while keeping the existing spine-safe acceptance rule intact.
- Verified with `.\scripts\build.ps1 -Test PhysAnim.Component`, `.\scripts\build.ps1 -Test PhysAnim.PIE.BalanceModeSmoke`, and `python .\scripts\read_logs.py`.
- Live smoke remained on the same truthful result: `phase1_root_on_readiness_pelvis_thigh_margin_insufficient`, with the winning runtime path still ending at `..._spine_interp_a0.10_worst_thigh_interp_thigh_r_a0.01_worst_thigh_margin_sweep_y-0.50_p0.05_r-0.50` and metrics still at `pelvisThighLAngular=31.49`, `pelvisThighRAngular=33.89`, and `pelvisSpine01Angular=17.98`.
- This rules out another focused-sample selection gap and narrows the remaining work to deeper thigh candidate generation or genuine Phase 1 physical viability.

## 2026-03-27 — Phase 1 Transactional Auto-Calibration Harness

- Added the design doc [`phase1-transactional-auto-calibration-harness.md`](/f:/NewEngine/plans/stage1/40-design/phase1-transactional-auto-calibration-harness.md) and wired the first dev-only implementation around the existing balance-transition path instead of introducing a second solver.
- Added debug-only Phase 1 transactional snapshot/export-import surfaces on `UPhysAnimComponent` and `FPhysAnimBalanceReadyTransition`, plus transient solver-override parameters limited to the planned candidate-generation knobs.
- Implemented `UPhysAnimPhase1AutoCalibSubsystem`, staged candidate generation/refinement/repro passes, deterministic restore preflight, artifact-path setup, and `pa.RunPhase1AutoCalib` / `pa.StopPhase1AutoCalib`.
- Added TDD for transition snapshot round-trip, auto-calib score ordering, contract-threshold non-mutation, Stage A candidate coverage, and reproducibility classification.
- Verified with `.\scripts\build.ps1 -Test PhysAnim.Component` and `.\scripts\build.ps1 -Test PhysAnim.Bridge.BalanceStateless`.
- Attempted a PIE smoke for the harness, but the live runtime still races existing balance-entry queue gates (`queue_bring_up_incomplete`, then `queue_policy_influence_below_threshold`) before a stable transactional start point is available in that map/runtime path, so the failing smoke was not left in the main suite.
