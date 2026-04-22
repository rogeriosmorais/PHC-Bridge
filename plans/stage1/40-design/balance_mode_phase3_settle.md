# Balance Mode Phase 3 Settle Spec

Status: Authoritative implementation design  
Scope: Stage 1 behavior for `BRT_Phase3_Settle` / runtime `BalanceEntry_Settle`

## 1. Purpose

This document defines the post-RootOn Settle continuity phase for Balance Mode entry.

It is authoritative for:

- the boundary between Phase 2 and Phase 3
- Settle success and timeout timing
- post-RootOn continuity checks
- emitted Phase 3 failure reasons
- current implementation notes where runtime behavior differs from the stricter target contract

## 2. Phase Boundary

Phase 3 begins only after all of the following are true:

- technical RootOn succeeded
- the Phase 2 guard window completed without terminal failure
- runtime crossed `BRT_Phase2_ReadyForPhase3`

Canonical mapping:

- internal transition phase `BRT_Phase3_Settle`
- runtime state `BalanceEntry_Settle`
- design term `Phase3_Settle`

Interpretation rule:

- Settle is its own continuity phase
- Settle is not "late Phase 2"
- Settle is not active balance mode

## 3. Target Contract Vs Current Implementation

### Target contract

Settle is the post-RootOn continuity window before activation.

It exists to prove that the accepted post-RootOn topology can hold without hidden support long enough to enter active balance truthfully.

### Current implementation

The runtime currently allows normal policy activity to resume during Settle.

That does not make Settle permissive. The continuity validator still requires:

- idle locomotion authority
- preserved shell lock
- no shell reference reseed
- no pending resets
- no topology regression
- no material shell correction
- no truthful post-RootOn instability

Implementation note:

- if root raw simulation holds but the pelvis modifier record disagrees, current runtime logs `PHASE3_ROOT_MODIFIER_DIAGNOSTIC` rather than emitting a terminal `phase3_root_modifier_mismatch`

### Current truthful frontier

In the latest truthful smoke:

- Phase 1 Prepare / LateValidate pass
- Phase 2 RootOn passes truthfully
- Phase 3 Settle emits `PHASE3_FIRST_FAILURE_AUDIT reason=phase3_material_shell_correction`
- the transition then ends immediately on safe denial with `phase3_material_shell_correction`

Interpretation:

- the current blocker has moved past the earlier Phase 1 readiness frontier and past the earlier Phase 2 spike frontier
- the active engineering question is now shell-maintenance truth in Settle, not restart-path cleanup
- Settle documentation must therefore stay focused on first-failure continuity classification rather than on post-failure recovery behavior

## 4. Timer Rules

Phase 3 timing is:

- Phase 2 hands off after `BalancePhase2GuardWindowDuration`
- Settle success requires a contiguous ready hold of `BalancePhase3RequiredStableHoldDuration`
- Settle fails with `phase3_no_convergence_path` after `BalancePhase3TimeoutDuration`

Interpretation rules:

- non-ready Settle frames reset the stable-hold timer
- Settle success is based on the ready-hold window, not merely surviving for the timeout duration
- activation is allowed only after the stable-hold requirement is satisfied

## 5. Truth Model And Continuity Checks

### 5.1 Required observables

During Settle, the runtime must keep these observables separate:

1. certified post-RootOn topology and continuity contract
2. raw post-RootOn body continuity
3. modifier-record ownership continuity
4. shell-maintenance bookkeeping state
5. shell-correction materiality
6. locomotion/reset authority state

Interpretation:

- the certified post-RootOn contract defines what continuity is supposed to hold
- raw continuity determines whether root simulation and the accepted topology are still physically present
- modifier-record ownership remains routing evidence, but does not by itself prove or disprove raw continuity
- shell-maintenance bookkeeping state means whether shell lock is still held and whether the shell reference was reseeded
- shell-correction materiality means whether shell correction has become materially active on the Settle path, not merely whether shell bookkeeping exists
- locomotion/reset authority state remains separate from shell state and must not be merged into shell-correction classification

### 5.2 Source-of-truth order

For Settle failure classification, use this order:

1. certified post-RootOn topology and continuity contract
2. raw post-RootOn body continuity
3. modifier-record ownership continuity
4. shell-maintenance bookkeeping state
5. shell-correction materiality
6. locomotion/reset authority state

Interpretation rules:

- continuity intent is not proof that the accepted post-RootOn continuity still holds
- raw continuity is the deciding proof for root-simulation loss, topology regression, and post-RootOn instability
- modifier disagreement under preserved raw continuity is routing evidence and diagnostic value, not by itself a `phase3_material_shell_correction` failure
- shell lock state and shell-reference state are bookkeeping observables; they are not identical to shell-correction materiality
- `phase3_material_shell_correction` is valid only if the earlier continuity layers still hold and shell correction is the first material failure

### 5.3 First-failure classification

Phase 3 continuity must reject truthfully when the first material failure is one of:

- root simulation dropped after RootOn
- post-RootOn instability exceeded the Settle thresholds
- the accepted topology regressed
- transition-owned shell lock was lost
- shell reference was reseeded after lock
- startup or gameplay locomotion authority reclaimed control
- a reset became pending
- shell correction became materially active

Classification rules:

- if raw root simulation is no longer present before any shell-maintenance failure wins, classify `phase3_root_simulation_dropped`
- if raw continuity still exists but post-RootOn instability exceeds Settle thresholds first, classify `phase3_post_root_on_instability`
- if the accepted post-RootOn topology no longer matches the raw state first, classify `phase3_topology_regressed`
- if transition-owned shell lock is already lost before shell correction becomes the deciding failure, classify `phase3_shell_lock_lost`
- if shell reference reseed occurs before shell correction becomes the deciding failure, classify `phase3_shell_reference_reseeded`
- if startup or gameplay locomotion authority reclaims control first, classify `phase3_startup_or_gameplay_authority_reclaimed`
- if reset-pending state returns first, classify `phase3_reset_pending`
- classify `phase3_material_shell_correction` only when the certified post-RootOn contract still stands, raw continuity still holds, modifier disagreement is not the deciding failure, shell lock still holds, the shell reference has not been reseeded, and shell correction becomes materially active first
- if raw continuity still holds and only the pelvis/root modifier record disagrees, emit `PHASE3_ROOT_MODIFIER_DIAGNOSTIC` rather than relabeling that routing mismatch as `phase3_material_shell_correction`

### 5.4 Retryability

Current Settle retry boundary:

- `phase3_material_shell_correction` is not retryable within the current attempt
- when `phase3_material_shell_correction` is the first truthful failure, the runtime should end the transition as a truthful safe denial rather than recover and auto-retry into a secondary artifact
- `phase3_no_convergence_path` must remain reserved for genuine timeout-without-earlier-material-failure cases; it must not overwrite an earlier `phase3_material_shell_correction`

Current emitted reasons:

| Reason | Meaning | Owner classification |
| :--- | :--- | :--- |
| `phase3_root_simulation_dropped` | root raw simulation is no longer present | `Phase2RootOnExecution` |
| `phase3_post_root_on_instability` | post-RootOn linear / angular instability exceeded Settle thresholds | `Phase2RootOnExecution` |
| `phase3_topology_regressed` | accepted topology no longer matches expected post-RootOn topology | `Phase2TopologyEnforcement` |
| `phase3_shell_lock_lost` | transition-owned shell lock no longer holds | `None` in current owner mapping |
| `phase3_shell_reference_reseeded` | shell reference was reseeded after lock | `None` in current owner mapping |
| `phase3_startup_or_gameplay_authority_reclaimed` | locomotion authority is no longer idle | `None` in current owner mapping |
| `phase3_reset_pending` | reset state reappeared during Settle | `Phase1ResetSuppression` |
| `phase3_material_shell_correction` | shell correction became materially active | `ShellAuthorityMaintenance` |
| `phase3_no_convergence_path` | stable-hold success never completed before timeout | `TransitionRecovery` |

Reserved / classified-but-not-currently-emitted reason:

- `phase3_root_modifier_mismatch`

## 6. Logging Rule

Settle documentation must preserve the current one-shot log expectations:

- `PHASE2_READY_FOR_PHASE3`
- `PHASE3_ENTRY_AUDIT`
- `PHASE3_PRE_GUARD_ROOT_STATE`
- `PHASE3_FIRST_FAILURE_AUDIT`
- Settle success log before activation

The Phase 3 docs must describe these as continuity-facing evidence, not as proof that balance mode is already active.

## 7. Acceptance Criteria

This spec is satisfied only when:

- Phase 3 is documented as a first-class post-RootOn phase
- `BalanceEntry_Settle` is mapped explicitly to `Phase3_Settle`
- Settle timers are explicit
- emitted Phase 3 failure reasons match current runtime
- reserved reasons are marked as reserved rather than overclaimed
- the docs distinguish target contract from current implementation where Settle policy semantics differ
