# Authority Matrix

## Purpose

This document defines subsystem ownership by runtime mode.

It is the primary operational doc for the continuous-balance rewrite.

Hard invariant:

- no subsystem may silently reclaim authority over the balance-critical chain
- no subsystem may use mesh-wide side effects to evade that invariant

## Runtime Modes

| Runtime mode | Body simulation mode | Control targets | Locomotion intent | Movement component | Capsule state | Shell correction | Resets |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BridgeActive` | startup or normal runtime ownership; continuous-balance mode inactive | startup or normal bridge | allowed by normal bridge | normal runtime ownership | normal character-movement ownership | diagnostic or disabled per runtime | normal runtime ownership |
| `BalanceActivation_Ready` | balance activation owns continuous simulation for the balance-critical chain and support set | zeroed or rebased readiness targets only | forced idle | must not reclaim balance-critical chain or support set | collision-passive; frozen at rebase origin | disabled on balance-critical chain and support set in `V0` | blocked unless explicitly terminating attempt |
| `BalanceActivation_BlendIn` | balance activation owns continuous simulation for the balance-critical chain and support set | balance activation owns blended control targets | forced idle | must not reclaim balance-critical chain or support set | collision-passive; frozen at rebase origin | disabled on balance-critical chain and support set in `V0`; helper use is failure | blocked unless explicitly terminating attempt |
| `BalanceActivation_Validate` | balance activation owns continuous simulation for the balance-critical chain and support set | balance activation owns live standing targets | forced idle | must not reclaim balance-critical chain or support set | collision-passive; frozen at rebase origin | disabled on balance-critical chain and support set in `V0`; helper use is failure | blocked unless explicitly terminating attempt |
| `BalanceActive_Standing` | balance mode owns continuous simulation for the balance-critical chain and support set | balance mode owns live control targets | standing only unless a later mode expands scope | must not reclaim balance-critical chain or support set | collision-passive; frozen at rebase origin | disabled in `V0` | explicit only |
| `BalanceActive_Recovery` | balance mode owns continuous simulation unless the attempt is being terminated | recovery-specific control targets | recovery only | must not silently reclaim | collision-passive; frozen at rebase origin | disabled in `V0` | explicit recovery semantics only |
| `SafeDenied` / `Failed` | no ambiguous overlap allowed; balance activation releases cleanly | no further standing targets | inactive | restored only after termination is explicit | restored to normal character-movement ownership | diagnostic only after termination | explicit termination handling only |

## Per-Mode Contract

| Runtime mode | Entry preconditions | Exit conditions | Fail conditions | Forbidden writes | Authoritative owner | Required emitted metrics |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `BalanceActivation_Ready` | valid bridge context; physics-asset contract satisfied; balance-critical chain and support set simulated; no movement-component reclaim; no pending reset | enter `BalanceActivation_BlendIn` when rebased targets are valid and readiness quiet window passes | physics-asset contract violation; any topology change on critical chain; loss of continuous simulation; shell helper use on critical/support set; movement reclaim; capsule contract violation | locomotion-drive writes; shell helper writes; kinematic body-mode writes on critical/support set; plant-profile swaps; capsule transform/collision writes | balance activation runtime | topology change count, authority conflict count, readiness quiet time, shell helper used flag, physics-asset baseline id |
| `BalanceActivation_BlendIn` | all `Ready` conditions hold; rebased target history exists; `ControlAuthorityAlpha=0.0`; readiness quiet gate already passed | enter `BalanceActivation_Validate` when `ControlAuthorityAlpha=1.0` and no fail condition fired | physics-asset contract violation, target discontinuity, gain/damping instability, contact/support failure, pose/reference mismatch, authority conflict, shell helper use | abrupt full-authority writes, movement-component writes, shell helper writes, reset writes, plant-profile swaps during attempt | balance activation runtime | alpha, blend start time, target discontinuity, controller effort proxy, authority conflict count, support uptime |
| `BalanceActivation_Validate` | blend complete; no fail condition in previous mode; standing timer reset | enter `BalanceActive_Standing` after contiguous hold completes | physics-asset contract violation, any support failure, instability threshold breach, topology change, non-contiguous hold, shell helper use, movement reclaim | shell helper writes, movement-component writes, topology edits on critical/support set, reset writes, plant-profile swaps during attempt | balance activation runtime | contiguous hold time, root tilt envelope, peak angular speed by family, contact uptime, COM/support proxy drift |
| `BalanceActive_Standing` | contiguous hold complete | remain active or enter recovery/termination per later mode rules | loss of standing validity, explicit recovery trigger | legacy activation writes that bypass standing mode ownership | balance mode runtime | sustained hold time, ongoing stability metrics |

## Interpretation Rules

- any overlap that is not explicitly named in this matrix is a bug
- shell correction is never a primary truth source
- resets are termination or recovery tools, not hidden continuity helpers
- movement-component ownership must not silently drag or contain the balance-critical chain or support set during balance mode
- capsule behavior must be collision-passive and frozen at the rebase origin during activation to ensure honest standing truth
- body-set ownership is a truth contract, not proof that engine effects remain local to those bodies
- Physics Control and body-modifier writes must be evaluated at both the body-set level and the mesh-wide effect level
- mesh-wide flags or blend state that materially help the balance-critical chain or support set count as authority conflicts even when the initiating write was nominally per-body
- non-critical and excluded bodies remain part of the falsification surface even though they are not primary truth sets
- if a non-critical body, excluded body, or shared mesh setting materially changes the standing outcome of the balance-critical chain or support set, the run is invalid rather than "out of scope"

## Runtime Subsystem Appendix

This appendix maps ownership to actual runtime mechanisms rather than only conceptual owners.

| Runtime responsibility | Concrete runtime mechanism | Allowed writer in `V0` |
| :--- | :--- | :--- |
| `ControlAuthorityAlpha` | `UPhysAnimComponent` balance-activation state | `UPhysAnimComponent` only |
| Body-mode changes on the balance-critical chain or support set | physics simulate-state changes and body-modifier movement-type writes | `UPhysAnimComponent` balance-activation path only |
| Control-target publication | `UPhysicsControlComponent` target writes issued by the bridge | `UPhysAnimComponent` only |
| Capsule transform and collision | `UCapsuleComponent` primitive settings | `UPhysAnimComponent` balance-activation path only |
| Mesh-wide physics side effects | skeletal-mesh-level physics blend state, `bUpdateMeshWhenKinematic`, and equivalent whole-mesh flags touched by Physics Control or body-modifier paths | `UPhysAnimComponent` balance-activation path only, with explicit diagnostics |
| Physics-asset contract verification | physics-asset identity check, constraint-profile check, physical-material check, collision-disable-table check, mass/inertia audit snapshot | `UPhysAnimComponent` pre-activation validation only |
| Plant-profile mutation | physics-asset swaps, constraint-profile swaps, runtime mass edits, collision-profile edits | forbidden during an active `V0` attempt |
| Resets | cached-target or transform reset path | explicit termination or recovery path only |
| Locomotion intent | bridge locomotion-intent path | must be inert during `V0` activation |
| Movement component authority | `CharacterMovementComponent` or equivalent, including floor finding, based movement, root motion, post-physics correction, deferred mesh movement, network correction, and depenetration paths | must be inert during `V0` activation |
| Shell helper / correction | shell-maintenance helper path | must be inert during `V0` activation |

Mesh-wide side-effect rule:

- any activation write path that can alter whole-mesh update behavior, mesh physics blending, or shared body-modifier state is a first-class authority surface
- those surfaces may not be changed implicitly by non-activation systems during `V0`
- if a nominally local write causes a mesh-wide assist that materially changes standing outcome, the run is not truth-clean
- if that assist is routed through bodies outside the truth sets, the run still fails truthfully; the truth-set boundary does not shield off-mesh contamination

Movement-component do-not-own list for `V0`:

- no floor finding or floor-adjustment ownership
- no based-movement ownership
- no regular velocity-driven actor or capsule movement ownership
- no post-physics correction ownership
- no deferred mesh movement ownership
- no root-motion application ownership
- no network smoothing or correction ownership
- no depenetration or reanchor ownership from the movement path

## Required Runtime Event Counters

The runtime must surface at least:

- authority conflict count
- mesh-wide side-effect event count
- topology change event count
- reset event count
- shell helper used event count
