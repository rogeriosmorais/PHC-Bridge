# Stage 2A Locomotion & Watchdog Decisions

## Locomotion Request State Management
- **Context:** The `Stage2ALocomotionRequestState` was prematurely resetting to `BalanceActiveStanding` upon transitioning into `LocomotionActiveShellDenied` during a failed locomotion request. This masked denial feedback and caused test drift.
- **Decision:** Modified `TransitionRuntimeState` to preserve request states across locomotion denial boundaries (i.e. skipping reset when entering `LocomotionActiveShell` or `LocomotionActiveShellDenied`).
- **Consequence:** `TryActivateStage2AWalkIntent` and `TryActivateStage2ATurnIntent` now guarantee precise clearance of intent and trajectory states upon failure, aligning with graph contract requirements.

## Watchdog Initialization Robustness
- **Context:** The policy liveness watchdog was susceptible to T=0 false-positives and missed initializations because it implicitly assumed `LastPolicyControlUpdateTimeSeconds` was valid immediately after `PolicyControlTicksExecuted` advanced.
- **Decision:** Hardened `IsStage2APolicyOutputActive` by requiring `LastPolicyControlUpdateTimeSeconds >= 0.0`. Added public `TestOnly` helper seams to the component to permit strict verification via unit tests (avoiding friend-class visibility issues).
- **Consequence:** The watchdog correctly filters out uninitialized states and clock drift.
