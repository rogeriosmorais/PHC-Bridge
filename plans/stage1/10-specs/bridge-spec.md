# Stage 1 PHC Bridge Spec

## Purpose

This document defines the Stage 1 `PoseSearch -> PHC -> Physics Control` runtime contract.

It covers both:

- the model-facing bridge contract
- the runtime state-machine contract required for normal bridge operation and balance entry

It also makes one distinction explicit:

- **contract correctness** and **physical viability** are not the same thing

## Scope

This spec covers:

- runtime input/output contract
- bridge runtime phases
- the relationship between normal bridge startup and balance entry
- the accepted Phase 1 topology
- the write-suppression / hold-path contract
- convergence snapshot timing
- allowed terminal states for the balance smoke
- the frozen rules implementation must follow

---

## Runtime phases

Stage 1 uses these runtime phases:

1. `Uninitialized`
2. `RuntimeReady`
3. `WaitingForPoseSearch`
4. `BridgeActive`
5. `BalanceEntry_Prepare`
6. `BalanceEntry_LateValidate`
7. `BalanceEntry_RootOn`
8. `BalanceEntry_Settle`
9. `BalanceActive`
10. `BalanceSafeDeny`
11. `Failed`

Rules:

- `BridgeActive` means the normal bridge is alive
- `BalanceEntry_*` states are public balance-entry runtime states
- `BalanceActive` is the intended successful balance runtime
- `BalanceSafeDeny` is an explicit valid terminal outcome
- balance entry is not represented by plain `BridgeActive`

---

## Normal bridge contract

Normal bridge startup may use staged non-root bring-up.

Normal staged bring-up is allowed to:

- promote non-root simulation groups gradually
- ramp control authority gradually
- ramp policy influence gradually
- unlock groups based on startup stabilization windows

Normal bring-up is not itself the balance-entry contract.

---

## Balance-entry contract

Balance entry is a separate contract layered on top of an already running bridge.

When balance mode is requested:

- the runtime must leave plain `BridgeActive`
- the runtime must enter public balance-entry states
- the accepted Phase 1 topology must be frozen explicitly
- convergence gating must use the accepted Phase 1 topology and authoritative post-update telemetry
- the path must end in either a later balance phase or explicit safe denial

The detailed Phase 1 contract is defined in:

- `plans/stage1/10-specs/balance-mode-entry-spec.md`

---

## Accepted Phase 1 topology

Under the current Stage 1 design, the accepted Phase 1 topology is:

- root: kinematic
- proximal set: simulated
- distal set: simulated
- upper body: kinematic

The accepted topology is the source of truth during Prepare and LateValidate.

### Root-side rule

The current design does **not** require the pelvis/root side to be simulating during Phase 1.

What the design requires is:

- a valid root/pelvis-side body source
- a valid uprightness source
- correct topology ownership
- sufficient dynamic stability margin

`pelvisSimulating` is diagnostic information only under the current accepted topology.

### Proximal simulated set

Current intended proximal simulated bones:

- `thigh_l`
- `thigh_r`
- `spine_01`
- `spine_02`
- `spine_03`

### Distal simulated set

Current intended distal simulated bones:

- `calf_l`
- `foot_l`
- `ball_l`
- `calf_r`
- `foot_r`
- `ball_r`

### Upper-body kinematic set

The upper body remains kinematic during Phase 1.

---

## Frozen topology rule

The accepted Phase 1 topology snapshot must be established before quiet-proof timing starts.

The frozen accepted topology is then the source of truth for:

- root ownership mode
- proximal ownership mode
- distal ownership mode
- upper-body ownership mode
- `simCount`
- `upperBodySimCount`
- suppression state during Phase 1

The accepted topology must not be recomputed from the ordinary startup bring-up counters on every frame.

---

## Prepare / LateValidate control-write contract

During:

- `BalanceEntry_Prepare`
- `BalanceEntry_LateValidate`

normal policy publishing must not run over the accepted Phase 1 set.

### Hold-only rule

Prepare and LateValidate are hold-only from the control-write side.

That means:

- only explicitly approved Phase 1 kinematic bones may receive held-pose writes
- accepted Phase 1 simulated bones must receive no target writes
- normal policy writes and held-pose writes must be tracked separately in diagnostics

This separation is part of the runtime contract, not just logging style.

---

## Suppression rule

During the accepted Phase 1 path, the logs must truthfully reflect whether:

- normal policy publishing is suppressed
- reset behavior is suppressed
- hold-only behavior is active

Any Phase 1 log that claims suppression is inactive while hold-only behavior is actually running is contract-invalid.

---

## Convergence snapshot timing rule

Prepare / LateValidate admission and failure checks must use an authoritative cached post-update convergence snapshot.

That snapshot must be populated only after the relevant runtime work for that frame, including:

- runtime tuning
- pending reset application
- control-target writes
- physics/control update
- body-motion and instability telemetry collection

Admission or terminal failure decisions must not rely on stale pre-update data when authoritative post-update data exists.

---

## Root tilt rule

Root tilt is a root-side uprightness check.

It must be computed from the authoritative root-side source, not from an arbitrary or misleading mesh-root frame.

Root tilt is not a proxy for distal-body instability.

---

## Contract correctness vs physical viability

This document defines both concepts explicitly.

### Contract correctness

The bridge is contract-correct when:

- the accepted topology is explicit and preserved
- state ownership is explicit
- suppression behavior is explicit and truthful
- write-path separation is correct
- convergence checks use authoritative telemetry
- freeze lifetime covers the full attempt
- terminal reasons are specific and truthful

### Physical viability

The accepted setup is physically viable when:

- the accepted sim set remains dynamically quiet enough to survive Prepare and LateValidate
- contact behavior does not immediately destabilize the setup
- tuning and target application do not inject unacceptable energy
- the setup has enough admission margin to survive the balance-entry path

A Phase 1 attempt may be contract-correct and still physically non-viable.

This is an allowed and meaningful result.

---

## Balance smoke terminal-state contract

`PhysAnim.PIE.BalanceModeSmoke` passes only if the runtime finishes in one of:

- `BalanceActive`
- `BalanceSafeDeny`

The smoke fails if the runtime ends in:

- `BridgeActive`
- unresolved `BalanceEntry_*` states
- `Failed`

Safe denial is an explicit terminal outcome.

Safe denial is not:

- stalling in `BridgeActive`
- bouncing indefinitely between Prepare and LateValidate
- silently timing out in an ambiguous state

---

## Observation contract

The required inference-time inputs remain:

- `self_obs`: `358` floats
- `mimic_target_poses`: `6495` floats
- `terrain`: `256` floats

These tensors are unchanged by the Phase 1 contract updates.

---

## Action contract

The model output remains:

- one action tensor of `69` floats

The action-to-target mapping contract is unchanged by the Phase 1 contract updates.

---

## Logging contract

Balance-mode logs must distinguish:

- normal bridge bring-up
- balance-entry accept
- accepted topology snapshot
- Prepare admission / blocking
- LateValidate start
- LateValidate reset / deny reason
- terminal success or safe deny
- freeze acquire and freeze release

At minimum, logs during Phase 1 must be able to answer:

- what topology was accepted
- what telemetry snapshot was used
- which threshold failed
- whether the failure was contract-level or physical-level

---

## Rejected contract shapes

The following are explicitly rejected:

- treating LateValidate as a plain continuation of normal staged bring-up
- requiring pelvis/root simulation while the accepted topology is still `root=kin`
- letting held-pose writes and normal policy writes blur together in Prepare / LateValidate
- ending the smoke in plain `BridgeActive`
- using repeated Prepare / LateValidate bouncing as a terminal outcome
- collapsing known body-motion instability into an uninformative generic result when a specific deny reason is available

---

## Acceptance criteria

This spec is satisfied only when all of the following are true:

- balance entry is documented as a separate runtime contract
- the accepted Phase 1 topology is explicit
- Prepare and LateValidate use hold-only write semantics
- convergence checks use cached post-update telemetry
- root tilt uses the authoritative root-side source
- pelvis simulation is not treated as a required Phase 1 condition under `root=kin`
- smoke terminal states are limited to `BalanceActive` or `BalanceSafeDeny`
