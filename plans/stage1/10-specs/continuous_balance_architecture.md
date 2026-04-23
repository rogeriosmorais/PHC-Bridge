# Continuous Balance Architecture

## Purpose

This is the main architecture doc for the continuous-balance rewrite.

The new system must not be described as `Prepare/LateValidate/RootOn/Settle, but cleaner`.

Core rules:

- the balance-critical proximal chain remains continuously simulated during balance mode
- the controller is blended onto an already-physical state instead of triggering a discrete ownership flip
- success is defined only by sustained standing stability over time
- shell or reference diagnostics may be recorded, but may not certify success
- any topology or ownership change in the balance-critical chain is a diagnostic event, not normal operation

## Preferred Runtime States

These are the preferred design names for the rewrite:

1. `BalanceActivation_Ready`
2. `BalanceActivation_BlendIn`
3. `BalanceActivation_Validate`
4. `BalanceActive_Standing`
5. `BalanceActive_Recovery`
6. `SafeDenied`
7. `Failed`

Compatibility mapping:

- `BridgeActive_Physical` maps to `BalanceActivation_Ready`
- `BalanceActivation_StandingValidation` maps to `BalanceActivation_Validate`

## First Rewrite Scope

The first rewrite goal is intentionally narrow.

### `V0`

- always-sim proximal chain
- idle stance
- flat ground
- no perturbation
- no locomotion authority
- no shell cleverness

### `V1`

- sustained standing

### `V2`

- recovery from small pushes

### `V3`

- locomotion coupling

Hard non-goal:

- no distal or upper-body sophistication before proximal standing is honest

## Body Sets

### Balance-Critical Proximal Chain

The first rewrite target keeps this chain continuously simulated:

- `pelvis`
- `spine_01`
- `spine_02`
- `spine_03`
- `thigh_l`
- `thigh_r`

Interpretation rules:

- this chain is the architectural center of the rewrite
- topology or ownership changes in this chain are diagnostic events
- they are not normal operation
- this chain is a primary truth set, not a claim that engine side effects stay local to these bodies

### Support Set

The support set is required so support truth is physically meaningful in `V0`:

- `calf_l`
- `calf_r`
- `foot_l`
- `foot_r`
- `ball_l`
- `ball_r`

Interpretation rules:

- the support set is continuously simulated in `V0`
- support truth may be primary only because these bodies are physical in `V0`
- kinematic containment of the support set is not allowed in `V0`
- the support set is a primary truth set, not an isolation boundary for mesh-wide runtime effects

### Upper-Body Set

- `clavicle_l`
- `upperarm_l`
- `lowerarm_l`
- `hand_l`
- `clavicle_r`
- `upperarm_r`
- `lowerarm_r`
- `hand_r`
- `neck_01`
- `head`

### Excluded Or Non-Essential Bodies

- any cosmetic, helper, or non-contact body not listed above

## Non-Critical Body Ownership

| Body set | Allowed movement type in `V0` | Target source in `V0` | May inject force indirectly into critical chain? |
| :--- | :--- | :--- | :--- |
| Upper body | simulated only in `V0` | rebased `V0` standing reference only; no advanced policy shaping | yes through normal articulation only; no per-tick kinematic forcing that stabilizes the proximal chain |
| Distal lower limbs / support set | simulated only in `V0` | rebased `V0` standing reference only; no advanced policy shaping | yes through honest contact only; no helper forcing |
| Root-adjacent but non-critical bodies | match nearest articulated parent; no ad hoc flips during an active attempt | rebased `V0` standing reference or none | yes only through normal articulation |
| Excluded bodies | disabled or passive only | none | no |

Scope rule:

- "non-critical" means "not a primary truth set," not "physically outside evaluation"
- non-critical bodies remain inside the full mesh effect domain and may still contaminate standing truth through articulation, collision, shared mesh settings, body-modifier state, or mesh-follow behavior
- if a non-critical body or mesh-wide setting materially stabilizes, drags, reanchors, or contains the balance-critical chain or support set, the run is not truth-clean

Upper-body rule:

- upper-body kinematic hold is banned in `V0`
- any future compatibility path must use a separate explicit mode name and must emit `compatibility_path_used=true`

## Truth Sets Vs Effect Domain

The rewrite distinguishes between:

- truth sets: the balance-critical chain and support set used to define standing truth
- effect domain: the full skeletal mesh and any mesh-wide runtime behavior influenced by Physics Control, body modifiers, physics blend state, mesh-follow flags, or deferred mesh updates

Interpretation rules:

- owning the balance-critical chain and support set does not imply the rest of the mesh is inert
- nominally per-body writes may still leak into whole-mesh behavior
- `V0` acceptance therefore requires both truthful continuity on the truth sets and no unauthorized mesh-wide assist that materially stabilizes, drags, or contains them
- non-critical bodies, excluded bodies, and nominally "out-of-scope" mesh regions may still falsify a run if they materially change the standing outcome of the truth sets
- truth-set membership narrows what defines standing truth; it does not narrow what can invalidate standing truth

Weak assumption explicitly rejected:

- "If we own only these bodies, the rest of the mesh is inert enough."

## Standing Reference Source

The `V0` fixed stance reference is one explicit authored standing pose, not an inferred shell state, locomotion state, or leftover transition snapshot.

`V0` standing-reference contract:

- exact source asset: one versioned authored idle-standing reference asset bound to the active Manny/Quinn-derived runtime skeleton; this is the only admissible `V0` stance source
- authored-versus-live rule: the standing reference comes from that authored asset, not from a live sampled pose, shell snapshot, locomotion frame, or previous activation attempt
- pose convention: parent-local bone rotations for every driven body in authored skeleton-reference space
- translation convention: no authored per-body translation offsets are consumed in `V0`; the authored reference is rotational plus pelvis-frame anchoring only
- velocity contract: desired local angular velocity and desired body linear velocity are zero for the standing reference
- driven sets: balance-critical chain, support set, and non-critical simulated bodies that still receive stance targets
- variant rule: `V0` uses the same authored standing-reference source asset for the balance-critical chain, support set, and upper body; no per-set derived stance variants are allowed in `V0`
- excluded sources: shell state, locomotion intent, prior handoff phase labels, and per-run helper corrections are not allowed to define the standing reference

Reference-space interpretation rules:

- authored parent-local rotations are first resolved against the active runtime skeleton mapping
- the runtime standing-reference frame is then built from one live pelvis/world capture, not from per-bone live fitting
- under arbitrary gravity-up, the runtime first constructs a gravity-aligned world frame from the captured gravity-up vector and projected pelvis yaw, then composes each body's authored parent-local rotation through the skeletal hierarchy into that frame
- rebasing changes the world-frame placement of the authored reference, not the authored local pose itself
- target publication derived from the reference is still not proof that the body achieved that pose

One-time rebasing is the only allowed adaptation step for this source in `V0`:

- capture time: exactly once at the `BalanceActivation_Ready -> BalanceActivation_BlendIn` boundary
- rebase origin: live `pelvis` world position at capture time
- rebase up axis: runtime gravity-up
- rebase yaw: live `pelvis` yaw projected around gravity-up at capture time
- rebased quantities: pelvis/world anchor, target position frame, target orientation frame, and zero-velocity target frame for every driven `V0` body
- unrebased quantities: authored parent-local pose values themselves, constraint profile selection, and control-point topology
- no per-body fitting: the runtime may not solve a custom body-by-body target pose to make the reference look easier
- no repeated rebasing: the standing reference frame remains frozen for the rest of the activation attempt

History rule:

- the one-time rebase applies to target publication and target-history initialization at blend start
- no pre-blend live target history is allowed to leak into the rebased `V0` standing-reference history
- velocity history is rebased to the standing-reference zero-velocity contract at blend entry; it is not inherited from locomotion, shell, or prior transition phases

Control-space projection rule:

- the runtime control target for each driven body is the rebased world-space body frame implied by:
  - the authored parent-local rotation for that body
  - the resolved runtime skeleton hierarchy
  - the one-time pelvis/world rebase frame
- control-point offsets are not authored by the standing-reference asset and are not derived from live pose fitting
- in `V0`, control-point offsets remain the fixed per-attempt values declared by the active Physics Control setup and are applied after standing-reference projection as part of the control primitive configuration
- parent/child dominance and target-space transforms are also fixed per attempt and may not be changed to make the authored stance easier

Physical-compatibility rule:

- the authored standing-reference asset is not assumed physically admissible by itself
- `V0` admissibility requires that exact authored stance source to be part of the audited baseline with the active physics asset, constraint profile set, collision setup, and control configuration
- if the authored reference cannot be projected into runtime control space without violating the audited plant/control baseline, the run is non-admissible rather than a controller pass/fail result

Interpretation rule:

- `V0` targets come from the authored standing reference expressed in that one rebased frame
- target publication is still not proof that the body achieved the pose

## Shell Rule

Pick one rule and make it executable:

- in `V0`, shell assistance is fully disabled on the balance-critical chain and the support set

Interpretation rules:

- shell metrics may still be logged
- shell helper application on the balance-critical chain or support set is a failure
- no helper-ceiling path exists for `V0`

There is no compatibility backdoor for shell help in `V0`.

## Support And Contact Rule

`V0` standing uses a support model built from the physical support set.

Support truth is measured by:

- contact persistence on `foot_*` and `ball_*`
- support-side uptime over the active validation window
- support loss events

Active-contact measurement contract for `V0`:

- contact source: Chaos rigid-body contact data for the support-set bodies
- walkable world support in `V0` means non-character `WorldStatic` level geometry only
- slope handling in `V0`: the accepted support contact normal must be within `5.0 deg` of gravity-up
- moving platforms are banned in `V0`; any support surface with sampled point velocity above `5.0 cm/s` does not count as walkable world support
- contacts with dynamic rigid bodies, `WorldDynamic` bodies, other characters, and the character's own bodies do not count as walkable world support
- self-contact and body-on-body character contact must be excluded explicitly before support truth is computed
- an active contact means at least one accepted Chaos contact on `foot_*` or `ball_*` against walkable world support during the current sample window
- traces may be logged as secondary diagnostics, but they do not define support truth in `V0`
- contact aggregation rule: `foot_* OR ball_*` on the same side counts as that side being in support
- support-hull points come from the accepted world-space manifold point with greatest penetration depth per contributing support body in the final qualifying Chaos substep of the frame
- substep contacts are reduced into the sampled truth signal with substep debouncing first; transient single-substep flips do not count as support-state changes
- churn counting is evaluated at substep truth cadence after debounce, then summarized into the `30 Hz` instrumentation stream; each false->true or true->false side-support state transition counts as one churn event

`V0` numeric support thresholds:

- minimum support-contact count: `1`
- one foot is sufficient in `V0`; both feet are not required
- max support-loss gap: `100 ms`
- max contact-churn rate: `12 Hz` aggregated across both sides during `BalanceActivation_Validate`
- support-loss timer and support-proxy-outside-region timer are independent timers; either one may fail the run first

Support failure means any of:

- all support contacts absent for longer than the configured tolerance window
- contact churn exceeds the allowed event rate while standing still fails
- support truth depends on non-physical helper behavior

Support truth precedence:

- if support truth fails, the run fails even if COM or root-side metrics still look temporarily acceptable

### Physics Asset Contract

`V0` is defined against one audited physical plant, not just against runtime ownership rules.

Interpretation rule:

- if the active physics asset violates this contract, the run is not admissible as a `V0` controller evaluation

Required `V0` plant prerequisites:

- physics-asset identity: the active Manny/Quinn-derived skeletal asset, physics asset, authored constraint-profile set, physical-material set, and collision-disable table must match a declared audited `V0` baseline
- mass distribution: total character mass must remain within `+/- 5%` of the audited `V0` baseline, and family mass totals for the balance-critical chain and support set must remain within `+/- 10%` of that baseline
- inertia expectations: principal inertia components for truth-set bodies must remain within `+/- 15%` of the audited baseline and may not be degenerate or silently recomputed to materially different values at runtime
- constraint policy: truth-set bodies must use the approved authored `V0` constraint profile only; runtime widening, loosening, or profile swapping during an activation attempt is forbidden
- self-collision policy: self-contact within the support set and between the support set and the proximal chain must follow the approved collision-disable table so self-contact cannot become hidden support or churn noise
- foot/support collision filtering: `foot_*` and `ball_*` must collide with walkable world support; dynamic-body, self-body, and character-body contacts may be logged, but may not count as support truth
- upper-body collision policy: upper-body collision is disabled for `V0` acceptance and may not create environmental support, containment, or stabilization forces
- solver-sensitive damping assumption: body-level linear/angular damping and physical-material damping/friction behavior must remain at the declared audited baseline; runtime claims of controller success may not depend on untracked plant-side damping edits
- support-geometry quality: `calf_*`, `foot_*`, and `ball_*` bodies must use authored collision geometry that is bilateral, non-degenerate, and broad enough to produce stable plantar support contacts on flat ground; capsule-tip or needle-contact support geometry is not admissible for `V0`

## Capsule Contract

The character capsule (`UCapsuleComponent`) is part of the physical plant and must not provide hidden assistance.

Required `V0` capsule rules:

- **Collision**: Capsule collision with the world (`WorldStatic`, `WorldDynamic`) and with the character's own bodies must be DISABLED during an active attempt. The capsule may not provide physical support, containment, or depenetration forces to the simulating mesh.
- **Gravity**: Capsule gravity must be DISABLED. The capsule must not fall and drag the skeletal mesh via the root attachment.
- **Transform**: The capsule must remain frozen at the world-space rebase origin and yaw captured at blend start. It must not "shadow-follow" the simulating pelvis in a way that injects forces or constrains the root's physical freedom.
- **Attachment**: The skeletal mesh's relationship to the capsule must not result in "hard" kinematic containment. The simulating bodies must be physically free to fall or move away from the capsule's origin if the controller fails.

Weak assumption rejected:
- "If the movement component is idle, the capsule state is a non-material implementation detail."

## What Is Explicitly Not Allowed

- treating phase completion as success
- treating shell status as proof of balance
- treating a protected ownership flip as the intended activation mechanism
- reintroducing grace logic to hide controller weakness
- adding distal or upper-body sophistication before proximal standing is honest
- allowing hidden shell assistance on the balance-critical chain or support set in `V0`

## Success Definition

The current rewrite success ladder is:

- Milestone 1: honest continuous-physics diagnostics
- Milestone 2: `1.0` second stable hold
- Milestone 3: `3.0` second stable hold
- Milestone 4: small perturbation recovery

The production benchmark remains:

- `BalanceActive_Standing` held continuously for `3.0` seconds

For `V0` acceptance:

- `BalanceActive_Recovery` is non-authoritative
- recovery behavior must not be used to satisfy standing success

## Expected Early Regressions That Are Acceptable

The rewrite may initially:

- fail more obviously
- expose controller weakness earlier
- expose authority conflicts earlier
- remove protective guard or grace behavior that previously hid instability

That is acceptable if the new mode is more honest about why standing fails.
