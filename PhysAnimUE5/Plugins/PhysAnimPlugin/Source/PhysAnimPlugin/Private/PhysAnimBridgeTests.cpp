#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"
#include "PhysAnimStage1InitializerComponent.h"

#include "Misc/AutomationTest.h"
#include "PhysicsControlActor.h"

namespace
{
	using namespace PhysAnimBridge;

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

		TestTrue(TEXT("Create controls at begin play is enabled"), Initializer->bCreateControlsAtBeginPlay);
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
		}

		const FInitialBodyModifier* const PelvisModifier =
			Initializer->InitialBodyModifiers.Find(MakeBodyModifierName(TEXT("pelvis")));
		TestNotNull(TEXT("pelvis body modifier exists"), PelvisModifier);
		if (PelvisModifier)
		{
			TestFalse(TEXT("Body modifier actor starts unset"), PelvisModifier->Actor.IsValid());
			TestEqual(TEXT("Body modifier mesh component name"), PelvisModifier->MeshComponentName, FName(TEXT("CharacterMesh0")));
			TestEqual(TEXT("Body modifier bone"), PelvisModifier->BoneName, FName(TEXT("pelvis")));
			TestEqual(TEXT("Body modifier movement type"), PelvisModifier->BodyModifierData.MovementType, EPhysicsMovementType::Simulated);
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
}

#endif
