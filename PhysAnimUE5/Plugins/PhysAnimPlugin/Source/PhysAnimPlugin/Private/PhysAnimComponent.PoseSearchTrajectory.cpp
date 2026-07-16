#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

FQuat UPhysAnimComponent::ResolveBridgePoseSearchAnimationFrameRotation(
	const FQuat& ActorWorldRotation,
	const FQuat& MeshWorldRotation)
{
	const FQuat NormalizedActorRotation = ActorWorldRotation.GetNormalized();
	const FQuat NormalizedMeshRotation = MeshWorldRotation.GetNormalized();
	const FQuat ActorToMeshRotation =
		(NormalizedActorRotation.Inverse() * NormalizedMeshRotation).GetNormalized();
	return (
		NormalizedActorRotation *
		ActorToMeshRotation.Inverse() *
		NormalizedActorRotation.Inverse()).GetNormalized();
}


void UPhysAnimComponent::GetGravity(FVector& OutGravityAccel)
{
	const UWorld* const World = GetWorld();
	OutGravityAccel = FVector(0.0f, 0.0f, World ? World->GetGravityZ() : -980.0f);
}


void UPhysAnimComponent::GetCurrentState(FVector& OutPosition, FQuat& OutFacing, FVector& OutVelocity)
{
	const AActor* const OwnerActor = GetOwner();
	const FQuat ActorWorldRotation = OwnerActor ? OwnerActor->GetActorQuat() : FQuat::Identity;
	FQuat PoseSearchFrameRotation = FQuat::Identity;
	if (OwnerActor)
	{
		OutPosition = OwnerActor->GetActorLocation();
		if (const USkeletalMeshComponent* const SkeletalMesh = GetMeshComponent())
		{
			PoseSearchFrameRotation = ResolveBridgePoseSearchAnimationFrameRotation(
				ActorWorldRotation,
				SkeletalMesh->GetComponentQuat());
		}
	}
	else
	{
		OutPosition = FVector::ZeroVector;
	}
	OutFacing = (PoseSearchFrameRotation * ActorWorldRotation).GetNormalized();

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	FVector WorldQueryVelocity = FVector::ZeroVector;
	ResolveBridgePoseSearchQueryVelocity(EffectiveSettings, WorldQueryVelocity);
	WorldQueryVelocity.Z = 0.0f;
	BridgePoseSearchQueryVelocityCmPerSecond = WorldQueryVelocity;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = WorldQueryVelocity;
	OutVelocity = PoseSearchFrameRotation.RotateVector(WorldQueryVelocity);
	OutVelocity.Z = 0.0f;
}


void UPhysAnimComponent::GetVelocity(FVector& OutVelocity)
{
	const AActor* const OwnerActor = GetOwner();
	FQuat PoseSearchFrameRotation = FQuat::Identity;
	if (OwnerActor)
	{
		if (const USkeletalMeshComponent* const SkeletalMesh = GetMeshComponent())
		{
			PoseSearchFrameRotation = ResolveBridgePoseSearchAnimationFrameRotation(
				OwnerActor->GetActorQuat(),
				SkeletalMesh->GetComponentQuat());
		}
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	FVector WorldQueryVelocity = FVector::ZeroVector;
	ResolveBridgePoseSearchQueryVelocity(EffectiveSettings, WorldQueryVelocity);
	WorldQueryVelocity.Z = 0.0f;
	BridgePoseSearchQueryVelocityCmPerSecond = WorldQueryVelocity;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = WorldQueryVelocity;
	OutVelocity = PoseSearchFrameRotation.RotateVector(WorldQueryVelocity);
	OutVelocity.Z = 0.0f;
}


void UPhysAnimComponent::Predict(FTransformTrajectory& InOutTrajectory, int32 NumPredictionSamples, float SecondsPerPredictionSample, int32 NumHistorySamples)
{
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();

	const FQuat ActorWorldRotation = OwnerActor->GetActorQuat();
	FQuat PoseSearchFrameRotation = FQuat::Identity;
	if (const USkeletalMeshComponent* const SkeletalMesh = GetMeshComponent())
	{
		PoseSearchFrameRotation = ResolveBridgePoseSearchAnimationFrameRotation(
			ActorWorldRotation,
			SkeletalMesh->GetComponentQuat());
	}

	FVector SimulatedPosition = OwnerActor->GetActorLocation();
	FQuat SimulatedFacing = (PoseSearchFrameRotation * ActorWorldRotation).GetNormalized();

	float IntentMagnitude = 0.0f;
	FVector WorldQueryVelocity = FVector::ZeroVector;
	ResolveBridgePoseSearchQueryVelocity(
		EffectiveSettings,
		WorldQueryVelocity,
		&IntentMagnitude);
	WorldQueryVelocity.Z = 0.0f;
	BridgePoseSearchQueryVelocityCmPerSecond = WorldQueryVelocity;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = WorldQueryVelocity;

	FVector SimulatedVelocity = PoseSearchFrameRotation.RotateVector(WorldQueryVelocity);
	SimulatedVelocity.Z = 0.0f;
	const FVector DesiredVelocity = SimulatedVelocity;

	float DesiredWorldYawDegrees = ActorWorldRotation.Rotator().Yaw;
	if (BridgeIntentState.bHasDesiredFacing)
	{
		DesiredWorldYawDegrees = BridgeIntentState.DesiredFacingYawDegrees;
	}
	else if (!WorldQueryVelocity.IsNearlyZero())
	{
		DesiredWorldYawDegrees = WorldQueryVelocity.Rotation().Yaw;
	}
	const FQuat DesiredWorldFacing = FQuat(FRotator(0.0f, DesiredWorldYawDegrees, 0.0f));
	const FQuat DesiredFacing =
		(PoseSearchFrameRotation * DesiredWorldFacing).GetNormalized();

	const float SafePredictionDt = FMath::Max(SecondsPerPredictionSample, KINDA_SMALL_NUMBER);
	const float RotationAlphaPerStep = FMath::Clamp(
		SafePredictionDt * FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementRotationInterpSpeed),
		0.0f,
		1.0f);

	for (int32 PredictionIndex = 1; PredictionIndex <= NumPredictionSamples; ++PredictionIndex)
	{
		const bool bUseCleanWalkQuerySpeed =
			EffectiveSettings.bBridgePoseSearchUseStabilizedWalkQuerySpeed &&
			IntentMagnitude >= EffectiveSettings.BridgePoseSearchWalkIntentThreshold &&
			!DesiredVelocity.IsNearlyZero();

		if (bUseCleanWalkQuerySpeed)
		{
			SimulatedVelocity = DesiredVelocity;
		}
		else
		{
			const float VelocityBlendRate = DesiredVelocity.IsNearlyZero()
				? FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementDecelerationCmPerSecondSq)
				: FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementAccelerationCmPerSecondSq);

			SimulatedVelocity = FMath::VInterpConstantTo(
				SimulatedVelocity,
				DesiredVelocity,
				SafePredictionDt,
				VelocityBlendRate);
		}
		SimulatedVelocity.Z = 0.0f;

		SimulatedPosition += SimulatedVelocity * SafePredictionDt;
		SimulatedFacing = FQuat::Slerp(SimulatedFacing, DesiredFacing, RotationAlphaPerStep).GetNormalized();

		const int32 SampleIndex = NumHistorySamples + PredictionIndex;
		if (!InOutTrajectory.Samples.IsValidIndex(SampleIndex))
		{
			break;
		}

		FTransformTrajectorySample& Sample = InOutTrajectory.Samples[SampleIndex];
		Sample.Position = SimulatedPosition;
		Sample.Facing = SimulatedFacing;
	}
}


void UPhysAnimComponent::UpdateBridgePoseSearchTrajectory(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	BridgeTrajectoryState.LastDeltaTimeSeconds = DeltaTime;
	ResolveBridgePoseSearchQueryVelocity(EffectiveSettings, BridgePoseSearchQueryVelocityCmPerSecond);
	BridgeTrajectoryState.QueryVelocityCmPerSecond = BridgePoseSearchQueryVelocityCmPerSecond;

	TScriptInterface<IPoseSearchTrajectoryPredictorInterface> PredictorInterface;
	PredictorInterface.SetObject(this);
	PredictorInterface.SetInterface(static_cast<IPoseSearchTrajectoryPredictorInterface*>(this));

	FTransformTrajectory GeneratedTrajectory;
	UPoseSearchTrajectoryLibrary::PoseSearchGenerateTransformTrajectoryWithPredictor(
		PredictorInterface,
		FMath::Max(0.0001f, DeltaTime),
		BridgeTrajectoryState.QueryTrajectory,
		BridgeTrajectoryState.DesiredControllerYawLastUpdate,
		GeneratedTrajectory,
		0.04f,
		10,
		0.20f,
		8);

	BridgeTrajectoryState.QueryTrajectory = GeneratedTrajectory;
	BridgeTrajectoryState.bInitialized = true;
	BridgePoseSearchTrajectory = BridgeTrajectoryState.QueryTrajectory;
	BridgePoseSearchDesiredControllerYawLastUpdate = BridgeTrajectoryState.DesiredControllerYawLastUpdate;
	bBridgePoseSearchTrajectoryInitialized = true;
}

