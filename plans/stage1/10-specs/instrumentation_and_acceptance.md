# Instrumentation And Acceptance

## Purpose

This document defines the minimum instrumentation, run artifacts, and pass/fail rules for the continuous-balance rewrite.

Build observability before behavior.

## Run Artifact Schema

Every continuous-balance run must emit a structured artifact with the following minimum schema.

### Run Config Fields

| Field | Type | Units / format | Required |
| :--- | :--- | :--- | :--- |
| `commit_sha` | string | git SHA | yes |
| `map_name` | string | UE map name | yes |
| `character_asset` | string | asset path | yes |
| `policy_build` | string | build or model identifier | yes |
| `dt_seconds` | number | seconds | yes |
| `substeps` | integer | count | yes |
| `solver_settings` | object | named key/value set | yes |
| `mode_name` | string | runtime mode set | yes |
| `shell_mode` | string | `disabled_v0` or later explicit mode | yes |

### Summary Fields

| Field | Type | Units | Required |
| :--- | :--- | :--- | :--- |
| `run_start_time_utc` | string | ISO-8601 | yes |
| `run_duration_seconds` | number | seconds | yes |
| `terminal_state` | string | enum | yes |
| `terminal_reason_family` | string | enum | yes |
| `terminal_reason` | string | enum | yes |
| `sustained_hold_time_seconds` | number | seconds | yes |
| `contiguous_hold_time_seconds` | number | seconds | yes |
| `root_tilt_envelope_deg` | number | degrees | yes |
| `peak_angular_speed_by_family_deg_per_sec` | object | deg/s | yes |
| `contact_uptime_seconds` | number | seconds | yes |
| `control_effort_proxy` | number | unitless normalized scalar | yes |
| `authority_conflict_count` | integer | count | yes |
| `topology_change_event_count` | integer | count | yes |
| `shell_helper_used_count` | integer | count | yes |
| `support_loss_gap_max_ms` | number | milliseconds | yes |
| `contact_churn_rate_hz` | number | changes per second | yes |
| `min_support_contact_count_seen` | integer | count | yes |

### Time-Series Fields

| Field | Type | Units | Cadence | Required |
| :--- | :--- | :--- | :--- | :--- |
| `samples[].t_seconds` | number | seconds | fixed cadence | yes |
| `samples[].runtime_mode` | string | enum | fixed cadence | yes |
| `samples[].root_tilt_deg` | number | degrees | fixed cadence | yes |
| `samples[].support_contact_active` | boolean | boolean | fixed cadence | yes |
| `samples[].support_contact_count` | integer | count | fixed cadence | yes |
| `samples[].support_contact_source` | string | enum | fixed cadence | yes |
| `samples[].peak_family_angular_deg_per_sec` | object | deg/s | fixed cadence | yes |
| `samples[].control_authority_alpha` | number | `0..1` | fixed cadence | yes |
| `samples[].target_discontinuity_deg` | number | degrees | fixed cadence | yes |
| `samples[].support_proxy_world_xy_cm` | object | centimeters | fixed cadence | yes |
| `samples[].support_region_valid` | boolean | boolean | fixed cadence | yes |
| `samples[].authority_conflict_events` | integer | count in sample window | fixed cadence | yes |
| `samples[].topology_change_events` | integer | count in sample window | fixed cadence | yes |
| `samples[].terminal_reason_candidate` | string | enum or empty | fixed cadence | yes |

### Cadence And Time Windows

- sample cadence: `30 Hz` minimum
- validation hold window: contiguous interval spent in `BalanceActivation_Validate`
- sustained hold window: contiguous interval spent in `BalanceActive_Standing`
- support uptime window: total time with valid support contact during validate and standing windows

### Terminal Reason Contract

- `terminal_reason_family` may be used for coarse grouping only
- `terminal_reason` must carry the first truthful leaf-level reason that ended the run
- `samples[].terminal_reason_candidate` records the best current leaf-level reason at each sample, or empty when no terminal condition exists yet

Required leaf-level `terminal_reason` values for `V0`:

- `activation_target_discontinuity`
- `activation_unstable_gain_or_damping`
- `activation_support_failure`
- `activation_proxy_outside_support_region`
- `activation_movement_reclaim`
- `activation_shell_helper_violation`

Contact-measurement rules:

- `support_contact_source` must be `chaos_contacts` for `V0`
- `support_contact_active=true` means at least one active Chaos contact exists on either support side during the sample window
- `support_contact_count` is the count of support sides currently active: `0`, `1`, or `2`
- contact churn is counted from side-support state transitions at the sample cadence, not from raw manifold creation/destruction counts
- `support_loss_gap_max_ms` is measured from consecutive samples where `support_contact_count=0`
- support-proxy-outside-region time is measured independently from consecutive samples where `support_region_valid=false`

## Forbidden Metrics

These metrics are not allowed to stand in for success:

- phase-completion counters
- shell-lock or shell-reference status by itself
- “clean transition” counters
- retry counts that do not end in sustained standing

## Acceptance Gates

### Milestone 1

- honest continuous-physics diagnostics exist
- run artifact schema is populated
- failure can be explained without leaning on legacy phase completion

### Milestone 2

- `1.0` second contiguous stable hold exists under the continuous-balance mode

### Milestone 3

- `3.0` second contiguous stable hold exists under the continuous-balance mode

### Milestone 4

- small perturbation recovery is demonstrated after Milestone 3 is real

## Must-Fail Gates

Activation must fail if:

- the balance-critical chain loses continuous simulation
- shell helper exceeds `V0` policy by being used at all on the balance-critical chain or support set
- the movement component reclaims authority over the balance-critical chain or support set
- a topology change occurs on the balance-critical chain
- standing duration is not contiguous

Support truth must fail if:

- support-loss gap exceeds `100 ms`
- support-contact churn exceeds `12 Hz`
- minimum support-contact count drops below `1`

## Regression Gates

The rewrite branch is acceptable if it delivers:

- more honest failure
- less hidden assistance
- stronger observability

even when early visual behavior looks worse than the legacy path.

## Expected Early Regressions That Are Acceptable

- worse-looking early stance behavior
- earlier failure under continuous physics
- higher visible oscillation because grace logic is no longer hiding it
- clearer controller weakness that used to be misread as a transition problem

These are acceptable during the rewrite if instrumentation quality improves and hidden assistance decreases.
