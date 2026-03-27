#include "PhysAnimBridge.h"
#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimBridge;

	void DeleteTraceTestRootPath(const FString& RootPath)
	{
		IFileManager::Get().DeleteDirectory(*RootPath, false, true);
	}

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
		TestEqual(
			TEXT("Unclamped future target time keeps the requested offset"),
			ResolveFutureTargetTimeSeconds(0.5f, 0.2f, 2.0f),
			0.2f);
		TestEqual(
			TEXT("Clamped future target time shrinks at animation end"),
			ResolveFutureTargetTimeSeconds(1.9f, 0.4f, 2.0f),
			0.1f);
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
		TestTrue(TEXT("SMPL local authoring Y-up becomes UE Z-up"), UeUp.Equals(FVector(0.0, 0.0, 1.0), KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity SMPL local rotation stays identity after UE local basis conversion"),
			SmplQuaternionToUe(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("Identity UE local rotation stays identity after SMPL local basis conversion"),
			UeQuaternionToSmpl(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));

		const FQuat SmplRotation = ExpMapToQuaternion(FVector(0.2, -0.1, 0.3));
		const FQuat RoundTrip = UeQuaternionToSmpl(SmplQuaternionToUe(SmplRotation));
		TestTrue(TEXT("Quaternion roundtrip should stay close"), RoundTrip.Equals(SmplRotation, 1.0e-3f));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimRuntimeWorldFrameConversionTest,
		"PhysAnim.Bridge.RuntimeWorldFrameConversion",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimRuntimeWorldFrameConversionTest::RunTest(const FString& Parameters)
	{
		TestTrue(
			TEXT("UE runtime world up stays Proto runtime up"),
			UeWorldRotationVectorToProtoRuntime(FVector::UpVector).Equals(FVector::UpVector, KINDA_SMALL_NUMBER));
		TestTrue(
			TEXT("UE runtime world forward stays Proto runtime forward"),
			UeWorldRotationVectorToProtoRuntime(FVector::ForwardVector).Equals(FVector::ForwardVector, KINDA_SMALL_NUMBER));

		const FVector SamplePos(100.0f, 0.0f, 0.0f);
		TestTrue(
			TEXT("UE runtime world position is scaled to meters (100cm -> 1m)"),
			UeWorldPositionToProtoRuntime(SamplePos).Equals(FVector(1.0f, 0.0f, 0.0f), KINDA_SMALL_NUMBER));

		const FVector SampleVel(0.0f, 200.0f, 0.0f);
		TestTrue(
			TEXT("UE runtime world velocity is scaled and swizzled (200cm/s -> -2m/s)"),
			UeWorldVelocityToProtoRuntime(SampleVel).Equals(FVector(0.0f, -2.0f, 0.0f), KINDA_SMALL_NUMBER));

		TestTrue(
			TEXT("Identity UE runtime world rotation stays identity in Proto runtime"),
			UeWorldQuaternionToProtoRuntime(FQuat::Identity).Equals(FQuat::Identity, KINDA_SMALL_NUMBER));

		const FQuat UeYaw = FQuat(FVector::UpVector, FMath::DegreesToRadians(25.0));
		TestTrue(
			TEXT("UE runtime world yaw is preserved in Proto runtime"),
			UeWorldQuaternionToProtoRuntime(UeYaw).Equals(UeYaw, 1.0e-4f));
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
		FPhysAnimTerrainSampleOffsetsTest,
		"PhysAnim.Bridge.TerrainSampleOffsets",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTerrainSampleOffsetsTest::RunTest(const FString& Parameters)
	{
		const TArray<FVector2D>& Offsets = GetTerrainSampleOffsets();
		TestEqual(TEXT("Terrain sample offset count"), Offsets.Num(), TerrainSize);
		TestEqual(TEXT("First sample X"), static_cast<float>(Offsets[0].X), -TerrainSampleWidth);
		TestEqual(TEXT("First sample Y"), static_cast<double>(Offsets[1].Y), -static_cast<double>(TerrainSampleWidth) + (2.0 * static_cast<double>(TerrainSampleWidth)) / static_cast<double>(TerrainSamplesPerAxis - 1));
		TestEqual(TEXT("Last sample X"), static_cast<float>(Offsets.Last().X), TerrainSampleWidth);
		TestEqual(TEXT("Last sample Y"), static_cast<float>(Offsets.Last().Y), TerrainSampleWidth);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimTerrainObservationPackingTest,
		"PhysAnim.Bridge.TerrainObservationPacking",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimTerrainObservationPackingTest::RunTest(const FString& Parameters)
	{
		TArray<float> GroundHeights;
		GroundHeights.Init(10.0f, TerrainSize);
		GroundHeights[0] = 7.5f;

		TArray<float> Terrain;
		FString Error;
		TestTrue(TEXT("Terrain observation packing should succeed"), BuildTerrainObservation(12.0f, GroundHeights, Terrain, Error));
		TestEqual(TEXT("Terrain observation size"), Terrain.Num(), TerrainSize);
		TestEqual(TEXT("Terrain root-height delta uses per-sample ground height"), Terrain[0], 4.5f);
		TestEqual(TEXT("Flat terrain sample uses root-minus-ground height"), Terrain[1], 2.0f);
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
		FPhysAnimBalanceStatelessTests,
		"PhysAnim.Bridge.BalanceStateless",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimBalanceStatelessTests::RunTest(const FString& Parameters)
	{
		auto GetDefaultSettings = []()
		{
			FPhysAnimStabilizationSettings S;
			S.MaxRootLinearSpeedCmPerSecond = 100.0f;
			S.MaxRootAngularSpeedDegPerSecond = 45.0f;
			S.BalanceEntryMaxGroundDistanceCm = 15.0f;
			S.BalancePhase2EntryMaxRootTiltDeg = 20.0f;
			S.BalancePhase2EntryMaxShellOffsetDelta = 5.0f;
			S.BalancePhase2EntryMaxShellVelocityDelta = 10.0f;
			S.BalancePhase2EntryMaxTargetDeltaDeg = 15.0f;
			return S;
		};

		auto GetStableDomain = []()
		{
			FPhysAnimStabilizationDomain D;
			D.bRootSimulating = true;
			D.RootLinearSpeed = 10.0f;
			D.RootAngularSpeed = 5.0f;
			D.RootGroundDistance = 5.0f;
			D.RootTiltDeg = 2.0f;
			D.ShellPlanarOffsetCm = 1.0f;
			D.ShellPlanarVelocityCmPerSec = 2.0f;
			D.MaxTargetDeltaDegrees = 3.0f;
			D.MeanTargetDeltaDegrees = 1.0f;
			return D;
		};

		FString Reason;
		TestTrue(TEXT("Baseline stable domain"), FPhysAnimBalanceReadyTransition::IsSnapshotReady(GetStableDomain(), GetDefaultSettings(), Reason));
		return true;
	}
}
