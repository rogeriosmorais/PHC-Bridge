#include "PhysAnimLocomotionFrameAdapter.h"

#include "Misc/Crc.h"

namespace PhysAnimLocomotionFrameAdapter
{
	namespace
	{
		bool Fail(FString& OutError, const FString& Message)
		{
			OutError = Message;
			return false;
		}

		bool IsFiniteTransform(const FTransform& Transform)
		{
			return !Transform.ContainsNaN() && Transform.GetRotation().IsNormalized();
		}

		bool IsFiniteBodySample(const FPhysAnimBodySample& Body)
		{
			return !Body.Position.ContainsNaN() &&
				!Body.Rotation.ContainsNaN() &&
				Body.Rotation.IsNormalized() &&
				!Body.LinearVelocity.ContainsNaN() &&
				!Body.AngularVelocity.ContainsNaN();
		}

		bool IsFiniteFloatArray(const TArray<float>& Values)
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

		FTransform ResolveFacingToWorld(
			const FPhysAnimActorWorldFrame& ActorFrame,
			const FPhysAnimMeshWorldFrame& MeshFrame,
			EPhysAnimPoseSearchFacingBasis FacingBasis)
		{
			return FacingBasis == EPhysAnimPoseSearchFacingBasis::MeshWorld
				? MeshFrame.MeshToWorld
				: ActorFrame.ActorToWorld;
		}

		FTransform ResolveAnimationReferenceToWorld(
			const FPhysAnimLocomotionReplayRecord& Replay,
			EPhysAnimAnimationReferenceBasis Basis)
		{
			return Basis == EPhysAnimAnimationReferenceBasis::MeshWorld
				? Replay.MeshFrame.MeshToWorld
				: Replay.ActorFrame.ActorToWorld;
		}

		void AddFailure(
			FPhysAnimFrameCandidateResult& Result,
			bool bFailed,
			const TCHAR* Message)
		{
			if (bFailed)
			{
				Result.FailedInvariants.Add(Message);
			}
		}
	}

	bool WorldTrajectoryToPoseSearchQuery(
		const FPhysAnimWorldTrajectoryState& WorldTrajectory,
		const FPhysAnimActorWorldFrame& ActorFrame,
		const FPhysAnimMeshWorldFrame& MeshFrame,
		EPhysAnimPoseSearchFacingBasis FacingBasis,
		FPhysAnimPoseSearchQueryState& OutQuery,
		FString& OutError)
	{
		OutQuery = FPhysAnimPoseSearchQueryState();
		OutError.Reset();
		if (WorldTrajectory.Samples.IsEmpty())
		{
			return Fail(OutError, TEXT("World trajectory has no samples."));
		}
		if (!IsFiniteTransform(ActorFrame.ActorToWorld) || !IsFiniteTransform(MeshFrame.MeshToWorld))
		{
			return Fail(OutError, TEXT("Actor or mesh frame is not finite and normalized."));
		}

		OutQuery.FacingBasis = FacingBasis;
		OutQuery.FacingToWorld = ResolveFacingToWorld(ActorFrame, MeshFrame, FacingBasis);
		const FTransform WorldToFacing = OutQuery.FacingToWorld.Inverse();
		OutQuery.Samples.Reserve(WorldTrajectory.Samples.Num());
		for (const FPhysAnimWorldTrajectorySample& WorldSample : WorldTrajectory.Samples)
		{
			if (!FMath::IsFinite(WorldSample.TimeSeconds) ||
				WorldSample.WorldPositionCm.ContainsNaN() ||
				WorldSample.WorldFacing.ContainsNaN() ||
				!WorldSample.WorldFacing.IsNormalized() ||
				WorldSample.WorldVelocityCmPerSecond.ContainsNaN())
			{
				OutQuery = FPhysAnimPoseSearchQueryState();
				return Fail(OutError, TEXT("World trajectory contains a malformed sample."));
			}
			FPhysAnimPoseSearchFacingTrajectorySample QuerySample;
			QuerySample.TimeSeconds = WorldSample.TimeSeconds;
			QuerySample.FacingLocalPositionCm = WorldToFacing.TransformPosition(WorldSample.WorldPositionCm);
			QuerySample.FacingLocalRotation = (
				OutQuery.FacingToWorld.GetRotation().Inverse() *
				WorldSample.WorldFacing).GetNormalized();
			QuerySample.FacingLocalVelocityCmPerSecond =
				WorldToFacing.TransformVectorNoScale(WorldSample.WorldVelocityCmPerSecond);
			OutQuery.Samples.Add(QuerySample);
		}
		return true;
	}

	bool AnimationPoseToCanonicalProtoPose(
		const FPhysAnimAnimationDataPose& AnimationPose,
		const FTransform& AnimationReferenceToWorld,
		FPhysAnimProtoCanonicalBodyState& OutCanonicalState,
		FString& OutError)
	{
		OutCanonicalState.ProtoRuntimeBodySamplesMeters.Reset();
		OutError.Reset();
		if (!IsFiniteTransform(AnimationReferenceToWorld))
		{
			return Fail(OutError, TEXT("Animation reference frame is malformed."));
		}
		if (AnimationPose.AnimationBodySamplesUeWorldCm.Num() != PhysAnimBridge::NumSmplBodies)
		{
			return Fail(OutError, FString::Printf(
				TEXT("Animation pose requires %d body samples but found %d."),
				PhysAnimBridge::NumSmplBodies,
				AnimationPose.AnimationBodySamplesUeWorldCm.Num()));
		}
		const FTransform WorldToReference = AnimationReferenceToWorld.Inverse();
		OutCanonicalState.ProtoRuntimeBodySamplesMeters.Reserve(PhysAnimBridge::NumSmplBodies);
		for (const FPhysAnimBodySample& WorldBody : AnimationPose.AnimationBodySamplesUeWorldCm)
		{
			if (!IsFiniteBodySample(WorldBody))
			{
				OutCanonicalState.ProtoRuntimeBodySamplesMeters.Reset();
				return Fail(OutError, TEXT("Animation pose contains a malformed body sample."));
			}
			FPhysAnimBodySample ProtoBody;
			ProtoBody.Position = PhysAnimBridge::UeWorldPositionToProtoRuntime(
				WorldToReference.TransformPosition(WorldBody.Position));
			ProtoBody.Rotation = PhysAnimBridge::UeWorldQuaternionToProtoRuntime((
				AnimationReferenceToWorld.GetRotation().Inverse() *
				WorldBody.Rotation).GetNormalized());
			ProtoBody.LinearVelocity = PhysAnimBridge::UeWorldVelocityToProtoRuntime(
				WorldToReference.TransformVectorNoScale(WorldBody.LinearVelocity));
			ProtoBody.AngularVelocity = PhysAnimBridge::UeWorldRotationVectorToProtoRuntime(
				WorldToReference.TransformVectorNoScale(WorldBody.AngularVelocity));
			OutCanonicalState.ProtoRuntimeBodySamplesMeters.Add(ProtoBody);
		}
		return true;
	}

	bool CanonicalBodyToPolicyObservation(
		const FPhysAnimProtoCanonicalBodyState& CanonicalState,
		float ProtoGroundHeightMeters,
		FPhysAnimPolicyHeadingLocalObservation& OutObservation,
		FString& OutError)
	{
		OutObservation.Values.Reset();
		return PhysAnimBridge::BuildSelfObservation(
			CanonicalState.ProtoRuntimeBodySamplesMeters,
			ProtoGroundHeightMeters,
			OutObservation.Values,
			OutError);
	}

	bool AnimationFutureToPolicyMimicTarget(
		const FPhysAnimProtoCanonicalBodyState& CurrentCanonicalState,
		const TArray<FPhysAnimAnimationDataPose>& FutureAnimationPoses,
		const FTransform& AnimationReferenceToWorld,
		FPhysAnimPolicyMimicTarget& OutMimicTarget,
		FString& OutError)
	{
		OutMimicTarget.Values.Reset();
		OutError.Reset();
		if (FutureAnimationPoses.Num() != PhysAnimBridge::NumFutureSteps)
		{
			return Fail(OutError, FString::Printf(
				TEXT("Expected %d future animation poses but found %d."),
				PhysAnimBridge::NumFutureSteps,
				FutureAnimationPoses.Num()));
		}

		TArray<FPhysAnimFuturePoseSample> FutureSamples;
		FutureSamples.Reserve(PhysAnimBridge::NumFutureSteps);
		for (const FPhysAnimAnimationDataPose& AnimationPose : FutureAnimationPoses)
		{
			FPhysAnimProtoCanonicalBodyState CanonicalPose;
			if (!AnimationPoseToCanonicalProtoPose(
				AnimationPose,
				AnimationReferenceToWorld,
				CanonicalPose,
				OutError))
			{
				OutMimicTarget.Values.Reset();
				return false;
			}
			FPhysAnimFuturePoseSample FutureSample;
			FutureSample.FutureTimeSeconds = AnimationPose.SampleTimeSeconds;
			FutureSample.BodyTransforms.Reserve(PhysAnimBridge::NumSmplBodies);
			for (const FPhysAnimBodySample& Body : CanonicalPose.ProtoRuntimeBodySamplesMeters)
			{
				FutureSample.BodyTransforms.Add(FTransform(Body.Rotation, Body.Position));
			}
			FutureSamples.Add(MoveTemp(FutureSample));
		}
		return PhysAnimBridge::BuildMimicTargetPoses(
			CurrentCanonicalState.ProtoRuntimeBodySamplesMeters,
			FutureSamples,
			OutMimicTarget.Values,
			OutError);
	}

	FPhysAnimPolicyActionSignature BuildPolicyActionSignature(TConstArrayView<float> PolicyActions)
	{
		FPhysAnimPolicyActionSignature Signature;
		Signature.ValueCount = PolicyActions.Num();
		Signature.StableCrc32 = PolicyActions.IsEmpty()
			? 0u
			: FCrc::MemCrc32(
				PolicyActions.GetData(),
				static_cast<int32>(PolicyActions.Num() * sizeof(float)));
		return Signature;
	}

	bool ValidateReplayRecord(
		const FPhysAnimLocomotionReplayRecord& Replay,
		FString& OutError)
	{
		OutError.Reset();
		if (!IsFiniteTransform(Replay.ActorFrame.ActorToWorld) ||
			!IsFiniteTransform(Replay.MeshFrame.MeshToWorld))
		{
			return Fail(OutError, TEXT("Replay actor or mesh frame is malformed."));
		}
		if (Replay.WorldTrajectory.Samples.Num() < 2)
		{
			return Fail(OutError, TEXT("Replay requires at least two world trajectory samples."));
		}
		if (Replay.SelectedAnimation.AssetPath.IsEmpty())
		{
			return Fail(OutError, TEXT("Replay selected animation identity is missing."));
		}
		if (Replay.CurrentAnimationPose.AnimationBodySamplesUeWorldCm.Num() != PhysAnimBridge::NumSmplBodies)
		{
			return Fail(OutError, TEXT("Replay current animation pose is incomplete."));
		}
		if (Replay.FutureAnimationPoses.Num() != PhysAnimBridge::NumFutureSteps)
		{
			return Fail(OutError, TEXT("Replay future animation stream is incomplete."));
		}
		if (Replay.PhysicalBodySamplesUeWorldCm.Num() != PhysAnimBridge::NumSmplBodies ||
			Replay.CanonicalBodyState.ProtoRuntimeBodySamplesMeters.Num() != PhysAnimBridge::NumSmplBodies)
		{
			return Fail(OutError, TEXT("Replay physical or canonical body stream is incomplete."));
		}
		if (Replay.SelfObservation.Values.Num() != PhysAnimBridge::SelfObsSize ||
			!IsFiniteFloatArray(Replay.SelfObservation.Values))
		{
			return Fail(OutError, TEXT("Replay self observation does not match the training contract."));
		}
		if (Replay.MimicTarget.Values.Num() != PhysAnimBridge::MimicTargetPosesSize ||
			!IsFiniteFloatArray(Replay.MimicTarget.Values))
		{
			return Fail(OutError, TEXT("Replay mimic target does not match the training contract."));
		}
		if (Replay.ActionSignature.ValueCount != PhysAnimBridge::NumActionFloats)
		{
			return Fail(OutError, TEXT("Replay policy action signature is incomplete."));
		}
		if (Replay.AuthoredForwardAnimationLocal.IsNearlyZero())
		{
			return Fail(OutError, TEXT("Replay authored-forward axis is missing."));
		}
		return true;
	}

	FPhysAnimFrameCandidateResult EvaluateCandidate(
		const FPhysAnimLocomotionReplayRecord& Replay,
		const FPhysAnimFrameCandidate& Candidate)
	{
		FPhysAnimFrameCandidateResult Result;
		Result.Candidate = Candidate;
		FString Error;
		if (!ValidateReplayRecord(Replay, Error))
		{
			Result.FailedInvariants.Add(FString::Printf(TEXT("invalid-replay:%s"), *Error));
			return Result;
		}

		FPhysAnimPoseSearchQueryState Query;
		if (!WorldTrajectoryToPoseSearchQuery(
			Replay.WorldTrajectory,
			Replay.ActorFrame,
			Replay.MeshFrame,
			Candidate.PoseSearchFacingBasis,
			Query,
			Error))
		{
			Result.FailedInvariants.Add(FString::Printf(TEXT("query-build:%s"), *Error));
			return Result;
		}

		const FVector AuthoredForward = Replay.AuthoredForwardAnimationLocal.GetSafeNormal();
		const FVector MovingLocalVelocity = Query.Samples.Last().FacingLocalVelocityCmPerSecond;
		Result.bActorForwardMapsToAuthoredForward =
			FVector::DotProduct(MovingLocalVelocity.GetSafeNormal(), AuthoredForward) > 0.999;

		const double WorldYawDelta = FMath::FindDeltaAngleDegrees(
			Replay.WorldTrajectory.Samples[0].WorldFacing.Rotator().Yaw,
			Replay.WorldTrajectory.Samples.Last().WorldFacing.Rotator().Yaw);
		const double QueryYawDelta = FMath::FindDeltaAngleDegrees(
			Query.Samples[0].FacingLocalRotation.Rotator().Yaw,
			Query.Samples.Last().FacingLocalRotation.Rotator().Yaw);
		Result.bYawDirectionPreserved =
			FMath::IsNearlyZero(WorldYawDelta, 1.0e-5) ||
			FMath::Sign(WorldYawDelta) == FMath::Sign(QueryYawDelta);

		const FTransform AnimationReferenceToWorld = ResolveAnimationReferenceToWorld(
			Replay,
			Candidate.AnimationReferenceBasis);
		const FTransform WorldToAnimationReference = AnimationReferenceToWorld.Inverse();
		const FVector CurrentAnimationRootLocal = WorldToAnimationReference.TransformPosition(
			Replay.CurrentAnimationPose.AnimationRootWorld.GetLocation());
		const FVector FutureAnimationRootLocal = WorldToAnimationReference.TransformPosition(
			Replay.FutureAnimationPoses.Last().AnimationRootWorld.GetLocation());
		Result.bFutureRootProgressionPositive = FVector::DotProduct(
			FutureAnimationRootLocal - CurrentAnimationRootLocal,
			AuthoredForward) > 0.0;

		const FVector CurrentPhysicalRootLocal = WorldToAnimationReference.TransformPosition(
			Replay.PhysicalBodySamplesUeWorldCm[0].Position);
		Result.bCurrentFutureConventionConsistent =
			CurrentPhysicalRootLocal.Equals(CurrentAnimationRootLocal, 1.0e-4);

		Result.bZeroVelocityPreserved =
			Query.Samples[0].FacingLocalVelocityCmPerSecond == FVector::ZeroVector;
		Result.bMagnitudePreserved = FMath::IsNearlyEqual(
			Replay.WorldTrajectory.Samples.Last().WorldVelocityCmPerSecond.Size(),
			Query.Samples.Last().FacingLocalVelocityCmPerSecond.Size(),
			1.0e-4);
		const FQuat RoundTripFacing = (
			Query.FacingToWorld.GetRotation() *
			Query.Samples.Last().FacingLocalRotation).GetNormalized();
		Result.bOrientationRoundTripPreserved =
			RoundTripFacing.AngularDistance(
				Replay.WorldTrajectory.Samples.Last().WorldFacing) <= 1.0e-5;
		Result.bTrainingContractSatisfied =
			Candidate.PolicyHeadingBasis == EPhysAnimPolicyHeadingBasis::CurrentCanonicalRoot;

		AddFailure(Result, !Result.bActorForwardMapsToAuthoredForward,
			TEXT("actor-forward does not map to authored-forward"));
		AddFailure(Result, !Result.bYawDirectionPreserved,
			TEXT("yaw-direction is not preserved"));
		AddFailure(Result, !Result.bFutureRootProgressionPositive,
			TEXT("future-root-progression is not positive"));
		AddFailure(Result, !Result.bCurrentFutureConventionConsistent,
			TEXT("current-future convention is inconsistent"));
		AddFailure(Result, !Result.bZeroVelocityPreserved,
			TEXT("zero-velocity is not preserved"));
		AddFailure(Result, !Result.bMagnitudePreserved,
			TEXT("velocity magnitude is not preserved"));
		AddFailure(Result, !Result.bOrientationRoundTripPreserved,
			TEXT("orientation roundtrip is not preserved"));
		AddFailure(Result, !Result.bTrainingContractSatisfied,
			TEXT("policy heading violates the training-contract"));
		return Result;
	}

	FPhysAnimFrameFactorialReport EvaluateFactorial(
		const FPhysAnimLocomotionReplayRecord& Replay,
		TConstArrayView<FPhysAnimFrameCandidate> Candidates)
	{
		FPhysAnimFrameFactorialReport Report;
		Report.Results.Reserve(Candidates.Num());
		for (const FPhysAnimFrameCandidate& Candidate : Candidates)
		{
			FPhysAnimFrameCandidateResult Result = EvaluateCandidate(Replay, Candidate);
			if (Result.PassesAll())
			{
				Report.SurvivingCandidates.Add(Candidate);
			}
			Report.Results.Add(MoveTemp(Result));
		}
		return Report;
	}
}
