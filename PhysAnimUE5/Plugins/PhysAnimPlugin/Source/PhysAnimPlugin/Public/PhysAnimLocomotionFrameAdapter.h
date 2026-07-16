#pragma once

#include "CoreMinimal.h"
#include "PhysAnimBridge.h"

enum class EPhysAnimPoseSearchFacingBasis : uint8
{
	ActorWorld,
	MeshWorld
};

enum class EPhysAnimAnimationReferenceBasis : uint8
{
	ActorWorld,
	MeshWorld
};

enum class EPhysAnimPolicyHeadingBasis : uint8
{
	CurrentCanonicalRoot,
	AnimationReferenceRoot
};

struct PHYSANIMPLUGIN_API FPhysAnimActorWorldFrame
{
	FTransform ActorToWorld = FTransform::Identity;
};

struct PHYSANIMPLUGIN_API FPhysAnimMeshWorldFrame
{
	FTransform MeshToWorld = FTransform::Identity;
};

struct PHYSANIMPLUGIN_API FPhysAnimWorldTrajectorySample
{
	float TimeSeconds = 0.0f;
	FVector WorldPositionCm = FVector::ZeroVector;
	FQuat WorldFacing = FQuat::Identity;
	FVector WorldVelocityCmPerSecond = FVector::ZeroVector;
};

struct PHYSANIMPLUGIN_API FPhysAnimWorldTrajectoryState
{
	TArray<FPhysAnimWorldTrajectorySample> Samples;
};

struct PHYSANIMPLUGIN_API FPhysAnimPoseSearchFacingTrajectorySample
{
	float TimeSeconds = 0.0f;
	FVector FacingLocalPositionCm = FVector::ZeroVector;
	FQuat FacingLocalRotation = FQuat::Identity;
	FVector FacingLocalVelocityCmPerSecond = FVector::ZeroVector;
};

struct PHYSANIMPLUGIN_API FPhysAnimPoseSearchQueryState
{
	EPhysAnimPoseSearchFacingBasis FacingBasis = EPhysAnimPoseSearchFacingBasis::MeshWorld;
	FTransform FacingToWorld = FTransform::Identity;
	TArray<FPhysAnimPoseSearchFacingTrajectorySample> Samples;
};

struct PHYSANIMPLUGIN_API FPhysAnimAnimationDataPose
{
	float SampleTimeSeconds = 0.0f;
	FTransform AnimationRootWorld = FTransform::Identity;
	TArray<FPhysAnimBodySample> AnimationBodySamplesUeWorldCm;
};

struct PHYSANIMPLUGIN_API FPhysAnimProtoCanonicalBodyState
{
	TArray<FPhysAnimBodySample> ProtoRuntimeBodySamplesMeters;
};

struct PHYSANIMPLUGIN_API FPhysAnimPolicyHeadingLocalObservation
{
	TArray<float> Values;
};

struct PHYSANIMPLUGIN_API FPhysAnimPolicyMimicTarget
{
	TArray<float> Values;
};

struct PHYSANIMPLUGIN_API FPhysAnimPolicyActionSignature
{
	int32 ValueCount = 0;
	uint32 StableCrc32 = 0;
};

struct PHYSANIMPLUGIN_API FPhysAnimSelectedAnimationSample
{
	FString AssetPath;
	float TimeSeconds = 0.0f;
};

struct PHYSANIMPLUGIN_API FPhysAnimLocomotionReplayRecord
{
	FPhysAnimActorWorldFrame ActorFrame;
	FPhysAnimMeshWorldFrame MeshFrame;
	FPhysAnimWorldTrajectoryState WorldTrajectory;
	FPhysAnimSelectedAnimationSample SelectedAnimation;
	FPhysAnimAnimationDataPose CurrentAnimationPose;
	TArray<FPhysAnimAnimationDataPose> FutureAnimationPoses;
	TArray<FPhysAnimBodySample> PhysicalBodySamplesUeWorldCm;
	FPhysAnimProtoCanonicalBodyState CanonicalBodyState;
	FPhysAnimPolicyHeadingLocalObservation SelfObservation;
	FPhysAnimPolicyMimicTarget MimicTarget;
	FPhysAnimPolicyActionSignature ActionSignature;
	FVector AuthoredForwardAnimationLocal = FVector::RightVector;
};

struct PHYSANIMPLUGIN_API FPhysAnimFrameCandidate
{
	EPhysAnimPoseSearchFacingBasis PoseSearchFacingBasis = EPhysAnimPoseSearchFacingBasis::MeshWorld;
	EPhysAnimAnimationReferenceBasis AnimationReferenceBasis = EPhysAnimAnimationReferenceBasis::MeshWorld;
	EPhysAnimPolicyHeadingBasis PolicyHeadingBasis = EPhysAnimPolicyHeadingBasis::CurrentCanonicalRoot;
};

struct PHYSANIMPLUGIN_API FPhysAnimFrameCandidateResult
{
	FPhysAnimFrameCandidate Candidate;
	bool bActorForwardMapsToAuthoredForward = false;
	bool bYawDirectionPreserved = false;
	bool bFutureRootProgressionPositive = false;
	bool bCurrentFutureConventionConsistent = false;
	bool bZeroVelocityPreserved = false;
	bool bMagnitudePreserved = false;
	bool bOrientationRoundTripPreserved = false;
	bool bTrainingContractSatisfied = false;
	TArray<FString> FailedInvariants;

	bool PassesAll() const
	{
		return FailedInvariants.IsEmpty();
	}
};

struct PHYSANIMPLUGIN_API FPhysAnimFrameFactorialReport
{
	TArray<FPhysAnimFrameCandidateResult> Results;
	TArray<FPhysAnimFrameCandidate> SurvivingCandidates;
};

namespace PhysAnimLocomotionFrameAdapter
{
	PHYSANIMPLUGIN_API bool WorldTrajectoryToPoseSearchQuery(
		const FPhysAnimWorldTrajectoryState& WorldTrajectory,
		const FPhysAnimActorWorldFrame& ActorFrame,
		const FPhysAnimMeshWorldFrame& MeshFrame,
		EPhysAnimPoseSearchFacingBasis FacingBasis,
		FPhysAnimPoseSearchQueryState& OutQuery,
		FString& OutError);

	PHYSANIMPLUGIN_API bool AnimationPoseToCanonicalProtoPose(
		const FPhysAnimAnimationDataPose& AnimationPose,
		const FTransform& AnimationReferenceToWorld,
		FPhysAnimProtoCanonicalBodyState& OutCanonicalState,
		FString& OutError);

	PHYSANIMPLUGIN_API bool CanonicalBodyToPolicyObservation(
		const FPhysAnimProtoCanonicalBodyState& CanonicalState,
		float ProtoGroundHeightMeters,
		FPhysAnimPolicyHeadingLocalObservation& OutObservation,
		FString& OutError);

	PHYSANIMPLUGIN_API bool AnimationFutureToPolicyMimicTarget(
		const FPhysAnimProtoCanonicalBodyState& CurrentCanonicalState,
		const TArray<FPhysAnimAnimationDataPose>& FutureAnimationPoses,
		const FTransform& AnimationReferenceToWorld,
		FPhysAnimPolicyMimicTarget& OutMimicTarget,
		FString& OutError);

	PHYSANIMPLUGIN_API FPhysAnimPolicyActionSignature BuildPolicyActionSignature(
		TConstArrayView<float> PolicyActions);

	PHYSANIMPLUGIN_API bool ValidateReplayRecord(
		const FPhysAnimLocomotionReplayRecord& Replay,
		FString& OutError);

	PHYSANIMPLUGIN_API FPhysAnimFrameCandidateResult EvaluateCandidate(
		const FPhysAnimLocomotionReplayRecord& Replay,
		const FPhysAnimFrameCandidate& Candidate);

	PHYSANIMPLUGIN_API FPhysAnimFrameFactorialReport EvaluateFactorial(
		const FPhysAnimLocomotionReplayRecord& Replay,
		TConstArrayView<FPhysAnimFrameCandidate> Candidates);
}
