#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimBridge.h"

#include "Misc/AutomationTest.h"

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
}

#endif
