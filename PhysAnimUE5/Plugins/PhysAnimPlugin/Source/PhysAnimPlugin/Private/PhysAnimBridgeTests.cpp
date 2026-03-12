#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBridge.h"
#include "PhysAnimComparisonSubsystem.h"
#include "PhysAnimComponent.h"
#include "PhysAnimStage1InitializerComponent.h"

#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsControlActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Tests/AutomationCommon.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealEdGlobals.h"
#endif

namespace
{
	using namespace PhysAnimBridge;

#if WITH_EDITOR
	const FString PhysAnimPieSmokeMap = TEXT("/Game/ThirdPerson/Lvl_ThirdPerson");
	const TCHAR* PhysAnimPieSmokePrefix = TEXT("[PhysAnimPieSmoke]");
	const TCHAR* PhysAnimPieMovementSmokePrefix = TEXT("[PhysAnimPieMovementSmoke]");
	const TCHAR* PhysAnimPieMovementSoakPrefix = TEXT("[PhysAnimPieMovementSoak]");
	const TCHAR* PhysAnimPieG2PresentationPrefix = TEXT("[PhysAnimPieG2Presentation]");
	constexpr float PhysAnimPieSmokeDurationSeconds = 65.0f;
	constexpr float PhysAnimPieMovementSmokeDurationSeconds = 50.0f;
	constexpr int32 PhysAnimPieMovementSoakLoopCount = 3;
	constexpr float PhysAnimPieMovementSoakDurationSeconds = 115.0f;
	constexpr float PhysAnimPieG2PresentationLeadInSeconds = 1.0f;
	constexpr float PhysAnimPieG2PresentationDurationSeconds = 35.0f;
	constexpr float PhysAnimPieSmokeStartTimeoutSeconds = 30.0f;
	constexpr float PhysAnimPieSmokeStopTimeoutSeconds = 30.0f;

	DEFINE_LATENT_AUTOMATION_COMMAND_TWO_PARAMETER(FSetIntConsoleVariableCommand, FString, Name, int32, Value);
	bool FSetIntConsoleVariableCommand::Update()
	{
		if (IConsoleVariable* const ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*Name))
		{
			ConsoleVariable->Set(Value);
		}
		return true;
	}

	DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FExecPieConsoleCommand, FString, Command);
	bool FExecPieConsoleCommand::Update()
	{
		if (GEditor && IsValid(GEditor->PlayWorld))
		{
			GEditor->Exec(GEditor->PlayWorld, *Command);
		}
		return true;
	}

#endif

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFutureScheduleTest,
		"PhysAnim.Bridge.FutureSchedule",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFutureScheduleTest::RunTest(const FString& Parameters)
	{
		const TArray<float> Schedule = BuildFutureSampleTimeSchedule();
		TestEqual(TEXT("Future step count"), Schedule.Num(), NumFutureSteps);
		TestEqual(TEXT("First future step"), Schedule[0], FutureStepSeconds);
		TestEqual(TEXT("Last future step"), Schedule.Last(), NumFutureSteps * FutureStepSeconds);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTensorIndexMapTest,
		"PhysAnim.Bridge.TensorIndexMap",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTensorIndexMapTest::RunTest(const FString& Parameters)
	{
		TArray<UE::NNE::FTensorDesc> TensorDescs;
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("self_obs"), UE::NNE::FSymbolicTensorShape::Make({1, SelfObsSize}), ENNETensorDataType::Float));
		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("mimic_target_poses"), UE::NNE::FSymbolicTensorShape::Make({1, MimicTargetPosesSize}), ENNETensorDataType::Float));

		FPhysAnimTensorIndexMap IndexMap;
		FString Error;
		TestTrue(TEXT("Tensor map should succeed"), BuildInputTensorIndexMap(TensorDescs, IndexMap, Error));
		TestEqual(TEXT("self_obs index"), IndexMap.SelfObs, 1);
		TestEqual(TEXT("mimic_target_poses index"), IndexMap.MimicTargetPoses, 2);
		TestEqual(TEXT("terrain index"), IndexMap.Terrain, 0);

		TensorDescs.Add(UE::NNE::FTensorDesc::Make(TEXT("terrain"), UE::NNE::FSymbolicTensorShape::Make({1, TerrainSize}), ENNETensorDataType::Float));
		TestFalse(TEXT("Duplicate tensor map should fail"), BuildInputTensorIndexMap(TensorDescs, IndexMap, Error));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFrameConversionTest,
		"PhysAnim.Bridge.FrameConversion",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFrameConversionTest::RunTest(const FString& Parameters)
	{
		const FVector SmplUp(0.0, 1.0, 0.0);
		const FVector UeUp = SmplVectorToUe(SmplUp);
		TestTrue(TEXT("SMPL Y-up becomes UE Z-up"), UeUp.Equals(FVector(0.0, 0.0, 1.0), KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity SMPL local rotation stays identity after UE basis conversion"),
			SmplQuaternionToUe(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity UE local rotation stays identity after SMPL basis conversion"),
			UeQuaternionToSmpl(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));

		const FQuat SmplRotation = ExpMapToQuaternion(FVector(0.2, -0.1, 0.3));
		const FQuat RoundTrip = UeQuaternionToSmpl(SmplQuaternionToUe(SmplRotation));
		TestTrue(TEXT("Quaternion roundtrip should stay close"), RoundTrip.Equals(SmplRotation, 1.0e-3f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimObservationPackingTest,
		"PhysAnim.Bridge.ObservationPacking",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimObservationPackingTest::RunTest(const FString& Parameters)
	{
		TArray<FPhysAnimBodySample> BodySamples;
		BodySamples.SetNum(NumSmplBodies);
		for (int32 Index = 0; Index < NumSmplBodies; ++Index)
		{
			BodySamples[Index].Position = FVector(Index * 1.0, Index * 2.0, Index * 3.0);
			BodySamples[Index].Rotation = FQuat::Identity;
			BodySamples[Index].LinearVelocity = FVector::ZeroVector;
			BodySamples[Index].AngularVelocity = FVector::ZeroVector;
		}

		TArray<float> SelfObs;
		FString Error;
		TestTrue(TEXT("self_obs build should succeed"), BuildSelfObservation(BodySamples, 0.0f, SelfObs, Error));
		TestEqual(TEXT("self_obs size"), SelfObs.Num(), SelfObsSize);

		TArray<FPhysAnimFuturePoseSample> FutureSamples;
		FutureSamples.Reserve(NumFutureSteps);
		const TArray<float> Schedule = BuildFutureSampleTimeSchedule();
		for (int32 FutureIndex = 0; FutureIndex < NumFutureSteps; ++FutureIndex)
		{
			FPhysAnimFuturePoseSample FutureSample;
			FutureSample.FutureTimeSeconds = Schedule[FutureIndex];
			FutureSample.BodyTransforms.Reserve(NumSmplBodies);
			for (int32 BodyIndex = 0; BodyIndex < NumSmplBodies; ++BodyIndex)
			{
				FutureSample.BodyTransforms.Add(FTransform(FQuat::Identity, FVector(BodyIndex + FutureIndex, BodyIndex, 0.0)));
			}
			FutureSamples.Add(MoveTemp(FutureSample));
		}

		TArray<float> MimicObs;
		TestTrue(TEXT("mimic_target_poses build should succeed"), BuildMimicTargetPoses(BodySamples, FutureSamples, MimicObs, Error));
		TestEqual(TEXT("mimic_target_poses size"), MimicObs.Num(), MimicTargetPosesSize);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimActionCollapseTest,
		"PhysAnim.Bridge.ActionCollapse",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimActionCollapseTest::RunTest(const FString& Parameters)
	{
		TArray<float> Actions;
		Actions.Init(0.0f, NumActionFloats);
		Actions[16 * 3] = 0.5f;
		Actions[17 * 3] = 0.5f;

		TMap<FName, FQuat> ControlRotations;
		FString Error;
		TestTrue(TEXT("Action conversion should succeed"), ConvertModelActionsToControlRotations(Actions, ControlRotations, Error));
		TestEqual(TEXT("Control target count"), ControlRotations.Num(), NumControlledBones);
		TestTrue(TEXT("Collapsed hand target exists"), ControlRotations.Contains(TEXT("hand_l")));
		TestFalse(TEXT("There is no standalone wrist control"), ControlRotations.Contains(TEXT("wrist_l")));
		TestTrue(TEXT("Collapsed hand target is non-identity"), !ControlRotations[TEXT("hand_l")].Equals(FQuat::Identity, 1.0e-3f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimZeroActionConversionTest,
		"PhysAnim.Bridge.ZeroActionConversion",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimZeroActionConversionTest::RunTest(const FString& Parameters)
	{
		TArray<float> Actions;
		Actions.Init(0.0f, NumActionFloats);

		TMap<FName, FQuat> ControlRotations;
		FString Error;
		TestTrue(TEXT("Zero-action conversion should succeed"), ConvertModelActionsToControlRotations(Actions, ControlRotations, Error));
		TestEqual(TEXT("Zero-action conversion writes the full control set"), ControlRotations.Num(), NumControlledBones);

		for (const TPair<FName, FQuat>& Pair : ControlRotations)
		{
			TestTrue(
				FString::Printf(TEXT("Zero action keeps %s at identity"), *Pair.Key.ToString()),
				Pair.Value.Equals(FQuat::Identity, 1.0e-3f));
		}

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
	TestTrue(
		TEXT("Policy control interval resolves from the configured rate"),
		FMath::IsNearlyEqual(
			UPhysAnimComponent::ResolvePolicyControlIntervalSeconds(Settings.PolicyControlRateHz),
			1.0f / 30.0f,
			KINDA_SMALL_NUMBER));
	float PolicyAccumulatorSeconds = -1.0f;
	int32 ElapsedPolicySteps = 0;
	TestTrue(
		TEXT("Policy cadence primes the first update immediately after activation"),
		UPhysAnimComponent::AdvancePolicyControlAccumulator(
			1.0f / 60.0f,
			UPhysAnimComponent::ResolvePolicyControlIntervalSeconds(Settings.PolicyControlRateHz),
			PolicyAccumulatorSeconds,
			ElapsedPolicySteps));
	TestEqual(TEXT("Primed cadence emits one policy step"), ElapsedPolicySteps, 1);
	TestTrue(
		TEXT("Short render gaps can hold the previous policy target"),
		!UPhysAnimComponent::AdvancePolicyControlAccumulator(
			0.010f,
			UPhysAnimComponent::ResolvePolicyControlIntervalSeconds(Settings.PolicyControlRateHz),
			PolicyAccumulatorSeconds,
			ElapsedPolicySteps));
	TestEqual(TEXT("Held cadence reports zero elapsed policy steps"), ElapsedPolicySteps, 0);
	TestTrue(
		TEXT("Longer frame gaps coalesce multiple policy intervals"),
		UPhysAnimComponent::AdvancePolicyControlAccumulator(
			0.080f,
			UPhysAnimComponent::ResolvePolicyControlIntervalSeconds(Settings.PolicyControlRateHz),
			PolicyAccumulatorSeconds,
			ElapsedPolicySteps));
	TestTrue(TEXT("Coalesced cadence reports more than one elapsed policy step"), ElapsedPolicySteps > 1);
	UPhysAnimComponent::ApplyPresentationPerturbationStabilizationOverride(false, OverrideSettings);
	TestEqual(
		TEXT("Inactive presentation perturbation override leaves angular strength unchanged"),
		OverrideSettings.AngularStrengthMultiplier,
		Settings.AngularStrengthMultiplier);
	UPhysAnimComponent::ApplyPresentationPerturbationStabilizationOverride(true, OverrideSettings);
	TestTrue(
		TEXT("Presentation perturbation override relaxes angular strength"),
		OverrideSettings.AngularStrengthMultiplier < Settings.AngularStrengthMultiplier);
	TestTrue(
		TEXT("Presentation perturbation override relaxes angular damping ratio"),
		OverrideSettings.AngularDampingRatioMultiplier < Settings.AngularDampingRatioMultiplier);
	TestTrue(
		TEXT("Presentation perturbation override relaxes extra damping"),
		OverrideSettings.AngularExtraDampingMultiplier < Settings.AngularExtraDampingMultiplier);
	TestEqual(
		TEXT("Stress-test multiplier starts fully enabled"),
		UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(0, 0.0f, 45.0f, 0.0f, 0.0f, 0.0f),
		1.0f);
	TestEqual(
		TEXT("Stress-test multiplier halves at the midpoint"),
		UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(0, 22.5f, 45.0f, 0.0f, 0.0f, 0.0f),
		0.5f);
	TestEqual(
		TEXT("Stress-test multiplier clamps to zero after the ramp"),
		UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(0, 60.0f, 45.0f, 0.0f, 0.0f, 0.0f),
		0.0f);
	TestEqual(
		TEXT("Recovery profile reaches the target floor at the end of the down ramp"),
		UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(1, 10.0f, 10.0f, 0.4f, 3.0f, 5.0f),
		0.4f);
	TestEqual(
		TEXT("Recovery profile holds the floor during the hold window"),
		UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(1, 12.0f, 10.0f, 0.4f, 3.0f, 5.0f),
		0.4f);
	TestEqual(
		TEXT("Recovery profile ramps back to full strength"),
		UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(1, 18.0f, 10.0f, 0.4f, 3.0f, 5.0f),
		1.0f);
	FPhysAnimStabilizationSettings StressRampSettings = Settings;
	UPhysAnimComponent::ApplyStabilizationStressTestRamp(0.5f, 0, StressRampSettings);
	TestEqual(
		TEXT("Stress-test ramp scales angular strength uniformly"),
		StressRampSettings.AngularStrengthMultiplier,
		Settings.AngularStrengthMultiplier * 0.5f);
	TestEqual(
		TEXT("Stress-test ramp scales damping ratio uniformly"),
		StressRampSettings.AngularDampingRatioMultiplier,
		Settings.AngularDampingRatioMultiplier * 0.5f);
	TestEqual(
		TEXT("Stress-test ramp scales extra damping uniformly"),
		StressRampSettings.AngularExtraDampingMultiplier,
		Settings.AngularExtraDampingMultiplier * 0.5f);
	FPhysAnimStabilizationSettings StrengthOnlySettings = Settings;
	UPhysAnimComponent::ApplyStabilizationStressTestRamp(0.5f, 1, StrengthOnlySettings);
	TestEqual(TEXT("Strength-only sweep leaves damping ratio unchanged"), StrengthOnlySettings.AngularDampingRatioMultiplier, Settings.AngularDampingRatioMultiplier);
	TestEqual(TEXT("Strength-only sweep leaves extra damping unchanged"), StrengthOnlySettings.AngularExtraDampingMultiplier, Settings.AngularExtraDampingMultiplier);
	return true;
}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRotationLimitTest,
		"PhysAnim.Component.RotationLimit",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRotationLimitTest::RunTest(const FString& Parameters)
	{
		const FQuat PreviousRotation = FQuat::Identity;
		const FQuat TargetRotation(FVector::UpVector, FMath::DegreesToRadians(180.0));
		const FQuat LimitedRotation =
			UPhysAnimComponent::LimitTargetRotationStep(PreviousRotation, TargetRotation, 90.0f);
		const double LimitedAngleDegrees = FMath::RadiansToDegrees(PreviousRotation.AngularDistance(LimitedRotation));

		TestTrue(TEXT("Rotation limiting should cap the step"), FMath::IsNearlyEqual(LimitedAngleDegrees, 90.0, 0.5));
		TestTrue(
			TEXT("Zero limit disables limiting"),
			UPhysAnimComponent::LimitTargetRotationStep(PreviousRotation, TargetRotation, 0.0f).Equals(TargetRotation, 1.0e-4f));
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
		TestTrue(TEXT("Reference root location is captured"), State.bHasReferenceRootLocation);

		TestTrue(
			TEXT("Short threshold breach stays inside grace window"),
			UPhysAnimComponent::EvaluateRuntimeInstability(
				FVector(0.0f, 0.0f, 180.0f),
				FVector::ZeroVector,
				FVector::ZeroVector,
				0.10f,
				Settings,
				State,
				Diagnostics,
				Error));
		TestTrue(TEXT("Height threshold is detected"), Diagnostics.bHeightExceeded);
		TestTrue(TEXT("Unstable time accumulates"), FMath::IsNearlyEqual(Diagnostics.UnstableAccumulatedSeconds, 0.10f));

		TestFalse(
			TEXT("Threshold breach fails after grace window"),
			UPhysAnimComponent::EvaluateRuntimeInstability(
				FVector(0.0f, 0.0f, 180.0f),
				FVector::ZeroVector,
				FVector::ZeroVector,
				0.11f,
				Settings,
				State,
				Diagnostics,
				Error));
		TestTrue(TEXT("Fail-stop reason mentions runtime instability"), Error.Contains(TEXT("Runtime instability detected")));

		FVector EffectiveRootLocation;
		FVector EffectiveRootVelocity;
		UPhysAnimComponent::ResolveRuntimeInstabilityRootFrame(
			true,
			FVector(500.0f, 0.0f, 260.0f),
			FVector(600.0f, 0.0f, 0.0f),
			FVector(500.0f, 0.0f, 160.0f),
			FVector(600.0f, 0.0f, 0.0f),
			EffectiveRootLocation,
			EffectiveRootVelocity);
		TestEqual(
			TEXT("Gameplay-shell-relative instability frame removes owner translation"),
			EffectiveRootLocation,
			FVector(0.0f, 0.0f, 100.0f));
		TestEqual(
			TEXT("Gameplay-shell-relative instability frame removes owner linear velocity"),
			EffectiveRootVelocity,
			FVector::ZeroVector);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeInstabilityRecoveryTest,
		"PhysAnim.Component.RuntimeInstabilityRecovery",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeInstabilityRecoveryTest::RunTest(const FString& Parameters)
	{
		FPhysAnimRuntimeInstabilitySettings Settings;
		Settings.MaxRootHeightDeltaCm = 1000.0f;
		Settings.MaxRootLinearSpeedCmPerSecond = 100.0f;
		Settings.MaxRootAngularSpeedDegPerSecond = 1000.0f;
		Settings.UnstableGracePeriodSeconds = 0.5f;

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

		TestTrue(
			TEXT("Short speed breach starts instability accumulation"),
			UPhysAnimComponent::EvaluateRuntimeInstability(
				FVector(0.0f, 0.0f, 100.0f),
				FVector(150.0f, 0.0f, 0.0f),
				FVector::ZeroVector,
				0.2f,
				Settings,
				State,
				Diagnostics,
				Error));
		TestTrue(TEXT("Instability accumulated before recovery"), FMath::IsNearlyEqual(Diagnostics.UnstableAccumulatedSeconds, 0.2f));

		TestTrue(
			TEXT("Stable frame resets instability accumulation"),
			UPhysAnimComponent::EvaluateRuntimeInstability(
				FVector(0.0f, 0.0f, 100.0f),
				FVector::ZeroVector,
				FVector::ZeroVector,
				0.016f,
				Settings,
				State,
				Diagnostics,
				Error));
		TestTrue(TEXT("Instability accumulated resets to zero"), FMath::IsNearlyZero(Diagnostics.UnstableAccumulatedSeconds));

		TestTrue(
			TEXT("Fresh short breach after recovery does not fail"),
			UPhysAnimComponent::EvaluateRuntimeInstability(
				FVector(0.0f, 0.0f, 100.0f),
				FVector(150.0f, 0.0f, 0.0f),
				FVector::ZeroVector,
				0.2f,
				Settings,
				State,
				Diagnostics,
				Error));
		TestTrue(TEXT("Second accumulation starts from zero"), FMath::IsNearlyEqual(Diagnostics.UnstableAccumulatedSeconds, 0.2f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPerBodyInstabilityDiagnosticsTest,
		"PhysAnim.Component.PerBodyInstabilityDiagnostics.Basic",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPerBodyInstabilityDiagnosticsTest::RunTest(const FString& Parameters)
	{
		TArray<FPhysAnimBodyInstabilitySample> Samples;

		FPhysAnimBodyInstabilitySample& PelvisSample = Samples.AddDefaulted_GetRef();
		PelvisSample.BoneName = TEXT("pelvis");
		PelvisSample.Location = FVector(0.0f, 0.0f, 100.0f);
		PelvisSample.LinearVelocity = FVector::ZeroVector;
		PelvisSample.AngularVelocity = FVector::ZeroVector;
		PelvisSample.bIsSimulatingPhysics = false;

		FPhysAnimBodyInstabilitySample& HandSample = Samples.AddDefaulted_GetRef();
		HandSample.BoneName = TEXT("hand_l");
		HandSample.Location = FVector(0.0f, 0.0f, 180.0f);
		HandSample.LinearVelocity = FVector(10.0f, 0.0f, 0.0f);
		HandSample.AngularVelocity = FVector(900.0f, 0.0f, 0.0f);
		HandSample.bIsSimulatingPhysics = true;

		FPhysAnimBodyInstabilitySample& FootSample = Samples.AddDefaulted_GetRef();
		FootSample.BoneName = TEXT("foot_r");
		FootSample.Location = FVector(0.0f, 0.0f, 95.0f);
		FootSample.LinearVelocity = FVector(400.0f, 0.0f, 0.0f);
		FootSample.AngularVelocity = FVector(200.0f, 0.0f, 0.0f);
		FootSample.bIsSimulatingPhysics = true;

		FPhysAnimRuntimeInstabilityDiagnostics Diagnostics;
		PhysAnimBridge::EvaluatePerBodyInstabilitySamples(Samples, FVector(0.0f, 0.0f, 100.0f), Diagnostics);

		TestEqual(TEXT("Per-body diagnostics count all samples"), Diagnostics.NumBodiesConsidered, 3);
		TestEqual(TEXT("Per-body diagnostics count simulating samples"), Diagnostics.NumSimulatingBodies, 2);
		TestEqual(TEXT("Max linear speed bone is captured"), Diagnostics.MaxLinearSpeedBoneName, FName(TEXT("foot_r")));
		TestEqual(TEXT("Max linear speed magnitude is captured"), Diagnostics.MaxBodyLinearSpeedCmPerSecond, 400.0f);
		TestTrue(TEXT("Max linear speed bone sim state is captured"), Diagnostics.bMaxLinearSpeedBoneSimulatingPhysics);
		TestEqual(TEXT("Max angular speed bone is captured"), Diagnostics.MaxAngularSpeedBoneName, FName(TEXT("hand_l")));
		TestEqual(TEXT("Max angular speed magnitude is captured"), Diagnostics.MaxBodyAngularSpeedDegPerSecond, 900.0f);
		TestTrue(TEXT("Max angular speed bone sim state is captured"), Diagnostics.bMaxAngularSpeedBoneSimulatingPhysics);
		TestEqual(TEXT("Max body height delta bone is captured"), Diagnostics.MaxHeightDeltaBoneName, FName(TEXT("hand_l")));
		TestEqual(TEXT("Max body height delta magnitude is captured"), Diagnostics.MaxBodyHeightDeltaCm, 80.0f);
		TestTrue(TEXT("Max body height delta sim state is captured"), Diagnostics.bMaxHeightDeltaBoneSimulatingPhysics);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPerBodyInstabilityDiagnosticsEmptyTest,
		"PhysAnim.Component.PerBodyInstabilityDiagnostics.Empty",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPerBodyInstabilityDiagnosticsEmptyTest::RunTest(const FString& Parameters)
	{
		FPhysAnimRuntimeInstabilityDiagnostics Diagnostics;
		Diagnostics.MaxLinearSpeedBoneName = TEXT("stale");
		Diagnostics.MaxAngularSpeedBoneName = TEXT("stale");
		Diagnostics.MaxHeightDeltaBoneName = TEXT("stale");
		const TArray<FPhysAnimBodyInstabilitySample> Samples;

		PhysAnimBridge::EvaluatePerBodyInstabilitySamples(Samples, FVector(0.0f, 0.0f, 100.0f), Diagnostics);

		TestEqual(TEXT("Empty diagnostics reset body count"), Diagnostics.NumBodiesConsidered, 0);
		TestEqual(TEXT("Empty diagnostics reset simulating count"), Diagnostics.NumSimulatingBodies, 0);
		TestEqual(TEXT("Empty diagnostics clear max linear speed bone"), Diagnostics.MaxLinearSpeedBoneName, NAME_None);
		TestEqual(TEXT("Empty diagnostics clear max angular speed bone"), Diagnostics.MaxAngularSpeedBoneName, NAME_None);
		TestEqual(TEXT("Empty diagnostics clear max height delta bone"), Diagnostics.MaxHeightDeltaBoneName, NAME_None);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimInitialPoseSearchTimeoutTest,
		"PhysAnim.Component.InitialPoseSearchTimeout",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimInitialPoseSearchTimeoutTest::RunTest(const FString& Parameters)
	{
		TestFalse(TEXT("Negative elapsed time does not time out"), UPhysAnimComponent::IsInitialPoseSearchWaitTimedOut(-0.1, 2.0));
		TestFalse(TEXT("Elapsed time below timeout does not time out"), UPhysAnimComponent::IsInitialPoseSearchWaitTimedOut(1.99, 2.0));
		TestTrue(TEXT("Elapsed time at timeout does time out"), UPhysAnimComponent::IsInitialPoseSearchWaitTimedOut(2.0, 2.0));
		TestFalse(TEXT("Non-positive timeout disables timeout"), UPhysAnimComponent::IsInitialPoseSearchWaitTimedOut(10.0, 0.0));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeStateOwnershipTest,
		"PhysAnim.Component.RuntimeStateOwnership",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeStateOwnershipTest::RunTest(const FString& Parameters)
	{
		TestFalse(TEXT("Uninitialized does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::Uninitialized));
		TestFalse(TEXT("RuntimeReady does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::RuntimeReady));
		TestFalse(TEXT("WaitingForPoseSearch does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::WaitingForPoseSearch));
		TestFalse(TEXT("ReadyForActivation does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::ReadyForActivation));
		TestTrue(TEXT("BridgeActive owns bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::BridgeActive));
		TestFalse(TEXT("FailStopped does not own bridge physics"), UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState::FailStopped));
		TestTrue(
			TEXT("BridgeActive status text reports active"),
			UPhysAnimComponent::BuildBridgeStatusIndicatorText(EPhysAnimRuntimeState::BridgeActive, true).Contains(TEXT("ACTIVE")));
		TestTrue(
			TEXT("ReadyForActivation status text reports inactive"),
			UPhysAnimComponent::BuildBridgeStatusIndicatorText(EPhysAnimRuntimeState::ReadyForActivation, false).Contains(TEXT("INACTIVE")));
		TestEqual(
			TEXT("BridgeActive indicator is green"),
			UPhysAnimComponent::ResolveBridgeStatusIndicatorColor(EPhysAnimRuntimeState::BridgeActive, true),
			FColor::Green);
		TestEqual(
			TEXT("FailStopped indicator is red"),
			UPhysAnimComponent::ResolveBridgeStatusIndicatorColor(EPhysAnimRuntimeState::FailStopped, false),
			FColor::Red);
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
		TestEqual(
			TEXT("Action-enabled startup resolves to live bridge activation"),
			UPhysAnimComponent::ResolveInitialPoseSearchSuccessState(false),
			EPhysAnimRuntimeState::BridgeActive);
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
		TestFalse(
			TEXT("ReadyForActivation does not activate bridge physics while zero-action mode remains enabled"),
			UPhysAnimComponent::ShouldActivateBridgeFromSafeMode(EPhysAnimRuntimeState::ReadyForActivation, true));
		TestFalse(
			TEXT("WaitingForPoseSearch does not activate bridge physics directly"),
			UPhysAnimComponent::ShouldActivateBridgeFromSafeMode(EPhysAnimRuntimeState::WaitingForPoseSearch, false));

		TestTrue(
			TEXT("BridgeActive returns to deferred activation when zero-action mode is enabled"),
			UPhysAnimComponent::ShouldDeactivateBridgeToSafeMode(EPhysAnimRuntimeState::BridgeActive, true));
		TestFalse(
			TEXT("BridgeActive stays active while zero-action mode remains disabled"),
			UPhysAnimComponent::ShouldDeactivateBridgeToSafeMode(EPhysAnimRuntimeState::BridgeActive, false));
		TestFalse(
			TEXT("ReadyForActivation cannot deactivate what it does not own"),
			UPhysAnimComponent::ShouldDeactivateBridgeToSafeMode(EPhysAnimRuntimeState::ReadyForActivation, true));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimExplicitTargetDefaultsTest,
		"PhysAnim.Component.ExplicitTargetDefaults",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimExplicitTargetDefaultsTest::RunTest(const FString& Parameters)
	{
		FPhysAnimStabilizationSettings Settings;
		TestFalse(TEXT("Explicit targets are the default active-bridge mode"), Settings.bUseSkeletalAnimationTargets);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimCurrentPoseTargetOrientationTest,
		"PhysAnim.Component.CurrentPoseTargetOrientation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimCurrentPoseTargetOrientationTest::RunTest(const FString& Parameters)
	{
		const FQuat ParentWorldRotation(FVector::UpVector, FMath::DegreesToRadians(90.0f));
		const FQuat ChildWorldRotation(FVector::UpVector, FMath::DegreesToRadians(135.0f));
		const FQuat TargetOrientation =
			UPhysAnimComponent::BuildCurrentPoseControlTargetOrientation(ParentWorldRotation, ChildWorldRotation);
		const FQuat ExpectedRelative(FVector::UpVector, FMath::DegreesToRadians(45.0f));

		TestTrue(
			TEXT("Current-pose orientation seeding produces parent-space child rotation"),
			TargetOrientation.Equals(ExpectedRelative, 1.0e-4f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBodyModifierRuntimeModeTest,
		"PhysAnim.Component.BodyModifierRuntimeMode",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBodyModifierRuntimeModeTest::RunTest(const FString& Parameters)
	{
		EPhysicsMovementType MovementType = EPhysicsMovementType::Default;
		float PhysicsBlendWeight = -1.0f;
		bool bUpdateKinematicFromSimulation = true;

		UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
			true,
			true,
			true,
			false,
			false,
			MovementType,
			PhysicsBlendWeight,
			bUpdateKinematicFromSimulation);
		TestEqual(TEXT("Force-zero mode keeps body modifiers kinematic"), MovementType, EPhysicsMovementType::Kinematic);
		TestEqual(TEXT("Force-zero mode keeps body modifiers at zero blend"), PhysicsBlendWeight, 0.0f);
		TestFalse(TEXT("Force-zero mode disables update kinematic from simulation"), bUpdateKinematicFromSimulation);

		UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
			false,
			false,
			false,
			false,
			false,
			MovementType,
			PhysicsBlendWeight,
			bUpdateKinematicFromSimulation);
		TestEqual(TEXT("Settling phase keeps non-root body modifiers kinematic"), MovementType, EPhysicsMovementType::Kinematic);
		TestEqual(TEXT("Settling phase keeps non-root body modifiers at zero blend"), PhysicsBlendWeight, 0.0f);
		TestFalse(TEXT("Settling phase disables update kinematic from simulation"), bUpdateKinematicFromSimulation);

		UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
			false,
			true,
			true,
			true,
			false,
			MovementType,
			PhysicsBlendWeight,
			bUpdateKinematicFromSimulation);
		TestEqual(TEXT("Completed handoff still keeps root body modifier kinematic"), MovementType, EPhysicsMovementType::Kinematic);
		TestEqual(TEXT("Completed handoff keeps root body modifier at zero blend"), PhysicsBlendWeight, 0.0f);
		TestFalse(TEXT("Completed handoff keeps root update kinematic from simulation disabled"), bUpdateKinematicFromSimulation);

		UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
			false,
			true,
			true,
			false,
			false,
			MovementType,
			PhysicsBlendWeight,
			bUpdateKinematicFromSimulation);
		TestEqual(TEXT("Completed handoff enables non-root simulated body modifiers"), MovementType, EPhysicsMovementType::Simulated);
		TestEqual(TEXT("Completed handoff enables full blend"), PhysicsBlendWeight, 1.0f);
		TestFalse(TEXT("Completed handoff keeps simulated body modifiers from writing back to kinematic"), bUpdateKinematicFromSimulation);

		UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
			false,
			true,
			false,
			false,
			false,
			MovementType,
			PhysicsBlendWeight,
			bUpdateKinematicFromSimulation);
		TestEqual(TEXT("Locked bring-up group keeps non-root body modifiers kinematic"), MovementType, EPhysicsMovementType::Kinematic);
		TestEqual(TEXT("Locked bring-up group keeps non-root body modifiers at zero blend"), PhysicsBlendWeight, 0.0f);

		UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
			false,
			true,
			true,
			true,
			true,
			MovementType,
			PhysicsBlendWeight,
			bUpdateKinematicFromSimulation);
		TestEqual(TEXT("Presentation perturbation can temporarily simulate the root body modifier"), MovementType, EPhysicsMovementType::Simulated);
		TestEqual(TEXT("Presentation perturbation enables full blend on the root body modifier"), PhysicsBlendWeight, 1.0f);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBodyModifierCollisionModeTest,
		"PhysAnim.Component.BodyModifierCollisionMode",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBodyModifierCollisionModeTest::RunTest(const FString& Parameters)
	{
		TestEqual(
			TEXT("Force-zero mode keeps body modifier collision disabled"),
			UPhysAnimComponent::ResolveBodyModifierCollisionType(true, true, true, false, false),
			ECollisionEnabled::NoCollision);
		TestEqual(
			TEXT("Settling phase keeps non-root body modifier collision disabled"),
			UPhysAnimComponent::ResolveBodyModifierCollisionType(false, false, false, false, false),
			ECollisionEnabled::NoCollision);
		TestEqual(
			TEXT("Completed handoff still keeps root body modifier collision disabled"),
			UPhysAnimComponent::ResolveBodyModifierCollisionType(false, true, true, true, false),
			ECollisionEnabled::NoCollision);
		TestEqual(
			TEXT("Completed handoff enables non-root body modifier collision"),
			UPhysAnimComponent::ResolveBodyModifierCollisionType(false, true, true, false, false),
			ECollisionEnabled::QueryAndPhysics);
		TestEqual(
			TEXT("Locked bring-up group keeps non-root body modifier collision disabled"),
			UPhysAnimComponent::ResolveBodyModifierCollisionType(false, true, false, false, false),
			ECollisionEnabled::NoCollision);
		TestEqual(
			TEXT("Presentation perturbation enables root body modifier collision"),
			UPhysAnimComponent::ResolveBodyModifierCollisionType(false, true, true, true, true),
			ECollisionEnabled::QueryAndPhysics);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBodyModifierCachedResetTest,
		"PhysAnim.Component.BodyModifierCachedReset",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBodyModifierCachedResetTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Force-zero mode does not reset body modifiers to cached transforms"),
			UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(true, true, true, false, false));
		TestFalse(
			TEXT("Non-settling ticks do not reset body modifiers to cached transforms"),
			UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(false, false, true, false, false));
		TestFalse(
			TEXT("Root body modifier does not reset on settle tick"),
			UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(false, true, true, true, false));
		TestTrue(
			TEXT("Non-root body modifier resets on settle tick"),
			UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(false, true, true, false, false));
		TestFalse(
			TEXT("Locked bring-up group does not reset on settle tick"),
			UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(false, true, false, false, false));
		TestTrue(
			TEXT("Presentation perturbation resets the root body modifier when it becomes simulated"),
			UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(false, true, true, true, true));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBringUpGroupMappingTest,
		"PhysAnim.Component.BringUpGroupMapping",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBringUpGroupMappingTest::RunTest(const FString& Parameters)
	{
		TestEqual(TEXT("Bring-up group count matches staged unlock plan"), UPhysAnimComponent::GetBringUpGroupCount(), 5);
		TestEqual(TEXT("Spine group unlocks first"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("spine_01")), 0);
		TestEqual(TEXT("Thigh group unlocks first"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("thigh_r")), 0);
		TestEqual(TEXT("Calves unlock with the rest of the lower leg"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("calf_l")), 1);
		TestEqual(TEXT("Feet and balls unlock with calves"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("ball_l")), 1);
		TestEqual(TEXT("Upper-arm chain unlocks after the full lower leg"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("upperarm_l")), 2);
		TestEqual(TEXT("Lower arms unlock with the upper-arm chain"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("lowerarm_r")), 2);
		TestEqual(TEXT("Head and neck unlock before hands"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("head")), 3);
		TestEqual(TEXT("Hands unlock last"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("hand_r")), 4);
		TestEqual(TEXT("Root pelvis is not a staged non-root group"), UPhysAnimComponent::ResolveBringUpGroupIndex(TEXT("pelvis")), INDEX_NONE);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimBringUpGroupControlRampTest,
		"PhysAnim.Component.BringUpGroupControlRamp",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBringUpGroupControlRampTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Non-final bring-up groups do not require a post-unlock settle delay"),
			UPhysAnimComponent::ShouldDelayBringUpGroupControlRamp(3, 5));
		TestTrue(
			TEXT("Final bring-up group requires a post-unlock settle delay"),
			UPhysAnimComponent::ShouldDelayBringUpGroupControlRamp(4, 5));
		TestFalse(
			TEXT("Force-zero mode blocks bring-up group control ramp start"),
			UPhysAnimComponent::ShouldStartBringUpGroupControlRamp(true, true, false, true));
		TestFalse(
			TEXT("Locked bring-up groups cannot start their control ramp"),
			UPhysAnimComponent::ShouldStartBringUpGroupControlRamp(false, false, false, true));
		TestTrue(
			TEXT("Unlocked non-delayed bring-up groups start their control ramp immediately"),
			UPhysAnimComponent::ShouldStartBringUpGroupControlRamp(false, true, false, false));
		TestFalse(
			TEXT("Delayed bring-up groups wait for a post-unlock settle window"),
			UPhysAnimComponent::ShouldStartBringUpGroupControlRamp(false, true, true, false));
		TestTrue(
			TEXT("Delayed bring-up groups can start after the post-unlock settle window completes"),
			UPhysAnimComponent::ShouldStartBringUpGroupControlRamp(false, true, true, true));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimControlAuthorityAlphaTest,
		"PhysAnim.Component.ControlAuthorityAlpha",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMovementSmokeScriptTest,
		"PhysAnim.Component.MovementSmokeScript",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimControlAuthorityAlphaTest::RunTest(const FString& Parameters)
	{
		TestEqual(
			TEXT("Force-zero mode keeps control authority at zero"),
			UPhysAnimComponent::CalculateControlAuthorityAlpha(true, true, 1.0f, 1.0f),
			0.0f);
		TestEqual(
			TEXT("Unsettled handoff keeps control authority at zero"),
			UPhysAnimComponent::CalculateControlAuthorityAlpha(false, false, 1.0f, 1.0f),
			0.0f);
		TestEqual(
			TEXT("Settled handoff starts with zero control authority"),
			UPhysAnimComponent::CalculateControlAuthorityAlpha(false, true, 0.0f, 1.0f),
			0.0f);
		TestEqual(
			TEXT("Control authority ramps linearly during startup"),
			UPhysAnimComponent::CalculateControlAuthorityAlpha(false, true, 0.25f, 1.0f),
			0.25f);
		TestEqual(
			TEXT("Control authority reaches one after the ramp duration"),
			UPhysAnimComponent::CalculateControlAuthorityAlpha(false, true, 1.0f, 1.0f),
			1.0f);
		TestEqual(
			TEXT("Non-positive ramp durations snap to full authority once settled"),
			UPhysAnimComponent::CalculateControlAuthorityAlpha(false, true, 0.0f, 0.0f),
			1.0f);
		return true;
	}

	bool FPhysAnimMovementSmokeScriptTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("BridgeActive releases the gameplay shell when both preservation flags are off"),
			UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(false, false));
		TestTrue(
			TEXT("Movement smoke preserves the gameplay shell during BridgeActive"),
			UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(true, false));
		TestTrue(
			TEXT("Runtime movement enablement preserves the gameplay shell during BridgeActive"),
			UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(false, true));
		TestTrue(
			TEXT("Either preservation path keeps the gameplay shell active"),
			UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(true, true));
		TestTrue(
			TEXT("Initial movement smoke phase is idle"),
			UPhysAnimComponent::ResolveMovementSmokePhaseName(1.0f) == TEXT("Idle_00"));
		TestEqual(
			TEXT("Forward phase emits forward intent"),
			UPhysAnimComponent::ResolveMovementSmokeLocalIntent(4.0f),
			FVector(1.0f, 0.0f, 0.0f));
		TestTrue(
			TEXT("Left strafe phase is named correctly"),
			UPhysAnimComponent::ResolveMovementSmokePhaseName(12.0f) == TEXT("StrafeLeft"));
		TestEqual(
			TEXT("Left strafe emits negative local right intent"),
			UPhysAnimComponent::ResolveMovementSmokeLocalIntent(12.0f),
			FVector(0.0f, -1.0f, 0.0f));
		TestTrue(
			TEXT("Right strafe phase is named correctly"),
			UPhysAnimComponent::ResolveMovementSmokePhaseName(21.0f) == TEXT("StrafeRight"));
		TestEqual(
			TEXT("Backward phase emits backward intent"),
			UPhysAnimComponent::ResolveMovementSmokeLocalIntent(29.0f),
			FVector(-1.0f, 0.0f, 0.0f));
		TestTrue(
			TEXT("Completed movement smoke reports the complete phase"),
			UPhysAnimComponent::ResolveMovementSmokePhaseName(40.0f) == TEXT("Complete"));
		TestEqual(
			TEXT("Movement smoke duration is the frozen scripted window"),
			UPhysAnimComponent::GetMovementSmokeDurationSeconds(),
			32.0f);
		TestEqual(
			TEXT("Movement smoke total duration scales by loop count"),
			UPhysAnimComponent::GetMovementSmokeTotalDurationSeconds(3),
			96.0f);
		TestEqual(
			TEXT("Movement smoke total duration clamps loop count to one"),
			UPhysAnimComponent::GetMovementSmokeTotalDurationSeconds(0),
			32.0f);
		TestEqual(
			TEXT("Physics-driven comparison label is stable"),
			UPhysAnimComparisonSubsystem::ResolveComparisonRoleLabel(true),
			FString(TEXT("Physics-Driven")));
		TestEqual(
			TEXT("Kinematic comparison label is stable"),
			UPhysAnimComparisonSubsystem::ResolveComparisonRoleLabel(false),
			FString(TEXT("Kinematic")));
		TestEqual(
			TEXT("Physics-driven comparison actor is offset to positive Y"),
			UPhysAnimComparisonSubsystem::ResolveComparisonSpawnOffset(true, 250.0f),
			FVector(0.0f, 125.0f, 0.0f));
		TestEqual(
			TEXT("Kinematic comparison actor is offset to negative Y"),
			UPhysAnimComparisonSubsystem::ResolveComparisonSpawnOffset(false, 250.0f),
			FVector(0.0f, -125.0f, 0.0f));
		TestEqual(
			TEXT("Pending movement input takes priority when mirroring"),
			UPhysAnimComparisonSubsystem::ResolveMirroredWorldInput(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f)),
			FVector(1.0f, 0.0f, 0.0f));
		TestEqual(
			TEXT("Last movement input is used when pending input is empty"),
			UPhysAnimComparisonSubsystem::ResolveMirroredWorldInput(FVector::ZeroVector, FVector(0.0f, 1.0f, 0.0f)),
			FVector(0.0f, 1.0f, 0.0f));
		TestTrue(
			TEXT("G2 presentation now starts with the locomotion-coupled perturbation phase"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPhaseName(0.5f) == TEXT("WalkPerturbation"));
		TestEqual(
			TEXT("G2 perturbation phase uses forward walking intent"),
			UPhysAnimComparisonSubsystem::ResolvePresentationLocalIntent(0.5f),
			FVector(1.0f, 0.0f, 0.0f));
		TestEqual(
			TEXT("G2 perturbation phase uses walk-scale input"),
			UPhysAnimComparisonSubsystem::ResolvePresentationInputScale(0.5f),
			0.32f);
		TestFalse(
			TEXT("G2 perturbation does not fire before the lead-in ends"),
			UPhysAnimComparisonSubsystem::ShouldApplyPresentationPerturbation(0.5f));
		TestTrue(
			TEXT("G2 perturbation fires after the lead-in ends"),
			UPhysAnimComparisonSubsystem::ShouldApplyPresentationPerturbation(1.0f));
		TestEqual(
			TEXT("G2 perturbation pusher starts from the actor's left side"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPusherStartOffsetCm(),
			FVector(52.0f, -86.0f, 54.0f));
		TestEqual(
			TEXT("G2 perturbation pusher uses the frozen half extent"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPusherHalfExtentCm(),
			FVector(18.0f, 24.0f, 48.0f));
		TestEqual(
			TEXT("G2 perturbation pusher sweeps a fixed travel distance"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPusherTravelDistanceCm(),
			118.0f);
		TestEqual(
			TEXT("G2 perturbation pusher sweeps over the frozen contact window"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPusherTravelSeconds(),
			1.1f);
		TestEqual(
			TEXT("G2 perturbation does not apply a shell-level shove"),
			UPhysAnimComparisonSubsystem::ResolvePresentationShellPushForce(),
			FVector::ZeroVector);
		TestEqual(
			TEXT("G2 perturbation applies the frozen stabilization relaxation window"),
			UPhysAnimComparisonSubsystem::ResolvePresentationStabilizationOverrideSeconds(),
			0.45f);
		TestEqual(
			TEXT("G2 perturbation relaxes angular strength by the frozen movement-safe multiplier"),
			UPhysAnimComparisonSubsystem::ResolvePresentationStrengthRelaxationMultiplier(),
			0.72f);
		TestEqual(
			TEXT("G2 perturbation relaxes damping ratio by the frozen movement-safe multiplier"),
			UPhysAnimComparisonSubsystem::ResolvePresentationDampingRatioRelaxationMultiplier(),
			0.78f);
		TestEqual(
			TEXT("G2 perturbation relaxes extra damping by the frozen movement-safe multiplier"),
			UPhysAnimComparisonSubsystem::ResolvePresentationExtraDampingRelaxationMultiplier(),
			0.74f);
		TestTrue(
			TEXT("G2 presentation reaches idle ready after the perturbation intro"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPhaseName(4.5f) == TEXT("IdleReady"));
		TestEqual(
			TEXT("G2 presentation walk uses forward intent"),
			UPhysAnimComparisonSubsystem::ResolvePresentationLocalIntent(8.0f),
			FVector(1.0f, 0.0f, 0.0f));
		TestEqual(
			TEXT("G2 presentation walk uses reduced input scale"),
			UPhysAnimComparisonSubsystem::ResolvePresentationInputScale(8.0f),
			0.45f);
		TestTrue(
			TEXT("G2 presentation jog phase is named correctly"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPhaseName(14.0f) == TEXT("JogForward"));
		TestEqual(
			TEXT("G2 presentation jog uses full input scale"),
			UPhysAnimComparisonSubsystem::ResolvePresentationInputScale(14.0f),
			1.0f);
		TestTrue(
			TEXT("G2 presentation brake phase is named correctly"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPhaseName(17.0f) == TEXT("BrakeStop"));
		TestEqual(
			TEXT("G2 presentation turn uses diagonal intent"),
			UPhysAnimComparisonSubsystem::ResolvePresentationLocalIntent(20.0f),
			FVector(0.7f, 0.7f, 0.0f));
		TestTrue(
			TEXT("G2 presentation recovery phase is named correctly"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPhaseName(23.5f) == TEXT("Recovery"));
		TestTrue(
			TEXT("G2 presentation reports complete after the frozen duration"),
			UPhysAnimComparisonSubsystem::ResolvePresentationPhaseName(30.0f) == TEXT("Complete"));
		TestEqual(
			TEXT("G2 presentation duration is the frozen scripted window"),
			UPhysAnimComparisonSubsystem::GetPresentationDurationSeconds(),
			25.0f);
		TestEqual(
			TEXT("G2 perturbation camera offset is the closer framing"),
			UPhysAnimComparisonSubsystem::ResolvePresentationCameraOffsetCm(true),
			FVector(235.0f, 0.0f, 138.0f));
		TestEqual(
			TEXT("G2 locomotion camera offset is the wider framing"),
			UPhysAnimComparisonSubsystem::ResolvePresentationCameraOffsetCm(false),
			FVector(325.0f, 0.0f, 170.0f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyInfluenceAlphaTest,
		"PhysAnim.Component.PolicyInfluenceAlpha",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyInfluenceRampGateTest,
		"PhysAnim.Component.PolicyInfluenceRampGate",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPresentationPerturbationPolicySuspensionTest,
		"PhysAnim.Component.PresentationPerturbationPolicySuspension",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyTargetBoneFilterTest,
		"PhysAnim.Component.PolicyTargetBoneFilter",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyTargetRepresentationModeTest,
		"PhysAnim.Component.PolicyTargetRepresentationMode",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimControlTargetDeltaTest,
		"PhysAnim.Component.ControlTargetDelta",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPolicyTargetBlendTest,
		"PhysAnim.Component.PolicyTargetBlend",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPolicyInfluenceAlphaTest::RunTest(const FString& Parameters)
	{
		TestEqual(
			TEXT("Force-zero mode keeps policy influence at zero"),
			UPhysAnimComponent::CalculatePolicyInfluenceAlpha(true, true, 1.0f, 1.0f),
			0.0f);
		TestEqual(
			TEXT("Locked bring-up groups keep policy influence at zero"),
			UPhysAnimComponent::CalculatePolicyInfluenceAlpha(false, false, 1.0f, 1.0f),
			0.0f);
		TestEqual(
			TEXT("Final group activation starts policy influence at zero"),
			UPhysAnimComponent::CalculatePolicyInfluenceAlpha(false, true, 0.0f, 1.0f),
			0.0f);
		TestEqual(
			TEXT("Policy influence ramps linearly after all groups unlock"),
			UPhysAnimComponent::CalculatePolicyInfluenceAlpha(false, true, 0.5f, 1.0f),
			0.5f);
		TestEqual(
			TEXT("Policy influence reaches one after the ramp duration"),
			UPhysAnimComponent::CalculatePolicyInfluenceAlpha(false, true, 1.0f, 1.0f),
			1.0f);
		return true;
	}

	bool FPhysAnimPolicyInfluenceRampGateTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Force-zero mode blocks policy influence ramp start"),
			UPhysAnimComponent::ShouldStartPolicyInfluenceRamp(true, true, true, true));
		TestFalse(
			TEXT("Policy influence waits until all bring-up groups are unlocked"),
			UPhysAnimComponent::ShouldStartPolicyInfluenceRamp(false, false, true, true));
		TestFalse(
			TEXT("Policy influence waits until final-group control ramp is active"),
			UPhysAnimComponent::ShouldStartPolicyInfluenceRamp(false, true, false, true));
		TestFalse(
			TEXT("Policy influence waits for a post-final-group-control settle window"),
			UPhysAnimComponent::ShouldStartPolicyInfluenceRamp(false, true, true, false));
		TestTrue(
			TEXT("Policy influence can start after final-group control settles"),
			UPhysAnimComponent::ShouldStartPolicyInfluenceRamp(false, true, true, true));
		return true;
	}

	bool FPhysAnimPresentationPerturbationPolicySuspensionTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Presentation perturbation override does not suspend policy influence in the default runtime path"),
			UPhysAnimComponent::ShouldSuspendPolicyInfluenceDuringPresentationPerturbation(true));
		TestFalse(
			TEXT("Policy influence remains available when no presentation perturbation override is active"),
			UPhysAnimComponent::ShouldSuspendPolicyInfluenceDuringPresentationPerturbation(false));
		return true;
	}

	bool FPhysAnimPolicyTargetBoneFilterTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Policy-inactive frames never write targets"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("spine_01"), false));
		TestTrue(
			TEXT("Active policy can still drive lower-body bones"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("thigh_l"), true));
		TestFalse(
			TEXT("Active policy now defers spine_01 targets during the current scope split"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("spine_01"), true));
		TestFalse(
			TEXT("Active policy now defers spine_02 targets during the current scope split"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("spine_02"), true));
		TestFalse(
			TEXT("Active policy now defers spine_03 targets during the current scope split"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("spine_03"), true));
		TestFalse(
			TEXT("Active policy defers left clavicle targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("clavicle_l"), true));
		TestFalse(
			TEXT("Active policy defers left upper-arm targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("upperarm_l"), true));
		TestFalse(
			TEXT("Active policy defers left lower-arm targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("lowerarm_l"), true));
		TestFalse(
			TEXT("Active policy defers left-hand targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("hand_l"), true));
		TestFalse(
			TEXT("Active policy defers neck targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("neck_01"), true));
		TestFalse(
			TEXT("Active policy defers head targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("head"), true));
		TestFalse(
			TEXT("Active policy defers right clavicle targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("clavicle_r"), true));
		TestFalse(
			TEXT("Active policy defers right upper-arm targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("upperarm_r"), true));
		TestFalse(
			TEXT("Active policy defers right lower-arm targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("lowerarm_r"), true));
		TestFalse(
			TEXT("Active policy defers right-hand targets during the current stabilization pass"),
			UPhysAnimComponent::ShouldApplyPolicyTargetToBone(TEXT("hand_r"), true));
		return true;
	}

	bool FPhysAnimControlTargetDeltaTest::RunTest(const FString& Parameters)
	{
		TestTrue(
			TEXT("Identical targets produce zero angular delta"),
			FMath::IsNearlyZero(
				UPhysAnimComponent::CalculateControlTargetDeltaDegrees(FQuat::Identity, FQuat::Identity),
				KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Quarter-turn target delta is reported in degrees"),
			FMath::IsNearlyEqual(
				UPhysAnimComponent::CalculateControlTargetDeltaDegrees(
					FQuat::Identity,
					FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0f))),
				90.0f,
				0.1f));
		return true;
	}

	bool FPhysAnimPolicyTargetRepresentationModeTest::RunTest(const FString& Parameters)
	{
		TestFalse(
			TEXT("Configured explicit-target mode stays disabled when policy is inactive"),
			UPhysAnimComponent::ShouldUseSkeletalAnimationTargetRepresentation(false, false));
		TestTrue(
			TEXT("Live policy phase uses skeletal-animation target representation"),
			UPhysAnimComponent::ShouldUseSkeletalAnimationTargetRepresentation(false, true));
		TestTrue(
			TEXT("Configured skeletal-animation target mode stays enabled"),
			UPhysAnimComponent::ShouldUseSkeletalAnimationTargetRepresentation(true, false));
		TestTrue(
			TEXT("First policy frame resets all control offsets when switching into skeletal-animation target mode"),
			UPhysAnimComponent::ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(true, true));
		TestFalse(
			TEXT("Later policy frames do not keep resetting all control offsets"),
			UPhysAnimComponent::ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(true, false));
		TestFalse(
			TEXT("Explicit-target mode never resets all control offsets on policy start"),
			UPhysAnimComponent::ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(false, true));
		TestEqual(
			TEXT("First policy frame in skeletal-animation target mode zeros target angular velocity delta time"),
			UPhysAnimComponent::ResolvePolicyTargetWriteDeltaTime(true, true, 0.25f),
			0.0f);
		TestEqual(
			TEXT("Subsequent policy frames keep the runtime delta time"),
			UPhysAnimComponent::ResolvePolicyTargetWriteDeltaTime(true, false, 0.25f),
			0.25f);
		TestEqual(
			TEXT("Explicit-target mode keeps the runtime delta time even on the first policy frame"),
			UPhysAnimComponent::ResolvePolicyTargetWriteDeltaTime(false, true, 0.25f),
			0.25f);
		return true;
	}

	bool FPhysAnimPolicyTargetBlendTest::RunTest(const FString& Parameters)
	{
		const FQuat Baseline = FQuat::Identity;
		const FQuat PolicyTarget = FQuat(FVector::UpVector, FMath::DegreesToRadians(90.0f));

		TestTrue(
			TEXT("Zero policy alpha preserves the baseline target"),
			UPhysAnimComponent::BlendPolicyTargetRotation(Baseline, PolicyTarget, 0.0f).Equals(Baseline, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Full policy alpha reaches the policy target"),
			UPhysAnimComponent::BlendPolicyTargetRotation(Baseline, PolicyTarget, 1.0f).Equals(PolicyTarget, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Half policy alpha blends to the midpoint rotation"),
			FMath::IsNearlyEqual(
				UPhysAnimComponent::CalculateControlTargetDeltaDegrees(
					Baseline,
					UPhysAnimComponent::BlendPolicyTargetRotation(Baseline, PolicyTarget, 0.5f)),
				45.0f,
				0.1f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimStage1InitializerDefaultsTest,
		"PhysAnim.Component.Stage1InitializerDefaults",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimStage1InitializerDefaultsTest::RunTest(const FString& Parameters)
	{
		UPhysAnimStage1InitializerComponent* const Initializer =
			NewObject<UPhysAnimStage1InitializerComponent>();
		TestNotNull(TEXT("Stage 1 initializer should exist"), Initializer);
		if (!Initializer)
		{
			return false;
		}

		TestFalse(TEXT("Create controls at begin play is disabled"), Initializer->bCreateControlsAtBeginPlay);
		TestEqual(TEXT("Control count matches Stage 1 expectation"), Initializer->InitialControls.Num(), NumControlledBones);
		TestEqual(
			TEXT("Body modifier count matches Stage 1 expectation"),
			Initializer->InitialBodyModifiers.Num(),
			NumRequiredBodyModifiers);

		const FInitialPhysicsControl* const LeftThighControl = Initializer->InitialControls.Find(MakeControlName(TEXT("thigh_l")));
		TestNotNull(TEXT("thigh_l control exists"), LeftThighControl);
		if (LeftThighControl)
		{
			TestFalse(TEXT("Control parent actor starts unset"), LeftThighControl->ParentActor.IsValid());
			TestFalse(TEXT("Control child actor starts unset"), LeftThighControl->ChildActor.IsValid());
			TestEqual(TEXT("Control parent mesh component name"), LeftThighControl->ParentMeshComponentName, FName(TEXT("CharacterMesh0")));
			TestEqual(TEXT("Control child mesh component name"), LeftThighControl->ChildMeshComponentName, FName(TEXT("CharacterMesh0")));
			TestEqual(TEXT("Control parent bone"), LeftThighControl->ParentBoneName, FName(TEXT("pelvis")));
			TestEqual(TEXT("Control child bone"), LeftThighControl->ChildBoneName, FName(TEXT("thigh_l")));
			TestTrue(TEXT("Control is enabled"), LeftThighControl->ControlData.bEnabled);
			TestEqual(TEXT("Angular strength uses Stage 1 default"), LeftThighControl->ControlData.AngularStrength, 800.0f);
			TestFalse(TEXT("Stage 1 defaults use explicit targets, not skeletal-animation targets"), LeftThighControl->ControlData.bUseSkeletalAnimation);
		}

		const FInitialBodyModifier* const PelvisModifier =
			Initializer->InitialBodyModifiers.Find(MakeBodyModifierName(TEXT("pelvis")));
		TestNotNull(TEXT("pelvis body modifier exists"), PelvisModifier);
		if (PelvisModifier)
		{
			TestFalse(TEXT("Body modifier actor starts unset"), PelvisModifier->Actor.IsValid());
			TestEqual(TEXT("Body modifier mesh component name"), PelvisModifier->MeshComponentName, FName(TEXT("CharacterMesh0")));
			TestEqual(TEXT("Body modifier bone"), PelvisModifier->BoneName, FName(TEXT("pelvis")));
			TestEqual(TEXT("Body modifier movement type"), PelvisModifier->BodyModifierData.MovementType, EPhysicsMovementType::Kinematic);
			TestEqual(TEXT("Body modifier collision type"), PelvisModifier->BodyModifierData.CollisionType, ECollisionEnabled::NoCollision);
			TestEqual(TEXT("Body modifier physics blend weight"), PelvisModifier->BodyModifierData.PhysicsBlendWeight, 0.0f);
			TestFalse(
				TEXT("Body modifier defaults disable update kinematic from simulation"),
				PelvisModifier->BodyModifierData.bUpdateKinematicFromSimulation);
			TestEqual(
				TEXT("Body modifier target space"),
				PelvisModifier->BodyModifierData.KinematicTargetSpace,
				EPhysicsControlKinematicTargetSpace::OffsetInBoneSpace);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimStage1InitializerRuntimeRefsTest,
		"PhysAnim.Component.Stage1InitializerRuntimeRefs",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyConstraintInventoryTest,
		"PhysAnim.Component.MannyConstraintInventory",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimStage1InitializerRuntimeRefsTest::RunTest(const FString& Parameters)
	{
		AActor* const Owner = NewObject<AActor>();
		UPhysAnimStage1InitializerComponent* const Initializer =
			NewObject<UPhysAnimStage1InitializerComponent>(Owner, TEXT("PhysAnimStage1Initializer"));
		TestNotNull(TEXT("Transient owner actor should exist"), Owner);
		TestNotNull(TEXT("Transient initializer should exist"), Initializer);
		if (!Owner || !Initializer)
		{
			return false;
		}

		Owner->AddOwnedComponent(Initializer);

		AActor* const ParentOverride = NewObject<AActor>();
		AActor* const ChildOverride = NewObject<AActor>();
		AActor* const BodyOverride = NewObject<AActor>();

		Initializer->DefaultControlParentActor = ParentOverride;
		Initializer->DefaultControlChildActor = ChildOverride;
		Initializer->DefaultBodyModifierActor = BodyOverride;
		Initializer->PrepareRuntimeDefaults();

		const FInitialPhysicsControl* const LeftThighControl = Initializer->InitialControls.Find(MakeControlName(TEXT("thigh_l")));
		TestNotNull(TEXT("thigh_l control exists after runtime prep"), LeftThighControl);
		if (LeftThighControl)
		{
			TestTrue(TEXT("Runtime prep applies parent actor override"), LeftThighControl->ParentActor.Get() == ParentOverride);
			TestTrue(TEXT("Runtime prep applies child actor override"), LeftThighControl->ChildActor.Get() == ChildOverride);
		}

		const FInitialBodyModifier* const PelvisModifier =
			Initializer->InitialBodyModifiers.Find(MakeBodyModifierName(TEXT("pelvis")));
		TestNotNull(TEXT("pelvis body modifier exists after runtime prep"), PelvisModifier);
		if (PelvisModifier)
		{
			TestTrue(TEXT("Runtime prep applies body actor override"), PelvisModifier->Actor.Get() == BodyOverride);
		}

		return true;
	}

	bool FPhysAnimMannyConstraintInventoryTest::RunTest(const FString& Parameters)
	{
		UPhysicsAsset* const PhysicsAsset =
			LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		TestNotNull(TEXT("Expected Manny physics asset should load"), PhysicsAsset);
		if (!PhysicsAsset)
		{
			return false;
		}

		UPhysAnimStage1InitializerComponent* const Initializer =
			NewObject<UPhysAnimStage1InitializerComponent>();
		TestNotNull(TEXT("Stage 1 initializer should exist for constraint inventory"), Initializer);
		if (!Initializer)
		{
			return false;
		}

		auto ConstraintMotionToString = [](EAngularConstraintMotion Motion) -> const TCHAR*
		{
			switch (Motion)
			{
			case ACM_Free:
				return TEXT("Free");
			case ACM_Limited:
				return TEXT("Limited");
			case ACM_Locked:
				return TEXT("Locked");
			default:
				return TEXT("Unknown");
			}
		};

		const TSet<FName> ExpectedMissingDirectConstraintChildren =
		{
			TEXT("neck_01"),
			TEXT("head"),
			TEXT("clavicle_l"),
			TEXT("clavicle_r")
		};

		int32 NumConstraintPairsFound = 0;
		TSet<FName> MissingDirectConstraintChildren;
		for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
		{
			const FInitialPhysicsControl* const InitialControl =
				Initializer->InitialControls.Find(PhysAnimBridge::MakeControlName(BoneName));
			TestNotNull(
				*FString::Printf(TEXT("Stage 1 initializer should contain control for %s"), *BoneName.ToString()),
				InitialControl);
			if (!InitialControl)
			{
				continue;
			}

			const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(
				InitialControl->ChildBoneName,
				InitialControl->ParentBoneName);
			if (ConstraintIndex == INDEX_NONE)
			{
				MissingDirectConstraintChildren.Add(InitialControl->ChildBoneName);
				AddInfo(FString::Printf(
					TEXT("[PhysAnimLimitAudit] child=%s parent=%s directConstraint=missing"),
					*InitialControl->ChildBoneName.ToString(),
					*InitialControl->ParentBoneName.ToString()));
				continue;
			}

			UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex)
				? PhysicsAsset->ConstraintSetup[ConstraintIndex]
				: nullptr;
			FConstraintInstance* const ConstraintInstance = ConstraintTemplate
				? &ConstraintTemplate->DefaultInstance
				: nullptr;
			TestNotNull(
				*FString::Printf(
					TEXT("Constraint instance should exist child=%s parent=%s"),
					*InitialControl->ChildBoneName.ToString(),
					*InitialControl->ParentBoneName.ToString()),
				ConstraintInstance);
			if (!ConstraintInstance)
			{
				continue;
			}

			++NumConstraintPairsFound;
			AddInfo(FString::Printf(
				TEXT("[PhysAnimLimitAudit] child=%s parent=%s twist=%s/%.1f swing1=%s/%.1f swing2=%s/%.1f"),
				*InitialControl->ChildBoneName.ToString(),
				*InitialControl->ParentBoneName.ToString(),
				ConstraintMotionToString(ConstraintInstance->GetAngularTwistMotion()),
				ConstraintInstance->GetAngularTwistLimit(),
				ConstraintMotionToString(ConstraintInstance->GetAngularSwing1Motion()),
				ConstraintInstance->GetAngularSwing1Limit(),
				ConstraintMotionToString(ConstraintInstance->GetAngularSwing2Motion()),
				ConstraintInstance->GetAngularSwing2Limit()));
		}

		TestEqual(
			TEXT("Manny direct-constraint gaps stay limited to the known aggregate bones"),
			MissingDirectConstraintChildren.Num(),
			ExpectedMissingDirectConstraintChildren.Num());
		for (const FName BoneName : ExpectedMissingDirectConstraintChildren)
		{
			TestTrue(
				*FString::Printf(TEXT("Expected missing direct constraint child %s is present"), *BoneName.ToString()),
				MissingDirectConstraintChildren.Contains(BoneName));
		}
		TestEqual(
			TEXT("Remaining controlled Manny bones expose direct constraint entries"),
			NumConstraintPairsFound,
			PhysAnimBridge::GetControlledBoneNames().Num() - ExpectedMissingDirectConstraintChildren.Num());
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimMannyMassInventoryTest,
		"PhysAnim.Component.MannyMassInventory",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimMannyMassInventoryTest::RunTest(const FString& Parameters)
	{
		UPhysicsAsset* const PhysicsAsset =
			LoadObject<UPhysicsAsset>(nullptr, TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin"));
		TestNotNull(TEXT("Expected Manny physics asset should load"), PhysicsAsset);
		if (!PhysicsAsset)
		{
			return false;
		}

		USkeletalMesh* const MannyMesh =
			LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
		TestNotNull(TEXT("Expected Manny skeletal mesh should load"), MannyMesh);
		if (!MannyMesh)
		{
			return false;
		}

		USkeletalMeshComponent* const MannyMeshComponent = NewObject<USkeletalMeshComponent>();
		TestNotNull(TEXT("Transient Manny mesh component should exist"), MannyMeshComponent);
		if (!MannyMeshComponent)
		{
			return false;
		}

		MannyMeshComponent->SetSkeletalMeshAsset(MannyMesh);
		MannyMeshComponent->SetPhysicsAsset(PhysicsAsset, false);
		MannyMeshComponent->SetWorldScale3D(FVector::OneVector);

		float TotalMassKg = 0.0f;
		for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (!BodySetup)
			{
				continue;
			}

			const float BodyMassKg = BodySetup->CalculateMass(MannyMeshComponent);
			TotalMassKg += BodyMassKg;
		}

		TestTrue(TEXT("Total Manny physics-asset mass should be positive"), TotalMassKg > 0.0f);
		AddInfo(FString::Printf(TEXT("[PhysAnimMassAudit] totalMassKg=%.3f"), TotalMassKg));

		int32 LoggedBodyCount = 0;
		for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (!BodySetup)
			{
				continue;
			}

			const float BodyMassKg = BodySetup->CalculateMass(MannyMeshComponent);
			const float BodyVolume = BodySetup->GetScaledVolume(FVector::OneVector);
			const float BodyMassScale = BodySetup->DefaultInstance.MassScale;
			const float BodyMassPercent = TotalMassKg > 0.0f ? (BodyMassKg / TotalMassKg) * 100.0f : 0.0f;
			const FString BoneNameString = BodySetup->BoneName.ToString();

			AddInfo(FString::Printf(
				TEXT("[PhysAnimMassAudit] bone=%s massKg=%.3f percent=%.2f volumeUU=%.3f massScale=%.3f"),
				*BoneNameString,
				BodyMassKg,
				BodyMassPercent,
				BodyVolume,
				BodyMassScale));
			++LoggedBodyCount;
		}

		TestEqual(TEXT("Every Manny body setup should be logged in the mass inventory"), LoggedBodyCount, PhysicsAsset->SkeletalBodySetups.Num());
		return true;
	}

#if WITH_EDITOR
	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieSmokeTest,
		"PhysAnim.PIE.Smoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieMovementSmokeTest,
		"PhysAnim.PIE.MovementSmoke",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieMovementSoakTest,
		"PhysAnim.PIE.MovementSoak",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimPieG2PresentationTest,
		"PhysAnim.PIE.G2Presentation",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPieSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieSmokePrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FEditorAutomationLogCommand(FString::Printf(
			TEXT("%s PIE smoke opening '%s'."),
			PhysAnimPieSmokePrefix,
			*PhysAnimPieSmokeMap)));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor != nullptr && IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not start within %.1f seconds."),
					PhysAnimPieSmokePrefix,
					PhysAnimPieSmokeStartTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieSmokeDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor == nullptr || !IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not stop within %.1f seconds."),
					PhysAnimPieSmokePrefix,
					PhysAnimPieSmokeStopTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStopTimeoutSeconds));

		return true;
	}

	bool FPhysAnimPieMovementSmokeTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieMovementSmokePrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FEditorAutomationLogCommand(FString::Printf(
			TEXT("%s PIE movement smoke opening '%s'."),
			PhysAnimPieMovementSmokePrefix,
			*PhysAnimPieSmokeMap)));
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 1));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor != nullptr && IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not start within %.1f seconds."),
					PhysAnimPieMovementSmokePrefix,
					PhysAnimPieSmokeStartTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieMovementSmokeDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor == nullptr || !IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not stop within %.1f seconds."),
					PhysAnimPieMovementSmokePrefix,
					PhysAnimPieSmokeStopTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStopTimeoutSeconds));
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 0));

		return true;
	}

	bool FPhysAnimPieMovementSoakTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieMovementSoakPrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FEditorAutomationLogCommand(FString::Printf(
			TEXT("%s PIE movement soak opening '%s'."),
			PhysAnimPieMovementSoakPrefix,
			*PhysAnimPieSmokeMap)));
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 1));
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeLoopCount"), PhysAnimPieMovementSoakLoopCount));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor != nullptr && IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not start within %.1f seconds."),
					PhysAnimPieMovementSoakPrefix,
					PhysAnimPieSmokeStartTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieMovementSoakDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor == nullptr || !IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not stop within %.1f seconds."),
					PhysAnimPieMovementSoakPrefix,
					PhysAnimPieSmokeStopTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStopTimeoutSeconds));
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeLoopCount"), 1));
		AddCommand(new FSetIntConsoleVariableCommand(TEXT("physanim.MovementSmokeMode"), 0));

		return true;
	}

	bool FPhysAnimPieG2PresentationTest::RunTest(const FString& Parameters)
	{
		if (!AutomationOpenMap(PhysAnimPieSmokeMap, true))
		{
			AddError(FString::Printf(TEXT("%s Failed to open map '%s'."), PhysAnimPieG2PresentationPrefix, *PhysAnimPieSmokeMap));
			return false;
		}

		AddCommand(new FEditorAutomationLogCommand(FString::Printf(
			TEXT("%s PIE G2 presentation opening '%s'."),
			PhysAnimPieG2PresentationPrefix,
			*PhysAnimPieSmokeMap)));
		AddCommand(new FStartPIECommand(false));
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor != nullptr && IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not start within %.1f seconds."),
					PhysAnimPieG2PresentationPrefix,
					PhysAnimPieSmokeStartTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStartTimeoutSeconds));
		AddCommand(new FWaitLatentCommand(PhysAnimPieG2PresentationLeadInSeconds));
		AddCommand(new FExecPieConsoleCommand(TEXT("PhysAnim.G2.StartPresentation")));
		AddCommand(new FWaitLatentCommand(PhysAnimPieG2PresentationDurationSeconds));
		AddCommand(new FEndPlayMapCommand());
		AddCommand(new FUntilCommand(
			[]() -> bool
			{
				return GEditor == nullptr || !IsValid(GEditor->PlayWorld);
			},
			[this]() -> bool
			{
				AddError(FString::Printf(
					TEXT("%s PIE did not stop within %.1f seconds."),
					PhysAnimPieG2PresentationPrefix,
					PhysAnimPieSmokeStopTimeoutSeconds));
				return true;
			},
			PhysAnimPieSmokeStopTimeoutSeconds));

		return true;
	}
#endif
}

#endif
