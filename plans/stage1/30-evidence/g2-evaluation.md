# Stage 1 Gate G2 Evaluation

## Purpose

This document defines how Gate G2 is evaluated.

Gate text from the engineering plan:

> Side-by-side - physics-driven vs kinematic PoseSearch. Must look noticeably more natural.

This package exists so the user does not have to invent the comparison method at evaluation time.

Use [instrumentation_and_acceptance.md](/F:/NewEngine/plans/stage1/10-specs/instrumentation_and_acceptance.md) as the tie-breaker when the verdict feels vague.
Use [comparison-sequence-lock.md](/F:/NewEngine/plans/stage1/50-content/comparison-sequence-lock.md) to decide what sequence is allowed to count as a valid G2 comparison.

## Gate Status

- `Current status`: blocked
- `Decision owner`: user, using orchestrator-prepared evidence
- `Final verdict`: pending
- `Latest automation check`: `PhysAnim.PIE.BalanceModeSmoke` ran on May 7, 2026 as a diagnostic smoke; the current run passes startup proof with `terminal_reason_name=None` and `runtime_simulating_body_count=22`, queues balance entry, clears Phase 1 RootOn readiness with `rootOnReady=1`, clears Phase 2 warm-start with baseline-compensated direct-link angular errors, enters Phase 3/Settle, clears the material shell correction label, then truthfully denies with `phase3_post_root_on_instability`

Use only:

- `pass`
- `fail`
- `blocked`

## Entry Criteria

Do not run G2 until:

- Phase 1 is complete enough for comparison
- the one-character bridge runs end to end
- `PhysAnim.PIE.BalanceModeSmoke` reaches `BalanceActive_Standing`
- the `BalanceActive_Standing` hold threshold already scores `pass`
- the comparison sequence is chosen and frozen
- the orchestrator confirms no `red` assumption blocks the quality comparison

## Comparison Setup

Compare two versions of the same motion sequence:

1. **Kinematic baseline**
   - Manny driven by the animation / PoseSearch path without the physics-driven PHC bridge taking over body motion
2. **Physics-driven version**
   - Manny driven by the full Stage 1 bridge using PHC outputs and `UPhysicsControlComponent`

Keep these fixed between both captures:

- same character
- same animation source / comparison sequence
- same camera framing
- same arena or environment setup
- same playback speed

Preferred setup:

1. **Live side-by-side PIE comparison**
   - one `Physics-Driven` Manny and one `Kinematic` Manny in the same session
   - identical scripted sequence or mirrored movement input
   - fixed tracking camera preferred
   - no pawn collision or physical interference between the pair
   - clear on-screen role labels
2. **Fallback**
   - manual side-by-side or two clearly comparable recordings only if the scripted presentation harness is unavailable

## User Procedure

1. Confirm the orchestrator has named the exact comparison sequence or build version.
2. Confirm the setup uses the same frozen sequence from [comparison-sequence-lock.md](/F:/NewEngine/plans/stage1/50-content/comparison-sequence-lock.md).
3. If the live side-by-side harness is available, prefer it:
   - start PIE
   - enter `BalanceActive_Standing`
   - run `PhysAnim.G2.StartPresentation`
4. Identify the labeled actors:
   - `Kinematic`
   - `Physics-Driven`
5. Watch the full frozen scripted sequence once without judging.
6. Watch it again and judge only the motion thesis, not missing features or unrelated polish.
7. Fill the evidence template before choosing the final verdict.

## What G2 Is Actually Judging

The goal is not perfect realism. The goal is whether the physics-driven version is clearly better on the thesis criteria:

- weight
- momentum
- balance recovery
- contact response
- overall non-robotic feel

The scripted presentation now begins with a fixed locomotion-coupled perturbation. This is intentional:

- both actors begin the same short scripted walk
- only the `Physics-Driven` actor receives the physical contact disturbance
- the `Kinematic` actor stays on the same scripted locomotion path without that extra contact
- the perturbation is delivered as a short scripted contact push, not a one-frame tap
- the earlier gameplay-shell shove was removed because it produced obvious sideways sliding without a convincing body response
- the perturbation phase uses a closer camera framing before the sequence returns to the standard comparison shot

Current orchestrator note:

- this locomotion-coupled path is the best stable perturbation format so far
- it produces measurable divergence without fail-stop
- the perturbation window now uses a real movement-safe stabilization override plus a lower-body, lead-leg-biased contact profile
- but the visible difference is still under evaluation and may remain too subtle for a clean G2 win

## Evaluation Rubric

### Pass

Choose `pass` if the physics-driven version is **noticeably more natural** on most of these:

- weight transfer looks more believable
- motion carries momentum instead of stopping abruptly
- body reacts to contact or balance shifts in a physically convincing way
- the result looks less robotic than the kinematic baseline

### Fail

Choose `fail` if any of these are true:

- the difference is negligible
- the physics-driven version looks worse overall
- the physics-driven version is too unstable or noisy to support the thesis
- the comparison is dominated by artifacts instead of improved physicality

### Blocked

Choose `blocked` if:

- the physics-driven runtime is still dominated by immediate startup instability
- the captures are not comparable
- the sequence is not frozen
- the user cannot make a fair judgment from the available evidence

## Required Evidence

- one scripted live side-by-side clip preferred, or a clearly comparable fallback capture
- one short written verdict from the user
- notes on which rubric points drove the decision

## User Scoring Checklist

Write `better`, `same`, or `worse` for the physics-driven version on each point:

- weight
- momentum
- balance recovery
- contact response
- overall non-robotic feel

Then choose:

- `pass` only if the physics-driven version is noticeably better overall
- `fail` if the difference is negligible or worse
- `blocked` if the captures are not fair to compare

## Manual Check Mapping

- Primary checkpoint: `MV-G2-01`

## Evidence Template

- `Comparison sequence`:
- `G2 format`: live side-by-side / two recordings
- `Kinematic clip or actor label`:
- `Physics-driven clip or actor label`:
- `Camera/setup parity confirmed?`: yes/no
- `User verdict`: pending
- `Why this verdict was chosen`:

## Orchestrator Review Before User Decision

The orchestrator must confirm:

- the captures are comparable
- the evidence is sufficient for a user judgment
- no major setup mismatch invalidates the comparison

## Final Decision Section

- `Final verdict`: blocked
- `Decision date`: May 7, 2026
- `Decision summary`: `PhysAnim.PIE.BalanceModeSmoke` previously reached `BalanceActive_Standing` beyond the 3.0 second timing threshold while metrics reported `PelvisSim=0 PelvisAwake=0`. The current checkout no longer accepts that as success. It now starts bridge physics ownership after valid PoseSearch, records `AfterActivationPrepass` with `rootBodySim=true`, samples `PelvisSim=1 PelvisAwake=1`, passes startup proof with `terminal_reason_name=None`, queues balance entry, reaches Phase 2 entry after baseline-compensated Phase 1 RootOn readiness reports `rootOnReady=1`, clears Phase 2 warm-start direct-link angular gates, and enters Phase 3/Settle with `rootRawSim=1` and `pelvisRawSim=1`. The current blocker is `BalanceSafeDeny` with `phase3_post_root_on_instability`, not missing physical continuity, BridgeActive proxy termination, Phase 1 RootOn readiness, Phase 2 warm-start angular coherence, or material shell correction. G2 remains blocked until the live physical-continuity path reaches `BalanceActive_Standing` and a fair side-by-side user comparison exists.
- `Can Phase 2 begin?`: no

## If Verdict Is Not Pass

- `Failure or block reason`: balance smoke now preserves raw simulation, passes startup proof, queues balance entry, clears Phase 1 RootOn readiness, clears Phase 2 warm-start direct-link angular coherence, enters Phase 3/Settle, and clears the material shell correction label, but Phase 3 denies with `phase3_post_root_on_instability`; no live physical-continuity `BalanceActive_Standing` pass or side-by-side user verdict exists
- `Need rework?`: yes
- `Fallback or stop decision`: continue Stage 1 balance-first stabilization
- `Next task or replan action`: fix Phase 3 post-RootOn instability so the physical-continuity path can progress from Phase 3/Settle to `BalanceActive_Standing`
