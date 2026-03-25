#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

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

	const TArray<FName>& BoneNames = PhysAnimBridge::GetSmplObservationBoneNames();
	const FQuat MeshWorldRotation = SkeletalMesh->GetComponentQuat();

	for (int32 i = 0; i < BoneNames.Num(); ++i)
	{
		const FName& BoneName = BoneNames[i];
		if (SkeletalMesh->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Missing observation bone '%s' on the skeletal mesh."), *BoneName.ToString());
			return false;
		}

		const FTransform BoneWorldTransform = SkeletalMesh->GetBoneTransform(BoneName, RTS_World);
		USkeletalMeshComponent* const MutableMesh = const_cast<USkeletalMeshComponent*>(SkeletalMesh);
		const FVector BoneLinearVelocity = MutableMesh->GetPhysicsLinearVelocity(BoneName);
		const FVector BoneAngularVelocity = MutableMesh->GetPhysicsAngularVelocityInRadians(BoneName);

		FTransform CurrentComponentTransform = BoneWorldTransform.GetRelativeTransform(SkeletalMesh->GetComponentTransform());
		CurrentComponentTransform.NormalizeRotation();

		FQuat CorrectedComponentRotation = CurrentComponentTransform.GetRotation().GetNormalized();
		if (CachedSmplObservationRestComponentTransforms.IsValidIndex(i))
		{
			const FMatrix CurrentComponentMatrix = CurrentComponentTransform.ToMatrixNoScale();
			const FMatrix RestComponentMatrix = CachedSmplObservationRestComponentTransforms[i].ToMatrixNoScale();
			const FMatrix CorrectedComponentMatrix = CurrentComponentMatrix * RestComponentMatrix.InverseFast();
			CorrectedComponentRotation = FQuat(CorrectedComponentMatrix).GetNormalized();
		}

		const FQuat CorrectedWorldRotation =
			(MeshWorldRotation * CorrectedComponentRotation).GetNormalized();

		OutBodySamples.Add(FPhysAnimBodySample(
			PhysAnimBridge::UeWorldPositionToProtoRuntime(BoneWorldTransform.GetLocation()),
			PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CorrectedWorldRotation),
			PhysAnimBridge::UeWorldVelocityToProtoRuntime(BoneLinearVelocity),
			PhysAnimBridge::UeWorldRotationVectorToProtoRuntime(BoneAngularVelocity)));
	}

	return true;
}


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
	if (!SkeletalMesh || !CharacterMovement || !CapsuleComponent)
	{
		return 0.0f;
	}

	const FVector RootWorldLocation = SkeletalMesh->GetBoneLocation(PhysAnimBridge::GetRootBoneName());
	const FFindFloorResult& CurrentFloor = CharacterMovement->CurrentFloor;
	const float GroundWorldZ = ResolveObservationGroundWorldZFromFloor(
		CurrentFloor.IsWalkableFloor(),
		CurrentFloor.HitResult.IsValidBlockingHit(),
		CurrentFloor.HitResult.ImpactPoint.Z,
		CapsuleComponent->GetComponentLocation().Z,
		CapsuleComponent->GetScaledCapsuleHalfHeight(),
		CurrentFloor.GetDistanceToFloor(),
		0.0f);

	return ResolveSelfObservationSyntheticGroundHeight(
		CurrentBodySamples[0].Position.Z,
		RootWorldLocation.Z,
		GroundWorldZ);
}


bool UPhysAnimComponent::BuildTerrainObservation(
	const TArray<FPhysAnimBodySample>& CurrentBodySamples,
	TArray<float>& OutTerrain,
	FString& OutError) const
{
	if (!CurrentBodySamples.IsValidIndex(0))
	{
		OutError = TEXT("Cannot build terrain observation without a root body sample.");
		return false;
	}

	const FPhysAnimBodySample& RootSample = CurrentBodySamples[0];
	const float GroundHeight = ResolveSelfObservationGroundHeight(CurrentBodySamples);

	TArray<float> SampleGroundHeights;
	if (!SampleTerrainGroundHeights(
		RootSample.Position,
		RootSample.Rotation,
		GroundHeight,
		SampleGroundHeights,
		OutError))
	{
		return false;
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
	const FQuat RootYawRotation = FRotator(0.0f, RootRotation.Rotator().Yaw, 0.0f).Quaternion();

	for (int32 SampleIndex = 0; SampleIndex < TerrainSampleOffsets.Num(); ++SampleIndex)
	{
		const FVector LocalOffset(TerrainSampleOffsets[SampleIndex].X, TerrainSampleOffsets[SampleIndex].Y, 0.0f);
		const FVector SampleLocation = RootLocation + RootYawRotation.RotateVector(LocalOffset);
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

