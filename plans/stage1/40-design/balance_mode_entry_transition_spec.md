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

This document also establishes the **entry-phase contract boundary**:

- this spec defines the state machine, ownership rules, gating, recovery, and phase success/failure semantics
- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md` defines the concrete stabilization recipe for Phase 1
- `plans/stage1/40-design/balance_mode_phase2.md` defines the root-on execution contract, denial path, and immediate guard-window rules

This spec also defines the contract boundary between:

- a valid Phase 1 late-validation success state
- a valid Phase 2 entry state

Those states are no longer equivalent by default.

True root-on success also requires an explicit end-to-end shell/capsule ownership contract:

- who owns shell/capsule authority before Phase 2
- who owns it during root-on and Phase 3 settle
- when that ownership may be transferred again

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

### 2.2 Relationship to the Phase 1 stabilization spec

This document deliberately does **not** try to encode the entire Phase 1 implementation recipe inline.

The companion document:

- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`

defines:

- authoritative Phase 1 bone sets
- authoritative Phase 1 target topology
- Phase 1 authority suppression rules
- posture-preservation behavior
- quiet-window metrics and hold rules
- Phase 1 failure classes
- Phase 1 recovery and retry rules

If there is ambiguity about how Phase 1 is supposed to converge, the Phase 1 stabilization spec is authoritative.

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

It also does not duplicate the detailed, bone-by-bone stabilization recipe for Phase 1.  
That material belongs in the dedicated Phase 1 stabilization spec.

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

### 5.7 Transition-critical set
The subset of bodies whose topology, target continuity, and policy suppression directly determine whether root-on is safe.

The exact composition of this set, for Phase 1 purposes, is defined in:

- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`

## 6. Required High-Level Behavior

When `pa.StartBalanceMode` is invoked:

- the runtime must never immediately “promote” into balance mode from an unsafe root state
- the runtime must never declare success while pelvis/root is still kinematic
- the runtime must never repeatedly reject forever without a convergence path
- the runtime must never enter a transition that is structurally impossible to satisfy
- the runtime must never keep policy driving the same frame that root/pelvis simulation flips on unless that exact flip is explicitly designed and validated

Additionally:

- Phase 1 must not be treated as a passive wait state
- Phase 1 must create a real convergence path toward root-on
- a clean Phase 1 is not sufficient if Phase 2 root-on still produces a deterministic spike
- if Phase 1 can only “sometimes” converge through unowned retries or incidental runtime drift, the design is not yet valid
- Phase 1 late-validation minimum success must not be treated as a Phase 2 handoff point unless the additional pre-root-on safety proof is already true

## 7. Authoritative Runtime States

The runtime must model entry using explicit states.

Minimum state set:

- `BridgeActive`
- `BalanceStartQueued`
- `BalanceTransition_Preflight`
- `BalanceTransition_Phase1_Prepare`
- `BalanceTransition_Phase1_LateValidate`
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

### 11.1 Quiet-window ownership

The quiet window used by Phase 1 is owned by `BalanceQuietProofAccumulator`.

That owner is responsible for:

- starting accumulation when Phase 1 quiet conditions first become true
- resetting accumulation when any quiet condition becomes false
- invalidating the proof when contamination, locomotion entry, or fail-stop occurs
- reporting the actual accumulated duration to Phase 1 and Phase 2 gating

If the design expects the quiet window to become true but no accumulator owns it, the transition contract is incomplete.

## 11. Ownership Rules for Preconditions

Every transition precondition must be categorized as one of:

- **Observed-only**
- **Transition-owned**
- **External-owned**

If a condition is Transition-owned, preflight must not reject because it is currently false.

Specifically:

- `finalGroupRampInactive` -> External-owned by the BridgeActive bring-up controller (`AdvanceBringUpState` / final-group control-ramp logic). Valid queue blocker.
- `policyInfluenceBelowThreshold` -> External-owned by the BridgeActive policy-influence ramp controller (`CalculateCurrentPolicyInfluenceAlpha` lifecycle after final-group settle). Valid queue blocker.
- `pelvisBodyNotSimulating` -> **Transition-owned. Must not be a permanent preflight rejection.**
- `simCount=21 distalSim=16` -> not automatically invalid; if the transition needs a different topology, Phase 1 body-modifier topology shaping owns changing it.
- `shell/capsule authority still owned by gameplay systems` -> Transition-owned once the success path commits to `TransitionOwnedShellLocked`; startup movement lock may seed suppression, but the dedicated balance-entry shell-lock lifecycle owns the actual transfer

### 11.2 Owner map for changing conditions

Any condition that is expected to become true later must name the system responsible for making it true.

- `finalGroupRampActive` owner = BridgeActive bring-up controller
- `policyInfluenceAtThreshold` owner = BridgeActive policy-influence ramp controller
- `transitionTopologyAchieved` owner = Phase 1 body-modifier topology shaping
- `policySuppressionAppliedToTransitionSet` owner = Phase 1 transition policy-routing logic
- `pendingCachedResetsDischargedOrPrevented` owner = Phase 1 reset suppression / transition reset policy
- `upperBodyOwnershipModeStabilized` owner = Phase 1 upper-body ownership controller
- `TransitionOwnedShellLocked` active = balance-entry shell-authority transfer lifecycle
- `shellReferenceReanchoredBeforeProof` = balance-entry shell-authority transfer lifecycle
- `startupUnlockSuppressedDuringEntry` = startup movement-lock / balance-entry shell-lock arbitration logic
- `PreRootOnShellSafetyProof` inputs become valid = combination of Phase 1 topology shaping, shell-authority transfer, and startup/gameplay suppression logic
- `pelvisBodySimulating` at root-on = Phase 2 root-on body-modifier flip
- `postRootOnTopologyPreserved` = Phase 2 body-modifier/runtime-mode enforcement
- `postRootOnShellAuthorityPreserved` = transition-owned shell-lock maintenance through guard window
- `shell/capsule authority handed to BalanceActiveShellAuthority` = Phase 3 settle / ownership-handoff logic

Ownership constraint:

- during balance entry transition, only `BalanceReadyTransition` Phase 2 / guard-window logic may enable pelvis simulation
- the general component bring-up/runtime-mode pipeline may shape non-root topology during Phase 1, but it must not compete for pelvis/root simulation enablement

### 11.1 Shell / capsule ownership classes

Balance entry must treat shell/capsule ownership as an explicit domain.

Minimum ownership classes:

- `GameplayShellAuthority`
- `TransitionOwnedShellLocked`
- `BalanceActiveShellAuthority`

## 12. Phase 1: Prepare

Phase 1 exists to make root-on safe.

Required actions:

- freeze new balance promotion attempts
- disable perturbation application
- disable or zero policy target writing into bodies affected by transition
- force the transition-critical topology required for safe root-on
- preserve current gross posture without introducing new target discontinuities

Critical clarification:

- meeting the late-validation minimum quiet/sustain window is a Phase 1 success condition
- it is not, by itself, permission to enter Phase 2
- Phase 2 entry additionally requires an explicit pre-root-on shell-correction safety proof

If that proof is absent, the runtime must remain in a Phase 1 safe-denial-capable state or deny explicitly without attempting root-on.
- optionally quarantine hip/thigh influence if required by implementation
- capture baseline metrics

For the first true root-on-success path, Phase 1 must also:

- intentionally converge to `RootCoupledReadyHandoff`
- transfer shell/capsule authority from `GameplayShellAuthority` to `TransitionOwnedShellLocked`
- keep that authority stable through the shell proof window

The detailed implementation contract for these actions is defined in:

- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`

### Phase 1 output contract

Phase 1 does not merely produce a boolean “ready” signal.

It must produce a **certified handoff state** consumed by Phase 2.

Minimum certified handoff fields:

- topology classification for root, proximal, distal, and upper-body sets
- `simCount`
- `proximalSimCount`
- `distalSimCount`
- policy suppression state for the transition-critical set
- control-authority settled state
- bounded target continuity summary for the transition-critical set
- quiet-proof duration actually achieved
- late-validation sustain duration achieved under initial policy influence
- upper-body stability classification at handoff
- shell authority mode at handoff
- whether shell reference was re-anchored before the proof window
- whether shell reference was reseeded after lock
- whether gameplay CharacterMovement/capsule ownership remained suppressed

If the runtime cannot report these fields coherently, it is not ready for Phase 2.

### Phase 1 late-validation rule

Phase 1 readiness must be split into two proofs:

- `Phase1QuietProof`
- `Phase1LateValidateProof`

`Phase1QuietProof` demonstrates that the shaped pre-root-on topology is quiet before policy influence resumes.
`Phase1LateValidateProof` demonstrates that the same handoff remains stable during a bounded initial-policy-influence sustain window.

The quiet proof window must use the documented minimum:

- `BalanceModeQuietRequiredSeconds = 1.0`

Phase 2 must not consume a handoff certified only by the first proof.

Late validation must explicitly prove all of the following:

- certified topology still holds after policy influence begins
- upper-body ownership and anchoring policy remain in the documented configuration
- sim coverage does not collapse materially away from the documented certified handoff topology
- target continuity remains within the named Phase 2 entry envelope
- no late cached-target reset or topology flip reappears

### Policy rule

During Phase 1 and root-on, policy may continue evaluating for diagnostics, but it must **not** drive the transitioning body set unless explicitly designed and validated.

Default requirement:

- policy influence to transitioning bodies = 0 during Phase 1 and Phase 2

### Distal-body rule

If distal bodies are known spike sources, Phase 1 must explicitly move them to the chosen transition configuration:

- either forced kinematic
- or explicitly controlled under bounded reset rules

The transition design must choose one intended pre-root-on topology and document it explicitly.

Not allowed:

- describing Phase 1 as a pure kinematic freeze while runtime logic depends on preserved non-root sim coverage
- allowing multiple incompatible handoff topologies without explicit classification

### Phase 1 compliance rule

Phase 1 is compliant only if it reaches readiness through the owned topology-and-authority shaping procedure defined by the Phase 1 stabilization spec.

It is not compliant if readiness is reached through:

- incidental drift
- unowned retries
- repeated recovery loops with no new convergence evidence
- topology states that still violate the Phase 1 target contract

### Exit criteria from Phase 1

Phase 1 may advance to late validation only if:

- configured body topology matches the intended transition topology
- policy influence to transition-critical bodies is disabled
- certified handoff fields are populated and internally consistent
- target continuity for the transition-critical set is within named entry bounds
- baseline root linear/angular speeds are below pre-root-on limits
- no fail-stop precursor is active
- no pending cached reset remains
- no transition-local hazard remains active

Phase 1 may advance from late validation to Phase 2 only if:

- the certified handoff payload still matches the runtime state after bounded initial policy influence
- upper-body stability remains within the late-validation sustain envelope
- sim coverage remains within the documented late-validation topology envelope
- no new target-discontinuity or topology-regression blocker appeared during sustain
- late-validation sustain duration completed continuously
- the resulting handoff topology is explicitly declared Phase-2-root-on-ready by the contract, or Phase 2 must deny safely without attempting root-on

Interpretation rule:

- `UpperOnlySafeDenyHandoff` is valid if and only if the Phase 1 stabilization spec declares it as the documented handoff mode
- `RootCoupledReadyHandoff` is valid if and only if the Phase 1 stabilization spec declares it as the documented handoff mode
- Phase 2 must validate against the documented topology class rather than against an implicit requirement that proximal/distal simulation remain non-zero
- `UpperOnlySafeDenyHandoff` is a safe-denial state
- `RootCoupledReadyHandoff` may be a root-on-ready state only if the explicit root-on-readiness proof is also true

### Handoff invalidation rule

Phase 1 readiness is revocable.

If any certified handoff field regresses after Phase 1 success but before or during Phase 2 entry, the runtime must:

- invalidate the previous readiness proof
- log the reason for invalidation
- deny Phase 2 entry or return to Phase 1, depending on ownership

Examples of invalidating regressions:

- sim coverage collapses below the certified handoff state
- policy suppression is no longer active
- target continuity exceeds the named entry bounds
- a cached reset or topology flip becomes pending
- upper-body anchoring/stability policy no longer matches the certified handoff topology

### 12.1 Handoff invalidation owner map

When a certified handoff regresses, the next action must be owned by the subsystem that can actually change the regressed condition.

- `sim coverage collapses below certified handoff` -> owner = Phase 1 body-modifier topology shaping; return to Phase 1, not queue-only denial
- `policy suppression no longer active` -> owner = Phase 1 transition policy-routing logic; return to Phase 1 unless the source runtime exited BridgeActive
- `target continuity exceeds named bounds` -> owner = Phase 1 hold-reference / control-target continuity logic; return to Phase 1
- `cached reset becomes pending` -> owner = Phase 1 reset suppression / recovery logic; return to Phase 1 or recovery
- `upper-body ownership/stability regresses` -> owner = Phase 1 upper-body ownership controller; return to Phase 1
- `shell lock released or reseeded` -> owner = balance-entry shell-authority transfer lifecycle; deny Phase 2 and return to Phase 1 only if that lifecycle can re-establish proof
- `startup/gameplay ownership reclaims shell lock` -> owner = startup-vs-balance shell ownership arbitration logic; deny Phase 2 and route through recovery if arbitration is not immediately restorable

## 13. Phase 2: Root On

Phase 2 is the frame or short interval where pelvis/root simulation is enabled.

Phase 2 may begin only from a still-valid certified Phase 1 handoff state.

Requirements:

- root modifier/control must exist and be valid
- pelvis/root simulation must become true
- no conflicting system may re-disable it in the same phase
- no shell-assist correction may inject velocity
- no target reset may produce a discontinuity on the same frame unless explicitly validated
- Phase 1 handoff certification must still be valid at the moment Phase 2 begins

For the first permitted root-on-success path, the contract is:

- certified handoff topology = `RootCoupledReadyHandoff`
- pre-root-on readiness classification = `root_coupled_ready`
- `PreRootOnShellSafetyProof` = true at the moment Phase 2 begins
- `shellAuthorityMode = TransitionOwnedShellLocked`
- distal set remains kinematic
- proximal set remains simulated
- upper-body ownership remains unchanged from late validation through the Phase 2 guard window
- shell reference is re-anchored once before the proof window and not reseeded before Phase 3
- startup/gameplay unlock paths must not regain shell/capsule ownership before Phase 3 success

### Phase 2 denial rule

Phase 2 must have an explicit safe denial path.

If entry preconditions are not satisfied at the moment of Phase 2 entry, the runtime must not “try anyway.”

Allowed outcomes:

- `PHASE2_DENIED <reason>` and return to Phase 1 / queued recovery path
- definitive transition failure if the condition is structural or non-retryable

Denial is a valid safe outcome.  
It is not equivalent to a root-on attempt.

### Hard rule about same-frame policy writes

Default contract:

- do not allow new policy-driven target writes to the pelvis transition set on the same frame root-on occurs

Immediately after root-on, record:

- root linear speed
- root angular speed
- shell offset delta
- shell velocity delta
- sim count
- proximal sim count
- distal sim count
- max body linear speed
- max body angular speed
- target continuity summary before and after root-on

### Phase 2 abort rule

A Phase 2 root-on spike is a first-class transition failure.

If root-on causes a spike above named abort thresholds, the runtime must:

- abort the transition
- classify the failure as `phase2_root_on_spike` or a more specific reason
- recover to coherent `BridgeActive`
- forbid immediate automatic re-entry unless recovery produced a materially different, provably retryable state

### Phase 2 retry prohibition

A Phase 2 failure must not immediately recycle into Phase 1 just because the request is still pending.

Automatic retry after Phase 2 failure is allowed only if all are true:

- recovery completed and restored coherent `BridgeActive`
- the previous failure class is marked retryable
- recovery changed something material about topology, authority, or thresholds
- a fresh BridgeActive quiet proof occurred after recovery
- retry budget is not exceeded

If these conditions are not met, the request must be cleared or remain blocked pending explicit user action.

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
- transition/balance runtime still owns shell/capsule authority
- no shell-reference reseed occurs
- gameplay CharacterMovement/capsule correction does not reactivate

Failure conditions:

- root simulation drops unexpectedly
- root linear spike above abort threshold
- root angular spike above abort threshold
- shell offset/velocity spike above abort threshold
- broad-body explosion / large uncontrolled sim propagation
- fail-stop precursor
- timeout without convergence
- gameplay shell/capsule ownership returns before settle success

### 14.1 Phase 3 ownership-handoff contract

Phase 3 must explicitly hand shell/capsule ownership from `TransitionOwnedShellLocked` into `BalanceActiveShellAuthority`.

## 15. Activation Contract

Only after Phase 3 success may the runtime enter `BalancePerturbationActive`.

Activation must perform:

- explicit state change log
- explicit “mode active” log
- enable perturbation scheduler
- enable active-mode diagnostics
- leave startup/transition logs behind
- declare the active-mode shell authority owner explicitly

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

### Recovery coherence rule

Recovery is not complete unless the runtime has returned to a **single coherent BridgeActive state**.

A recovered state is not coherent if any of the following remain true:

- pelvis/root simulation topology is still transition-shaped
- transition-local policy suppression remains half-applied
- transition-local pending resets remain armed
- the previous fail-stop precursor state is still active
- the system is effectively still in a post-root-on topology while claiming BridgeActive
- transition-owned shell lock remains active while claiming normal gameplay shell ownership

### Retry evidence rule

Automatic retry requires **new evidence**, not just a repeated tick.

Minimum evidence must include:

- completed recovery
- materially changed state or cleared hazard
- fresh quiet proof in BridgeActive after recovery

Without that evidence, preserving the queued request is not meaningful.

## 17. Logging Contract

Logs must be sparse, objective, and phase-authoritative.

Keep one-shot logs for:

- request queued + reason
- queue gate satisfied
- preflight begin
- preflight reject + reason
- transition phase changes
- Phase 1 topology/config summary
- Phase 1 late-validation start / reset / success
- root-on summary
- settle success or failure
- final mode activation
- transition cleanup summary
- shell-authority transfer begin / success / failure
- shell-reference re-anchor
- shell-reference reseed after lock

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
- `phase1_late_validate_sim_coverage_regressed`
- `phase1_late_validate_upper_body_unstable`
- `phase1_late_validate_target_discontinuity`
- `phase2_root_on_spike`
- `phase3_timeout`
- `phase3_fail_stop_precursor`

### Additional logging requirement for retry loops

If a transition attempt is retried automatically, the logs must show:

- why retry is allowed
- what changed since the last failure
- whether BridgeActive quiet proof was re-established
- remaining retry budget

This is required so repeated attempts are distinguishable from an accidental infinite loop.

## 18. Observability Requirements

The runtime must make it possible to answer these questions from logs:

1. Why was the request queued?
2. What condition allowed transition to begin?
3. Which bodies were transition-owned?
4. Was policy suppressed during root-on?
5. Did pelvis/root actually start simulating?
6. Did a spike happen before or after root-on?
7. Why did the transition fail or succeed?
8. Why was an automatic retry allowed or denied?
9. Who owned shell/capsule authority at each phase boundary?
10. Did startup unlock logic interfere with balance entry?
11. Was shell reference re-anchored exactly once and then held?
12. Did Phase 3 finish with a coherent active-mode owner?

## 19. Invariants

These must always hold:

- A queued request must have a concrete future satisfaction path
- Transition must not rely on a condition it does not own and that no external phase will change
- Balance mode cannot be marked active while pelvis/root is kinematic
- Policy influence cannot remain fully active across root-on unless explicitly validated by design
- A failed transition must restore a coherent BridgeActive state
- A Phase 2 root-on spike must not silently feed an immediate retry loop from a contaminated post-failure state
- startup readiness must not release shell/capsule ownership while balance entry still requires transition-owned shell lock
- root-on success is incomplete until post-root-on shell authority is coherent in Phase 3 / active mode

## 20. Recommended Threshold Structure

Use named thresholds instead of magic numbers.

Recommended categories:

- queue gate thresholds
- preflight accept thresholds
- root-on abort thresholds
- settle success thresholds
- fail-stop thresholds
- retry eligibility thresholds
- retry cooldown thresholds
- retry budget limits

## 21. Test Matrix

Minimum required tests:

- request before final ramp complete -> queues -> transition starts later -> success
- request before policy threshold -> queues -> threshold reached -> success
- request while pelvis sim false -> queues -> transition-owned root-on -> success
- request with many distal bodies simulating -> Phase 1 reshapes topology -> success
- request reaches Phase 2 with invalidated handoff state -> Phase 2 denied safely -> no root-on attempt
- request reaches Phase 2 with excessive target discontinuity -> Phase 2 denied or failed explicitly before root-on
- request follows the true success path -> `RootCoupledReadyHandoff` reached -> shell lock active -> root-on succeeds -> Phase 3 settle succeeds
- startup-ready unlock condition fires during balance entry -> shell ownership does not return to gameplay
- shell reference re-anchor occurs twice after lock -> Phase 2 denied safely
- root-on succeeds technically but gameplay shell ownership returns during Phase 3 -> settle fails explicitly
- induced spike at root-on -> transition aborts cleanly -> returns to BridgeActive
- missing root modifier/control -> no infinite retry loop -> stable failure
- failed transition followed by corrected conditions -> second attempt can succeed
- Phase 2 root-on spike followed by unchanged recovery state -> no immediate automatic retry
- Phase 2 root-on spike followed by materially different recovery and fresh quiet proof -> retry may occur if within budget

## 22. Implementation Guidance

- Separate queue gates from preflight gates
- Transition-owned conditions must not be used as preflight rejection reasons
- Make topology shaping explicit in Phase 1
- Implement Phase 1 according to the dedicated Phase 1 stabilization spec
- Disable conflicting authority at root-on
- treat startup movement lock and balance-entry shell lock as separate lifecycles
- move shell/capsule ownership with explicit transition-owned functions, not incidental startup codepaths
- Avoid combining root flip, target reset, policy write, posture rewrite, and shell correction in one uncontrolled frame
- Any automatic retry loop must have an owner, a reason, a convergence condition, and a retry budget
- Treat repeated Phase 2 root-on spikes as evidence of a design defect, not as something to brute-force through retries

## 23. Proposed Design Decision for Current Runtime

The most coherent design is:

1. Queue until final-group ramp and minimum policy influence gate are satisfied
2. Start transition from BridgeActive while pelvis is still kinematic
3. In Phase 1:
   - suppress policy influence to transition-critical set
   - force the transition-critical topology required by the Phase 1 stabilization spec
   - preserve posture without repeated spam
   - prove readiness through a real quiet-window hold
   - run a bounded late-validation sustain under initial policy influence
   - transfer shell/capsule ownership into `TransitionOwnedShellLocked`
   - emit a certified handoff state only after both proofs succeed
4. In Phase 2:
   - validate that the certified handoff state is still true
   - deny entry safely if the handoff proof has regressed
   - require transition-owned shell lock before root-on
   - enable pelvis/root simulation
   - no same-frame policy drive into the transition-critical set
   - abort cleanly if root-on spike thresholds are exceeded
5. In Phase 3:
   - require bounded settle success
   - hand shell/capsule ownership into the documented active-mode owner
6. Only then activate balance mode

This treats `pelvisBodyNotSimulating` as the reason for transition, not as a reason to reject forever.

It also treats repeated `phase2_root_on_spike` failures as a transition-design problem, not as a reason for indefinite automatic retries.

## 24. Contradictions Check Against Existing Design

I do **not** see a required contradiction with:

- `plans/stage1/40-design/balance-perturbation-mode-design.md`

The one change I do recommend in that existing doc is to explicitly state:

- Balance Perturbation Mode requires a bounded entry transition and is only active after transition success.

I also do not see a contradiction with the new Phase 1 stabilization spec.  
The documents are complementary:

- this doc defines the entry contract
- the Phase 1 stabilization spec defines the Phase 1 convergence mechanism

## 25. Acceptance Criteria

This spec is satisfied only when:

- queued requests converge instead of looping forever
- `pelvisBodyNotSimulating` is no longer an eternal rejection
- transition ownership is explicit
- root-on spikes are attributable and bounded
- repeated Phase 2 spike failures do not automatically recycle forever without new convergence evidence
- balance mode becomes active only after real root simulation validity
- logs are compact and decisive

## 26. Suggested File Placement

Recommended path in repo:

- `plans/stage1/40-design/balance_mode_entry_transition_spec.md`

Companion document:

- `plans/stage1/40-design/balance_mode_phase1_stabilization_spec.md`

Alternative:

- `plans/stage1/50-implementation/balance-mode-entry-transition-spec.md`

The first option is better if this is meant to become part of the official Stage 1 runtime contract.
