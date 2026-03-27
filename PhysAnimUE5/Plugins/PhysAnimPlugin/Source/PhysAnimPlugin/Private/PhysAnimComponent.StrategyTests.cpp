#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "PhysAnimComparisonSubsystem.h"
#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimBalance.TestHelpers.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimBridge;
	using namespace PhysAnimBalanceTestHelpers;

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
				35.5f,
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
				33.1f,
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
				34.54f,
				17.59f));
		TestTrue(
			TEXT("Spine-safe worst-thigh focused delta stays enabled for the current live thigh near-miss"),
			UPhysAnimComponent::TestOnlyShouldRunSpineSafeWorstThighFocusedDelta(
				31.49f,
				33.89f,
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
				33.30f,
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
				34.49f,
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
		FPhysAnimPhase1AutoCalibStageACandidatesTest,
		"PhysAnim.Component.Phase1AutoCalibStageACandidates",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPhase1AutoCalibStageACandidatesTest::RunTest(const FString& Parameters)
	{
		TArray<FPhase1AutoCalibParams> Candidates;
		UPhysAnimPhase1AutoCalibSubsystem::BuildStageACandidates(1337, INDEX_NONE, Candidates);

		TestEqual(TEXT("Stage A emits the fixed 24 samples for each of 6 presets"), Candidates.Num(), 24 * 6);

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

		TestEqual(TEXT("Stage A covers all six fixed presets"), SeenPresets.Num(), 6);
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
}

#endif
