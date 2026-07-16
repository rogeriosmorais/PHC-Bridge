#pragma once

#include "CoreMinimal.h"
#include "PhysAnimBridge.h"

namespace PhysAnimFrameContract
{
	template <typename Tag>
	class TTypedVector
	{
	public:
		TTypedVector() = default;
		explicit TTypedVector(const FVector& InValue)
			: Value(InValue)
		{
		}

		const FVector& Get() const { return Value; }
		bool IsFinite() const { return !Value.ContainsNaN(); }

	private:
		FVector Value = FVector::ZeroVector;
	};

	template <typename Tag>
	class TTypedQuat
	{
	public:
		TTypedQuat() = default;
		explicit TTypedQuat(const FQuat& InValue)
			: Value(InValue.GetNormalized())
		{
		}

		const FQuat& Get() const { return Value; }
		bool IsFinite() const { return !Value.ContainsNaN() && Value.IsNormalized(); }

	private:
		FQuat Value = FQuat::Identity;
	};

	template <typename Tag>
	class TTypedTransform
	{
	public:
		TTypedTransform() = default;
		explicit TTypedTransform(const FTransform& InValue)
			: Value(InValue)
		{
			Value.NormalizeRotation();
		}

		const FTransform& Get() const { return Value; }
		bool IsFinite() const
		{
			return !Value.ContainsNaN() && Value.GetRotation().IsNormalized();
		}

	private:
		FTransform Value = FTransform::Identity;
	};

	struct FWorldPositionCmTag;
	struct FWorldVelocityCmPerSecondTag;
	struct FAnimationDataPositionCmTag;
	struct FProtoCanonicalPositionMetersTag;
	struct FActorWorldFacingTag;
	struct FMeshWorldFacingTag;
	struct FPoseSearchWorldFacingTag;
	struct FAnimationDataFacingTag;
	struct FProtoCanonicalRotationTag;
	struct FWorldTransformCmTag;
	struct FAnimationDataTransformCmTag;
	struct FProtoCanonicalTransformMetersTag;
	struct FProtoWorldCanonicalTransformMetersTag;
	struct FProtoAnimationDataCanonicalTransformMetersTag;

	using FWorldPositionCm = TTypedVector<FWorldPositionCmTag>;
	using FWorldVelocityCmPerSecond = TTypedVector<FWorldVelocityCmPerSecondTag>;
	using FAnimationDataPositionCm = TTypedVector<FAnimationDataPositionCmTag>;
	using FProtoCanonicalPositionMeters = TTypedVector<FProtoCanonicalPositionMetersTag>;
	using FActorWorldFacing = TTypedQuat<FActorWorldFacingTag>;
	using FMeshWorldFacing = TTypedQuat<FMeshWorldFacingTag>;
	using FPoseSearchWorldFacing = TTypedQuat<FPoseSearchWorldFacingTag>;
	using FAnimationDataFacing = TTypedQuat<FAnimationDataFacingTag>;
	using FProtoCanonicalRotation = TTypedQuat<FProtoCanonicalRotationTag>;
	using FWorldTransformCm = TTypedTransform<FWorldTransformCmTag>;
	using FAnimationDataTransformCm = TTypedTransform<FAnimationDataTransformCmTag>;
	using FProtoCanonicalTransformMeters = TTypedTransform<FProtoCanonicalTransformMetersTag>;
	using FProtoWorldCanonicalTransformMeters = TTypedTransform<FProtoWorldCanonicalTransformMetersTag>;
	using FProtoAnimationDataCanonicalTransformMeters = TTypedTransform<FProtoAnimationDataCanonicalTransformMetersTag>;

	enum class EQueryFrameContract : uint8
	{
		E73_ActorFacing_WorldVelocity,
		E74_ActorFacing_RotatedVelocity,
		E75_RotatedFacing_RotatedVelocity,
		E77_MeshFacing_WorldVelocity,
	};

	enum class EFutureRootSource : uint8
	{
		RawAnimationRootMotion,
		QueryTrajectoryRootMotion,
	};

	struct FWorldTrajectoryStateCm
	{
		FWorldPositionCm Position;
		FWorldVelocityCmPerSecond Velocity;
		FActorWorldFacing ActorFacing;
		FMeshWorldFacing MeshFacing;
		FActorWorldFacing DesiredActorFacing;
		bool bHasMesh = false;
	};

	struct FPoseSearchQueryCm
	{
		FWorldPositionCm Position;
		FWorldVelocityCmPerSecond Velocity;
		FPoseSearchWorldFacing CurrentFacing;
		FPoseSearchWorldFacing DesiredFacing;
		FVector LocalVelocityCmPerSecond = FVector::ZeroVector;
	};

	struct FSelectedAnimationFrameCm
	{
		FWorldTransformCm SelectedWorldRoot;
		FAnimationDataTransformCm SelectedDataRoot;
	};

	struct FCanonicalAnimationDataPoseCm
	{
		FAnimationDataTransformCm Root;
		TArray<FAnimationDataTransformCm> Bodies;
		double FutureTimeSeconds = 0.0;
	};

	struct FCanonicalProtoBodyStateMeters
	{
		TArray<FPhysAnimBodySample> Bodies;
	};

	struct FCanonicalProtoFuturePoseMeters
	{
		FPhysAnimFuturePoseSample Sample;
	};

	struct FPolicyHeadingLocalObservation
	{
		TArray<float> Values;
	};

	struct FPolicyMimicTargetObservation
	{
		TArray<float> Values;
	};

	struct FReplayFixture
	{
		FWorldTransformCm ActorWorldRoot;
		FWorldTransformCm MeshWorldRoot;
		FWorldVelocityCmPerSecond RequestedWorldVelocity;
		FActorWorldFacing DesiredActorFacing;
		double HorizonSeconds = 0.5;
		double RequestedTurnDegrees = 30.0;
		FVector WalkForwardAuthoredAxis = FVector(0.0, 1.0, 0.0);
		FVector WalkForwardRootDeltaCm = FVector(0.0, 150.0, 0.0);
		FVector WalkLeftAuthoredAxis = FVector(1.0, 0.0, 0.0);
		FVector WalkLeftRootDeltaCm = FVector(151.830066, 0.0, 0.0);
	};

	struct FReplayCandidateResult
	{
		EQueryFrameContract QueryContract = EQueryFrameContract::E73_ActorFacing_WorldVelocity;
		EFutureRootSource FutureRootSource = EFutureRootSource::RawAnimationRootMotion;
		FString SelectedAsset;
		double AuthoredForwardAlignment = -1.0;
		double RouteProjectionCm = 0.0;
		double FutureRootSpeedCmPerSecond = 0.0;
		double SpeedRelativeError = TNumericLimits<double>::Max();
		double TurnDeltaDegrees = 0.0;
		bool bWorldQueryVelocityPreserved = false;
		bool bZeroVelocityExact = false;
		bool bRoundTripExact = false;
		bool bCurrentFutureFrameConsistent = false;
		bool bTensorContractsValid = false;
		bool bPass = false;
		TArray<FString> FailedCriteria;
	};

	FPoseSearchQueryCm WorldTrajectoryToPoseSearchQuery(
		const FWorldTrajectoryStateCm& State,
		EQueryFrameContract Contract);

	FAnimationDataTransformCm WorldTransformToAnimationData(
		const FWorldTransformCm& WorldTransform,
		const FSelectedAnimationFrameCm& SelectedFrame);

	FWorldTransformCm AnimationDataTransformToWorld(
		const FAnimationDataTransformCm& DataTransform,
		const FSelectedAnimationFrameCm& SelectedFrame);

	FCanonicalAnimationDataPoseCm PlaceAnimationPoseOnTrajectory(
		const FCanonicalAnimationDataPoseCm& RawAnimationPose,
		const FWorldTransformCm& QueryTrajectoryRoot,
		const FSelectedAnimationFrameCm& SelectedFrame,
		EFutureRootSource RootSource);

	FCanonicalProtoFuturePoseMeters AnimationPoseToCanonicalProtoPose(
		const FCanonicalAnimationDataPoseCm& AnimationPose);

	bool CanonicalBodyToPolicyObservation(
		const FCanonicalProtoBodyStateMeters& BodyState,
		float GroundHeightMeters,
		FPolicyHeadingLocalObservation& OutObservation,
		FString& OutError);

	bool AnimationFutureToPolicyMimicTarget(
		const FCanonicalProtoBodyStateMeters& CurrentBodyState,
		const TArray<FCanonicalAnimationDataPoseCm>& RawAnimationFuturePoses,
		const TArray<FWorldTransformCm>& QueryTrajectoryRoots,
		const FSelectedAnimationFrameCm& SelectedFrame,
		EFutureRootSource RootSource,
		FPolicyMimicTargetObservation& OutObservation,
		FString& OutError);

	bool PlaceProtoFuturePosesOnWorldTrajectory(
		const TArray<FPhysAnimFuturePoseSample>& RawFuturePoses,
		const FWorldTransformCm& CurrentQueryWorldRoot,
		const TArray<FWorldTransformCm>& FutureQueryWorldRoots,
		const FProtoWorldCanonicalTransformMeters& CurrentSelectedWorldRoot,
		const FProtoAnimationDataCanonicalTransformMeters& CurrentSelectedDataRoot,
		TArray<FPhysAnimFuturePoseSample>& OutPlacedFuturePoses,
		FString& OutError);

	TArray<FReplayCandidateResult> EvaluateReplayMatrix(const FReplayFixture& Fixture);
	FString QueryContractName(EQueryFrameContract Contract);
	FString FutureRootSourceName(EFutureRootSource Source);
}
