# Balance-First Refactor Migration Plan

## 1. Purpose

This document defines the compile-safe migration from the legacy balance-entry path to the balance-first continuous-physics path.

It is not a feature roadmap. It defines:

- current legacy call graph
- extraction seams
- first data structures
- dependency direction
- exact commit order
- test wiring
- proof that Slice 1 has no runtime dependency

## 2. Hard Migration Rules

- No runtime state-machine edits before pure support logic and validator logic compile green.
- No production behavior change in a slice whose purpose is extraction scaffolding.
- New pure modules must be additive first; legacy code must not call them until adapter slices.
- Each commit must compile by itself.
- Each deterministic behavior change must have a failing automation test first.
- Old symbols and public runtime states remain available until the final cutover.

## 3. Current Legacy Call Graph

The active balance-entry runtime still flows through the legacy transition controller:

1. `UPhysAnimComponent::TickComponent`
2. `UPhysAnimComponent::TickBalanceMode`
3. `FPhysAnimBalanceReadyTransition::Start`
4. `FPhysAnimBalanceReadyTransition::Tick`
5. phase helpers in:
   - `PhysAnimBalanceReadyTransition.Readiness.cpp`
   - `PhysAnimBalanceReadyTransition.LateValidation.cpp`
   - `PhysAnimBalanceReadyTransition.Certification.cpp`
   - `PhysAnimBalanceReadyTransition.PolicyAndShell.cpp`
   - `PhysAnimBalanceReadyTransition.Diagnostics.cpp`
6. public state mapping through:
   - `UPhysAnimComponent::MapBalanceTransitionPhaseToRuntimeState`
   - `UPhysAnimComponent::TransitionRuntimeState`

Runtime and reporting side effects currently pass through:

- `PhysAnimComponent.Balance.cpp` for balance request, readiness, public state, recovery, and cancellation behavior
- `PhysAnimBridge.cpp` for bridge math, policy-facing data, action conditioning, and low-level helper logic
- `PhysAnimComparisonSubsystem.cpp` and `PhysAnimPhase1AutoCalibSubsystem.cpp` for presentation and artifact-style reporting

This migration must not rewrite that call graph directly in Slice 1.

## 4. Extraction Seams

### `PhysAnimSupportTruth`

Owns deterministic support math only. This is the only seam implemented in Slice 1.

New files:

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`

Allowed inputs:

- plain structs
- arrays
- numbers
- enums
- value types such as `FVector2D`, `TArray`, `TOptional`, and `FName`

Allowed outputs:

- pure result structs
- enum values
- numeric metrics
- nullable value fields represented with `TOptional` or equivalent value-only types

Allowed dependencies:

- `Core`

Forbidden dependencies:

- `UObject`
- `UPhysAnimComponent`
- `UPhysicsControlComponent`
- `USkeletalMeshComponent`
- component pointers of any kind
- `FBodyInstance`
- `UWorld`
- `AActor`
- direct Chaos body handles
- `Chaos` runtime objects

Hard rule: Slice 1 fails review if any pure support function depends on live Unreal runtime objects.

### `Runtime Adapter`

Not part of Slice 1.

Owns later conversion from live Unreal/Chaos data into pure support and validator structs.

Allowed later inputs:

- skeletal mesh/component state
- body instances
- contact/manifold data
- Physics Control data
- actor/capsule/CMC state

Allowed later outputs:

- `PhysAnimSupportTruth` input structs
- `PhysAnimValidators` snapshot structs

Forbidden in this seam:

- terminal arbitration
- support math
- artifact policy
- hidden runtime state transitions

### `Artifact Mapper`

Not part of Slice 1.

Owns later mapping from pure support/validator results into `instrumentation_and_acceptance.md` artifact fields.

Allowed inputs:

- pure support results
- pure validator results
- terminal arbitration result

Allowed outputs:

- artifact field values only

Forbidden in this seam:

- reading live runtime objects
- recomputing support truth
- changing terminal reason precedence

### `Terminal Arbitration`

Not part of Slice 1.

Owns later consumption of validator results and application of `continuous_balance_truth_model.md` precedence.

Allowed inputs:

- failure candidates from validators
- substep timestamps
- precedence ranks

Allowed outputs:

- primary `terminal_reason`
- `co_terminal_reasons[]`

Forbidden in this seam:

- reading Unreal runtime objects
- constructing support hulls
- filling artifact fields directly

### Pure Validator Contracts

Owns contract adjudication over already-captured value snapshots.

New files:

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimValidators.h`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.cpp`
- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimValidators.Tests.cpp`

Allowed dependency direction:

- `PhysAnimValidators.h` includes `PhysAnimTruthTypes.h`.
- `PhysAnimValidators.h` includes `PhysAnimSupportTruth.h` only when it needs support result structs.
- `PhysAnimSupportTruth` must never include `PhysAnimValidators.h`.
- Runtime files may include both only after adapter slices.

## 5. Current Code Inventory

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBridge.cpp`

- **Relevant classes/functions**: `PhysAnimBridge` namespace helpers for tensor mapping, observation/action math, action conditioning, runtime instability helpers, and bridge-side pure-ish utilities used by `UPhysAnimComponent`.
- **Current responsibility**: PHC bridge math and policy-facing data transforms. It is not the owner of the balance-first support truth model yet.
- **Legacy behavior that must not be extended**: Do not add more balance-entry phase logic, shell-lock truth, RootOn/Settle assumptions, or direct artifact terminal-reason policy here.
- **New behavior that will eventually replace it**: thin runtime adapter helpers that convert Unreal/body/contact data into `FPhysAnimSupportTruth` and `FPhysAnimValidators` value snapshots.
- **Slice 1 may touch it**: **No. Do not touch.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.cpp`

- **Relevant classes/functions**: `FPhysAnimBalanceReadyTransition::Start`, `Tick`, `Cancel`, phase progression, failure/completion state, and public transition snapshot behavior.
- **Current responsibility**: legacy multi-phase balance-entry controller and compatibility state machine.
- **Legacy behavior that must not be extended**: Do not add new RootOn, LateValidate, shell-maintained containment, grace-based success, or ownership-flip certification logic.
- **New behavior that will eventually replace it**: balance-first state wiring that consumes validator outputs for `BalanceActivation_Ready`, `BalanceActivation_BlendIn`, and `BalanceActivation_Validate`.
- **Slice 1 may touch it**: **No. Do not touch.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.Readiness.cpp`

- **Relevant classes/functions**: readiness checks, quiet-state checks, pre-entry predicates.
- **Current responsibility**: legacy admission/readiness gates for the old transition controller.
- **Legacy behavior that must not be extended**: Do not add new support-hull, proxy-drift, or controller-stability truth here during Slice 1.
- **New behavior that will eventually replace it**: adapter-fed readiness predicates backed by `PhysAnimValidators`.
- **Slice 1 may touch it**: **No. Do not touch.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.LateValidation.cpp`

- **Relevant classes/functions**: late validation, baseline capture, body motion checks.
- **Current responsibility**: legacy validation after Prepare and before RootOn/Settle-style phases.
- **Legacy behavior that must not be extended**: Do not deepen late-validation as the source of balance-first truth.
- **New behavior that will eventually replace it**: continuous validation snapshots adjudicated by `PhysAnimValidators`.
- **Slice 1 may touch it**: **No. Do not touch.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.Certification.cpp`

- **Relevant classes/functions**: certified handoff, topology snapshots, convergence/shell proof helpers.
- **Current responsibility**: legacy handoff certification and topology proof for the old phase model.
- **Legacy behavior that must not be extended**: Do not add new certification paths for the rejected flip-centered architecture.
- **New behavior that will eventually replace it**: plant/capsule/continuity validators and artifact-backed terminal reasons.
- **Slice 1 may touch it**: **No. Do not touch.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimBalanceReadyTransition.PolicyAndShell.cpp`

- **Relevant classes/functions**: shell authority and policy influence handling during transition.
- **Current responsibility**: legacy shell and policy interaction during balance entry.
- **Legacy behavior that must not be extended**: Do not add shell-helper exceptions or grace paths to make balance pass.
- **New behavior that will eventually replace it**: authority-matrix validation and `activation_shell_helper_violation`/`activation_authority_conflict` reporting.
- **Slice 1 may touch it**: **No. Do not touch.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComponent.Balance.cpp`

- **Relevant classes/functions**: `TickBalanceMode`, `StartBalanceMode`, `StopBalanceMode`, `IsBalanceEntryState`, `IsBalanceActiveState`, `MapBalanceTransitionPhaseToRuntimeState`, pre-entry prerequisite helpers.
- **Current responsibility**: owner-facing balance-mode orchestration and public runtime-state mapping.
- **Legacy behavior that must not be extended**: Do not add new public success states, BridgeActive success fallback, or extra compatibility layers for RootOn/Settle passing.
- **New behavior that will eventually replace it**: canonical balance-first state publication and validator-driven terminal routing.
- **Slice 1 may touch it**: **No. Do not touch.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimComparisonSubsystem.cpp`

- **Relevant classes/functions**: G2 presentation commands, side-by-side comparison flow, reporting support.
- **Current responsibility**: comparison/presentation behavior and user-facing evidence path.
- **Legacy behavior that must not be extended**: Do not make G2/BridgeActive presentation semantics the source of balance-first success.
- **New behavior that will eventually replace it**: artifact emission/validation support that mirrors `instrumentation_and_acceptance.md`.
- **Slice 1 may touch it**: **No, except only if adding isolated test/artifact helpers with no runtime behavior change. Prefer not to touch in Slice 1.**

### `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimPhase1AutoCalibSubsystem.cpp`

- **Relevant classes/functions**: auto-calibration trials, standing-hold scoring, JSON/CSV report writing.
- **Current responsibility**: Phase 1 auto-calibration search and reporting around existing runtime behavior.
- **Legacy behavior that must not be extended**: Do not use auto-calib scoring as the balance-first truth model.
- **New behavior that will eventually replace it**: consumer of canonical terminal reasons and standing-hold artifacts after runtime wiring.
- **Slice 1 may touch it**: **No. Do not touch.**

### New Pure Support Files

- **File paths**:
  - `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimTruthTypes.h`
  - `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Public/PhysAnimSupportTruth.h`
  - `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.cpp`
  - `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`
- **Relevant classes/functions**: `FPhysAnimSupportPatch`, `FPhysAnimFrameSupportHull`, `EPhysAnimSupportMode`, `ExtractPatchHull`, `BuildFrameHull`, `ClassifySupportMode`, `AdjudicateProxy`, `CalculateChurnHz`, `ReduceSupportModeForReportWindow`.
- **Current responsibility**: new value-only support truth extraction.
- **Legacy behavior that must not be extended**: none; these files must not know legacy phases exist.
- **New behavior that will eventually replace it**: support truth used by runtime adapters and validators.
- **Slice 1 may touch it**: **Yes. These are the only normal Slice 1 production/test files.**

## 6. Slice 1 Data Types

These are the exact value-only types Slice 1 must introduce before implementation begins. Shared enums live in `PhysAnimTruthTypes.h`; support structs and function declarations live in `PhysAnimSupportTruth.h`, which includes `PhysAnimTruthTypes.h`.

### `FPhysAnimSupportPoint2D`

- **Fields**:
  - `FVector2D PositionCm`
  - `FName BodyName`
  - `EPhysAnimSupportSide SupportSide`
- **Units**:
  - `PositionCm`: world-space planar centimeters
- **Nullable fields**: none
- **Allowed enum values**:
  - values from `EPhysAnimSupportSide`
- **Classification**: future adapter input and Slice 1 production input

### `FPhysAnimSupportPatch`

- **Fields**:
  - `FName BodyName`
  - `EPhysAnimSupportSide SupportSide`
  - `TArray<FVector2D> HullPointsCm`
  - `double PatchAreaCm2`
  - `bool bValidInput`
- **Units**:
  - `HullPointsCm`: world-space planar centimeters
  - `PatchAreaCm2`: square centimeters
- **Nullable fields**: none
- **Allowed enum values**:
  - values from `EPhysAnimSupportSide`
- **Classification**: Slice 1 production result

### `FPhysAnimFrameHull`

- **Fields**:
  - `TArray<FVector2D> HullPointsCm`
  - `double SupportHullAreaCm2`
  - `int32 ActiveSupportSideCount`
- **Units**:
  - `HullPointsCm`: world-space planar centimeters
  - `SupportHullAreaCm2`: square centimeters
  - `ActiveSupportSideCount`: count in `[0, 2]` of distinct `EPhysAnimSupportSide` values represented by non-empty patches.
- **Nullable fields**: none
- **Allowed enum values**: none
- **Classification**: Slice 1 production result

### `EPhysAnimSupportSide`

- **Header**: `PhysAnimTruthTypes.h`
- **Fields**: enum only
- **Units**: none
- **Nullable fields**: none
- **Allowed enum values**:
  - `Left`
  - `Right`
- **Classification**: Slice 1 production enum

### `EPhysAnimSupportMode`

- **Header**: `PhysAnimTruthTypes.h`
- **Fields**: enum only
- **Units**: none
- **Nullable fields**: none
- **Allowed enum values**:
  - `TwoFootStable`
  - `SingleFootSurvival`
  - `TransientRecovery`
  - `Airborne`
- **Classification**: Slice 1 production enum

### `EPhysAnimTerminalReason`

- **Header**: `PhysAnimTruthTypes.h`
- **Fields**: enum only
- **Units**: none
- **Nullable fields**: none
- **Allowed enum values**:
  - `None`
  - `ActivationPhysicsAssetContractViolation`
  - `ActivationCapsuleContractViolation`
  - `ActivationTopologyChange`
  - `ActivationContinuousSimulationLost`
  - `ActivationSupportFailure`
  - `ActivationProxyOutsideSupportRegion`
  - `ActivationTargetDiscontinuity`
  - `ActivationUnstableGainOrDamping`
  - `ActivationInstabilityThresholdBreach`
  - `ActivationPoseReferenceMismatch`
  - `ActivationMovementReclaim`
  - `ActivationShellHelperViolation`
  - `ActivationAuthorityConflict`
  - `ActivationStandingValidationTimeout`
- **Classification**: Slice 1 shared value enum

### `FPhysAnimProxyAdjudicationInput`

- **Fields**:
  - `FVector2D ProxyPositionCm`
  - `TArray<FVector2D> HullPointsCm`
  - `int32 ActiveSupportSideCount`
  - `TOptional<double> PreviousProxyOutsideHullDurationMs`
  - `double DeltaMs`
  - `double ProxyDriftLimitMs`
- **Units**:
  - `ProxyPositionCm`: world-space planar centimeters
  - `HullPointsCm`: world-space planar centimeters
  - `PreviousProxyOutsideHullDurationMs`: milliseconds
  - `DeltaMs`: milliseconds
  - `ProxyDriftLimitMs`: milliseconds
- **Nullable fields**:
  - `PreviousProxyOutsideHullDurationMs`: unset only when proxy test was skipped in previous frame
- **Allowed enum values**: none
- **Classification**: Slice 1 production input

### `FPhysAnimProxyAdjudicationResult`

- **Fields**:
  - `TOptional<bool> ProxyInsideHull`
  - `TOptional<double> ProxyOutsideHullDurationMs`
  - `EPhysAnimTerminalReason TerminalReason`
- **Units**:
  - `ProxyOutsideHullDurationMs`: milliseconds
- **Nullable fields**:
  - `ProxyInsideHull`: unset maps to artifact `proxy_inside_hull = nullptr`
  - `ProxyOutsideHullDurationMs`: unset maps to artifact `proxy_outside_hull_duration_ms = nullptr`
- **Allowed enum values**:
  - values from `EPhysAnimTerminalReason`
- **Classification**: Slice 1 production result

### `FPhysAnimChurnEvent`

- **Fields**:
  - `double TimestampSec`
  - `EPhysAnimSupportSide SupportSide`
  - `bool bNewSupportState`
- **Units**:
  - `TimestampSec`: seconds
  - `bNewSupportState`: debounced side-support state after the transition
- **Nullable fields**: none
- **Allowed enum values**:
  - values from `EPhysAnimSupportSide`
- **Classification**: Slice 1 production input and future adapter input

### `FPhysAnimChurnResult`

- **Fields**:
  - `int32 SupportChurnCount`
  - `double SupportChurnHz`
- **Units**:
  - `SupportChurnCount`: transition count in the current measurement window
  - `SupportChurnHz`: transitions per second
- **Nullable fields**: none
- **Allowed enum values**: none
- **Classification**: Slice 1 production result

### `FPhysAnimChurnCalculationInput`

- **Fields**:
  - `double CurrentTimestampSec`
  - `double WindowSeconds`
  - `TArray<FPhysAnimChurnEvent> HistoricalEvents`
- **Units**:
  - `CurrentTimestampSec`: seconds
  - `WindowSeconds`: seconds (nominal 1.0)
- **Nullable fields**: none
- **Allowed enum values**: none
- **Classification**: Slice 1 production input

### `FPhysAnimSupportReportWindowInput`

- **Fields**:
  - `TArray<EPhysAnimSupportMode> Modes`
  - `TArray<double> DurationsMs`
- **Units**:
  - `DurationsMs`: milliseconds per corresponding mode
- **Nullable fields**: none
- **Allowed enum values**:
  - values from `EPhysAnimSupportMode`
- **Classification**: Slice 1 production input; tests may construct it directly

### `FPhysAnimSupportReportWindowResult`

- **Fields**:
  - `EPhysAnimSupportMode SupportMode`
  - `double TotalWindowDurationMs`
- **Units**:
  - `TotalWindowDurationMs`: milliseconds
- **Nullable fields**: none
- **Allowed enum values**:
  - values from `EPhysAnimSupportMode`
- **Classification**: Slice 1 production result

All terminal-reason empty values use `EPhysAnimTerminalReason::None`. Implementation must not introduce alternate Slice 1 structs or enum values without updating this section first.

### Artifact Mapping Rule (Post-Slice 1)

The artifact mapper must use these rules when converting pure results to JSON:
- `EPhysAnimTerminalReason::None` -> `terminal_reason = nullptr`
- All other values map to their lower-snake-case canonical strings.

### Validator Types

Introduce only after Slice 1 is green:

- `FPhysAnimFailureCandidate`
- `FPhysAnimFailureArbitrationResult`
- `FPhysAnimContinuitySnapshot`
- `FPhysAnimCapsuleContractSnapshot`
- `FPhysAnimPlantContractSnapshot`
- `FPhysAnimAuthoritySnapshot`
- `FPhysAnimControllerStabilitySnapshot`
- `FPhysAnimRunArtifactSnapshot`

These structs must contain fields named after `instrumentation_and_acceptance.md` wherever they represent emitted artifact data.

## 7. Slice 1 Public API

These exact signatures must be implemented in `PhysAnimSupportTruth.h`.

### Support Logic

```cpp
namespace PhysAnimSupportTruth
{
    /** Extract a convex hull from raw support points. 
     * Rule: Expects all points to belong to one BodyName and one SupportSide.
     * - empty input -> returns empty patch, PatchAreaCm2 = 0.0, bValidInput = true.
     * - mixed body or side input -> returns empty patch, PatchAreaCm2 = 0.0, bValidInput = false.
     * - collinear points -> returns empty patch, PatchAreaCm2 = 0.0, bValidInput = true.
     */
    FPhysAnimSupportPatch ExtractPatchHull(const TArray<FPhysAnimSupportPoint2D>& Points);

    /** Build the frame-level support hull from per-body patches. Sets ActiveSupportSideCount. 
     * Rule: ActiveSupportSideCount is the number of distinct EPhysAnimSupportSide values from non-empty patches.
     * - empty patch list -> 0
     * - multiple patches on Left only -> 1
     * - multiple patches on Right only -> 1
     * - any Left + any Right -> 2
     * - empty patches (no hull points) do not contribute to the count
     */
    FPhysAnimFrameHull BuildFrameHull(const TArray<FPhysAnimSupportPatch>& Patches);

    /** Classify the support mode based on side-support states and gap timing. */
    EPhysAnimSupportMode ClassifySupportMode(bool bLeftSupport, bool bRightSupport, double SupportGapTimerMs, double SupportGapMaxMs);

    /** Adjudicate proxy drift against the frame hull. Returns EPhysAnimTerminalReason::None if valid. */
    FPhysAnimProxyAdjudicationResult AdjudicateProxy(const FPhysAnimProxyAdjudicationInput& Input);

    /** AdjudicateProxy Logic Rules:
     * 1. If ActiveSupportSideCount == 0:
     *    - skip polygon test
     *    - ProxyInsideHull = unset
     *    - ProxyOutsideHullDurationMs = unset
     *    - TerminalReason = None
     * 2. If ActiveSupportSideCount > 0 and HullPointsCm.Num() < 3:
     *    - treat proxy as outside
     *    - proxy timer follows the normal outside rule (see rule 3/4)
     * 3. If previous duration is unset and current proxy is outside:
     *    - ProxyOutsideHullDurationMs = DeltaMs
     * 4. If previous duration is unset and current proxy is inside:
     *    - ProxyOutsideHullDurationMs = 0.0
     * 5. If proxy is outside and duration exceeds limit:
     *    - TerminalReason = ActivationProxyOutsideSupportRegion
     */

    /** Calculate transition frequency (Churn Hz) over a rolling window. */
    FPhysAnimChurnResult CalculateChurnHz(const FPhysAnimChurnCalculationInput& Input);

    /** CalculateChurnHz Logic Rules:
     * 1. Inclusion boundary: filter HistoricalEvents where (TimestampSec > CurrentTimestampSec - WindowSeconds) AND (TimestampSec <= CurrentTimestampSec).
     * 2. SupportChurnCount = count of filtered events.
     * 3. SupportChurnHz = SupportChurnCount / WindowSeconds.
     */

    /** Reduce 30Hz modes into the dominant mode for the artifact report window. */
    FPhysAnimSupportReportWindowResult ReduceSupportModeForReportWindow(const FPhysAnimSupportReportWindowInput& Input);

    /** ReduceSupportModeForReportWindow Logic Rules:
     * 1. Array parity: Input.Modes.Num() must equal Input.DurationsMs.Num().
     * 2. Empty input: returns SupportMode = Airborne, TotalWindowDurationMs = 0.0.
     * 3. Negative durations: clamped to 0.0 before accumulation.
     * 4. Zero total duration: returns SupportMode = Airborne, TotalWindowDurationMs = 0.0.
     * 5. Accumulation & Tie-break: accumulate duration by mode. If durations are equal, use severity tie-break (Airborne > TransientRecovery > SingleFootSurvival > TwoFootStable).
     */
}
```

## 8. Test Harness Wiring

Slice 1 tests are real Unreal Automation Tests, not conceptual matrix rows.

### Test File

- `PhysAnimUE5/Plugins/PhysAnimPlugin/Source/PhysAnimPlugin/Private/PhysAnimSupportTruth.Tests.cpp`

### Framework

- Unreal Automation Tests
- Use `IMPLEMENT_SIMPLE_AUTOMATION_TEST`.
- Use `EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter`.
- Include only:
  - `PhysAnimSupportTruth.h`
  - `Misc/AutomationTest.h`
  - minimal Core headers required by the pure value types

### Module Wiring

- Tests compile into the existing `PhysAnimPlugin` module.
- No `.Build.cs` dependency change is expected for Slice 1.
- `PhysAnimPlugin.Build.cs` already includes `Core`.
- UBT compiles module `.cpp` files under `Private`, so adding `PhysAnimSupportTruth.Tests.cpp` is enough for registration.

### Naming

- All Slice 1 support tests use the prefix `PhysAnim.SupportTruth.*`.
- The required first test is `PhysAnim.SupportTruth.Harness.CompilesAndRuns`.

### Run Path

Use the project automation path:

- Build only: `.\scripts\build.ps1`
- Build and run the harness test: `.\scripts\build.ps1 -Test PhysAnim.SupportTruth.Harness.CompilesAndRuns`
- Build and run the Slice 1 support suite: `.\scripts\build.ps1 -Test PhysAnim.SupportTruth`

The script runs the editor automation command:

- `Automation RunTests <TestName>; Quit`

Manual editor path, if needed:

- open the UE editor
- open Session Frontend / Automation
- filter for `PhysAnim.SupportTruth`
- run the selected tests

### Pass Condition

The harness is accepted only when all are true:

- `PhysAnim.SupportTruth.Harness.CompilesAndRuns` appears in the Automation list
- the test runs through Unreal Automation
- red-first tests fail before the targeted implementation exists
- the same tests pass after the minimal implementation for that slice
- no Slice 1 test requires PIE, a map, a skeletal mesh, `UWorld`, `UObject`, `FBodyInstance`, `UPhysicsControlComponent`, or `FPhysAnimBalanceReadyTransition`

### Forbidden Includes

Slice 1 test files must not include:

- `PhysAnimComponent.h`
- `PhysAnimBalanceReadyTransition.h`
- `PhysAnimComparisonSubsystem.h`
- `PhysicsControlComponent.h`
- `Engine/World.h`
- `GameFramework/Actor.h`

That include list is the compile-time proof that Slice 1 is value-only.

## 9. Slice 1 Commit Plan

Every Slice 1 commit must compile and pass its mapped tests. No Slice 1 commit may touch the runtime state machine.

**TDD Commit Policy**:
- **Red State**: Create and observe failing tests locally to verify the test surface. Do not commit red tests alone.
- **Green State**: Implement the minimal code required to pass the tests. Commit only when the mapped tests are green.
- **Exceptions**: Only explicitly marked temporary checkpoints may be committed in a failing state.

Use this exact order:

1. `Commit 1: Pure Support Module Scaffold`
   - Add `PhysAnimTruthTypes.h` and empty `PhysAnimSupportTruth.h/.cpp`.
   - `PhysAnimSupportTruth.h` includes `PhysAnimTruthTypes.h`.
   - No runtime references.
   - Compile only.
2. `Commit 2: Automation Harness Registration`
   - Add `PhysAnimSupportTruth.Tests.cpp`.
   - Verify `PhysAnim.SupportTruth.Harness.CompilesAndRuns` is discoverable and runs.
3. `Commit 3: Slice 1 Value Types`
   - Add pure data structs in `PhysAnimSupportTruth.h`.
   - Keep shared enums in `PhysAnimTruthTypes.h`.
   - No behavior yet.
4. `Commit 4: ExtractPatchHull`
   - Write mapped failing tests locally.
   - Implement minimal code.
   - Commit only when mapped tests are green.
5. `Commit 5: BuildFrameHull`
   - Write mapped failing tests locally.
   - Implement minimal code.
   - Commit only when mapped tests are green.
6. `Commit 6: ClassifySupportMode`
   - Write mapped failing tests locally.
   - Implement minimal code.
   - Commit only when mapped tests are green.
7. `Commit 7: AdjudicateProxy`
   - Write mapped failing tests locally.
   - Implement minimal code.
   - Commit only when mapped tests are green.
8. `Commit 8: CalculateChurnHz`
   - Write mapped failing tests locally.
   - Implement minimal code.
   - Commit only when mapped tests are green.
9. `Commit 9: ReduceSupportModeForReportWindow`
   - Write mapped failing tests locally.
   - Implement minimal code.
   - Commit only when mapped tests are green.
10. `Commit 10: Slice 1 Aggregation / No-Runtime-Dependency Proof`
   - Add one Slice 1 aggregation test proving all outputs can be produced without runtime dependencies.

No step may include `UPhysAnimComponent`, `FPhysAnimBalanceReadyTransition`, `UPhysicsControlComponent`, `FBodyInstance`, `UWorld`, `AActor`, or Chaos runtime handles.

## Slice-to-Test Mapping

No behavior commit is complete until its mapped tests are green.

| Commit | Function | Required Tests | Completion Gate |
|---|---|---|---|
| Commit 4 | `ExtractPatchHull` | `LOGIC-01`, `LOGIC-02`, `LOGIC-03`, `LOGIC-03A` | All mapped tests pass. |
| Commit 5 | `BuildFrameHull` | `LOGIC-04`, `LOGIC-04A`, `LOGIC-04B`, `LOGIC-04C` | All mapped tests pass. |
| Commit 6 | `ClassifySupportMode` | `LOGIC-05`, `LOGIC-06`, `LOGIC-07`, `LOGIC-08` | All mapped tests pass. |
| Commit 7 | `AdjudicateProxy` | `LOGIC-09`, `LOGIC-10`, `LOGIC-11`, `LOGIC-12`, `LOGIC-12A`, `LOGIC-12B`, `LOGIC-12C` | All mapped tests pass. |
| Commit 8 | `CalculateChurnHz` | `LOGIC-13` | All mapped tests pass. |
| Commit 9 | `ReduceSupportModeForReportWindow` | `LOGIC-14`, `LOGIC-14A`, `LOGIC-14B`, `LOGIC-14C` | All mapped tests pass. |

## 10. Slice 1 Forbidden Edits

Slice 1 forbids edits to:

- bridge activation state transitions
- `Ready -> BlendIn`
- `BlendIn -> Validate`
- `Validate -> Standing`
- runtime terminal failure routing
- artifact emission behavior
- PhysicsControl setup
- capsule/CMC behavior
- plant validation
- authority matrix enforcement
- legacy flip-centered handoff code

Slice 1 allows only:

- new pure support module
- new support tests
- build file edits required to compile tests


### Commit 11: Empty Validator Module Scaffolding

Goal: add compile-only validator scaffolding.

Writable paths:

- `Public/PhysAnimValidators.h`
- `Private/PhysAnimValidators.cpp`
- `Private/PhysAnimValidators.Tests.cpp`

Allowed include:

- `PhysAnimTruthTypes.h`
- `PhysAnimSupportTruth.h` only when support result structs are required

Forbidden:

- runtime object includes
- runtime state-machine edits

### Commit 12: Terminal Arbitration

Goal: implement `ARBIT-01` to `ARBIT-05`.

Production function:

- `PhysAnimValidators::ArbitrateFailure`

Inputs:

- terminal reason
- substep timestamp
- rank

Output:

- primary `terminal_reason`
- `co_terminal_reasons`

### Commit 13: Continuity And Capsule Validators

Goal: implement `VALID-01*` and `VALID-02*`.

Production functions:

- `PhysAnimValidators::ValidateContinuity`
- `PhysAnimValidators::ValidateCapsule`

Inputs are snapshots only. Runtime object reads remain forbidden.

### Commit 14: Plant And Authority Validators

Goal: implement `VALID-03*`, `VALID-04*`, `VALID-06A`, and `VALID-06B`.

Production functions:

- `PhysAnimValidators::ValidatePlant`
- `PhysAnimValidators::ValidateAuthority`
- `PhysAnimValidators::ValidateMovementReclaim`
- `PhysAnimValidators::ValidateShellHelper`

### Commit 15: Controller Stability Validator

Goal: implement `VALID-05A` to `VALID-05H`.

Production function:

- `PhysAnimValidators::ValidateControllerStability`

All comparisons must use named thresholds from `instrumentation_and_acceptance.md`.

### Commit 16: Artifact Snapshot Builder

Goal: create a value-only artifact snapshot that matches the schema.

Allowed changes:

- `PhysAnimValidators` value structs
- tests in `PhysAnimValidators.Tests.cpp`

Forbidden:

- changing `PhysAnimComparisonSubsystem.cpp` behavior

### Commit 17: Runtime Adapter Capture

Goal: add adapters that fill value snapshots from runtime objects without changing decisions.

Allowed files:

- `PhysAnimBridge.cpp`
- small private helper declarations if needed

Rules:

- adapters return snapshots
- adapters do not transition runtime state
- adapters do not emit terminal decisions yet

### Commit 18: Ready-State Contract Wiring

Goal: wire plant, capsule, continuity, movement reclaim, and shell-helper failures into `BalanceActivation_Ready`.

Allowed files:

- `PhysAnimBalanceReadyTransition*.cpp`
- `PhysAnimComponent.Balance.cpp`

Required tests:

- integration rows `INTEG-01`, `INTEG-02A`, `INTEG-02B1` to `INTEG-02B4`

### Commit 19: BlendIn And Validate Wiring

Goal: wire support truth, controller stability, and proxy drift into `BalanceActivation_BlendIn` and `BalanceActivation_Validate`.

Required tests:

- `INTEG-03` to `INTEG-07`

### Commit 20: Standing Success And Artifact Emission

Goal: wire the 3.0 second success path and terminal artifact emission.

Allowed files:

- `PhysAnimComparisonSubsystem.cpp`
- `PhysAnimComponent.Balance.cpp`
- smoke helper files if needed

Required tests:

- `INTEG-08`
- `SMOKE-01` to `SMOKE-05`

## 11. How Old Runtime Stays Untouched

Before Commit 12:

- no legacy runtime file may include `PhysAnimSupportTruth.h`
- no legacy runtime file may include `PhysAnimValidators.h`
- no existing transition phase may change behavior
- no existing CVar default may change
- no artifact format may change except documented schema additions

After Commit 12:

- adapters may read runtime objects
- validators decide only from snapshots
- state-machine code may consume validator results only in the assigned wiring commits

## 12. Dependency Direction

Exact dependency rule:

- `PhysAnimTruthTypes` owns shared enums and depends on no support or validator headers.
- `PhysAnimSupportTruth` depends on nothing from the bridge runtime.
- `PhysAnimSupportTruth.h` includes `PhysAnimTruthTypes.h`.
- `PhysAnimValidators.h` includes `PhysAnimTruthTypes.h`.
- `PhysAnimValidators` may depend on `PhysAnimSupportTruth` only when it consumes support result structs.
- runtime adapters may depend on validators and support truth.
- `PhysAnimBridge.cpp` may call adapters only after validators are green.
- `PhysAnimComparisonSubsystem.cpp` may consume artifact-ready outputs only after artifact schema helpers exist.
- no lower layer may include or call a higher layer.

Allowed:

```text
Tests -> PhysAnimValidators -> PhysAnimTruthTypes
Tests -> PhysAnimValidators -> PhysAnimSupportTruth (only for support result structs)
Tests -> PhysAnimSupportTruth -> PhysAnimTruthTypes
Runtime adapters -> PhysAnimValidators
Runtime adapters -> PhysAnimSupportTruth
State machine -> adapter results
Artifact emission -> validator/artifact snapshots
```

Forbidden:

```text
PhysAnimSupportTruth -> PhysAnimValidators
PhysAnimTruthTypes -> PhysAnimSupportTruth
PhysAnimTruthTypes -> PhysAnimValidators
PhysAnimSupportTruth -> UPhysAnimComponent
PhysAnimValidators -> UPhysAnimComponent
PhysAnimValidators -> UPhysicsControlComponent
Pure tests -> runtime transition controller
Slice 1 -> runtime adapter code
```

Slice 1 forbidden include/dependency list:

- no `PhysAnimBridge.h`
- no `PhysAnimComponent.h`
- no `PhysicsControlComponent.h`
- no `CharacterMovementComponent.h`
- no `BodyInstance` dependency
- no `UObject` ownership

## 13. Slice 1 Completion Proof

Slice 1 is complete only when:

- `PhysAnimSupportTruth.Tests.cpp` covers every test in the Slice-to-Test Mapping section, including `LOGIC-03A`, `LOGIC-04A` to `LOGIC-04C`, `LOGIC-12A` to `LOGIC-12C`, and `LOGIC-14A` to `LOGIC-14C`
- `PhysAnimSupportTruth.h` contains only value structs, shared truth-type includes, and pure function declarations
- `PhysAnimSupportTruth.cpp` contains no `UObject`, `AActor`, component, world, or Chaos runtime references
- no existing runtime `.cpp` includes `PhysAnimSupportTruth.h`
- `.\scripts\build.ps1` succeeds

Slice 1 is not complete if it requires editor runtime setup, PIE, smoke tests, skeletal mesh instances, body instances, or Physics Control components.

## 14. Stop Conditions

Stop and revise contracts if any of these occur:

- support truth cannot be represented with value-only inputs
- validator inputs require live Unreal objects instead of snapshots
- artifact schema cannot prove a terminal reason named in the test matrix
- runtime wiring requires changing architecture outside PoseSearch -> PHC Policy -> Physics Control -> Chaos -> Renderer
- a slice needs to edit unrelated systems to compile
