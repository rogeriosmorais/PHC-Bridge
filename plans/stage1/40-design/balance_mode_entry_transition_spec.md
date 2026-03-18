# Balance Mode Entry / Transition Spec

Status: Draft implementation spec  
Scope: Stage 1 runtime entry path into Balance Perturbation Mode  
Audience: runtime, controls, debugging, and validation work for PhysAnim bridge

## 1. Purpose

This document specifies the **entry and transition contract** for Balance Perturbation Mode.

It is intentionally narrower than the higher-level design in:

- `plans/stage1/40-design/balance-perturbation-mode-design.md`

That document defines what Balance Perturbation Mode *is for* and what must be true while it is active.  
This spec defines the exact runtime path required to enter that mode safely and deterministically.

The goal is to eliminate improvisation around:

- when balance mode requests are accepted
- when requests are queued
- what conditions must be satisfied before root simulation is enabled
- how policy, body modifiers, and distal bodies are handled during transition
- what constitutes a retryable block vs a hard failure
- which logs are authoritative

## 2. Relationship to Existing Design

This spec is intended to **refine and operationalize** the existing design, not replace it.

It preserves these existing design intentions:

- Balance Perturbation Mode is a **standing balance diagnostic mode**
- perturbations are applied as **pelvis/root impulses**
- shell assistance and CharacterMovement assistance are not part of the active mode
- the runtime must not fake successful balance behavior through non-policy support
- mode behavior must remain observable and debuggable

### 2.1 Clarification to add to the existing design doc

The current design doc should explicitly state that there is a **pre-activation transition pipeline** before the mode becomes active.

Recommended wording to add:

> Before Balance Perturbation Mode becomes active, the runtime may execute a bounded entry transition whose sole purpose is to establish a valid simulation configuration for the root/pelvis and eliminate startup transients. The mode is not considered active until that transition completes successfully.

This is a clarification, not a contradiction.

## 3. Core Problem This Spec Solves

Recent failures show that the runtime is mixing up three distinct situations:

1. **Balance mode request arrives before the runtime is eligible**
2. **Runtime is eligible but transition preconditions are unsafe**
3. **Transition begins but creates a simulation spike**

These situations must be handled differently.

The missing design contract has caused repeated trial-and-error in code because the system lacked:

- an explicit finite-state model
- entry gating rules
- transition ownership rules
- recovery rules
- a clear definition of “ready”

## 4. Non-Goals

This spec does **not** redesign:

- observation construction
- action decoding
- perturbation profiles
- training semantics of the PHC policy
- walking / locomotion mode
- floor / slope / terrain behavior outside the standing-balance entry path

## 5. Terminology

### 5.1 Balance request
A user or test command asking to enter Balance Perturbation Mode, e.g. `pa.StartBalanceMode`.

### 5.2 Active mode
Balance Perturbation Mode is **active** only after transition success and explicit mode activation.

### 5.3 Queued request
A deferred request remembered by runtime and retried automatically once eligibility conditions become true.

### 5.4 Root / pelvis simulation
For this spec, “root simulation valid” means the pelvis/root body exists, is under the expected modifier/control configuration, and is actually simulating.

### 5.5 Transition
A bounded temporary procedure whose job is to move the runtime from BridgeActive to Balance Perturbation Mode safely.

### 5.6 Distal bodies
At minimum: calves, feet, balls.  
These are the most spike-prone bodies and require special handling during entry.

## 6. Required High-Level Behavior

When `pa.StartBalanceMode` is invoked:

- the runtime must never immediately “promote” into balance mode from an unsafe root state
- the runtime must never declare success while pelvis/root is still kinematic
- the runtime must never repeatedly reject forever without a convergence path
- the runtime must never enter a transition that is structurally impossible to satisfy
- the runtime must never keep policy driving the same frame that root/pelvis simulation flips on unless that exact flip is explicitly designed and validated

## 7. Authoritative Runtime States

The runtime must model entry using explicit states.

Minimum state set:

- `BridgeActive`
- `BalanceStartQueued`
- `BalanceTransition_Preflight`
- `BalanceTransition_Phase1_Prepare`
- `BalanceTransition_Phase2_RootOn`
- `BalanceTransition_Phase3_Settle`
- `BalancePerturbationActive`
- `BalanceTransitionFailed`

## 8. Request Handling Contract

A balance request may result in exactly one of these outcomes:

1. Accepted immediately into queued state
2. Rejected as invalid command context
3. Accepted and transition begins
4. Transition fails and returns to BridgeActive
5. Transition succeeds and mode becomes active

Not allowed:

- silently dropping the request
- oscillating forever between “queued” and “rejected”
- re-running an impossible preflight every frame without changing the world state
- claiming readiness based on timers alone when hard requirements are false

## 9. Queueing Rules

A request must be **queued**, not rejected, when the runtime is in a valid bridge context but not yet eligible for transition because of temporary readiness conditions.

Queue-worthy temporary blockers include:

- final-group control ramp inactive
- policy influence below required threshold
- startup handoff incomplete
- pending control-authority ramp not complete

These are **normal not-yet-ready conditions**, not failures.

Once queued, the request remains pending until one of the following happens:

- transition starts
- runtime exits BridgeActive
- user explicitly cancels
- a hard invalidation occurs (component destroyed, stop bridge, map teardown)

Only one queued balance request may exist at a time.

## 10. Preflight Contract

Preflight is a **single evaluation step** performed only when all queue gates are satisfied.

Preflight must answer:

- Is the runtime in the correct source state?
- Are all required controls/modifiers present?
- Is the policy already disabled or about to be disabled for transition?
- Is the current body simulation topology compatible with the transition design?
- Is there a valid path to root-on that can converge?

Preflight is not a polling loop.

If preflight fails because the world is not in the required shape, the runtime must:

- perform explicit recovery actions that change the shape, then
- return to queued state, or
- fail definitively and clear the request

### Critical rule

A preflight condition that is expected to change **must have an owner**.

If the code checks a condition but no system changes it, the design is invalid.

## 11. Ownership Rules for Preconditions

Every transition precondition must be categorized as one of:

- **Observed-only**
- **Transition-owned**
- **External-owned**

If a condition is Transition-owned, preflight must not reject because it is currently false.

Specifically:

- `finalGroupRampInactive` -> External-owned. Valid queue blocker.
- `policyInfluenceBelowThreshold` -> External-owned. Valid queue blocker.
- `pelvisBodyNotSimulating` -> **Transition-owned. Must not be a permanent preflight rejection.**
- `simCount=21 distalSim=16` -> not automatically invalid; if the transition needs a different topology, Phase 1 must create it.

## 12. Phase 1: Prepare

Phase 1 exists to make root-on safe.

Required actions:

- freeze new balance promotion attempts
- disable perturbation application
- disable or zero policy target writing into bodies affected by transition
- force distal bodies into the intended transition topology
- preserve current gross posture without introducing new target discontinuities
- optionally quarantine hip/thigh influence if required by implementation
- capture baseline metrics

### Policy rule

During Phase 1 and root-on, policy may continue evaluating for diagnostics, but it must **not** drive the transitioning body set unless explicitly designed and validated.

Default requirement:

- policy influence to transitioning bodies = 0 during Phase 1 and Phase 2

### Distal-body rule

If distal bodies are known spike sources, Phase 1 must explicitly move them to the chosen transition configuration:

- either forced kinematic
- or explicitly controlled under bounded reset rules

### Exit criteria from Phase 1

Phase 1 may advance only if:

- configured body topology matches the intended transition topology
- policy influence to transition-critical bodies is disabled
- baseline root linear/angular speeds are below pre-root-on limits
- no fail-stop precursor is active

## 13. Phase 2: Root On

Phase 2 is the frame or short interval where pelvis/root simulation is enabled.

Requirements:

- root modifier/control must exist and be valid
- pelvis/root simulation must become true
- no conflicting system may re-disable it in the same phase
- no shell-assist correction may inject velocity
- no target reset may produce a discontinuity on the same frame unless explicitly validated

### Hard rule about same-frame policy writes

Default contract:

- do not allow new policy-driven target writes to the pelvis transition set on the same frame root-on occurs

Immediately after root-on, record:

- root linear speed
- root angular speed
- shell offset delta
- shell velocity delta
- sim count
- max body linear speed
- max body angular speed

## 14. Phase 3: Settle

Phase 3 is a bounded settle window after root-on.

Success conditions must hold continuously for a minimum settle duration:

- pelvis/root remains simulating
- root linear speed below settle threshold
- root angular speed below settle threshold
- shell offset delta below settle threshold
- no fail-stop precursor active
- no transition abort reason active
- required body topology preserved

Failure conditions:

- root simulation drops unexpectedly
- root linear spike above abort threshold
- root angular spike above abort threshold
- shell offset/velocity spike above abort threshold
- broad-body explosion / large uncontrolled sim propagation
- fail-stop precursor
- timeout without convergence

## 15. Activation Contract

Only after Phase 3 success may the runtime enter `BalancePerturbationActive`.

Activation must perform:

- explicit state change log
- explicit “mode active” log
- enable perturbation scheduler
- enable active-mode diagnostics
- leave startup/transition logs behind

The mode is **not active** during queueing, preflight, Phase 1, or Phase 2.

## 16. Recovery Contract

When transition fails:

- stop perturbation scheduling
- clear transition-only clamps/quarantines
- disable any forced transition policy suppression not needed in BridgeActive
- restore intended BridgeActive control topology
- clear transient counters and timers
- return to `BridgeActive`
- preserve queued request only if retry is actually meaningful

Retry is allowed only if recovery changes something that could plausibly make the next attempt succeed.

If the failure reason is structural and unchanged, do not retry automatically forever.

## 17. Logging Contract

Logs must be sparse, objective, and phase-authoritative.

Keep one-shot logs for:

- request queued + reason
- queue gate satisfied
- preflight begin
- preflight reject + reason
- transition phase changes
- Phase 1 topology/config summary
- root-on summary
- settle success or failure
- final mode activation
- transition cleanup summary

Remove or throttle per-frame spam for:

- repeated pre-entry posture writes
- repeated zero-valued distal/proximal angular values
- repeated waiting messages
- repeated “routing through pipeline” text
- unchanged shell telemetry during waiting

Use stable reason strings, such as:

- `queue_final_group_ramp_inactive`
- `queue_policy_influence_below_threshold`
- `preflight_missing_root_modifier`
- `preflight_invalid_source_state`
- `phase1_topology_not_achieved`
- `phase2_root_on_spike`
- `phase3_timeout`
- `phase3_fail_stop_precursor`

## 18. Observability Requirements

The runtime must make it possible to answer these questions from logs:

1. Why was the request queued?
2. What condition allowed transition to begin?
3. Which bodies were transition-owned?
4. Was policy suppressed during root-on?
5. Did pelvis/root actually start simulating?
6. Did a spike happen before or after root-on?
7. Why did the transition fail or succeed?

## 19. Invariants

These must always hold:

- A queued request must have a concrete future satisfaction path
- Transition must not rely on a condition it does not own and that no external phase will change
- Balance mode cannot be marked active while pelvis/root is kinematic
- Policy influence cannot remain fully active across root-on unless explicitly validated by design
- A failed transition must restore a coherent BridgeActive state

## 20. Recommended Threshold Structure

Use named thresholds instead of magic numbers.

Recommended categories:

- queue gate thresholds
- preflight accept thresholds
- root-on abort thresholds
- settle success thresholds
- fail-stop thresholds

## 21. Test Matrix

Minimum required tests:

- request before final ramp complete -> queues -> transition starts later -> success
- request before policy threshold -> queues -> threshold reached -> success
- request while pelvis sim false -> queues -> transition-owned root-on -> success
- request with many distal bodies simulating -> Phase 1 reshapes topology -> success
- induced spike at root-on -> transition aborts cleanly -> returns to BridgeActive
- missing root modifier/control -> no infinite retry loop -> stable failure
- failed transition followed by corrected conditions -> second attempt can succeed

## 22. Implementation Guidance

- Separate queue gates from preflight gates
- Transition-owned conditions must not be used as preflight rejection reasons
- Make topology shaping explicit in Phase 1
- Disable conflicting authority at root-on
- Avoid combining root flip, target reset, policy write, posture rewrite, and shell correction in one uncontrolled frame
- Any automatic retry loop must have an owner, a reason, and a convergence condition

## 23. Proposed Design Decision for Current Runtime

The most coherent design is:

1. Queue until final-group ramp and minimum policy influence gate are satisfied
2. Start transition from BridgeActive while pelvis is still kinematic
3. In Phase 1:
   - suppress policy influence to transition-critical set
   - force distal bodies into the desired topology
   - preserve posture without repeated spam
4. In Phase 2:
   - enable pelvis/root simulation
   - no same-frame policy drive into the transition-critical set
5. In Phase 3:
   - require bounded settle success
6. Only then activate balance mode

This treats `pelvisBodyNotSimulating` as the reason for transition, not as a reason to reject forever.

## 24. Contradictions Check Against Existing Design

I do **not** see a required contradiction with:

- `plans/stage1/40-design/balance-perturbation-mode-design.md`

The one change I do recommend in that existing doc is to explicitly state:

- Balance Perturbation Mode requires a bounded entry transition and is only active after transition success.

## 25. Acceptance Criteria

This spec is satisfied only when:

- queued requests converge instead of looping forever
- `pelvisBodyNotSimulating` is no longer an eternal rejection
- transition ownership is explicit
- root-on spikes are attributable and bounded
- balance mode becomes active only after real root simulation validity
- logs are compact and decisive

## 26. Suggested File Placement

Recommended path in repo:

- `plans/stage1/40-design/balance-mode-entry-transition-spec.md`

Alternative:

- `plans/stage1/50-implementation/balance-mode-entry-transition-spec.md`

The first option is better if this is meant to become part of the official Stage 1 runtime contract.
