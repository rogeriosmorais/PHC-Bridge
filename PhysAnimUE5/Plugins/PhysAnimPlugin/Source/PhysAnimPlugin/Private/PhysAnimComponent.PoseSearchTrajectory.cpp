#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

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
		OutFacing = OwnerActor->GetActorQuat();
	}
	else
	{
		OutPosition = FVector::ZeroVector;
		OutFacing = FQuat::Identity;
	}

	OutVelocity = BridgeTrajectoryState.AcceptedVelocityCmPerSecond;
	OutVelocity.Z = 0.0f;
}


void UPhysAnimComponent::GetVelocity(FVector& OutVelocity)
{
	OutVelocity = BridgeTrajectoryState.AcceptedVelocityCmPerSecond;
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
	FQuat SimulatedFacing = OwnerActor->GetActorQuat();

	const float IntentMagnitude = BridgeIntentState.IntentMagnitude;
	ResolveBridgePoseSearchQueryVelocity(EffectiveSettings, BridgePoseSearchQueryVelocityCmPerSecond, nullptr);
	BridgeTrajectoryState.QueryVelocityCmPerSecond = BridgePoseSearchQueryVelocityCmPerSecond;

	FVector SimulatedVelocity = BridgeTrajectoryState.AcceptedVelocityCmPerSecond;
	SimulatedVelocity.Z = 0.0f;

	const FVector DesiredVelocity = BridgeTrajectoryState.DesiredVelocityCmPerSecond;

	float DesiredYawDegrees = SimulatedFacing.Rotator().Yaw;
	if (BridgeIntentState.bHasDesiredFacing)
	{
		DesiredYawDegrees = BridgeIntentState.DesiredFacingYawDegrees;
	}
	else if (!DesiredVelocity.IsNearlyZero())
	{
		DesiredYawDegrees = DesiredVelocity.Rotation().Yaw;
	}
	else if (SimulatedVelocity.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		DesiredYawDegrees = SimulatedVelocity.Rotation().Yaw;
	}

	const FQuat DesiredFacing = FQuat(FRotator(0.0f, DesiredYawDegrees, 0.0f));

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

	const bool bBootstrapLocomotionHistory =
		BridgeIntentState.IntentMagnitude >= EffectiveSettings.BridgePoseSearchWalkIntentThreshold &&
		BridgeTrajectoryState.AcceptedVelocityCmPerSecond.Size2D() < EffectiveSettings.BridgePoseSearchSustainAcceptedSpeedThresholdCmPerSecond;

	if (bBootstrapLocomotionHistory)
	{
		AActor* const OwnerActor = GetOwner();
		FVector BootstrapVelocity = BridgeTrajectoryState.QueryVelocityCmPerSecond;
		if (BootstrapVelocity.IsNearlyZero())
		{
			BootstrapVelocity = BridgeTrajectoryState.DesiredVelocityCmPerSecond;
		}
		BootstrapVelocity.Z = 0.0f;

		if (OwnerActor && !BootstrapVelocity.IsNearlyZero() && GeneratedTrajectory.Samples.Num() > 0)
		{
			const int32 NumHistorySamples = FMath::Min(10, GeneratedTrajectory.Samples.Num() - 1);
			const int32 CurrentSampleIndex = FMath::Clamp(NumHistorySamples, 0, GeneratedTrajectory.Samples.Num() - 1);
			const FVector CurrentPosition = OwnerActor->GetActorLocation();
			const FQuat CurrentFacing = OwnerActor->GetActorQuat();
			const float HistorySampleIntervalSeconds = 0.04f;

			for (int32 SampleIndex = 0; SampleIndex <= CurrentSampleIndex; ++SampleIndex)
			{
				const float HistoryOffsetSeconds = static_cast<float>(CurrentSampleIndex - SampleIndex) * HistorySampleIntervalSeconds;
				FTransformTrajectorySample& Sample = GeneratedTrajectory.Samples[SampleIndex];
				Sample.Position = CurrentPosition - (BootstrapVelocity * HistoryOffsetSeconds);
				Sample.Facing = CurrentFacing;
			}
		}
	}

	BridgeTrajectoryState.QueryTrajectory = GeneratedTrajectory;
	BridgeTrajectoryState.bInitialized = true;
	BridgePoseSearchTrajectory = BridgeTrajectoryState.QueryTrajectory;
	BridgePoseSearchDesiredControllerYawLastUpdate = BridgeTrajectoryState.DesiredControllerYawLastUpdate;
	bBridgePoseSearchTrajectoryInitialized = true;
}

