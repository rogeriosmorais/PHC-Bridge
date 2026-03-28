#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "PhysAnimComparisonSubsystem.h"
#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimPhase1PelvisCouplingSearch.h"
#include "PhysAnimBalance.TestHelpers.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimBridge;
	using namespace PhysAnimBalanceTestHelpers;

#if !UE_BUILD_SHIPPING
	FPhase1AutoCalibTrialResult MakePhase1AutoCalibTrial(
		const EPhase1AutoCalibStrategyPreset Preset,
		const TCHAR* TerminalClass,
		const TCHAR* TruthfulBlocker,
		const bool bContractPassed,
		const float WorstDirectLinkAngularErrorDeg,
		const float ThighAsymmetryDeg)
	{
		static int32 NextTrialId = 0;
		FPhase1AutoCalibTrialResult Trial;
		Trial.TrialId = NextTrialId++;
		Trial.Params.SourcePreset = Preset;
		Trial.Params.SeedFamilyPreset = Preset;
		Trial.TerminalClass = TerminalClass;
		Trial.TruthfulBlocker = TruthfulBlocker;
		Trial.Score.bContractPassed = bContractPassed;
		Trial.Score.bReachedRootOn = bContractPassed;
		Trial.Score.bNoCouplingProofSatisfied = bContractPassed;
		Trial.Score.WorstDirectLinkAngularErrorDeg = WorstDirectLinkAngularErrorDeg;
		Trial.Score.MeanTargetDeltaDeg = 2.0f;
		Trial.Score.MaxTargetDeltaDeg = 4.0f;
		Trial.Score.ThighAsymmetryDeg = ThighAsymmetryDeg;
		Trial.Score.PeakRootTiltDeg = 15.0f;
		Trial.Score.ShellOffsetDeltaCm = 1.0f;
		Trial.Score.ShellVelocityDeltaCmPerSecond = 2.0f;
		Trial.Score.PeakRootLinearSpeedCmPerSecond = 25.0f;
		Trial.Score.PeakRootAngularSpeedDegPerSecond = 35.0f;
		Trial.TrialTimeoutBudgetSeconds = 0.75f;
		Trial.TimeToRootOnSeconds = bContractPassed ? 0.20f : -1.0f;
		Trial.TimeToNoCouplingProofSeconds = bContractPassed ? 0.23f : -1.0f;
		Trial.bTimedOutBeforeRootOn = !bContractPassed;
		Trial.bTimedOutBeforeNoCouplingProof = !bContractPassed;
		Trial.WinningSearchFamily = TEXT("direct_seed");
		Trial.WinningSearchSource = TEXT("animated_pelvis_rotation");
		Trial.ExecutedSearchFamilies = { TEXT("direct_seed") };
		UPhysAnimComponent::FinalizePhase1AutoCalibScore(Trial.Score);
		return Trial;
	}
#endif

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBalanceModeSmokeOutcomeTest,
		"PhysAnim.Component.BalanceModeSmokeOutcome",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBalanceModeSmokeOutcomeTest::RunTest(const FString& Parameters)
	{
		FString OutcomeError;

		TestTrue(
			TEXT("Balance recovery remains a passing smoke outcome"),
			EvaluateBalanceModeSmokeOutcome(
				EPhysAnimRuntimeState::BalanceActive_Recovery,
				false,
				EPhysAnimRuntimeState::BridgeActive,
				false,
				false,
				TEXT(""),
				TEXT(""),
				OutcomeError));
		TestTrue(TEXT("Successful recovery outcome emits no error"), OutcomeError.IsEmpty());

		OutcomeError.Reset();
		TestTrue(
			TEXT("Explicit safe deny is a passing smoke outcome"),
			EvaluateBalanceModeSmokeOutcome(
				EPhysAnimRuntimeState::BalanceSafeDeny,
				false,
				EPhysAnimRuntimeState::BridgeActive,
				false,
				true,
				TEXT("phase1_late_validate_sim_coverage_regressed"),
				TEXT(""),
				OutcomeError));
		TestTrue(TEXT("Safe deny pass emits no error"), OutcomeError.IsEmpty());

		OutcomeError.Reset();
		TestFalse(
			TEXT("BridgeActive remains a failing smoke outcome"),
			EvaluateBalanceModeSmokeOutcome(
				EPhysAnimRuntimeState::BridgeActive,
				false,
				EPhysAnimRuntimeState::BridgeActive,
				false,
				false,
				TEXT(""),
				TEXT(""),
				OutcomeError));
		TestTrue(TEXT("BridgeActive failure reports the runtime state"), OutcomeError.Contains(TEXT("BridgeActive")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimActionConditioningTest,
		"PhysAnim.Component.ActionConditioning",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimActionConditioningTest::RunTest(const FString& Parameters)
	{
		TArray<float> RawActions;
		RawActions.Init(0.0f, NumActionFloats);
		RawActions[0] = 1.0f;
		RawActions[1] = -1.0f;
		RawActions[2] = 0.5f;

		FPhysAnimActionConditioningSettings Settings;
		Settings.ActionScale = 0.5f;
		Settings.ActionClampAbs = 0.25f;
		Settings.ActionSmoothingAlpha = 1.0f;

		TArray<float> ConditionedActions;
		FPhysAnimActionDiagnostics Diagnostics;
		FString Error;
		TestTrue(
			TEXT("Conditioning should succeed"),
			UPhysAnimComponent::BuildConditionedActions(
				RawActions,
				nullptr,
				Settings,
				ConditionedActions,
				Diagnostics,
				Error));
		TestEqual(TEXT("Conditioned action count"), ConditionedActions.Num(), NumActionFloats);
		TestEqual(TEXT("Positive action is clamped"), ConditionedActions[0], 0.25f);
		TestEqual(TEXT("Negative action is clamped"), ConditionedActions[1], -0.25f);
		TestEqual(TEXT("Scaled half action remains inside clamp"), ConditionedActions[2], 0.25f);
		TestEqual(TEXT("Two action dimensions were clamped"), Diagnostics.NumClampedActionFloats, 2);

		Settings.bForceZeroActions = true;
		TestTrue(
			TEXT("Zero-action override should succeed"),
			UPhysAnimComponent::BuildConditionedActions(
				RawActions,
				nullptr,
				Settings,
				ConditionedActions,
				Diagnostics,
				Error));
		TestEqual(TEXT("Forced zero actions override positive sample"), ConditionedActions[0], 0.0f);
		TestEqual(TEXT("Forced zero actions override negative sample"), ConditionedActions[1], 0.0f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimActionSmoothingTest,
		"PhysAnim.Component.ActionSmoothing",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimActionSmoothingTest::RunTest(const FString& Parameters)
	{
		TArray<float> RawActions;
		RawActions.Init(0.0f, NumActionFloats);
		RawActions[0] = 1.0f;
		RawActions[1] = -1.0f;

		TArray<float> PreviousActions;
		PreviousActions.Init(0.0f, NumActionFloats);
		PreviousActions[1] = 0.2f;

		FPhysAnimActionConditioningSettings Settings;
		Settings.ActionScale = 0.5f;
		Settings.ActionClampAbs = 1.0f;
		Settings.ActionSmoothingAlpha = 0.25f;

		TArray<float> ConditionedActions;
		FPhysAnimActionDiagnostics Diagnostics;
		FString Error;
		TestTrue(
			TEXT("Smoothing should succeed"),
			UPhysAnimComponent::BuildConditionedActions(
				RawActions,
				&PreviousActions,
				Settings,
				ConditionedActions,
				Diagnostics,
				Error));
		TestEqual(TEXT("Smoothed first action"), ConditionedActions[0], 0.125f);
		TestEqual(TEXT("Smoothed second action"), ConditionedActions[1], 0.025f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimStabilizationDefaultsTest,
		"PhysAnim.Component.StabilizationDefaults",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimStabilizationDefaultsTest::RunTest(const FString& Parameters)
	{
		FPhysAnimStabilizationSettings Settings;
		FPhysAnimStabilizationSettings OverrideSettings = Settings;
		TestFalse(TEXT("Force-zero actions defaults to disabled"), Settings.bForceZeroActions);
		TestEqual(TEXT("Policy control rate defaults to the ProtoMotions-trained cadence"), Settings.PolicyControlRateHz, 30.0f);
		float PolicyAccumulatorSeconds = -1.0f;
		int32 ElapsedPolicySteps = 0;
		TestTrue(
			TEXT("Policy cadence primes the first update immediately after activation"),
			UPhysAnimComponent::AdvancePolicyControlAccumulator(1.0f / 60.0f, 1.0f / 30.0f, PolicyAccumulatorSeconds, ElapsedPolicySteps));
		TestEqual(TEXT("Primed cadence emits one policy step"), ElapsedPolicySteps, 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTrainingAlignedMassScaleTest,
		"PhysAnim.Component.TrainingAlignedMassScale",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTrainingAlignedMassScaleTest::RunTest(const FString& Parameters)
	{
		TestEqual(TEXT("Pelvis uses the audited family target scale"), UPhysAnimComponent::ResolveTrainingAlignedMassScaleForBone(TEXT("pelvis"), 1.0f), 0.815f);
		TestEqual(TEXT("Leg chain uses the audited family target scale"), UPhysAnimComponent::ResolveTrainingAlignedMassScaleForBone(TEXT("calf_l"), 1.0f), 1.569f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTrainingAlignedControlFamilyProfileTest,
		"PhysAnim.Component.TrainingAlignedControlFamilyProfile",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTrainingAlignedControlFamilyProfileTest::RunTest(const FString& Parameters)
	{
		TestEqual(TEXT("Torso family uses the strongest training-aligned control scale"), UPhysAnimComponent::ResolveTrainingAlignedControlStrengthScaleForBone(TEXT("spine_02"), 1.0f), 1.25f);
		TestEqual(TEXT("Hand family uses the weakest training-aligned strength scale"), UPhysAnimComponent::ResolveTrainingAlignedControlStrengthScaleForBone(TEXT("hand_l"), 1.0f), 0.375f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTrainingAlignedToeLimitPolicyTest,
		"PhysAnim.Component.TrainingAlignedToeLimitPolicy",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTrainingAlignedToeLimitPolicyTest::RunTest(const FString& Parameters)
	{
		FPhysAnimStabilizationSettings Settings;
		TestTrue(TEXT("Training-aligned toe limit policy is enabled by default"), Settings.bApplyTrainingAlignedToeLimitPolicy);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTrainingAlignedLowerLimbTargetRangePolicyTest,
		"PhysAnim.Component.TrainingAlignedLowerLimbTargetRangePolicy",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTrainingAlignedLowerLimbTargetRangePolicyTest::RunTest(const FString& Parameters)
	{
		TestEqual(TEXT("Calf chain uses the strongest lower-limb target-range reduction"), UPhysAnimComponent::ResolveTrainingAlignedLowerLimbTargetRangeScaleForBone(TEXT("calf_l"), 1.0f), 0.50f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeInstabilityThresholdTest,
		"PhysAnim.Component.RuntimeInstabilityThreshold",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeInstabilityThresholdTest::RunTest(const FString& Parameters)
	{
		FPhysAnimRuntimeInstabilitySettings Settings;
		Settings.MaxRootHeightDeltaCm = 50.0f;
		Settings.MaxRootLinearSpeedCmPerSecond = 500.0f;
		Settings.MaxRootAngularSpeedDegPerSecond = 360.0f;
		Settings.UnstableGracePeriodSeconds = 0.2f;

		FPhysAnimRuntimeInstabilityState State;
		FPhysAnimRuntimeInstabilityDiagnostics Diagnostics;
		FString Error;

		TestTrue(
			TEXT("First stable sample seeds reference location"),
			UPhysAnimComponent::EvaluateRuntimeInstability(
				FVector(0.0f, 0.0f, 100.0f),
				FVector::ZeroVector,
				FVector::ZeroVector,
				0.016f,
				Settings,
				State,
				Diagnostics,
				Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeStateOwnershipTest,
		"PhysAnim.Component.RuntimeStateOwnership",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeStateOwnershipTest::RunTest(const FString& Parameters)
	{
		TestFalse(TEXT("Uninitialized does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::Uninitialized));
		TestTrue(TEXT("BridgeActive owns bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::BridgeActive));
		TestTrue(TEXT("Balance entry Prepare owns bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::BalanceEntry_Prepare));
		TestTrue(TEXT("Balance entry RootOn owns bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::BalanceEntry_RootOn));
		TestFalse(TEXT("BalanceSafeDeny does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::BalanceSafeDeny));
		TestFalse(TEXT("FailStopped does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::FailStopped));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimInitialPoseSearchSuccessStateTest,
		"PhysAnim.Component.InitialPoseSearchSuccessState",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimInitialPoseSearchSuccessStateTest::RunTest(const FString& Parameters)
	{
		TestEqual(
			TEXT("Zero-action startup resolves to deferred activation"),
			UPhysAnimComponent::ResolveInitialPoseSearchSuccessState(true),
			EPhysAnimRuntimeState::ReadyForActivation);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimSafeModeActivationTransitionTest,
		"PhysAnim.Component.SafeModeActivationTransition",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimSafeModeActivationTransitionTest::RunTest(const FString& Parameters)
	{
		TestTrue(
			TEXT("ReadyForActivation activates bridge physics only when zero-action mode is disabled"),
			UPhysAnimComponent::ShouldActivateBridgeFromSafeMode(EPhysAnimRuntimeState::ReadyForActivation, false));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBalanceStateClassificationTest,
		"PhysAnim.Component.BalanceStateClassification",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBalanceStateClassificationTest::RunTest(const FString& Parameters)
	{
		TestTrue(TEXT("Prepare is a public balance-entry state"), UPhysAnimComponent::TestOnlyIsBalanceEntryState(EPhysAnimRuntimeState::BalanceEntry_Prepare));
		TestTrue(TEXT("LateValidate is a public balance-entry state"), UPhysAnimComponent::TestOnlyIsBalanceEntryState(EPhysAnimRuntimeState::BalanceEntry_LateValidate));
		TestTrue(TEXT("RootOn is a public balance-entry state"), UPhysAnimComponent::TestOnlyIsBalanceEntryState(EPhysAnimRuntimeState::BalanceEntry_RootOn));
		TestTrue(TEXT("Settle is a public balance-entry state"), UPhysAnimComponent::TestOnlyIsBalanceEntryState(EPhysAnimRuntimeState::BalanceEntry_Settle));
		TestFalse(TEXT("BalanceSafeDeny is terminal, not an entry state"), UPhysAnimComponent::TestOnlyIsBalanceEntryState(EPhysAnimRuntimeState::BalanceSafeDeny));
		TestFalse(TEXT("BridgeActive is not an entry state"), UPhysAnimComponent::TestOnlyIsBalanceEntryState(EPhysAnimRuntimeState::BridgeActive));

		TestTrue(TEXT("Recovery is the only public active-balance state"), UPhysAnimComponent::TestOnlyIsBalanceActiveState(EPhysAnimRuntimeState::BalanceActive_Recovery));
		TestFalse(TEXT("BalanceSafeDeny is not active balance"), UPhysAnimComponent::TestOnlyIsBalanceActiveState(EPhysAnimRuntimeState::BalanceSafeDeny));
		TestFalse(TEXT("Settle is not active balance"), UPhysAnimComponent::TestOnlyIsBalanceActiveState(EPhysAnimRuntimeState::BalanceEntry_Settle));
		TestTrue(
			TEXT("Ultra-fine RootOn readiness cleanup still runs for the current 1.55 degree near-miss class"),
			UPhysAnimComponent::TestOnlyShouldRunRootOnReadinessUltraFineMarginSweep(1.55f));
		TestFalse(
			TEXT("Ultra-fine RootOn readiness cleanup stays off for clearly larger deficits"),
			UPhysAnimComponent::TestOnlyShouldRunRootOnReadinessUltraFineMarginSweep(2.5f));
		TestFalse(
			TEXT("Step limiting must not throw away a tilt-admissible ready pelvis candidate"),
			UPhysAnimComponent::TestOnlyShouldAcceptStepLimitedPhase1PelvisRotation(
				true,
				true,
				true,
				true,
				false,
				false));
		TestTrue(
			TEXT("Step limiting can still be used when it preserves readiness"),
			UPhysAnimComponent::TestOnlyShouldAcceptStepLimitedPhase1PelvisRotation(
				true,
				false,
				false,
				true,
				false,
				false));
		TestTrue(
			TEXT("Spine-only rescue sweep runs for the current near-miss pattern"),
			UPhysAnimComponent::TestOnlyShouldRunSpineOnlyRootOnReadinessRescueSweep(
				31.82f,
				32.74f,
				19.55f));
		TestFalse(
			TEXT("Spine-only rescue sweep stays off when a thigh also misses readiness"),
			UPhysAnimComponent::TestOnlyShouldRunSpineOnlyRootOnReadinessRescueSweep(
				39.5f,
				32.74f,
				19.55f));
		TestTrue(
			TEXT("Spine-biased direct blend sweep runs for the current thigh-safe spine near-miss"),
			UPhysAnimComponent::TestOnlyShouldRunSpineBiasedDirectConstraintBlendSweep(
				31.82f,
				32.74f,
				19.55f));
		TestFalse(
			TEXT("Spine-biased direct blend sweep stays off when a thigh already misses readiness"),
			UPhysAnimComponent::TestOnlyShouldRunSpineBiasedDirectConstraintBlendSweep(
				39.1f,
				32.74f,
				19.55f));
		TestTrue(
			TEXT("Spine-focused pair blend sweep runs for the same thigh-safe spine near-miss"),
			UPhysAnimComponent::TestOnlyShouldRunSpineFocusedPairBlendSweep(
				31.82f,
				32.74f,
				19.55f));
		TestTrue(
			TEXT("Alternate-reference direct blend sweep runs for the same thigh-safe spine near-miss"),
			UPhysAnimComponent::TestOnlyShouldRunAlternateReferenceDirectConstraintBlendSweep(
				31.82f,
				32.74f,
				19.55f));
		TestTrue(
			TEXT("Spine-constraint interpolation sweep runs for the same thigh-safe spine near-miss"),
			UPhysAnimComponent::TestOnlyShouldRunSpineConstraintInterpolationSweep(
				31.82f,
				32.74f,
				19.55f));
		TestTrue(
			TEXT("Worst-thigh interpolation sweep runs once spine is back inside readiness and a thigh remains the blocker"),
			UPhysAnimComponent::TestOnlyShouldRunWorstThighConstraintInterpolationSweep(
				32.14f,
				39.54f,
				17.59f));
		TestTrue(
			TEXT("Spine-safe worst-thigh focused delta stays enabled for the current live thigh near-miss"),
			UPhysAnimComponent::TestOnlyShouldRunSpineSafeWorstThighFocusedDelta(
				31.49f,
				38.89f,
				17.98f));
		TestFalse(
			TEXT("Worst-thigh interpolation sweep stays off while spine is still the blocker"),
			UPhysAnimComponent::TestOnlyShouldRunWorstThighConstraintInterpolationSweep(
				31.82f,
				32.74f,
				19.55f));
		TestFalse(
			TEXT("Spine-safe worst-thigh focused delta stays off while spine is still the blocker"),
			UPhysAnimComponent::TestOnlyShouldRunSpineSafeWorstThighFocusedDelta(
				31.82f,
				32.74f,
				19.55f));
		TestTrue(
			TEXT("Focused-bone sample relevance includes weighted blends that name the blocked thigh in source"),
			UPhysAnimComponent::TestOnlyIsConstraintSampleRelevantToFocusedBone(
				NAME_None,
				TEXT("blend_weighted_direct_thigh_l_0.10_thigh_r_0.20_spine_01_0.70"),
				TEXT("thigh_r")));
		TestFalse(
			TEXT("Focused-bone sample relevance excludes blends that do not include the blocked thigh"),
			UPhysAnimComponent::TestOnlyIsConstraintSampleRelevantToFocusedBone(
				NAME_None,
				TEXT("blend_weighted_direct_thigh_l_0.25_spine_01_0.75"),
				TEXT("thigh_r")));
		TestFalse(
			TEXT("Worst-thigh follow-through must not re-break spine readiness after spine interpolation fixed it"),
			UPhysAnimComponent::TestOnlyShouldAcceptWorstThighConstraintInterpolationCandidate(
				32.14f,
				34.54f,
				17.59f,
				31.30f,
				32.81f,
				19.20f));
		TestTrue(
			TEXT("Worst-thigh follow-through may improve the worst thigh when spine readiness stays intact"),
			UPhysAnimComponent::TestOnlyShouldAcceptWorstThighConstraintInterpolationCandidate(
				32.14f,
				34.54f,
				17.59f,
				31.30f,
				32.81f,
				17.80f));
		TestTrue(
			TEXT("Spine-safe worst-thigh margin sweep may spend a small amount of recovered spine margin to improve the live thigh blocker"),
			UPhysAnimComponent::TestOnlyShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
				31.97f,
				34.19f,
				17.91f,
				32.02f,
				33.72f,
				17.98f));
		TestFalse(
			TEXT("Spine-safe worst-thigh margin sweep must reject candidates that spend too much recovered spine margin"),
			UPhysAnimComponent::TestOnlyShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
				31.97f,
				34.19f,
				17.91f,
				32.02f,
				33.72f,
				18.40f));
		TestTrue(
			TEXT("Spine-only rescue scoring prefers a candidate that improves the actual spine blocker while keeping thighs ready"),
			UPhysAnimComponent::TestOnlyShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
				31.82f,
				32.74f,
				19.55f,
				31.95f,
				32.90f,
				18.95f));
		TestFalse(
			TEXT("Spine-only rescue scoring rejects candidates that fix spine by breaking thigh readiness"),
			UPhysAnimComponent::TestOnlyShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
				31.82f,
				32.74f,
				19.55f,
				31.95f,
				39.30f,
				18.95f));
		TestTrue(
			TEXT("Spine-only rescue acceptance keeps a spine-improving candidate that preserves thigh readiness"),
			UPhysAnimComponent::TestOnlyShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
				31.82f,
				32.74f,
				19.55f,
				31.95f,
				32.90f,
				18.95f));
		TestFalse(
			TEXT("Spine-only rescue acceptance refuses a candidate that only wins by generic score after breaking thigh readiness"),
			UPhysAnimComponent::TestOnlyShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
				31.82f,
				32.74f,
				19.55f,
				32.66f,
				39.49f,
				19.53f));
		TestTrue(
			TEXT("Spine-only rescue acceptance still allows a small thigh-margin trade when it preserves readiness and improves the live spine blocker"),
			UPhysAnimComponent::TestOnlyShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
				31.82f,
				32.74f,
				19.55f,
				31.95f,
				32.90f,
				19.53f));
		TestEqual(
			TEXT("Internal ReadyForPhase3 handoff publishes as Settle, not BridgeActive"),
			UPhysAnimComponent::MapBalanceTransitionPhaseToRuntimeState(EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3),
			EPhysAnimRuntimeState::BalanceEntry_Settle);
		TestEqual(
			TEXT("SafeDenied transition publishes as BalanceSafeDeny"),
			UPhysAnimComponent::MapBalanceTransitionPhaseToRuntimeState(EBalanceReadyTransitionPhase::BRT_SafeDenied),
			EPhysAnimRuntimeState::BalanceSafeDeny);
		return true;
	}

#if !UE_BUILD_SHIPPING
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1PelvisCouplingSearchConfigMappingTest,
		"PhysAnim.Component.Phase1PelvisCouplingSearchConfigMapping",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1PelvisCouplingSearchConfigMappingTest::RunTest(const FString& Parameters)
	{
		const FPhase1PelvisCouplingSearchConfig RuntimeDefaultConfig = BuildPhase1PelvisCouplingSearchConfig(TOptional<FPhase1AutoCalibParams>());

		FPhase1AutoCalibParams DefaultParams;
		DefaultParams.SourcePreset = EPhase1AutoCalibStrategyPreset::CurrentDefault;
		DefaultParams.SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::CurrentDefault;
		const FPhase1PelvisCouplingSearchConfig CurrentDefaultConfig =
			BuildPhase1PelvisCouplingSearchConfig(TOptional<FPhase1AutoCalibParams>(DefaultParams));
		TestEqual(TEXT("Runtime default config matches CurrentDefault preset mapping"), RuntimeDefaultConfig.SpineInterpolationAlpha, CurrentDefaultConfig.SpineInterpolationAlpha);
		TestEqual(TEXT("Runtime default keeps coupled trade disabled"), RuntimeDefaultConfig.bEnableCoupledTradeControlPass, false);

		FPhase1AutoCalibParams RescueParams;
		RescueParams.SourcePreset = EPhase1AutoCalibStrategyPreset::RescueOnly;
		RescueParams.SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::RescueOnly;
		const FPhase1PelvisCouplingSearchConfig RescueConfig =
			BuildPhase1PelvisCouplingSearchConfig(TOptional<FPhase1AutoCalibParams>(RescueParams));
		TestFalse(TEXT("RescueOnly disables spine-biased direct-blend seeds"), RescueConfig.bEnableSpineBiasedDirectBlendSeeds);
		TestFalse(TEXT("RescueOnly disables pair-blend seeds"), RescueConfig.bEnablePairBlendSeeds);
		TestFalse(TEXT("RescueOnly disables worst-thigh interpolation sweep"), RescueConfig.bEnableWorstThighInterpolationSweep);

		FPhase1AutoCalibParams CoupledParams;
		CoupledParams.SourcePreset = EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily;
		CoupledParams.SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily;
		const FPhase1PelvisCouplingSearchConfig CoupledConfig =
			BuildPhase1PelvisCouplingSearchConfig(TOptional<FPhase1AutoCalibParams>(CoupledParams));
		TestTrue(TEXT("CoupledTradeControlFamily enables the bounded coupled trade pass"), CoupledConfig.bEnableCoupledTradeControlPass);
		TestEqual(TEXT("CoupledTradeControlFamily reuses SpineThenWorstThigh as its seed family"), CoupledConfig.SeedFamilyPreset, EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh);

		FPhase1AutoCalibParams PairFrontierParams;
		PairFrontierParams.SourcePreset = EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough;
		PairFrontierParams.SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough;
		const FPhase1PelvisCouplingSearchConfig PairFrontierConfig =
			BuildPhase1PelvisCouplingSearchConfig(TOptional<FPhase1AutoCalibParams>(PairFrontierParams));
		TestTrue(TEXT("PairBlendFrontierFollowThrough enables the local follow-through pass"), PairFrontierConfig.bEnablePairBlendFrontierFollowThroughPass);
		TestTrue(TEXT("PairBlendFrontierFollowThrough enables local interpolation"), PairFrontierConfig.bEnablePairBlendFrontierInterpolationPass);
		TestFalse(TEXT("PairBlendFrontierFollowThrough keeps coupled-trade disabled"), PairFrontierConfig.bEnableCoupledTradeControlPass);
		TestEqual(TEXT("PairBlendFrontierFollowThrough reuses RescueOnly as its seed family"), PairFrontierConfig.SeedFamilyPreset, EPhase1AutoCalibStrategyPreset::RescueOnly);
		TestEqual(TEXT("Pair frontier sources classify distinctly from generic pair blends"), ClassifyPhase1PelvisCouplingSearchFamily(TEXT("pair_frontier_weight_thigh_l_0.10_thigh_r_0.30_spine_01_0.60")), EPhase1PelvisCouplingSearchFamily::PairBlendFrontierFollowThrough);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1CoupledTradeControlAcceptanceTest,
		"PhysAnim.Component.Phase1CoupledTradeControlAcceptance",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1CoupledTradeControlAcceptanceTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Coupled trade control rejects a spine improvement that re-breaks the paired thigh frontier too far"),
			ShouldAcceptPhase1CoupledTradeControlCandidate(
				31.40f,
				33.10f,
				18.20f,
				31.35f,
				33.65f,
				17.60f,
				1.25f,
				1.00f,
				0.25f));
		TestTrue(
			TEXT("Coupled trade control accepts a bounded paired trade when weighted gain beats regression"),
			ShouldAcceptPhase1CoupledTradeControlCandidate(
				31.40f,
				33.10f,
				18.20f,
				31.20f,
				33.28f,
				17.92f,
				1.25f,
				1.00f,
				0.25f));
		TestTrue(
			TEXT("Pair-blend frontier follow-through accepts a spine-priority improvement with bounded thigh regression"),
			ShouldAcceptPhase1PairBlendFrontierCandidate(
				31.40f,
				33.10f,
				18.20f,
				31.50f,
				33.22f,
				17.90f,
				true,
				1.50f,
				1.00f,
				0.25f));
		TestFalse(
			TEXT("Pair-blend frontier follow-through rejects a spine-priority candidate that does not improve the blocker"),
			ShouldAcceptPhase1PairBlendFrontierCandidate(
				31.40f,
				33.10f,
				18.20f,
				31.10f,
				32.90f,
				18.20f,
				true,
				1.50f,
				1.00f,
				0.25f));
		TestTrue(
			TEXT("Pair-blend frontier follow-through accepts a thigh-priority improvement with bounded spine regression"),
			ShouldAcceptPhase1PairBlendFrontierCandidate(
				31.40f,
				39.10f,
				17.80f,
				31.30f,
				38.86f,
				17.96f,
				false,
				1.50f,
				1.00f,
				0.25f));
		TestFalse(
			TEXT("Pair-blend frontier follow-through rejects candidates that re-break the paired margin beyond the cap"),
			ShouldAcceptPhase1PairBlendFrontierCandidate(
				31.40f,
				39.10f,
				17.80f,
				31.30f,
				38.86f,
				18.20f,
				false,
				1.50f,
				1.00f,
				0.25f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibParamsDoNotMutateContractSettingsTest,
		"PhysAnim.Component.Phase1AutoCalibParamsDoNotMutateContractSettings",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibParamsDoNotMutateContractSettingsTest::RunTest(const FString& Parameters)
	{
		UPhysAnimComponent* const Component = NewObject<UPhysAnimComponent>();
		TestNotNull(TEXT("Transient component exists"), Component);
		if (!Component)
		{
			return false;
		}

		const FPhysAnimStabilizationSettings Before = Component->GetConfiguredStabilizationSettings();

		FPhase1AutoCalibParams Params;
		Params.SourcePreset = EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh;
		Params.SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::RescueOnly;
		Params.SpineInterpolationAlpha = 0.33f;
		Params.WorstThighInterpolationAlpha = 0.08f;
		Params.FocusedDeltaScale = 1.75f;
		Params.UprightnessWeightScale = 1.20f;
		Params.ClampStrengthScale = 0.85f;
		Params.PelvisPitchBiasDeg = 0.4f;
		Params.PelvisRollBiasDeg = -0.3f;
		Component->ApplyPhase1AutoCalibParams(Params);

		const FPhysAnimStabilizationSettings After = Component->GetConfiguredStabilizationSettings();
		TestEqual(TEXT("Prepare duration remains a contract setting"), After.BalancePhase1PrepareDuration, Before.BalancePhase1PrepareDuration);
		TestEqual(TEXT("LateValidate duration remains a contract setting"), After.BalancePhase1LateValidateRequiredSeconds, Before.BalancePhase1LateValidateRequiredSeconds);
		TestEqual(TEXT("Quiet root linear speed remains a contract setting"), After.BalancePhase1QuietRootLinearSpeed, Before.BalancePhase1QuietRootLinearSpeed);
		TestEqual(TEXT("Quiet root angular speed remains a contract setting"), After.BalancePhase1QuietRootAngularSpeed, Before.BalancePhase1QuietRootAngularSpeed);
		TestEqual(TEXT("Quiet shell offset remains a contract setting"), After.BalancePhase1QuietShellOffsetDelta, Before.BalancePhase1QuietShellOffsetDelta);
		TestEqual(TEXT("Quiet shell velocity remains a contract setting"), After.BalancePhase1QuietShellVelocityDelta, Before.BalancePhase1QuietShellVelocityDelta);
		TestEqual(TEXT("Phase 2 tilt gate remains a contract setting"), After.BalancePhase2EntryMaxRootTiltDeg, Before.BalancePhase2EntryMaxRootTiltDeg);

		Component->ClearPhase1AutoCalibParams();
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibScoreOrderingTest,
		"PhysAnim.Component.Phase1AutoCalibScoreOrdering",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibScoreOrderingTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibScore ContractPassing;
		ContractPassing.bContractPassed = true;
		ContractPassing.bReachedRootOn = true;
		ContractPassing.bNoCouplingProofSatisfied = true;
		ContractPassing.WorstDirectLinkAngularErrorDeg = 18.0f;
		ContractPassing.MeanTargetDeltaDeg = 2.0f;
		ContractPassing.MaxTargetDeltaDeg = 5.0f;
		ContractPassing.ThighAsymmetryDeg = 1.0f;
		ContractPassing.PeakRootTiltDeg = 10.0f;
		ContractPassing.ShellOffsetDeltaCm = 1.0f;
		ContractPassing.ShellVelocityDeltaCmPerSecond = 2.0f;
		ContractPassing.PeakRootLinearSpeedCmPerSecond = 20.0f;
		ContractPassing.PeakRootAngularSpeedDegPerSecond = 30.0f;
		UPhysAnimComponent::FinalizePhase1AutoCalibScore(ContractPassing);

		FPhase1AutoCalibScore ContractFailing = ContractPassing;
		ContractFailing.bContractPassed = false;
		ContractFailing.bReachedRootOn = false;
		UPhysAnimComponent::FinalizePhase1AutoCalibScore(ContractFailing);

		TestTrue(
			TEXT("A contract-passing candidate outranks a contract-failing candidate"),
			UPhysAnimComponent::IsBetterPhase1AutoCalibScore(ContractPassing, ContractFailing));
		TestFalse(
			TEXT("A contract-failing candidate cannot outrank a contract-passing candidate"),
			UPhysAnimComponent::IsBetterPhase1AutoCalibScore(ContractFailing, ContractPassing));

		FPhase1AutoCalibScore LowerWorstAngular = ContractPassing;
		LowerWorstAngular.WorstDirectLinkAngularErrorDeg = 16.0f;
		UPhysAnimComponent::FinalizePhase1AutoCalibScore(LowerWorstAngular);
		TestTrue(
			TEXT("Among passing candidates, lower worst direct-link angular error wins first"),
			UPhysAnimComponent::IsBetterPhase1AutoCalibScore(LowerWorstAngular, ContractPassing));

		FPhase1AutoCalibScore LowerMeanTarget = ContractPassing;
		LowerMeanTarget.MeanTargetDeltaDeg = 1.5f;
		UPhysAnimComponent::FinalizePhase1AutoCalibScore(LowerMeanTarget);
		TestTrue(
			TEXT("Mean target delta breaks ties after worst direct-link angular error"),
			UPhysAnimComponent::IsBetterPhase1AutoCalibScore(LowerMeanTarget, ContractPassing));

		FPhase1AutoCalibScore TimedOut = ContractPassing;
		TimedOut.bTimedOut = true;
		UPhysAnimComponent::FinalizePhase1AutoCalibScore(TimedOut);
		TestFalse(
			TEXT("Timed-out candidates cannot outrank a passing non-timeout candidate"),
			UPhysAnimComponent::IsBetterPhase1AutoCalibScore(TimedOut, ContractPassing));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibRequestDefaultsTest,
		"PhysAnim.Component.Phase1AutoCalibRequestDefaults",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibRequestDefaultsTest::RunTest(const FString& Parameters)
	{
		const FPhase1AutoCalibRequest Request;
		TestEqual(TEXT("Phase 1 auto-calibration defaults to full-search mode"), Request.BudgetMode, EPhase1AutoCalibBudgetMode::FullSearch);
		TestEqual(TEXT("Phase 1 auto-calibration defaults to no explicit trial cap"), Request.MaxTrials, INDEX_NONE);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibTimeoutStartsOnTrialEntryTest,
		"PhysAnim.Component.Phase1AutoCalibTimeoutStartsOnTrialEntry",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibTimeoutStartsOnTrialEntryTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Pre-start queue wait does not consume the active trial timeout budget"),
			UPhysAnimPhase1AutoCalibSubsystem::IsActiveTrialTimeoutReached(false, -1.0, 10.0, 0.75));

		TestFalse(
			TEXT("Started trials below the timeout budget stay active"),
			UPhysAnimPhase1AutoCalibSubsystem::IsActiveTrialTimeoutReached(true, 10.0, 10.5, 0.75));

		TestTrue(
			TEXT("Started trials time out once the configured budget has elapsed"),
			UPhysAnimPhase1AutoCalibSubsystem::IsActiveTrialTimeoutReached(true, 10.0, 10.75, 0.75));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibMetricsStartOnTrialEntryTest,
		"PhysAnim.Component.Phase1AutoCalibMetricsStartOnTrialEntry",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibMetricsStartOnTrialEntryTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Pre-start queue wait does not contribute to active trial peak metrics"),
			UPhysAnimPhase1AutoCalibSubsystem::ShouldAccumulateActiveTrialMetrics(false));

		TestTrue(
			TEXT("Only started trials contribute to active trial peak metrics"),
			UPhysAnimPhase1AutoCalibSubsystem::ShouldAccumulateActiveTrialMetrics(true));

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibActionHistorySnapshotRoundTripTest,
		"PhysAnim.Component.Phase1AutoCalibActionHistorySnapshotRoundTrip",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibActionHistorySnapshotRoundTripTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibBaselineSnapshot Snapshot;
		const TArray<float> ConditionedActions = { 1.0f, 2.0f, 3.0f };
		const TArray<float> PreviousConditionedActions = { 4.0f, 5.0f };
		const TArray<float> ActionOutputs = { 6.0f, 7.0f, 8.0f, 9.0f };
		const TArray<float> PreviousActionOutputs = { 10.0f, 11.0f, 12.0f };

		UPhysAnimComponent::TestOnlyStorePhase1AutoCalibActionHistory(
			Snapshot,
			ConditionedActions,
			PreviousConditionedActions,
			ActionOutputs,
			PreviousActionOutputs);

		TestEqual(TEXT("Snapshot stores conditioned actions"), Snapshot.ConditionedActionBuffer, ConditionedActions);
		TestEqual(TEXT("Snapshot stores previous conditioned actions"), Snapshot.PreviousConditionedActionBuffer, PreviousConditionedActions);
		TestEqual(TEXT("Snapshot stores action outputs"), Snapshot.ActionOutputBuffer, ActionOutputs);
		TestEqual(TEXT("Snapshot stores previous action outputs distinctly"), Snapshot.PreviousActionOutputBuffer, PreviousActionOutputs);

		TArray<float> RestoredConditionedActions = { -1.0f };
		TArray<float> RestoredPreviousConditionedActions = { -2.0f };
		TArray<float> RestoredActionOutputs = { -3.0f };
		TArray<float> RestoredPreviousActionOutputs = { -4.0f };
		UPhysAnimComponent::TestOnlyRestorePhase1AutoCalibActionHistory(
			Snapshot,
			RestoredConditionedActions,
			RestoredPreviousConditionedActions,
			RestoredActionOutputs,
			RestoredPreviousActionOutputs);

		TestEqual(TEXT("Restore round-trips conditioned actions"), RestoredConditionedActions, ConditionedActions);
		TestEqual(TEXT("Restore round-trips previous conditioned actions"), RestoredPreviousConditionedActions, PreviousConditionedActions);
		TestEqual(TEXT("Restore round-trips action outputs"), RestoredActionOutputs, ActionOutputs);
		TestEqual(TEXT("Restore round-trips previous action outputs"), RestoredPreviousActionOutputs, PreviousActionOutputs);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibStageACandidatesTest,
		"PhysAnim.Component.Phase1AutoCalibStageACandidates",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibStageACandidatesTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibRequest FullSearchRequest;
		FullSearchRequest.Seed = 1337;
		FullSearchRequest.BudgetMode = EPhase1AutoCalibBudgetMode::FullSearch;

		TArray<FPhase1AutoCalibParams> Candidates;
		UPhysAnimPhase1AutoCalibSubsystem::BuildStageACandidates(FullSearchRequest, Candidates);

		TestEqual(TEXT("Stage A emits the fixed 24 samples for each of 8 presets"), Candidates.Num(), 24 * 8);

		TSet<EPhase1AutoCalibStrategyPreset> SeenPresets;
		for (const FPhase1AutoCalibParams& Candidate : Candidates)
		{
			SeenPresets.Add(Candidate.SourcePreset);
			TestEqual(TEXT("Stage A seed family tracks the source preset in v1"), Candidate.SeedFamilyPreset, Candidate.SourcePreset);
			TestTrue(TEXT("Stage A spine interpolation stays in range"), Candidate.SpineInterpolationAlpha >= 0.01f && Candidate.SpineInterpolationAlpha <= 0.80f);
			TestTrue(TEXT("Stage A worst-thigh interpolation stays in range"), Candidate.WorstThighInterpolationAlpha >= 0.01f && Candidate.WorstThighInterpolationAlpha <= 0.20f);
			TestTrue(TEXT("Stage A focused-delta scale stays in range"), Candidate.FocusedDeltaScale >= 0.50f && Candidate.FocusedDeltaScale <= 2.00f);
			TestTrue(TEXT("Stage A uprightness scale stays in range"), Candidate.UprightnessWeightScale >= 0.50f && Candidate.UprightnessWeightScale <= 1.50f);
			TestTrue(TEXT("Stage A clamp scale stays in range"), Candidate.ClampStrengthScale >= 0.50f && Candidate.ClampStrengthScale <= 1.50f);
			TestTrue(TEXT("Stage A pelvis pitch bias stays in range"), Candidate.PelvisPitchBiasDeg >= -1.0f && Candidate.PelvisPitchBiasDeg <= 1.0f);
			TestTrue(TEXT("Stage A pelvis roll bias stays in range"), Candidate.PelvisRollBiasDeg >= -1.0f && Candidate.PelvisRollBiasDeg <= 1.0f);
		}

		TestEqual(TEXT("Stage A covers all eight fixed presets"), SeenPresets.Num(), 8);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibSmokeBudgetStageACandidatesTest,
		"PhysAnim.Component.Phase1AutoCalibSmokeBudgetStageACandidates",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibSmokeBudgetStageACandidatesTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibRequest SmokeRequest;
		SmokeRequest.Seed = 1337;
		SmokeRequest.BudgetMode = EPhase1AutoCalibBudgetMode::Smoke;

		TArray<FPhase1AutoCalibParams> CandidatesA;
		TArray<FPhase1AutoCalibParams> CandidatesB;
		UPhysAnimPhase1AutoCalibSubsystem::BuildStageACandidates(SmokeRequest, CandidatesA);
		UPhysAnimPhase1AutoCalibSubsystem::BuildStageACandidates(SmokeRequest, CandidatesB);

		TestEqual(TEXT("Smoke Stage A stays intentionally small"), CandidatesA.Num(), 8);
		TestEqual(TEXT("Smoke Stage A generation is deterministic"), CandidatesB.Num(), CandidatesA.Num());

		TSet<EPhase1AutoCalibStrategyPreset> SeenPresets;
		for (int32 Index = 0; Index < CandidatesA.Num(); ++Index)
		{
			SeenPresets.Add(CandidatesA[Index].SourcePreset);
			TestEqual(TEXT("Smoke Stage A remains deterministic candidate-by-candidate"), CandidatesB[Index].SourcePreset, CandidatesA[Index].SourcePreset);
			TestEqual(TEXT("Smoke Stage A seed families track source presets"), CandidatesA[Index].SeedFamilyPreset, CandidatesA[Index].SourcePreset);
		}

		TestEqual(TEXT("Smoke Stage A still covers all eight fixed presets"), SeenPresets.Num(), 8);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibReportAggregationTest,
		"PhysAnim.Component.Phase1AutoCalibReportAggregation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibReportAggregationTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibReport Report;
		Report.Trials.Add(MakePhase1AutoCalibTrial(
			EPhase1AutoCalibStrategyPreset::CurrentDefault,
			TEXT("timed_out"),
			TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient"),
			false,
			34.0f,
			2.5f));
		Report.Trials.Add(MakePhase1AutoCalibTrial(
			EPhase1AutoCalibStrategyPreset::SpineBiased,
			TEXT("timed_out"),
			TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient"),
			false,
			31.0f,
			2.0f));
		Report.Trials.Add(MakePhase1AutoCalibTrial(
			EPhase1AutoCalibStrategyPreset::SpineBiased,
			TEXT("timed_out"),
			TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient"),
			false,
			32.0f,
			1.8f));
		Report.Trials.Add(MakePhase1AutoCalibTrial(
			EPhase1AutoCalibStrategyPreset::BalancedCoupled,
			TEXT("reached_root_on"),
			TEXT("ready"),
			true,
			17.0f,
			0.8f));
		Report.Trials.Last().bReproducible = true;
	Report.Trials[0].WinningSearchFamily = TEXT("worst_thigh_interpolation");
	Report.Trials[1].WinningSearchFamily = TEXT("spine_constraint_interpolation");
	Report.Trials[2].WinningSearchFamily = TEXT("tilt_spine_rescue");
	Report.Trials[3].WinningSearchFamily = TEXT("coupled_trade_control");
	Report.Trials[3].bCoupledTradeControlWon = true;

	UPhysAnimPhase1AutoCalibSubsystem::FinalizeReportData(Report);

		TestTrue(TEXT("Report picks the contract-passing best candidate"), Report.bHasBestCandidate);
		TestEqual(TEXT("Best candidate comes from the passing preset"), Report.BestCandidate.Params.SourcePreset, EPhase1AutoCalibStrategyPreset::BalancedCoupled);
	TestTrue(TEXT("Report marks the reproducible truthful pass when one exists"), Report.bHasReproducibleTruthfulPass);
	TestEqual(TEXT("Truthful pass classification wins when a passing candidate exists"), Report.FrontierClassification, EPhase1AutoCalibFrontierClassification::TruthfulPassFound);
	TestEqual(TEXT("Passing frontier recommends promotion"), Report.RecommendedAction, EPhase1AutoCalibRecommendedAction::PromoteBestCandidate);
	TestEqual(TEXT("Passing frontier does not name a follow-up expansion"), Report.RecommendedExpansionName, FString());
		TestTrue(TEXT("Report picks a near-pass when non-passing trials exist"), Report.bHasBestNearPass);
		TestEqual(TEXT("Best near-pass comes from the lowest-error failing preset"), Report.BestNearPass.Params.SourcePreset, EPhase1AutoCalibStrategyPreset::SpineBiased);
		TestEqual(TEXT("Best candidate keeps winning search attribution"), Report.BestCandidate.WinningSearchFamily, FString(TEXT("coupled_trade_control")));
		TestTrue(TEXT("Report surfaces timeout-before-RootOn telemetry when any failing trial never reached RootOn"), Report.bAnyTimedOutBeforeRootOn);
		TestTrue(TEXT("Report surfaces timeout-before-proof telemetry when any failing trial never reached proof"), Report.bAnyTimedOutBeforeNoCouplingProof);
		TestEqual(TEXT("Preset summaries are emitted for each preset seen in the trials"), Report.PresetSummaries.Num(), 3);

		const FPhase1AutoCalibPresetSummary* SpineSummary = nullptr;
		const FPhase1AutoCalibPresetSummary* DefaultSummary = nullptr;
		for (const FPhase1AutoCalibPresetSummary& Summary : Report.PresetSummaries)
		{
			if (Summary.Preset == EPhase1AutoCalibStrategyPreset::SpineBiased)
			{
				SpineSummary = &Summary;
			}
			else if (Summary.Preset == EPhase1AutoCalibStrategyPreset::CurrentDefault)
			{
				DefaultSummary = &Summary;
			}
		}

		TestNotNull(TEXT("SpineBiased preset summary exists"), SpineSummary);
		TestNotNull(TEXT("CurrentDefault preset summary exists"), DefaultSummary);
		if (SpineSummary)
		{
			TestEqual(TEXT("Preset summary tracks trial count"), SpineSummary->TrialCount, 2);
			TestTrue(TEXT("Preset summary captures its best near-pass"), SpineSummary->bHasBestNearPass);
			TestEqual(TEXT("Preset summary blocker histogram keeps repeated blocker counts"), SpineSummary->BlockerCounts.Num(), 2);
			TestTrue(TEXT("Preset summary detects improvement over CurrentDefault worst direct-link error"), SpineSummary->bImprovesWorstDirectLinkVsCurrentDefault);
			TestTrue(TEXT("Preset summary detects improvement over CurrentDefault thigh asymmetry"), SpineSummary->bImprovesThighAsymmetryVsCurrentDefault);
		}

		if (DefaultSummary)
		{
			TestEqual(TEXT("CurrentDefault baseline improvement stays zero"), DefaultSummary->WorstDirectLinkImprovementVsCurrentDefaultDeg, 0.0f);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibFrontierClassificationTest,
		"PhysAnim.Component.Phase1AutoCalibFrontierClassification",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibFrontierClassificationTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibReport Report;
		Report.Trials.Add(MakePhase1AutoCalibTrial(
			EPhase1AutoCalibStrategyPreset::CurrentDefault,
			TEXT("timed_out"),
			TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient"),
			false,
			34.0f,
			1.0f));
		Report.Trials.Add(MakePhase1AutoCalibTrial(
			EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh,
			TEXT("timed_out"),
			TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient"),
			false,
			32.0f,
			3.0f));
		Report.Trials.Add(MakePhase1AutoCalibTrial(
			EPhase1AutoCalibStrategyPreset::RescueOnly,
			TEXT("timed_out"),
			TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient"),
			false,
			33.5f,
			1.5f));

		UPhysAnimPhase1AutoCalibSubsystem::FinalizeReportData(Report);

		TestEqual(TEXT("Mixed thigh/spine blockers with frontier improvement classify as coupled flip"), Report.FrontierClassification, EPhase1AutoCalibFrontierClassification::CoupledSpineThighFlip);
		TestEqual(TEXT("Coupled flip recommends the bounded coupled trade-control expansion"), Report.RecommendedAction, EPhase1AutoCalibRecommendedAction::AddCoupledTradeControlExpansion);
		TestEqual(TEXT("Coupled flip names the bounded next expansion"), Report.RecommendedExpansionName, FString(TEXT("CoupledTradeControlFamily")));
		TestEqual(TEXT("Overall dominant blocker keeps the highest-count truthful blocker"), Report.DominantTruthfulBlocker, FString(TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibStageBCandidateFeedTest,
		"PhysAnim.Component.Phase1AutoCalibStageBCandidateFeed",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibStageBCandidateFeedTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibRequest FullSearchRequest;
		FullSearchRequest.BudgetMode = EPhase1AutoCalibBudgetMode::FullSearch;

		TArray<FPhase1AutoCalibTrialResult> StageAResults;
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::CurrentDefault, TEXT("timed_out"), TEXT("blocker_default"), false, 35.0f, 3.0f));
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::SpineBiased, TEXT("timed_out"), TEXT("blocker_spine"), false, 31.0f, 2.1f));
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::WorstThighBiased, TEXT("timed_out"), TEXT("blocker_worst_thigh"), false, 30.5f, 1.9f));
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::BalancedCoupled, TEXT("timed_out"), TEXT("blocker_balanced"), false, 29.5f, 1.8f));
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh, TEXT("timed_out"), TEXT("blocker_spine_then_thigh"), false, 29.0f, 1.7f));
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::RescueOnly, TEXT("timed_out"), TEXT("blocker_rescue"), false, 28.5f, 1.6f));
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily, TEXT("timed_out"), TEXT("blocker_coupled_trade"), false, 28.0f, 1.5f));
		StageAResults.Add(MakePhase1AutoCalibTrial(EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough, TEXT("timed_out"), TEXT("blocker_pair_frontier"), false, 27.8f, 1.4f));

		TArray<FPhase1AutoCalibParams> StageBCandidates;
		UPhysAnimPhase1AutoCalibSubsystem::BuildStageBRefinementCandidates(StageAResults, FullSearchRequest, StageBCandidates);

		TestTrue(TEXT("Stage B emits refinement candidates from the broadened Stage A set"), StageBCandidates.Num() > 0);

		TSet<EPhase1AutoCalibStrategyPreset> SeenPresets;
		for (const FPhase1AutoCalibParams& Candidate : StageBCandidates)
		{
			SeenPresets.Add(Candidate.SourcePreset);
		}

		TestTrue(TEXT("Stage B receives a broadened Stage A set rather than collapsing to one preset family"), SeenPresets.Num() > 1);

		TArray<FPhase1AutoCalibTrialResult> StageBResults;
		for (int32 PresetIndex = 0; PresetIndex < 8; ++PresetIndex)
		{
			StageBResults.Add(MakePhase1AutoCalibTrial(
				static_cast<EPhase1AutoCalibStrategyPreset>(PresetIndex),
				TEXT("timed_out"),
				TEXT("stage_b_blocker"),
				false,
				28.0f + static_cast<float>(PresetIndex),
				1.0f + 0.1f * static_cast<float>(PresetIndex)));
		}

		TArray<FPhase1AutoCalibParams> StageCCandidates;
		UPhysAnimPhase1AutoCalibSubsystem::BuildStageCReproCandidates(StageBResults, FullSearchRequest, StageCCandidates);
		TSet<EPhase1AutoCalibStrategyPreset> StageCSeenPresets;
		for (const FPhase1AutoCalibParams& Candidate : StageCCandidates)
		{
			StageCSeenPresets.Add(Candidate.SourcePreset);
		}
		TestTrue(TEXT("Stage C also receives multiple preset families from the broadened feed"), StageCSeenPresets.Num() > 1);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1AutoCalibReproducibilityTest,
		"PhysAnim.Component.Phase1AutoCalibReproducibility",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibReproducibilityTest::RunTest(const FString& Parameters)
	{
		FPhase1AutoCalibTrialResult TrialA;
		TrialA.TerminalClass = TEXT("failed");
		TrialA.TruthfulBlocker = TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient");
		TrialA.Score.bContractPassed = false;
		TrialA.Score.WorstDirectLinkAngularErrorDeg = 33.89f;
		TrialA.Score.MeanTargetDeltaDeg = 2.0f;
		TrialA.Score.MaxTargetDeltaDeg = 4.0f;
		TrialA.Score.ThighAsymmetryDeg = 2.4f;
		TrialA.Score.PeakRootTiltDeg = 17.98f;
		TrialA.Score.ShellOffsetDeltaCm = 1.0f;
		TrialA.Score.ShellVelocityDeltaCmPerSecond = 2.0f;
		TrialA.Score.PeakRootLinearSpeedCmPerSecond = 25.0f;
		TrialA.Score.PeakRootAngularSpeedDegPerSecond = 35.0f;
		TrialA.TrialTimeoutBudgetSeconds = 0.75f;
		TrialA.TimeToRootOnSeconds = 0.20f;
		TrialA.TimeToNoCouplingProofSeconds = 0.23f;

		FPhase1AutoCalibTrialResult TrialB = TrialA;
		TrialB.Score.WorstDirectLinkAngularErrorDeg += 1.0e-4f;

		TArray<FPhase1AutoCalibTrialResult> MatchingTrials;
		MatchingTrials.Add(TrialA);
		MatchingTrials.Add(TrialB);
		TestTrue(
			TEXT("Reproducibility accepts matching terminal class, blocker, and score breakdown within epsilon"),
			UPhysAnimPhase1AutoCalibSubsystem::AreTrialResultsReproducible(MatchingTrials));

		TrialB.TruthfulBlocker = TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient");
		MatchingTrials[1] = TrialB;
		TestFalse(
			TEXT("Reproducibility rejects a blocker mismatch"),
			UPhysAnimPhase1AutoCalibSubsystem::AreTrialResultsReproducible(MatchingTrials));
		return true;
	}
#endif

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPhase1ConstraintFrameBaselineAngularErrorTest,
		"PhysAnim.Component.Phase1ConstraintFrameBaselineAngularError",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1ConstraintFrameBaselineAngularErrorTest::RunTest(const FString& Parameters)
	{
		// This diagnostic test checks whether the PhysicsAsset constraint reference frames for the three
		// Phase 1 critical links (pelvis→thigh_l, pelvis→thigh_r, pelvis→spine_01) carry an intrinsic
		// angular offset at the reference pose (no rotation applied).
		//
		// If the constraint frames already disagree at identity, that disagreement is a permanent
		// structural floor that no pelvis rotation search can eliminate. The solver can only reduce the
		// error above that floor.
		//
		// Key question answered:
		//   Is the ~33° thigh error a rotational problem (wrong pelvis orientation)
		//   or a structural problem (SMPL-to-Manny constraint frame mismatch)?

		struct FLinkDiagnostic
		{
			FName ParentBone;
			FName ChildBone;
			float ConstraintFrameAngularMismatchDeg = 0.0f;
			FVector ParentAnchorLocalCm = FVector::ZeroVector;
			FVector ChildAnchorLocalCm = FVector::ZeroVector;
			FQuat ParentRefFrameQuat = FQuat::Identity;
			FQuat ChildRefFrameQuat = FQuat::Identity;
			bool bConstraintFound = false;
			float MaxThresholdDeg = 0.0f;
			float ReadinessThresholdDeg = 0.0f;
		};

		// Phase 1 critical links and their thresholds
		const float ThighMaxDeg = BalanceTransitionSets::Phase2MaxPelvisThighDirectLinkAngularErrorDeg;
		const float ThighReadinessDeg = BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg;
		const float SpineMaxDeg = BalanceTransitionSets::Phase2MaxPelvisSpineDirectLinkAngularErrorDeg;
		const float SpineReadinessDeg = BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg;

		TArray<FLinkDiagnostic> Links;
		{
			FLinkDiagnostic D;
			D.ParentBone = TEXT("pelvis");
			D.ChildBone = TEXT("thigh_l");
			D.MaxThresholdDeg = ThighMaxDeg;
			D.ReadinessThresholdDeg = ThighReadinessDeg;
			Links.Add(D);
		}
		{
			FLinkDiagnostic D;
			D.ParentBone = TEXT("pelvis");
			D.ChildBone = TEXT("thigh_r");
			D.MaxThresholdDeg = ThighMaxDeg;
			D.ReadinessThresholdDeg = ThighReadinessDeg;
			Links.Add(D);
		}
		{
			FLinkDiagnostic D;
			D.ParentBone = TEXT("pelvis");
			D.ChildBone = TEXT("spine_01");
			D.MaxThresholdDeg = SpineMaxDeg;
			D.ReadinessThresholdDeg = SpineReadinessDeg;
			Links.Add(D);
		}

		// Try to load the default Manny PhysicsAsset
		// We use the long-form paths that match the PhysAnim component's expectations
		const FString MeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
		const FString PhysicsAssetPath = TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin");
		
		USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
		UPhysicsAsset* PhysicsAsset = SkelMesh ? SkelMesh->GetPhysicsAsset() : nullptr;
		
		// Fallback to direct physics asset load if the mesh doesn't have it (this might happen with SKM_Manny vs PA_Mannequin separation)
		if (!PhysicsAsset)
		{
			PhysicsAsset = LoadObject<UPhysicsAsset>(nullptr, *PhysicsAssetPath);
		}

		if (!PhysicsAsset)
		{
			// In editor-only test contexts the asset may not be loadable — report but don't fail
			AddInfo(FString::Printf(
				TEXT("DIAGNOSTIC: Could not load PhysicsAsset from '%s'. "
					 "This test must run in an editor context with Manny content mounted. "
					 "Constraint frame baseline diagnostic is inconclusive."),
				*PhysicsAssetPath));

			// Still report the threshold geometry for reference
			AddInfo(FString::Printf(
				TEXT("THRESHOLDS: thigh_max=%.1f° thigh_readiness=%.1f° (margin=%.1f°)  spine_max=%.1f° spine_readiness=%.1f° (margin=%.1f°)"),
				ThighMaxDeg, ThighReadinessDeg, ThighMaxDeg - ThighReadinessDeg,
				SpineMaxDeg, SpineReadinessDeg, SpineMaxDeg - SpineReadinessDeg));
			return true;
		}

		// For each link, extract the constraint reference frames and compute
		// the intrinsic angular offset when both bodies are at identity orientation
		for (FLinkDiagnostic& Link : Links)
		{
			const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(Link.ChildBone, Link.ParentBone);
			if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
			{
				AddWarning(FString::Printf(
					TEXT("DIAGNOSTIC: No constraint found for %s→%s in PhysicsAsset. "
						 "Cannot compute baseline angular offset."),
					*Link.ParentBone.ToString(), *Link.ChildBone.ToString()));
				continue;
			}

			const UPhysicsConstraintTemplate* ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
			if (!ConstraintTemplate)
			{
				continue;
			}

			const FConstraintInstance& Constraint = ConstraintTemplate->DefaultInstance;
			Link.bConstraintFound = true;
			Link.ParentAnchorLocalCm = Constraint.Pos2;
			Link.ChildAnchorLocalCm = Constraint.Pos1;
			Link.ParentRefFrameQuat = Constraint.GetRefFrame(EConstraintFrame::Frame2).GetRotation();
			Link.ChildRefFrameQuat = Constraint.GetRefFrame(EConstraintFrame::Frame1).GetRotation();

			// The constraint angular error as measured by BuildDirectPelvisLinkForensicRecord is:
			//   (ChildBodyWorld * ChildRefFrame).AngularDistance(ParentBodyWorld * ParentRefFrame)
			//
			// At identity body orientations (reference / bind pose), this reduces to:
			//   ChildRefFrame.AngularDistance(ParentRefFrame)
			//
			// This is the permanent structural floor that no pelvis rotation can eliminate,
			// because rotating the pelvis rotates BOTH constraint frames together.
			//
			// CORRECTION: rotating the pelvis changes ParentBodyWorld but NOT ChildBodyWorld.
			// So the solver CAN change the error. But the reference-pose baseline tells us
			// how far apart the frames start and whether the solver needs to cover the gap or
			// just fine-tune from a mostly-aligned starting point.
			Link.ConstraintFrameAngularMismatchDeg = FMath::RadiansToDegrees(
				Link.ChildRefFrameQuat.AngularDistance(Link.ParentRefFrameQuat));
		}

		// Emit diagnostic report
		AddInfo(TEXT("============================================================"));
		AddInfo(TEXT("PHASE 1 CONSTRAINT FRAME BASELINE ANGULAR DIAGNOSTIC"));
		AddInfo(TEXT("============================================================"));
		AddInfo(FString::Printf(
			TEXT("PhysicsAsset: %s"), *PhysicsAsset->GetPathName()));

		for (const FLinkDiagnostic& Link : Links)
		{
			if (!Link.bConstraintFound)
			{
				continue;
			}

			const float RemainingBudget = Link.ReadinessThresholdDeg - Link.ConstraintFrameAngularMismatchDeg;
			const TCHAR* Verdict =
				Link.ConstraintFrameAngularMismatchDeg <= 1.0f ? TEXT("ALIGNED (rotational problem)")
				: Link.ConstraintFrameAngularMismatchDeg >= Link.ReadinessThresholdDeg ? TEXT("STRUCTURAL BLOCKER (exceeds readiness threshold at identity)")
				: TEXT("SIGNIFICANT OFFSET (solver must compensate)");

			AddInfo(FString::Printf(
				TEXT("LINK: %s -> %s"),
				*Link.ParentBone.ToString(), *Link.ChildBone.ToString()));
			AddInfo(FString::Printf(
				TEXT("  Constraint ref-frame angular mismatch at identity: %.2f°"),
				Link.ConstraintFrameAngularMismatchDeg));
			AddInfo(FString::Printf(
				TEXT("  Readiness threshold: %.1f°   Max threshold: %.1f°"),
				Link.ReadinessThresholdDeg, Link.MaxThresholdDeg));
			AddInfo(FString::Printf(
				TEXT("  Remaining rotational budget for solver: %.2f°"),
				RemainingBudget));
			AddInfo(FString::Printf(
				TEXT("  Parent ref-frame quat: (%.4f, %.4f, %.4f, %.4f)"),
				Link.ParentRefFrameQuat.X, Link.ParentRefFrameQuat.Y,
				Link.ParentRefFrameQuat.Z, Link.ParentRefFrameQuat.W));
			AddInfo(FString::Printf(
				TEXT("  Child ref-frame quat:  (%.4f, %.4f, %.4f, %.4f)"),
				Link.ChildRefFrameQuat.X, Link.ChildRefFrameQuat.Y,
				Link.ChildRefFrameQuat.Z, Link.ChildRefFrameQuat.W));
			AddInfo(FString::Printf(
				TEXT("  Parent anchor local: (%.2f, %.2f, %.2f) cm"),
				Link.ParentAnchorLocalCm.X, Link.ParentAnchorLocalCm.Y, Link.ParentAnchorLocalCm.Z));
			AddInfo(FString::Printf(
				TEXT("  Child anchor local:  (%.2f, %.2f, %.2f) cm"),
				Link.ChildAnchorLocalCm.X, Link.ChildAnchorLocalCm.Y, Link.ChildAnchorLocalCm.Z));
			AddInfo(FString::Printf(
				TEXT("  VERDICT: %s"), Verdict));
			AddInfo(TEXT("------------------------------------------------------------"));
		}

		AddInfo(TEXT("============================================================"));
		AddInfo(TEXT("INTERPRETATION GUIDE:"));
		AddInfo(TEXT("  ALIGNED (<= 1°):"));
		AddInfo(TEXT("    The constraint frames match at identity. The ~33° runtime"));
		AddInfo(TEXT("    error is purely from the simulated body orientations."));
		AddInfo(TEXT("    The pelvis rotation solver CAN theoretically close the gap."));
		AddInfo(TEXT("  SIGNIFICANT OFFSET (1° - threshold°):"));
		AddInfo(TEXT("    The constraint frames start misaligned. The solver must"));
		AddInfo(TEXT("    first cover this structural offset before any useful work."));
		AddInfo(TEXT("    Consider adjusting constraint frame authoring."));
		AddInfo(TEXT("  STRUCTURAL BLOCKER (>= threshold°):"));
		AddInfo(TEXT("    The constraint frames alone exceed the readiness threshold."));
		AddInfo(TEXT("    No pelvis rotation can satisfy this gate. Fix the constraint"));
		AddInfo(TEXT("    frame setup in the PhysicsAsset."));
		AddInfo(TEXT("============================================================"));

		// Verify we found all three constraints
		int32 FoundCount = 0;
		for (const FLinkDiagnostic& Link : Links)
		{
			if (Link.bConstraintFound)
			{
				FoundCount++;
			}
		}
		TestEqual(TEXT("All three Phase 1 critical constraints found"), FoundCount, 3);

		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPhysAnimPhase1ZeroSolverForensicDumpTest, "PhysAnim.Component.Phase1ZeroSolverForensicDump", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimPhase1ZeroSolverForensicDumpTest::RunTest(const FString& Parameters)
{
	// We use the long-form paths that match the PhysAnim component's expectations
	const FString MeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	USkeletalMesh* SkelMesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);

	if (!SkelMesh)
	{
		AddInfo(TEXT("DUMP: Could not load SkeletalMesh from '/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple'. This test must run in an editor context with Manny content mounted."));
		return true;
	}

	AActor* Actor = GEditor->GetEditorWorldContext().World()->SpawnActor<AActor>();
	USkeletalMeshComponent* MeshComp = NewObject<USkeletalMeshComponent>(Actor);
	MeshComp->SetSkeletalMesh(SkelMesh);
	MeshComp->RegisterComponent();

	TArray<FName> Bones = { TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01") };
	FName RootBone = TEXT("pelvis");

	const auto RunDumpForPose = [&](const TCHAR* PoseName)
	{
		AddInfo(TEXT("============================================================"));
		AddInfo(FString::Printf(TEXT("ZERO-SOLVER FORENSIC DUMP: %s"), PoseName));
		AddInfo(TEXT("============================================================"));

		TArray<BalanceTransitionSets::FDirectPelvisLinkForensicRecord> Records;
		for (FName Bone : Bones)
		{
			BalanceTransitionSets::FDirectPelvisLinkForensicRecord Record;
			if (BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(MeshComp, RootBone, Bone, Record))
			{
				Records.Add(Record);
			}
		}

		BalanceTransitionSets::LogDirectPelvisLinkForensicRecords(Records, PoseName, true);

		for (const auto& Record : Records)
		{
			const FQuat ChildConstraintWorldRotation = (Record.ChildWorldRotation * Record.AuthoredChildRefFrame).GetNormalized();
			const FQuat ParentConstraintWorldRotation = (Record.ParentWorldRotation * Record.AuthoredParentRefFrame).GetNormalized();
			const float ErrorDeg = FMath::RadiansToDegrees(ChildConstraintWorldRotation.AngularDistance(ParentConstraintWorldRotation));

			AddInfo(FString::Printf(TEXT("LINK: %s"), *Record.LinkName));
			AddInfo(FString::Printf(TEXT("  AngularError: %.3f deg"), ErrorDeg));
			AddInfo(FString::Printf(TEXT("  ParentWorldQuat: %s"), *Record.ParentWorldRotation.ToString()));
			AddInfo(FString::Printf(TEXT("  ChildWorldQuat:  %s"), *Record.ChildWorldRotation.ToString()));
			AddInfo(FString::Printf(TEXT("  ParentRefFrame:  %s"), *Record.AuthoredParentRefFrame.ToString()));
			AddInfo(FString::Printf(TEXT("  ChildRefFrame:   %s"), *Record.AuthoredChildRefFrame.ToString()));
		}
	};

	// 1. REF POSE
	MeshComp->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MeshComp->SetAnimation(nullptr);
	MeshComp->RefreshBoneTransforms();
	RunDumpForPose(TEXT("ImportedRefPose"));

	// 2. T-POSE (if available)
	// For Manny_Simple, the RefPose is usually a T-Pose. 
	// If the project has a specific T-Pose asset, we could load it here.
	// For now, RefPose is the primary baseline.

	Actor->Destroy();
	return true;
}

#endif
