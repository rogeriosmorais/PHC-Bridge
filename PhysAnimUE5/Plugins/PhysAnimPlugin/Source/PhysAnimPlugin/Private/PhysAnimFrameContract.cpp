#include "PhysAnimFrameContract.h"

namespace PhysAnimFrameContract
{
	namespace
	{
		FQuat NormalizeFacing(const FQuat& Facing)
		{
			return Facing.GetNormalized();
		}

		FQuat ResolveActorToMeshRelativeFacing(const FWorldTrajectoryStateCm& State)
		{
			if (!State.bHasMesh)
			{
				return FQuat::Identity;
			}
			return (
				State.ActorFacing.Get().Inverse() *
				State.MeshFacing.Get()).GetNormalized();
		}

		FQuat ResolveAuthoredWorldRotation(const FWorldTrajectoryStateCm& State)
		{
			const FQuat ActorFacing = State.ActorFacing.Get();
			const FQuat ActorToMesh = ResolveActorToMeshRelativeFacing(State);
			return (
				ActorFacing *
				ActorToMesh.Inverse() *
				ActorFacing.Inverse()).GetNormalized();
		}

		FTransform TransformWorldToData(
			const FTransform& WorldTransform,
			const FTransform& SelectedWorldRoot,
			const FTransform& SelectedDataRoot)
		{
			const FQuat WorldToDataRotation = (
				SelectedDataRoot.GetRotation() *
				SelectedWorldRoot.GetRotation().Inverse()).GetNormalized();
			const FVector DataPosition =
				SelectedDataRoot.GetLocation() +
				WorldToDataRotation.RotateVector(
					WorldTransform.GetLocation() - SelectedWorldRoot.GetLocation());
			const FQuat DataRotation = (
				WorldToDataRotation * WorldTransform.GetRotation()).GetNormalized();
			return FTransform(DataRotation, DataPosition, WorldTransform.GetScale3D());
		}

		FTransform TransformDataToWorld(
			const FTransform& DataTransform,
			const FTransform& SelectedWorldRoot,
			const FTransform& SelectedDataRoot)
		{
			const FQuat DataToWorldRotation = (
				SelectedWorldRoot.GetRotation() *
				SelectedDataRoot.GetRotation().Inverse()).GetNormalized();
			const FVector WorldPosition =
				SelectedWorldRoot.GetLocation() +
				DataToWorldRotation.RotateVector(
					DataTransform.GetLocation() - SelectedDataRoot.GetLocation());
			const FQuat WorldRotation = (
				DataToWorldRotation * DataTransform.GetRotation()).GetNormalized();
			return FTransform(WorldRotation, WorldPosition, DataTransform.GetScale3D());
		}

		FTransform PlaceBodyOnRoot(
			const FTransform& RawBody,
			const FTransform& RawRoot,
			const FTransform& DesiredRoot)
		{
			const FQuat RawRootRotation = RawRoot.GetRotation().GetNormalized();
			const FQuat DesiredRootRotation = DesiredRoot.GetRotation().GetNormalized();
			const FVector LocalPosition = RawRootRotation.Inverse().RotateVector(
				RawBody.GetLocation() - RawRoot.GetLocation());
			const FQuat LocalRotation = (
				RawRootRotation.Inverse() * RawBody.GetRotation()).GetNormalized();
			return FTransform(
				(DesiredRootRotation * LocalRotation).GetNormalized(),
				DesiredRoot.GetLocation() + DesiredRootRotation.RotateVector(LocalPosition),
				RawBody.GetScale3D());
		}

		bool IsFiniteArray(const TArray<float>& Values)
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

		FString SelectAuthoredAsset(
			const FVector& LocalVelocity,
			const FReplayFixture& Fixture,
			FVector& OutAuthoredAxis,
			FVector& OutHalfSecondRootDelta)
		{
			const FVector LocalDirection = LocalVelocity.GetSafeNormal();
			const double ForwardScore = FVector::DotProduct(
				LocalDirection,
				Fixture.WalkForwardAuthoredAxis.GetSafeNormal());
			const double LeftScore = FVector::DotProduct(
				LocalDirection,
				Fixture.WalkLeftAuthoredAxis.GetSafeNormal());
			if (ForwardScore >= LeftScore)
			{
				OutAuthoredAxis = Fixture.WalkForwardAuthoredAxis;
				OutHalfSecondRootDelta = Fixture.WalkForwardRootDeltaCm;
				return TEXT("Walk_Fwd");
			}
			OutAuthoredAxis = Fixture.WalkLeftAuthoredAxis;
			OutHalfSecondRootDelta = Fixture.WalkLeftRootDeltaCm;
			return TEXT("Walk_Left");
		}

		FCanonicalAnimationDataPoseCm BuildSyntheticAnimationPose(
			const FVector& RootPositionCm,
			const FQuat& RootRotation,
			double FutureTimeSeconds)
		{
			FCanonicalAnimationDataPoseCm Pose;
			Pose.FutureTimeSeconds = FutureTimeSeconds;
			Pose.Root = FAnimationDataTransformCm(FTransform(RootRotation, RootPositionCm));
			Pose.Bodies.Reserve(PhysAnimBridge::NumSmplBodies);
			for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
			{
				const FVector LocalOffset(
					2.0 * static_cast<double>(BodyIndex),
					-1.0 * static_cast<double>(BodyIndex),
					90.0 + 0.5 * static_cast<double>(BodyIndex));
				Pose.Bodies.Add(FAnimationDataTransformCm(FTransform(
					RootRotation,
					RootPositionCm + RootRotation.RotateVector(LocalOffset))));
			}
			return Pose;
		}

		FCanonicalProtoBodyStateMeters BuildSyntheticCurrentBodyState(
			const FSelectedAnimationFrameCm& SelectedFrame)
		{
			FCanonicalProtoBodyStateMeters State;
			State.Bodies.Reserve(PhysAnimBridge::NumSmplBodies);
			const FTransform DataRoot = SelectedFrame.SelectedDataRoot.Get();
			for (int32 BodyIndex = 0; BodyIndex < PhysAnimBridge::NumSmplBodies; ++BodyIndex)
			{
				const FVector DataPositionCm =
					DataRoot.GetLocation() +
					FVector(
						2.0 * static_cast<double>(BodyIndex),
						-1.0 * static_cast<double>(BodyIndex),
						90.0 + 0.5 * static_cast<double>(BodyIndex));
				FPhysAnimBodySample Body;
				Body.Position = PhysAnimBridge::UeWorldPositionToProtoRuntime(DataPositionCm);
				Body.Rotation = PhysAnimBridge::UeWorldQuaternionToProtoRuntime(DataRoot.GetRotation());
				Body.LinearVelocity = FVector::ZeroVector;
				Body.AngularVelocity = FVector::ZeroVector;
				State.Bodies.Add(Body);
			}
			return State;
		}

		bool BuildTensorContracts(
			const FReplayFixture& Fixture,
			const FSelectedAnimationFrameCm& SelectedFrame,
			const FVector& AuthoredHalfSecondDeltaCm,
			EFutureRootSource RootSource)
		{
			const FCanonicalProtoBodyStateMeters Current = BuildSyntheticCurrentBodyState(SelectedFrame);
			FPolicyHeadingLocalObservation SelfObservation;
			FString Error;
			if (!CanonicalBodyToPolicyObservation(Current, 0.0f, SelfObservation, Error) ||
				SelfObservation.Values.Num() != PhysAnimBridge::SelfObsSize ||
				!IsFiniteArray(SelfObservation.Values))
			{
				return false;
			}

			const FQuat ActorToMesh = (
				Fixture.ActorWorldRoot.Get().GetRotation().Inverse() *
				Fixture.MeshWorldRoot.Get().GetRotation()).GetNormalized();
			const FQuat DesiredMeshFacing = (
				Fixture.DesiredActorFacing.Get() * ActorToMesh).GetNormalized();
			TArray<FCanonicalAnimationDataPoseCm> RawPoses;
			TArray<FWorldTransformCm> QueryRoots;
			RawPoses.Reserve(PhysAnimBridge::NumFutureSteps);
			QueryRoots.Reserve(PhysAnimBridge::NumFutureSteps);
			for (int32 FutureIndex = 0; FutureIndex < PhysAnimBridge::NumFutureSteps; ++FutureIndex)
			{
				const double TimeSeconds =
					static_cast<double>(FutureIndex + 1) * PhysAnimBridge::FutureStepSeconds;
				const double FractionOfHalfSecond = TimeSeconds / Fixture.HorizonSeconds;
				RawPoses.Add(BuildSyntheticAnimationPose(
					AuthoredHalfSecondDeltaCm * FractionOfHalfSecond,
					FQuat::Identity,
					TimeSeconds));
				const double TurnFraction = FMath::Clamp(
					TimeSeconds / Fixture.HorizonSeconds,
					0.0,
					1.0);
				const FQuat QueryFacing = FQuat::Slerp(
					Fixture.MeshWorldRoot.Get().GetRotation(),
					DesiredMeshFacing,
					TurnFraction).GetNormalized();
				QueryRoots.Add(FWorldTransformCm(FTransform(
					QueryFacing,
					Fixture.ActorWorldRoot.Get().GetLocation() +
					Fixture.RequestedWorldVelocity.Get() * TimeSeconds)));
			}

			FPolicyMimicTargetObservation MimicObservation;
			Error.Reset();
			return AnimationFutureToPolicyMimicTarget(
				Current,
				RawPoses,
				QueryRoots,
				SelectedFrame,
				RootSource,
				MimicObservation,
				Error) &&
				Error.IsEmpty() &&
				MimicObservation.Values.Num() == PhysAnimBridge::MimicTargetPosesSize &&
				IsFiniteArray(MimicObservation.Values);
		}

		void AddFailureIf(bool bFailed, const TCHAR* Criterion, FReplayCandidateResult& Result)
		{
			if (bFailed)
			{
				Result.FailedCriteria.Add(Criterion);
			}
		}
	}

	FPoseSearchQueryCm WorldTrajectoryToPoseSearchQuery(
		const FWorldTrajectoryStateCm& State,
		EQueryFrameContract Contract)
	{
		FPoseSearchQueryCm Query;
		Query.Position = State.Position;
		const FQuat ActorFacing = NormalizeFacing(State.ActorFacing.Get());
		const FQuat MeshFacing = State.bHasMesh
			? NormalizeFacing(State.MeshFacing.Get())
			: ActorFacing;
		const FQuat DesiredActorFacing = NormalizeFacing(State.DesiredActorFacing.Get());
		const FQuat ActorToMesh = ResolveActorToMeshRelativeFacing(State);
		const FQuat AuthoredWorldRotation = ResolveAuthoredWorldRotation(State);

		FVector QueryVelocity = State.Velocity.Get();
		FQuat CurrentFacing = ActorFacing;
		FQuat DesiredFacing = DesiredActorFacing;
		switch (Contract)
		{
		case EQueryFrameContract::E73_ActorFacing_WorldVelocity:
			break;
		case EQueryFrameContract::E74_ActorFacing_RotatedVelocity:
			QueryVelocity = AuthoredWorldRotation.RotateVector(QueryVelocity);
			break;
		case EQueryFrameContract::E75_RotatedFacing_RotatedVelocity:
			QueryVelocity = AuthoredWorldRotation.RotateVector(QueryVelocity);
			CurrentFacing = (AuthoredWorldRotation * ActorFacing).GetNormalized();
			DesiredFacing = (AuthoredWorldRotation * DesiredActorFacing).GetNormalized();
			break;
		case EQueryFrameContract::E77_MeshFacing_WorldVelocity:
			CurrentFacing = MeshFacing;
			DesiredFacing = State.bHasMesh
				? (DesiredActorFacing * ActorToMesh).GetNormalized()
				: DesiredActorFacing;
			break;
		default:
			break;
		}

		Query.Velocity = FWorldVelocityCmPerSecond(QueryVelocity);
		Query.CurrentFacing = FPoseSearchWorldFacing(CurrentFacing);
		Query.DesiredFacing = FPoseSearchWorldFacing(DesiredFacing);
		Query.LocalVelocityCmPerSecond = CurrentFacing.Inverse().RotateVector(QueryVelocity);
		return Query;
	}

	FAnimationDataTransformCm WorldTransformToAnimationData(
		const FWorldTransformCm& WorldTransform,
		const FSelectedAnimationFrameCm& SelectedFrame)
	{
		return FAnimationDataTransformCm(TransformWorldToData(
			WorldTransform.Get(),
			SelectedFrame.SelectedWorldRoot.Get(),
			SelectedFrame.SelectedDataRoot.Get()));
	}

	FWorldTransformCm AnimationDataTransformToWorld(
		const FAnimationDataTransformCm& DataTransform,
		const FSelectedAnimationFrameCm& SelectedFrame)
	{
		return FWorldTransformCm(TransformDataToWorld(
			DataTransform.Get(),
			SelectedFrame.SelectedWorldRoot.Get(),
			SelectedFrame.SelectedDataRoot.Get()));
	}

	FCanonicalAnimationDataPoseCm PlaceAnimationPoseOnTrajectory(
		const FCanonicalAnimationDataPoseCm& RawAnimationPose,
		const FWorldTransformCm& QueryTrajectoryRoot,
		const FSelectedAnimationFrameCm& SelectedFrame,
		EFutureRootSource RootSource)
	{
		if (RootSource == EFutureRootSource::RawAnimationRootMotion)
		{
			return RawAnimationPose;
		}

		FCanonicalAnimationDataPoseCm Result;
		Result.FutureTimeSeconds = RawAnimationPose.FutureTimeSeconds;
		Result.Root = WorldTransformToAnimationData(QueryTrajectoryRoot, SelectedFrame);
		Result.Bodies.Reserve(RawAnimationPose.Bodies.Num());
		for (const FAnimationDataTransformCm& RawBody : RawAnimationPose.Bodies)
		{
			Result.Bodies.Add(FAnimationDataTransformCm(PlaceBodyOnRoot(
				RawBody.Get(),
				RawAnimationPose.Root.Get(),
				Result.Root.Get())));
		}
		return Result;
	}

	FCanonicalProtoFuturePoseMeters AnimationPoseToCanonicalProtoPose(
		const FCanonicalAnimationDataPoseCm& AnimationPose)
	{
		FCanonicalProtoFuturePoseMeters Result;
		Result.Sample.FutureTimeSeconds = static_cast<float>(AnimationPose.FutureTimeSeconds);
		Result.Sample.BodyTransforms.Reserve(AnimationPose.Bodies.Num());
		for (const FAnimationDataTransformCm& Body : AnimationPose.Bodies)
		{
			Result.Sample.BodyTransforms.Add(FTransform(
				PhysAnimBridge::UeWorldQuaternionToProtoRuntime(Body.Get().GetRotation()),
				PhysAnimBridge::UeWorldPositionToProtoRuntime(Body.Get().GetLocation()),
				Body.Get().GetScale3D()));
		}
		return Result;
	}

	bool CanonicalBodyToPolicyObservation(
		const FCanonicalProtoBodyStateMeters& BodyState,
		float GroundHeightMeters,
		FPolicyHeadingLocalObservation& OutObservation,
		FString& OutError)
	{
		OutObservation.Values.Reset();
		return PhysAnimBridge::BuildSelfObservation(
			BodyState.Bodies,
			GroundHeightMeters,
			OutObservation.Values,
			OutError);
	}

	bool AnimationFutureToPolicyMimicTarget(
		const FCanonicalProtoBodyStateMeters& CurrentBodyState,
		const TArray<FCanonicalAnimationDataPoseCm>& RawAnimationFuturePoses,
		const TArray<FWorldTransformCm>& QueryTrajectoryRoots,
		const FSelectedAnimationFrameCm& SelectedFrame,
		EFutureRootSource RootSource,
		FPolicyMimicTargetObservation& OutObservation,
		FString& OutError)
	{
		OutObservation.Values.Reset();
		OutError.Reset();
		if (RawAnimationFuturePoses.Num() != PhysAnimBridge::NumFutureSteps ||
			QueryTrajectoryRoots.Num() != PhysAnimBridge::NumFutureSteps)
		{
			OutError = FString::Printf(
				TEXT("Expected %d typed future poses and query roots but found %d and %d."),
				PhysAnimBridge::NumFutureSteps,
				RawAnimationFuturePoses.Num(),
				QueryTrajectoryRoots.Num());
			return false;
		}

		TArray<FPhysAnimFuturePoseSample> FutureSamples;
		FutureSamples.Reserve(PhysAnimBridge::NumFutureSteps);
		for (int32 FutureIndex = 0; FutureIndex < PhysAnimBridge::NumFutureSteps; ++FutureIndex)
		{
			const FCanonicalAnimationDataPoseCm PlacedPose = PlaceAnimationPoseOnTrajectory(
				RawAnimationFuturePoses[FutureIndex],
				QueryTrajectoryRoots[FutureIndex],
				SelectedFrame,
				RootSource);
			FutureSamples.Add(AnimationPoseToCanonicalProtoPose(PlacedPose).Sample);
		}
		return PhysAnimBridge::BuildMimicTargetPoses(
			CurrentBodyState.Bodies,
			FutureSamples,
			OutObservation.Values,
			OutError);
	}

	TArray<FReplayCandidateResult> EvaluateReplayMatrix(const FReplayFixture& Fixture)
	{
		TArray<FReplayCandidateResult> Results;
		Results.Reserve(8);

		const FWorldTrajectoryStateCm State{
			FWorldPositionCm(Fixture.ActorWorldRoot.Get().GetLocation()),
			Fixture.RequestedWorldVelocity,
			FActorWorldFacing(Fixture.ActorWorldRoot.Get().GetRotation()),
			FMeshWorldFacing(Fixture.MeshWorldRoot.Get().GetRotation()),
			Fixture.DesiredActorFacing,
			true
		};
		const FQuat ActorToMesh = (
			Fixture.ActorWorldRoot.Get().GetRotation().Inverse() *
			Fixture.MeshWorldRoot.Get().GetRotation()).GetNormalized();
		const FQuat DesiredMeshFacing = (
			Fixture.DesiredActorFacing.Get() * ActorToMesh).GetNormalized();
		const FSelectedAnimationFrameCm SelectedFrame{
			Fixture.MeshWorldRoot,
			FAnimationDataTransformCm(FTransform::Identity)
		};
		const FVector RouteDirection = Fixture.RequestedWorldVelocity.Get().GetSafeNormal();
		const double RequestedSpeed = Fixture.RequestedWorldVelocity.Get().Size();

		for (const EQueryFrameContract QueryContract : {
			EQueryFrameContract::E73_ActorFacing_WorldVelocity,
			EQueryFrameContract::E74_ActorFacing_RotatedVelocity,
			EQueryFrameContract::E75_RotatedFacing_RotatedVelocity,
			EQueryFrameContract::E77_MeshFacing_WorldVelocity })
		{
			const FPoseSearchQueryCm Query = WorldTrajectoryToPoseSearchQuery(State, QueryContract);
			FVector SelectedAuthoredAxis = FVector::ZeroVector;
			FVector SelectedHalfSecondDelta = FVector::ZeroVector;
			const FString SelectedAsset = SelectAuthoredAsset(
				Query.LocalVelocityCmPerSecond,
				Fixture,
				SelectedAuthoredAxis,
				SelectedHalfSecondDelta);

			for (const EFutureRootSource RootSource : {
				EFutureRootSource::RawAnimationRootMotion,
				EFutureRootSource::QueryTrajectoryRootMotion })
			{
				FReplayCandidateResult Result;
				Result.QueryContract = QueryContract;
				Result.FutureRootSource = RootSource;
				Result.SelectedAsset = SelectedAsset;
				Result.AuthoredForwardAlignment = FVector::DotProduct(
					SelectedAuthoredAxis.GetSafeNormal(),
					Fixture.WalkForwardAuthoredAxis.GetSafeNormal());

				const FCanonicalAnimationDataPoseCm RawPose = BuildSyntheticAnimationPose(
					SelectedHalfSecondDelta,
					FQuat::Identity,
					Fixture.HorizonSeconds);
				const FWorldTransformCm QueryFutureRoot(FTransform(
					DesiredMeshFacing,
					Fixture.ActorWorldRoot.Get().GetLocation() +
					Fixture.RequestedWorldVelocity.Get() * Fixture.HorizonSeconds));
				const FCanonicalAnimationDataPoseCm PlacedPose = PlaceAnimationPoseOnTrajectory(
					RawPose,
					QueryFutureRoot,
					SelectedFrame,
					RootSource);
				const FWorldTransformCm PlacedWorldRoot = AnimationDataTransformToWorld(
					PlacedPose.Root,
					SelectedFrame);
				const FVector WorldDelta =
					PlacedWorldRoot.Get().GetLocation() -
					SelectedFrame.SelectedWorldRoot.Get().GetLocation();
				Result.RouteProjectionCm = FVector::DotProduct(WorldDelta, RouteDirection);
				Result.FutureRootSpeedCmPerSecond = WorldDelta.Size() / Fixture.HorizonSeconds;
				Result.SpeedRelativeError = RequestedSpeed > UE_DOUBLE_SMALL_NUMBER
					? FMath::Abs(Result.FutureRootSpeedCmPerSecond - RequestedSpeed) / RequestedSpeed
					: Result.FutureRootSpeedCmPerSecond;
				Result.TurnDeltaDegrees = FMath::FindDeltaAngleDegrees(
					SelectedFrame.SelectedWorldRoot.Get().Rotator().Yaw,
					PlacedWorldRoot.Get().Rotator().Yaw);
				Result.bWorldQueryVelocityPreserved =
					Query.Velocity.Get().Equals(Fixture.RequestedWorldVelocity.Get(), 1.0e-6);

				const FWorldTrajectoryStateCm ZeroState{
					State.Position,
					FWorldVelocityCmPerSecond(FVector::ZeroVector),
					State.ActorFacing,
					State.MeshFacing,
					State.DesiredActorFacing,
					State.bHasMesh
				};
				const FPoseSearchQueryCm ZeroQuery = WorldTrajectoryToPoseSearchQuery(ZeroState, QueryContract);
				Result.bZeroVelocityExact =
					ZeroQuery.Velocity.Get() == FVector::ZeroVector &&
					ZeroQuery.LocalVelocityCmPerSecond == FVector::ZeroVector;

				const FWorldTransformCm RoundTrip = AnimationDataTransformToWorld(
					WorldTransformToAnimationData(QueryFutureRoot, SelectedFrame),
					SelectedFrame);
				Result.bRoundTripExact =
					RoundTrip.Get().GetLocation().Equals(QueryFutureRoot.Get().GetLocation(), 1.0e-4) &&
					RoundTrip.Get().GetRotation().AngularDistance(QueryFutureRoot.Get().GetRotation()) <= 1.0e-5;
				const FAnimationDataTransformCm CurrentDataRoot = WorldTransformToAnimationData(
					SelectedFrame.SelectedWorldRoot,
					SelectedFrame);
				Result.bCurrentFutureFrameConsistent =
					CurrentDataRoot.Get().GetLocation().Equals(
						SelectedFrame.SelectedDataRoot.Get().GetLocation(),
						1.0e-5) &&
					CurrentDataRoot.Get().GetRotation().AngularDistance(
						SelectedFrame.SelectedDataRoot.Get().GetRotation()) <= 1.0e-5 &&
					Result.bRoundTripExact;
				Result.bTensorContractsValid = BuildTensorContracts(
					Fixture,
					SelectedFrame,
					SelectedHalfSecondDelta,
					RootSource);

				AddFailureIf(!Result.bWorldQueryVelocityPreserved, TEXT("world_query_velocity_preserved"), Result);
				AddFailureIf(Result.SelectedAsset != TEXT("Walk_Fwd"), TEXT("authored_forward_selection"), Result);
				AddFailureIf(Result.AuthoredForwardAlignment < 0.999, TEXT("authored_forward_alignment"), Result);
				AddFailureIf(Result.RouteProjectionCm <= 0.0, TEXT("positive_route_projection"), Result);
				AddFailureIf(Result.SpeedRelativeError > 0.05, TEXT("requested_speed_match"), Result);
				AddFailureIf(Result.TurnDeltaDegrees <= 0.0 ||
					FMath::Abs(Result.TurnDeltaDegrees - Fixture.RequestedTurnDegrees) > 0.1,
					TEXT("turn_sign_and_magnitude"), Result);
				AddFailureIf(!Result.bZeroVelocityExact, TEXT("zero_velocity_exact"), Result);
				AddFailureIf(!Result.bRoundTripExact, TEXT("world_data_roundtrip"), Result);
				AddFailureIf(!Result.bCurrentFutureFrameConsistent, TEXT("current_future_frame_consistency"), Result);
				AddFailureIf(!Result.bTensorContractsValid, TEXT("policy_tensor_contracts"), Result);
				Result.bPass = Result.FailedCriteria.IsEmpty();
				Results.Add(MoveTemp(Result));
			}
		}
		return Results;
	}

	FString QueryContractName(EQueryFrameContract Contract)
	{
		switch (Contract)
		{
		case EQueryFrameContract::E73_ActorFacing_WorldVelocity:
			return TEXT("E73_ActorFacing_WorldVelocity");
		case EQueryFrameContract::E74_ActorFacing_RotatedVelocity:
			return TEXT("E74_ActorFacing_RotatedVelocity");
		case EQueryFrameContract::E75_RotatedFacing_RotatedVelocity:
			return TEXT("E75_RotatedFacing_RotatedVelocity");
		case EQueryFrameContract::E77_MeshFacing_WorldVelocity:
			return TEXT("E77_MeshFacing_WorldVelocity");
		default:
			return TEXT("UnknownQueryContract");
		}
	}

	FString FutureRootSourceName(EFutureRootSource Source)
	{
		switch (Source)
		{
		case EFutureRootSource::RawAnimationRootMotion:
			return TEXT("RawAnimationRootMotion");
		case EFutureRootSource::QueryTrajectoryRootMotion:
			return TEXT("QueryTrajectoryRootMotion");
		default:
			return TEXT("UnknownFutureRootSource");
		}
	}
}
