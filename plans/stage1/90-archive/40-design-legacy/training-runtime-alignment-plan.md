# Training-to-UE Physics Alignment Plan

## Goal

Bring the UE5 Stage 1 runtime closer to the physical assumptions the ProtoMotions policy was trained under, without violating the locked Stage 1 architecture or blindly copying simulator settings that do not map 1:1 into UE PhysicsControl.

This plan remains focused on alignment surfaces such as:

1. mass distribution
2. PD gains / control response
3. timestep and control cadence
4. joint limits
5. observation and target-packing contract correctness

## Current Direction Check

As of March 23, 2026, it is still worth continuing in the broader training/runtime alignment direction.

But the current balance-mode work has clarified an important split:

- locomotion-time alignment work and balance-entry contract work are related
- they are not the same workstream
- the balance-entry path now has its own explicit contract and diagnostics that must stay aligned with the balance-mode design docs

## What this file is and is not

This file is the high-level alignment plan for Stage 1 runtime behavior.

It is not the authoritative source for:

- frozen Phase 1 topology
- LateValidate ownership behavior
- balance-entry failure-class semantics

Those now live primarily in:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`
- `plans/stage1/10-specs/ue-bridge-implementation-spec.md`

## Key engine-side finding now locked into the project understanding

Epic's PhysicsControl behavior means these concepts must be kept separate:

- intended ownership
- modifier-record ownership
- raw body state

And they are not guaranteed to be frame-synchronous inside the same component tick.

Project implication:

- balance-entry telemetry must use next-frame confirmation
- same-frame mismatch is not enough to prove a persistent ownership violation

## Keepable alignment results already established

The project already has several keepable alignment wins, including:

- fixed policy/control cadence at `30 Hz`
- terrain-relative self-observation root-height correction
- terrain input packing instead of zero terrain
- world-frame split for Proto runtime world-space packing
- mimic target-pose time-channel correction
- data-origin/current-reference corrections for mimic target poses
- training-aligned family mass policy
- training-aligned family control-response fit baseline
- useful trace and insights instrumentation for movement/runtime diagnosis

Those results should stay documented and should not be re-opened casually.

## Current balance-entry-specific read

The latest balance-mode work has clarified that several earlier blockers were contract bugs rather than pure physics problems.

Resolved or substantially improved contract areas:

- next-frame ownership confirmation model
- BridgeActive distal re-promotion suppression
- authoritative per-bone Phase 1 writes for topology-critical bones
- frozen upper-body ownership capture for LateValidate hold

Current first meaningful balance-entry blocker:

- the latest balance smoke now reaches explicit safe denial with:
  - `phase1_root_on_readiness_pelvis_spine_margin_insufficient`

Current interpretation of that blocker:

- Phase 1 LateValidate can satisfy many other gates at the same time
- the remaining failing proof in the latest run is the pelvis-spine RootOn-readiness margin
- this is now a narrower and more truthful blocker than the older broad sim-coverage confusion

That is now more important than the earlier ownership confusion.

## Alignment implication

The project should not assume every remaining balance-entry failure is a training/runtime mismatch.

Current rule of thumb:

- if the failure is about wrong frozen ownership, wrong writer precedence, wrong telemetry source, or wrong hold lifetime, that is a contract bug first
- if the contract is clean and the accepted setup still fails, that is a physical-viability or alignment problem

## Updated work order

Do not align every surface at once.

Recommended order now:

1. keep the balance-entry contract docs and implementation synchronized
2. finish isolating the current pelvis-spine RootOn-readiness margin insufficiency in LateValidate
3. only then decide whether the next blocker is:
   - a remaining contract bug
   - a PhysicsControl application/state-source issue
   - or a true physical-viability/alignment issue
4. continue locomotion-time alignment work in parallel only where it does not muddy the balance-entry diagnosis

## Deliverables that should continue to exist

Before broad retuning, keep these artifacts current:

1. `control-cadence-audit.md`
2. `smpl-to-manny-limit-table.md`
3. `smpl-to-manny-mass-table.md`
4. `pd-response-fit.md`
5. trace / insights artifacts for movement and balance-entry diagnosis
6. explicit balance-entry docs that describe:
   - frozen topology
   - ownership model
   - failure reasons
   - current leading blocker

## Verification matrix

Every alignment pass or balance-entry contract pass should still be validated against:

1. passive idle smoke
2. 65-second idle soak
3. deterministic movement smoke
4. movement soak
5. first-policy-active diagnostics
6. G2 presentation regression
7. balance-mode smoke

And for each pass, logs should make it possible to tell whether the failure is in:

- intended ownership
- modifier-record ownership
- raw body state
- frozen topology capture
- live sim coverage
- physical stability

## Current summary

The project is no longer at the stage where all remaining problems should be described as generic “alignment” issues.

The current balanced view is:

- several important alignment corrections were real and worth keeping
- several major balance-entry blockers were actually contract/spec/engine-timing issues
- the current leading open balance-entry issue is now a more meaningful Phase 1 viability/coverage problem

That is progress, and the docs should reflect that explicitly.
