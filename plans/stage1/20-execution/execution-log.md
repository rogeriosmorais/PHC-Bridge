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

- `Current phase`: Phase 1 / `S1-P1-A1` accepted / `S1-P1-A2` still in progress, but the practical focus has shifted from generic stabilization to explicit balance-entry contract work, comparison packaging, and transition diagnosis
- `Overall status`: UE install, project scaffold, ProtoMotions checkout, pretrained checkpoint, Python `3.11` environment, and the Isaac Sim / Isaac Lab runtime remain confirmed locally; Gate G1 remains `pass`; the selected Phase 1 runtime model remains the pretrained `motion_tracker/smpl` checkpoint; the full UE startup path still succeeds through `NNERuntimeORTDml`; the one-character bridge still has a stable idle baseline and stable movement-smoke baseline for the current smoke scope; preserved-gameplay-shell manual `WASD` still works in `BridgeActive`; however, the current active engineering problem is no longer blind startup stabilization or raw locomotion survivability, but rather truthful and deterministic entry into Balance Perturbation Mode
- `Last planning milestone`: the runtime/design milestone has moved from “make balance mode roughly work” to “make the balance transition truthful, phase-owned, and diagnosable”; a dedicated entry-transition state machine now exists, Phase 1 stabilization has been recognized as a distinct design problem, and the latest runtime evidence shows that the dominant failure is no longer the old invalid-entry loop but a repeatable Phase 2 root-on spike after apparently clean Phase 1 readiness

## Active Tasks

| Task ID | Owner | Status | Frozen Inputs | Writable Paths | Waiting On |
|---|---|---|---|---|---|
| P0-01 | Orchestrator | completed | planning bundle as initially frozen | `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none |
| S1-P0-U1 | User | completed | environment and setup specs | external tool/runtime setup and license acceptance | none |
| S1-P0-U2 | User | completed | scaffold and user setup docs | UE editor setup | none |
| S1-P0-A1 | AI | completed | frozen Phase 0 inputs | Phase 0 package + spec paths | none |
| S1-P0-A2 | AI + User | completed | Phase 0 execution package + G1 evidence paths | evidence + log paths | none |
| S1-P1-A1 | AI | completed | Phase 1 implementation package, ONNX/export specs, UE bridge implementation spec | ONNX export/runtime bridge paths | none |
| S1-P1-A2 | AI | in_progress | accepted `S1-P1-A1` handoff, Phase 1 implementation package, manual verification, acceptance thresholds, bring-up runbook, newer balance design docs | `plans/stage1/40-design/*.md`, `plans/stage1/30-evidence/g2-evaluation.md`, `plans/stage1/20-execution/execution-log.md`, `plans/stage1/20-execution/assumption-ledger.md` | none; active work now includes balance entry/transition design, comparison packaging, and transition-failure diagnosis |

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

## Current practical conclusion

Repository baseline:

- keep the current stable idle and movement-smoke runtime baseline
- keep the truthful phased balance-transition direction
- do not claim Balance Perturbation Mode is solved yet

Active engineering problem:

1. make Phase 1 converge by owned topology/authority shaping, not incidental drift
2. make Phase 2 root-on deterministic and spike-safe
3. prevent meaningless retry loops after root-on failure
4. keep G2 and comparison packaging honest to the current runtime state

Do **not** treat these as solved yet:

- flat-ground walk playback quality
- slope / ramp-following locomotion fidelity
- persuasive G2 comparison presentation
- full balance-mode activation success
- root-on stability under the new transition contract

## Notes

Known important reference points from this work:

- temporary proof-only logging and earlier identity-check scaffolding were intentionally non-final
- startup movement lock was diagnostic, not final architecture
- bridge-owned locomotion remains the intended direction for `BridgeActive`
- the current runtime/design shift is from broad stabilization effort toward explicit phase-owned balance transition design
- the latest honest failure point is a repeatable Phase 2 root-on spike, not the older invalid-entry rejection loop