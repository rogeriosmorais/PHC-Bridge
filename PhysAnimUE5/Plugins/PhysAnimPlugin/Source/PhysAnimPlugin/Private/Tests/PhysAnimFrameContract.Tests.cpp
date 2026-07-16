#include "PhysAnimFrameContract.h"

#include "Misc/AutomationTest.h"
#include "Templates/IsConstructible.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using namespace PhysAnimFrameContract;

	FReplayFixture MakeLockedReplayFixture()
	{
		FReplayFixture Fixture;
		Fixture.ActorWorldRoot = FWorldTransformCm(FTransform(
			FQuat::Identity,
			FVector::ZeroVector));
		Fixture.MeshWorldRoot = FWorldTransformCm(FTransform(
			FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.0)),
			FVector::ZeroVector));
		Fixture.RequestedWorldVelocity = FWorldVelocityCmPerSecond(FVector(160.0, 0.0, 0.0));
		Fixture.DesiredActorFacing = FActorWorldFacing(
			FQuat(FVector::UpVector, FMath::DegreesToRadians(30.0)));
		return Fixture;
	}

	bool AllFinite(const TArray<float>& Values)
	{
		for (const float Value : Values)
		{
			if (!FMath::IsFinite(Value))
			{
				return false;
			}
		}
		return true;
	}
}

static_assert(!TIsConstructible<FWorldPositionCm, FWorldVelocityCmPerSecond>::Value,
	"Position and velocity frame types must not be interchangeable.");
static_assert(!TIsConstructible<FWorldTransformCm, FAnimationDataTransformCm>::Value,
	"World and animation-data transforms must not be interchangeable.");
static_assert(!TIsConstructible<FActorWorldFacing, FMeshWorldFacing>::Value,
	"Actor and mesh facing types must not be interchangeable.");

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimTypedFrameRoundTripTest,
	"PhysAnim.Frames.TypedRoundTrips",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimTypedFrameRoundTripTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimFrameContract;

	const FSelectedAnimationFrameCm Frame{
		FWorldTransformCm(FTransform(
			FQuat(FVector::UpVector, FMath::DegreesToRadians(-60.0)),
			FVector(120.0, -45.0, 10.0))),
		FAnimationDataTransformCm(FTransform(
			FQuat(FVector::UpVector, FMath::DegreesToRadians(15.0)),
			FVector(25.0, 30.0, 2.0)))
	};
	const FWorldTransformCm InputWorld(FTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(25.0)),
		FVector(170.0, -12.0, 24.0)));
	const FAnimationDataTransformCm Data = WorldTransformToAnimationData(InputWorld, Frame);
	const FWorldTransformCm RecoveredWorld = AnimationDataTransformToWorld(Data, Frame);

	TestTrue(
		TEXT("World-data-world position roundtrip is exact"),
		RecoveredWorld.Get().GetLocation().Equals(InputWorld.Get().GetLocation(), 1.0e-4));
	TestTrue(
		TEXT("World-data-world rotation roundtrip is exact"),
		RecoveredWorld.Get().GetRotation().AngularDistance(InputWorld.Get().GetRotation()) <= 1.0e-5);

	const FWorldTrajectoryStateCm ZeroState{
		FWorldPositionCm(FVector::ZeroVector),
		FWorldVelocityCmPerSecond(FVector::ZeroVector),
		FActorWorldFacing(FQuat::Identity),
		FMeshWorldFacing(FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.0))),
		FActorWorldFacing(FQuat::Identity),
		true
	};
	for (const EQueryFrameContract Contract : {
		EQueryFrameContract::E73_ActorFacing_WorldVelocity,
		EQueryFrameContract::E74_ActorFacing_RotatedVelocity,
		EQueryFrameContract::E75_RotatedFacing_RotatedVelocity,
		EQueryFrameContract::E77_MeshFacing_WorldVelocity })
	{
		const FPoseSearchQueryCm Query = WorldTrajectoryToPoseSearchQuery(ZeroState, Contract);
		const FString ZeroMessage = FString::Printf(
			TEXT("%s preserves exact zero velocity"),
			*QueryContractName(Contract));
		TestTrue(
			*ZeroMessage,
			Query.Velocity.Get() == FVector::ZeroVector &&
			Query.LocalVelocityCmPerSecond == FVector::ZeroVector);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimFrameReplayMatrixTest,
	"PhysAnim.Frames.LocomotionReplayMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimFrameReplayMatrixTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimFrameContract;

	const TArray<FReplayCandidateResult> Results = EvaluateReplayMatrix(MakeLockedReplayFixture());
	TestEqual(TEXT("E79 evaluates all four query contracts against two future-root sources"), Results.Num(), 8);

	int32 PassingCount = 0;
	const FReplayCandidateResult* Survivor = nullptr;
	for (const FReplayCandidateResult& Result : Results)
	{
		AddInfo(FString::Printf(
			TEXT("E79_MATRIX query=%s root=%s asset=%s forward=%.3f route=%.3f speed=%.3f speed_error=%.3f turn=%.3f pass=%d failures=%s"),
			*QueryContractName(Result.QueryContract),
			*FutureRootSourceName(Result.FutureRootSource),
			*Result.SelectedAsset,
			Result.AuthoredForwardAlignment,
			Result.RouteProjectionCm,
			Result.FutureRootSpeedCmPerSecond,
			Result.SpeedRelativeError,
			Result.TurnDeltaDegrees,
			Result.bPass ? 1 : 0,
			*FString::Join(Result.FailedCriteria, TEXT(","))));
		if (Result.bPass)
		{
			++PassingCount;
			Survivor = &Result;
		}
		TestTrue(
			FString::Printf(
				TEXT("%s/%s always preserves zero velocity"),
				*QueryContractName(Result.QueryContract),
				*FutureRootSourceName(Result.FutureRootSource)),
			Result.bZeroVelocityExact);
		TestTrue(
			FString::Printf(
				TEXT("%s/%s always preserves world-data roundtrip"),
				*QueryContractName(Result.QueryContract),
				*FutureRootSourceName(Result.FutureRootSource)),
			Result.bRoundTripExact);
	}
	TestEqual(TEXT("E79 has exactly one algebraic survivor"), PassingCount, 1);
	TestNotNull(TEXT("E79 survivor exists"), Survivor);
	if (Survivor)
	{
		TestEqual(
			TEXT("E79 survivor uses mesh-facing/world-velocity query"),
			static_cast<uint8>(Survivor->QueryContract),
			static_cast<uint8>(EQueryFrameContract::E77_MeshFacing_WorldVelocity));
		TestEqual(
			TEXT("E79 survivor uses query-trajectory future root"),
			static_cast<uint8>(Survivor->FutureRootSource),
			static_cast<uint8>(EFutureRootSource::QueryTrajectoryRootMotion));
		TestEqual(TEXT("E79 survivor selects authored forward"), Survivor->SelectedAsset, FString(TEXT("Walk_Fwd")));
		TestTrue(TEXT("E79 survivor root progression follows the shell route"), Survivor->RouteProjectionCm > 0.0);
		TestTrue(TEXT("E79 survivor speed is within five percent"), Survivor->SpeedRelativeError <= 0.05);
		TestTrue(TEXT("E79 survivor preserves positive turn sign"), Survivor->TurnDeltaDegrees > 0.0);
		TestTrue(TEXT("E79 survivor current/future frames are consistent"), Survivor->bCurrentFutureFrameConsistent);
		TestTrue(TEXT("E79 survivor builds locked policy tensors"), Survivor->bTensorContractsValid);
	}

	const FReplayCandidateResult* RawE77 = Results.FindByPredicate([](const FReplayCandidateResult& Result)
	{
		return Result.QueryContract == EQueryFrameContract::E77_MeshFacing_WorldVelocity &&
			Result.FutureRootSource == EFutureRootSource::RawAnimationRootMotion;
	});
	TestNotNull(TEXT("E79 contains the current E77 raw-root contract"), RawE77);
	if (RawE77)
	{
		TestEqual(TEXT("Current E77 selects authored forward"), RawE77->SelectedAsset, FString(TEXT("Walk_Fwd")));
		TestTrue(TEXT("Current E77 fails requested-speed matching"), RawE77->SpeedRelativeError > 0.5);
		TestFalse(TEXT("Current E77 cannot survive the replay matrix"), RawE77->bPass);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimTypedPolicyTensorTest,
	"PhysAnim.Frames.TypedPolicyTensors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimTypedPolicyTensorTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimFrameContract;

	FCanonicalProtoBodyStateMeters Current;
	Current.Bodies.Reserve(PhysAnimBridge::NumSmplBodies);
	for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		FPhysAnimBodySample Body;
		Body.Position = FVector(0.02 * BodyIndex, -0.01 * BodyIndex, 0.9 + 0.005 * BodyIndex);
		Body.Rotation = FQuat::Identity;
		Body.LinearVelocity = FVector(0.1, 0.0, 0.0);
		Body.AngularVelocity = FVector::ZeroVector;
		Current.Bodies.Add(Body);
	}

	FPolicyHeadingLocalObservation SelfObservation;
	FString Error;
	TestTrue(
		TEXT("Typed canonical body state builds policy self observation"),
		CanonicalBodyToPolicyObservation(Current, 0.0f, SelfObservation, Error));
	TestTrue(TEXT("Typed self-observation error is empty"), Error.IsEmpty());
	TestEqual(TEXT("Typed self observation has locked width"), SelfObservation.Values.Num(), PhysAnimBridge::SelfObsSize);
	TestTrue(TEXT("Typed self observation is finite"), AllFinite(SelfObservation.Values));

	const FSelectedAnimationFrameCm SelectedFrame{
		FWorldTransformCm(FTransform(
			FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.0)),
			FVector::ZeroVector)),
		FAnimationDataTransformCm(FTransform::Identity)
	};
	TArray<FCanonicalAnimationDataPoseCm> RawFuturePoses;
	TArray<FWorldTransformCm> QueryRoots;
	for (int32 FutureIndex = 0; FutureIndex < PhysAnimBridge::NumFutureSteps; ++FutureIndex)
	{
		const double TimeSeconds = static_cast<double>(FutureIndex + 1) * PhysAnimBridge::FutureStepSeconds;
		FCanonicalAnimationDataPoseCm Pose;
		Pose.FutureTimeSeconds = TimeSeconds;
		Pose.Root = FAnimationDataTransformCm(FTransform(
			FQuat::Identity,
			FVector(0.0, 300.0 * TimeSeconds, 90.0)));
		Pose.Bodies.Reserve(PhysAnimBridge::NumSmplBodies);
		for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
		{
			Pose.Bodies.Add(FAnimationDataTransformCm(FTransform(
				FQuat::Identity,
				Pose.Root.Get().GetLocation() + FVector(2.0 * BodyIndex, -BodyIndex, 0.5 * BodyIndex))));
		}
		RawFuturePoses.Add(MoveTemp(Pose));
		QueryRoots.Add(FWorldTransformCm(FTransform(
			FQuat::Identity,
			FVector(160.0 * TimeSeconds, 0.0, 90.0))));
	}

	FPolicyMimicTargetObservation MimicObservation;
	Error.Reset();
	TestTrue(
		TEXT("Typed animation future builds policy mimic observation"),
		AnimationFutureToPolicyMimicTarget(
			Current,
			RawFuturePoses,
			QueryRoots,
			SelectedFrame,
			EFutureRootSource::QueryTrajectoryRootMotion,
			MimicObservation,
			Error));
	TestTrue(TEXT("Typed mimic error is empty"), Error.IsEmpty());
	TestEqual(TEXT("Typed mimic observation has locked width"), MimicObservation.Values.Num(), PhysAnimBridge::MimicTargetPosesSize);
	TestTrue(TEXT("Typed mimic observation is finite"), AllFinite(MimicObservation.Values));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimProtoFutureTrajectoryPlacementTest,
	"PhysAnim.Frames.ProtoFutureTrajectoryPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimProtoFutureTrajectoryPlacementTest::RunTest(const FString& Parameters)
{
	using namespace PhysAnimFrameContract;

	const FWorldTransformCm CurrentQueryRoot(FTransform(
		FQuat(FVector::UpVector, FMath::DegreesToRadians(-90.0)),
		FVector(0.0, 0.0, 90.0)));
	const FTransform CurrentSelectedWorldTransform(
		PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CurrentQueryRoot.Get().GetRotation()),
		PhysAnimBridge::UeWorldPositionToProtoRuntime(CurrentQueryRoot.Get().GetLocation()));
	const FTransform CurrentSelectedDataTransform = FTransform::Identity;
	const FProtoWorldCanonicalTransformMeters CurrentSelectedWorld(CurrentSelectedWorldTransform);
	const FProtoAnimationDataCanonicalTransformMeters CurrentSelectedData(CurrentSelectedDataTransform);

	TArray<FPhysAnimFuturePoseSample> RawFuturePoses;
	TArray<FWorldTransformCm> FutureQueryRoots;
	RawFuturePoses.Reserve(PhysAnimBridge::NumFutureSteps);
	FutureQueryRoots.Reserve(PhysAnimBridge::NumFutureSteps);
	for (int32 FutureIndex = 0; FutureIndex < PhysAnimBridge::NumFutureSteps; ++FutureIndex)
	{
		const double TimeSeconds =
			static_cast<double>(FutureIndex + 1) * PhysAnimBridge::FutureStepSeconds;
		const double Alpha = TimeSeconds / 0.5;
		FPhysAnimFuturePoseSample RawPose;
		RawPose.FutureTimeSeconds = static_cast<float>(TimeSeconds);
		const FTransform RawRoot(
			FQuat::Identity,
			FVector(0.0, 3.0 * TimeSeconds, 0.9));
		RawPose.BodyTransforms.Reserve(PhysAnimBridge::NumSmplBodies);
		for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
		{
			const FVector LocalOffset(
				0.02 * BodyIndex,
				-0.01 * BodyIndex,
				0.005 * BodyIndex);
			const FQuat LocalRotation(
				FVector::ForwardVector,
				FMath::DegreesToRadians(0.25 * BodyIndex));
			RawPose.BodyTransforms.Add(FTransform(
				(RawRoot.GetRotation() * LocalRotation).GetNormalized(),
				RawRoot.GetLocation() + RawRoot.GetRotation().RotateVector(LocalOffset)));
		}
		RawFuturePoses.Add(MoveTemp(RawPose));

		FutureQueryRoots.Add(FWorldTransformCm(FTransform(
			FQuat::Slerp(
				CurrentQueryRoot.Get().GetRotation(),
				FQuat(FVector::UpVector, FMath::DegreesToRadians(-60.0)),
				Alpha).GetNormalized(),
			FVector(160.0 * TimeSeconds, 0.0, 90.0))));
	}

	TArray<FPhysAnimFuturePoseSample> PlacedFuturePoses;
	FString Error;
	TestTrue(
		TEXT("E80 typed placement accepts the locked fifteen-step trajectory"),
		PlaceProtoFuturePosesOnWorldTrajectory(
			RawFuturePoses,
			CurrentQueryRoot,
			FutureQueryRoots,
			CurrentSelectedWorld,
			CurrentSelectedData,
			PlacedFuturePoses,
			Error));
	TestTrue(TEXT("E80 typed placement error is empty"), Error.IsEmpty());
	TestEqual(
		TEXT("E80 typed placement preserves future sample count"),
		PlacedFuturePoses.Num(),
		PhysAnimBridge::NumFutureSteps);

	const FTransform& RawFinalRoot = RawFuturePoses.Last().BodyTransforms[0];
	const FTransform& PlacedFinalRoot = PlacedFuturePoses.Last().BodyTransforms[0];
	const FQuat WorldToDataRotation = (
		CurrentSelectedDataTransform.GetRotation() *
		CurrentSelectedWorldTransform.GetRotation().Inverse()).GetNormalized();
	const FQuat DataToWorldRotation = WorldToDataRotation.Inverse();
	const FVector PlacedFinalWorldPosition =
		CurrentSelectedWorldTransform.GetLocation() +
		DataToWorldRotation.RotateVector(
			PlacedFinalRoot.GetLocation() - CurrentSelectedDataTransform.GetLocation());
	const FQuat PlacedFinalWorldRotation = (
		DataToWorldRotation * PlacedFinalRoot.GetRotation()).GetNormalized();
	const FVector ExpectedFinalWorldPosition =
		CurrentSelectedWorldTransform.GetLocation() +
		PhysAnimBridge::UeWorldPositionToProtoRuntime(FVector(80.0, 0.0, 0.0));
	const FQuat ExpectedFinalWorldRotation = (
		PhysAnimBridge::UeWorldQuaternionToProtoRuntime(
			FutureQueryRoots.Last().Get().GetRotation() *
			CurrentQueryRoot.Get().GetRotation().Inverse()) *
		CurrentSelectedWorldTransform.GetRotation()).GetNormalized();
	TestTrue(
		TEXT("E80 final query root replaces raw 1.5 m animation progression with the 0.8 m trajectory progression"),
		PlacedFinalWorldPosition.Equals(ExpectedFinalWorldPosition, 1.0e-5));
	TestTrue(
		TEXT("E80 final query root carries the requested thirty-degree turn"),
		PlacedFinalWorldRotation.AngularDistance(ExpectedFinalWorldRotation) <= 1.0e-5);
	TestTrue(
		TEXT("E80 raw animation root was materially different from the placed root"),
		!RawFinalRoot.GetLocation().Equals(PlacedFinalRoot.GetLocation(), 0.1));

	for (int32 BodyIndex = 1; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
	{
		const FTransform RawRelative =
			RawFuturePoses.Last().BodyTransforms[BodyIndex].GetRelativeTransform(RawFinalRoot);
		const FTransform PlacedRelative =
			PlacedFuturePoses.Last().BodyTransforms[BodyIndex].GetRelativeTransform(PlacedFinalRoot);
		TestTrue(
			*FString::Printf(TEXT("E80 body %d preserves animation-relative position"), BodyIndex),
			PlacedRelative.GetLocation().Equals(RawRelative.GetLocation(), 1.0e-5));
		TestTrue(
			*FString::Printf(TEXT("E80 body %d preserves animation-relative rotation"), BodyIndex),
			PlacedRelative.GetRotation().AngularDistance(RawRelative.GetRotation()) <= 1.0e-5);
	}

	TArray<FWorldTransformCm> ZeroQueryRoots;
	ZeroQueryRoots.Init(CurrentQueryRoot, PhysAnimBridge::NumFutureSteps);
	TArray<FPhysAnimFuturePoseSample> ZeroPlacedPoses;
	Error.Reset();
	TestTrue(
		TEXT("E80 zero trajectory placement succeeds"),
		PlaceProtoFuturePosesOnWorldTrajectory(
			RawFuturePoses,
			CurrentQueryRoot,
			ZeroQueryRoots,
			CurrentSelectedWorld,
			CurrentSelectedData,
			ZeroPlacedPoses,
			Error));
	TestTrue(
		TEXT("E80 zero trajectory preserves the current selected data root"),
		ZeroPlacedPoses.Last().BodyTransforms[0].GetLocation().Equals(
			CurrentSelectedDataTransform.GetLocation(),
			1.0e-6) &&
		ZeroPlacedPoses.Last().BodyTransforms[0].GetRotation().AngularDistance(
			CurrentSelectedDataTransform.GetRotation()) <= 1.0e-6);

	TArray<FWorldTransformCm> MalformedRoots = FutureQueryRoots;
	MalformedRoots.Pop();
	TArray<FPhysAnimFuturePoseSample> RejectedOutput;
	Error.Reset();
	TestFalse(
		TEXT("E80 rejects a mismatched trajectory sample count"),
		PlaceProtoFuturePosesOnWorldTrajectory(
			RawFuturePoses,
			CurrentQueryRoot,
			MalformedRoots,
			CurrentSelectedWorld,
			CurrentSelectedData,
			RejectedOutput,
			Error));
	TestTrue(TEXT("E80 rejected output remains empty"), RejectedOutput.IsEmpty());
	TestTrue(TEXT("E80 count rejection is explicit"), Error.Contains(TEXT("count")));
	return true;
}

#endif
