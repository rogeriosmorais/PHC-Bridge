#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

FQuat UPhysAnimComponent::ResolveBridgePoseSearchCurrentFacing(
	const FQuat& ActorWorldFacing,
	const FQuat& MeshWorldFacing,
	bool bHasMesh)
{
	return (bHasMesh ? MeshWorldFacing : ActorWorldFacing).GetNormalized();
}


FQuat UPhysAnimComponent::ResolveBridgePoseSearchDesiredFacing(
	const FQuat& DesiredActorWorldFacing,
	const FQuat& CurrentActorWorldFacing,
	const FQuat& CurrentMeshWorldFacing,
	bool bHasMesh)
{
	if (!bHasMesh)
	{
		return DesiredActorWorldFacing.GetNormalized();
	}

	const FQuat ActorToMeshRelativeFacing =
		(CurrentActorWorldFacing.GetNormalized().Inverse() *
		 CurrentMeshWorldFacing.GetNormalized()).GetNormalized();
	return (DesiredActorWorldFacing.GetNormalized() *
		ActorToMeshRelativeFacing).GetNormalized();
}


void UPhysAnimComponent::GetGravity(FVector& OutGravityAccel)
{
	const UWorld* const World = GetWorld();
	OutGravityAccel = FVector(0.0f, 0.0f, World ? World->GetGravityZ() : -980.0f);
}


void UPhysAnimComponent::GetCurrentState(FVector& OutPosition, FQuat& OutFacing, FVector& OutVelocity)
{
	if (const AActor* const OwnerActor = GetOwner())
	{
		OutPosition = OwnerActor->GetActorLocation();
		const USkeletalMeshComponent* const SkeletalMesh = GetMeshComponent();
		OutFacing = ResolveBridgePoseSearchCurrentFacing(
			OwnerActor->GetActorQuat(),
			SkeletalMesh ? SkeletalMesh->GetComponentQuat() : FQuat::Identity,
			SkeletalMesh != nullptr);
	}
	else
	{
		OutPosition = FVector::ZeroVector;
		OutFacing = FQuat::Identity;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	ResolveBridgePoseSearchQueryVelocity(EffectiveSettings, OutVelocity);
	BridgePoseSearchQueryVelocityCmPerSecond = OutVelocity;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = OutVelocity;
	OutVelocity.Z = 0.0f;
}


void UPhysAnimComponent::GetVelocity(FVector& OutVelocity)
{
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	ResolveBridgePoseSearchQueryVelocity(EffectiveSettings, OutVelocity);
	BridgePoseSearchQueryVelocityCmPerSecond = OutVelocity;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = OutVelocity;
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

	FVector SimulatedPosition = OwnerActor->GetActorLocation();
	const FQuat ActorWorldFacing = OwnerActor->GetActorQuat();
	const USkeletalMeshComponent* const SkeletalMesh = GetMeshComponent();
	const FQuat MeshWorldFacing = SkeletalMesh
		? SkeletalMesh->GetComponentQuat()
		: FQuat::Identity;
	FQuat SimulatedFacing = ResolveBridgePoseSearchCurrentFacing(
		ActorWorldFacing,
		MeshWorldFacing,
		SkeletalMesh != nullptr);

	float IntentMagnitude = 0.0f;
	ResolveBridgePoseSearchQueryVelocity(
		EffectiveSettings,
		BridgePoseSearchQueryVelocityCmPerSecond,
		&IntentMagnitude);
	BridgeTrajectoryState.QueryVelocityCmPerSecond = BridgePoseSearchQueryVelocityCmPerSecond;

	FVector SimulatedVelocity = BridgePoseSearchQueryVelocityCmPerSecond;
	SimulatedVelocity.Z = 0.0f;

	const FVector DesiredVelocity = BridgePoseSearchQueryVelocityCmPerSecond;

	float DesiredActorYawDegrees = ActorWorldFacing.Rotator().Yaw;
	if (BridgeIntentState.bHasDesiredFacing)
	{
		DesiredActorYawDegrees = BridgeIntentState.DesiredFacingYawDegrees;
	}
	else if (!DesiredVelocity.IsNearlyZero())
	{
		DesiredActorYawDegrees = DesiredVelocity.Rotation().Yaw;
	}
	else if (SimulatedVelocity.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		DesiredActorYawDegrees = SimulatedVelocity.Rotation().Yaw;
	}

	const FQuat DesiredActorFacing =
		FQuat(FRotator(0.0f, DesiredActorYawDegrees, 0.0f));
	const FQuat DesiredFacing = ResolveBridgePoseSearchDesiredFacing(
		DesiredActorFacing,
		ActorWorldFacing,
		MeshWorldFacing,
		SkeletalMesh != nullptr);

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

