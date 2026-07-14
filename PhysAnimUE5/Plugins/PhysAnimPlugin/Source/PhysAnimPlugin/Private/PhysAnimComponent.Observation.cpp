#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimProtoMannyAdapter.h"
#include "PhysicsEngine/BodyInstance.h"

bool UPhysAnimComponent::GatherCurrentBodySamples(TArray<FPhysAnimBodySample>& OutBodySamples, FString& OutError) const
{
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved.");
		return false;
	}

	OutBodySamples.Reset();
	OutBodySamples.Reserve(PhysAnimBridge::NumSmplBodies);
	if (CachedSmplObservationRestBodyComponentRotations.Num() != PhysAnimBridge::NumSmplBodies)
	{
		OutError = TEXT("The synchronized T-pose PhysicsAsset body-frame calibration is incomplete.");
		return false;
	}
	if (CachedSmplObservationRestComponentTransforms.Num() != PhysAnimBridge::NumSmplBodies)
	{
		OutError = TEXT("The T-pose bone-frame calibration is incomplete.");
		return false;
	}
	TArray<FQuat> CurrentBodyComponentRotations;
	CurrentBodyComponentRotations.Reserve(PhysAnimBridge::NumSmplBodies);
	FQuat CurrentRootBoneComponentRotation = FQuat::Identity;

	const TArray<FName>& BoneNames = PhysAnimBridge::GetSmplObservationBoneNames();
	const FQuat MeshWorldRotation = SkeletalMesh->GetComponentQuat().GetNormalized();
	const FQuat WorldToComponentRotation = MeshWorldRotation.Inverse();
	USkeletalMeshComponent* const MutableMesh = const_cast<USkeletalMeshComponent*>(SkeletalMesh);

	for (int32 i = 0; i < BoneNames.Num(); ++i)
	{
		const FName& BoneName = BoneNames[i];
		if (SkeletalMesh->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Missing observation bone '%s' on the skeletal mesh."), *BoneName.ToString());
			return false;
		}

		const FTransform BoneWorldTransform = SkeletalMesh->GetBoneTransform(BoneName, RTS_World);
		if (i == 0)
		{
			CurrentRootBoneComponentRotation =
				(WorldToComponentRotation * BoneWorldTransform.GetRotation().GetNormalized()).GetNormalized();
		}
		const FVector BoneLinearVelocity = MutableMesh->GetPhysicsLinearVelocity(BoneName);
		const FVector BoneAngularVelocity = MutableMesh->GetPhysicsAngularVelocityInRadians(BoneName);
		const FBodyInstance* const BodyInstance = MutableMesh->GetBodyInstance(BoneName);
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			OutError = FString::Printf(
				TEXT("Missing synchronized physics body for observation bone '%s'."),
				*BoneName.ToString());
			return false;
		}
		const FQuat BodyWorldRotation =
			BodyInstance->GetUnrealWorldTransform().GetRotation().GetNormalized();
		CurrentBodyComponentRotations.Add(
			(WorldToComponentRotation * BodyWorldRotation).GetNormalized());
		OutBodySamples.Add(FPhysAnimBodySample(
			PhysAnimBridge::UeWorldPositionToProtoRuntime(BoneWorldTransform.GetLocation()),
			FQuat::Identity,
			PhysAnimBridge::UeWorldVelocityToProtoRuntime(BoneLinearVelocity),
			PhysAnimBridge::UeWorldRotationVectorToProtoRuntime(BoneAngularVelocity)));
	}
	const FQuat RootCanonicalRotation =
		PhysAnimProtoMannyAdapter::BuildCanonicalSmplRootRotationFromBonePose(
			MeshWorldRotation,
			CachedSmplObservationRestComponentTransforms[0].GetRotation(),
			CurrentRootBoneComponentRotation);

	TArray<FQuat> CanonicalGlobalRotations;
	FString AdapterError;
	if (!PhysAnimProtoMannyAdapter::BuildCanonicalSmplRotationsFromBodyPose(
		RootCanonicalRotation,
		CachedSmplObservationRestBodyComponentRotations,
		CurrentBodyComponentRotations,
		CanonicalGlobalRotations,
		AdapterError))
	{
		OutBodySamples.Reset();
		OutError = FString::Printf(TEXT("Could not adapt live Manny bone rotations to SMPL: %s"), *AdapterError);
		return false;
	}

	for (int32 BodyIndex = 0; BodyIndex < OutBodySamples.Num(); ++BodyIndex)
	{
		OutBodySamples[BodyIndex].Rotation =
			PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CanonicalGlobalRotations[BodyIndex]);
	}

	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void UPhysAnimComponent::CaptureStartupChronologySampleForTesting(const TCHAR* Stage)
{
	const bool bRealOnnxPolicy = StandingVariantForTesting == EPhysAnimStandingVariant::RealOnnxPolicy;
	const bool bCaptureStartupChronology =
		bStartupChronologyTraceEnabledForTesting &&
		bRealOnnxPolicy &&
		!StartupChronologyTrace.bComplete &&
		StartupChronologyTrace.CaptureError.IsEmpty();
	const bool bCaptureFirstPolicyPriorSource =
		(bStartupChronologyTraceEnabledForTesting ||
			bReplayPriorBodySamplesAtFirstInferenceForTesting) &&
		bRealOnnxPolicy &&
		!FirstPolicyBodySourceTrace.Prior.bRecorded &&
		FirstPolicyBodySourceTrace.ValidationError.IsEmpty();
	if (!bCaptureStartupChronology && !bCaptureFirstPolicyPriorSource)
	{
		return;
	}

	const AActor* const OwnerActor = GetOwner();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	const UWorld* const World = GetWorld();
	if (!Stage || !OwnerActor || !SkeletalMesh || !World)
	{
		StartupChronologyTrace.CaptureError = TEXT("Runtime context was incomplete during startup chronology capture.");
		return;
	}

	TArray<FPhysAnimBodySample> BodySamples;
	FString CaptureError;
	if (!GatherCurrentBodySamples(BodySamples, CaptureError))
	{
		StartupChronologyTrace.CaptureError = CaptureError;
		return;
	}

	FirstPolicyBodySourceTrace.CapturePriorIf(
		bCaptureFirstPolicyPriorSource &&
			FCString::Strcmp(Stage, TEXT("pre_state_machine")) == 0 &&
			RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch,
		Stage,
		World->GetTimeSeconds(),
		GetRuntimeStateName(RuntimeState),
		PolicyControlTicksExecuted,
		BodySamples);
	if (!bCaptureStartupChronology)
	{
		return;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (SkeletalMesh->GetBoneIndex(RootBoneName) == INDEX_NONE)
	{
		StartupChronologyTrace.CaptureError = FString::Printf(
			TEXT("Root observation bone '%s' was missing during startup chronology capture."),
			*RootBoneName.ToString());
		return;
	}

	const bool bCaptured = StartupChronologyTrace.CaptureIf(
		true,
		Stage,
		World->GetTimeSeconds(),
		GetRuntimeStateName(RuntimeState),
		PolicyUpdateAccumulatorSeconds,
		LastPolicyElapsedSteps,
		PolicyControlTicksExecuted,
		FirstPolicyInputProvenanceSnapshot.bCaptured,
		OwnerActor->GetActorTransform(),
		SkeletalMesh->GetComponentTransform(),
		SkeletalMesh->GetBoneTransform(RootBoneName, RTS_World),
		BodySamples);
	if (!bCaptured && !StartupChronologyTrace.bComplete)
	{
		StartupChronologyTrace.CaptureError = TEXT("Startup chronology reached its fixed sample bound before the first policy tick.");
	}
}
#endif


float UPhysAnimComponent::ResolveSelfObservationGroundHeight(const TArray<FPhysAnimBodySample>& CurrentBodySamples) const
{
	if (!CurrentBodySamples.IsValidIndex(0))
	{
		return 0.0f;
	}

	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	const ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	const UCapsuleComponent* const CapsuleComponent = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	if (!SkeletalMesh)
	{
		return 0.0f;
	}

	const FVector RootWorldLocation = SkeletalMesh->GetBoneLocation(PhysAnimBridge::GetRootBoneName());
	FHitResult StaticGroundHit;
	bool bHasStaticGroundTrace = false;
	if (UWorld* const World = GetWorld())
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysAnimSelfObservationGround), false);
		if (const AActor* const Owner = GetOwner())
		{
			QueryParams.AddIgnoredActor(Owner);
		}

		const FVector TraceStart = RootWorldLocation + FVector(0.0, 0.0, PhysAnimComponentInternal::TerrainTraceStartAboveRootCm);
		const FVector TraceEnd = RootWorldLocation - FVector(0.0, 0.0, PhysAnimComponentInternal::TerrainTraceEndBelowRootCm);
		bHasStaticGroundTrace = World->LineTraceSingleByObjectType(
			StaticGroundHit,
			TraceStart,
			TraceEnd,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects),
			QueryParams) && StaticGroundHit.IsValidBlockingHit();
	}

	const FFindFloorResult* const CurrentFloor = CharacterMovement ? &CharacterMovement->CurrentFloor : nullptr;
	const float GroundWorldZ = ResolveObservationGroundWorldZ(
		bHasStaticGroundTrace,
		StaticGroundHit.ImpactPoint.Z,
		CurrentFloor && CurrentFloor->IsWalkableFloor(),
		CurrentFloor && CurrentFloor->HitResult.IsValidBlockingHit(),
		CurrentFloor ? CurrentFloor->HitResult.ImpactPoint.Z : 0.0f,
		CapsuleComponent ? CapsuleComponent->GetComponentLocation().Z : 0.0f,
		CapsuleComponent ? CapsuleComponent->GetScaledCapsuleHalfHeight() : 0.0f,
		CurrentFloor ? CurrentFloor->GetDistanceToFloor() : 0.0f,
		0.0f);

	return ResolveSelfObservationSyntheticGroundHeight(
		CurrentBodySamples[0].Position.Z,
		RootWorldLocation.Z,
		GroundWorldZ);
}


bool UPhysAnimComponent::BuildTerrainObservation(
	const TArray<FPhysAnimBodySample>& CurrentBodySamples,
	TArray<float>& OutTerrain,
	FString& OutError,
	TArray<float>* OutGroundHeightsForDiagnostics,
	FTransform* OutRootWorldTransformForDiagnostics) const
{
	if (!CurrentBodySamples.IsValidIndex(0))
	{
		OutError = TEXT("Cannot build terrain observation without a root body sample.");
		return false;
	}

	const FPhysAnimBodySample& RootSample = CurrentBodySamples[0];
	const float GroundHeight = ResolveSelfObservationGroundHeight(CurrentBodySamples);
	const USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!Mesh)
	{
		OutError = TEXT("Cannot build terrain observation without a skeletal mesh component.");
		return false;
	}
	const FTransform RootWorldTransform = Mesh->GetSocketTransform(PhysAnimBridge::GetRootBoneName(), RTS_World);
	if (OutRootWorldTransformForDiagnostics)
	{
		*OutRootWorldTransformForDiagnostics = RootWorldTransform;
	}

	TArray<float> SampleGroundHeights;
	if (!SampleTerrainGroundHeights(
		RootWorldTransform.GetLocation(),
		RootWorldTransform.GetRotation(),
		GroundHeight,
		SampleGroundHeights,
		OutError))
	{
		return false;
	}
	if (OutGroundHeightsForDiagnostics)
	{
		*OutGroundHeightsForDiagnostics = SampleGroundHeights;
	}

	return PhysAnimBridge::BuildTerrainObservation(
		RootSample.Position.Z,
		SampleGroundHeights,
		OutTerrain,
		OutError);
}


bool UPhysAnimComponent::SampleTerrainGroundHeights(
	const FVector& RootLocation,
	const FQuat& RootRotation,
	float FallbackGroundHeight,
	TArray<float>& OutGroundHeights,
	FString& OutError) const
{
	UWorld* const World = GetWorld();
	if (!World)
	{
		OutError = TEXT("Cannot sample terrain observation without a valid world.");
		return false;
	}

	const TArray<FVector2D>& TerrainSampleOffsets = PhysAnimBridge::GetTerrainSampleOffsets();
	if (TerrainSampleOffsets.Num() != PhysAnimBridge::TerrainSize)
	{
		OutError = FString::Printf(
			TEXT("Expected %d terrain sample offsets but found %d."),
			PhysAnimBridge::TerrainSize,
			TerrainSampleOffsets.Num());
		return false;
	}

	OutGroundHeights.SetNumUninitialized(TerrainSampleOffsets.Num());

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysAnimTerrainObservation), false);
	if (const AActor* const Owner = GetOwner())
	{
		QueryParams.AddIgnoredActor(Owner);
	}

	const FCollisionObjectQueryParams ObjectQueryParams(FCollisionObjectQueryParams::InitType::AllStaticObjects);
	for (int32 SampleIndex = 0; SampleIndex < TerrainSampleOffsets.Num(); ++SampleIndex)
	{
		const FVector SampleLocation = PhysAnimBridge::BuildTerrainSampleWorldLocation(
			RootLocation,
			RootRotation,
			TerrainSampleOffsets[SampleIndex]);
		const FVector TraceStart(SampleLocation.X, SampleLocation.Y, RootLocation.Z + PhysAnimComponentInternal::TerrainTraceStartAboveRootCm);
		const FVector TraceEnd(SampleLocation.X, SampleLocation.Y, RootLocation.Z - PhysAnimComponentInternal::TerrainTraceEndBelowRootCm);

		FHitResult HitResult;
		if (World->LineTraceSingleByObjectType(HitResult, TraceStart, TraceEnd, ObjectQueryParams, QueryParams)
			&& HitResult.IsValidBlockingHit())
		{
			OutGroundHeights[SampleIndex] = HitResult.ImpactPoint.Z * PhysAnimBridge::CmToMeters;
		}
		else
		{
			OutGroundHeights[SampleIndex] = FallbackGroundHeight;
		}
	}

	return true;
}

