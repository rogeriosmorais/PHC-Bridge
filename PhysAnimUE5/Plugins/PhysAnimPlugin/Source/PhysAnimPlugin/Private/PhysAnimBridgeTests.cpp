#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"

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
		FPhysAnimPopulateDefaultsTest,
		"PhysAnim.Component.PopulateStage1Defaults",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimPopulateDefaultsTest::RunTest(const FString& Parameters)
	{
		AActor* const Owner = NewObject<AActor>();
		TestNotNull(TEXT("Transient owner actor should exist"), Owner);
		if (!Owner)
		{
			return false;
		}

		UPhysicsControlInitializerComponent* const Initializer =
			NewObject<UPhysicsControlInitializerComponent>(Owner, TEXT("PhysicsControlInitializer"));
		UPhysAnimComponent* const PhysAnimComponent =
			NewObject<UPhysAnimComponent>(Owner, TEXT("PhysAnim"));

		Owner->AddOwnedComponent(Initializer);
		Owner->AddOwnedComponent(PhysAnimComponent);

		FInitialPhysicsControl StaleControl;
		Initializer->InitialControls.Add(TEXT("StaleControl"), StaleControl);

		FInitialBodyModifier StaleModifier;
		Initializer->InitialBodyModifiers.Add(TEXT("StaleModifier"), StaleModifier);

		PhysAnimComponent->PopulateStage1PhysicsControlDefaults();

		TestTrue(TEXT("Create controls at begin play is enabled"), Initializer->bCreateControlsAtBeginPlay);
		TestEqual(TEXT("Control count matches Stage 1 expectation"), Initializer->InitialControls.Num(), NumControlledBones);
		TestEqual(
			TEXT("Body modifier count matches Stage 1 expectation"),
			Initializer->InitialBodyModifiers.Num(),
			NumRequiredBodyModifiers);
		TestFalse(TEXT("Stale control entry is removed"), Initializer->InitialControls.Contains(TEXT("StaleControl")));
		TestFalse(TEXT("Stale modifier entry is removed"), Initializer->InitialBodyModifiers.Contains(TEXT("StaleModifier")));

		const FInitialPhysicsControl* const LeftThighControl = Initializer->InitialControls.Find(MakeControlName(TEXT("thigh_l")));
		TestNotNull(TEXT("thigh_l control exists"), LeftThighControl);
		if (LeftThighControl)
		{
			TestTrue(TEXT("Control parent actor points at owner"), LeftThighControl->ParentActor.Get() == Owner);
			TestTrue(TEXT("Control child actor points at owner"), LeftThighControl->ChildActor.Get() == Owner);
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
			TestTrue(TEXT("Body modifier actor points at owner"), PelvisModifier->Actor.Get() == Owner);
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
}

#endif
