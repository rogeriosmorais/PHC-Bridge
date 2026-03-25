#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::ValidatePhysicsControlAuthoring(FString& OutError) const
{
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutError = TEXT("Physics Control authoring validation requires an owning actor.");
		return false;
	}

	if (const UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
	{
		return PhysAnimComponentInternal::ValidateInitialPhysicsControlAuthoring(
			Stage1Initializer->InitialControls,
			Stage1Initializer->InitialBodyModifiers,
			OutError);
	}

	if (const UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
	{
		return PhysAnimComponentInternal::ValidateInitialPhysicsControlAuthoring(
			Initializer->InitialControls,
			Initializer->InitialBodyModifiers,
			OutError);
	}

	OutError = TEXT("Owning actor is missing a Stage 1 Physics Control initializer.");
	return false;
}


bool UPhysAnimComponent::ResolveRuntimeContext(FString& OutError)
{
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutError = TEXT("PhysAnimComponent has no owning actor.");
		return false;
	}

	USkeletalMeshComponent* const SkeletalMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Owning actor is missing a skeletal mesh component.");
		return false;
	}

	UPhysicsControlComponent* const PhysicsControl = OwnerActor->FindComponentByClass<UPhysicsControlComponent>();
	if (!PhysicsControl)
	{
		OutError = TEXT("Owning actor is missing a pre-authored Physics Control component.");
		return false;
	}

	UAnimInstance* const LocalAnimInstance = SkeletalMesh->GetAnimInstance();
	if (!LocalAnimInstance)
	{
		OutError = TEXT("The live AnimInstance was not resolved from the skeletal mesh.");
		return false;
	}

	MeshComponent = SkeletalMesh;
	PhysicsControlComponent = PhysicsControl;
	this->AnimInstance = LocalAnimInstance;

	AddTickPrerequisiteComponent(SkeletalMesh);
	PhysicsControl->SetComponentTickEnabled(false);
	return true;
}


bool UPhysAnimComponent::ValidateRequiredBodies(FString& OutError) const
{
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved.");
		return false;
	}

	const FString MeshAssetPath = GetPathNameSafe(SkeletalMesh->GetSkeletalMeshAsset());
	if (MeshAssetPath != PhysAnimComponentInternal::ExpectedMeshPath)
	{
		OutError = FString::Printf(TEXT("Expected Manny mesh '%s' but found '%s'."), PhysAnimComponentInternal::ExpectedMeshPath, *MeshAssetPath);
		return false;
	}

	const UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	const FString PhysicsAssetPath = PhysicsAsset ? PhysicsAsset->GetPathName() : FString();
	if (PhysicsAssetPath != PhysAnimComponentInternal::ExpectedPhysicsAssetPath)
	{
		OutError = FString::Printf(TEXT("Expected physics asset '%s' but found '%s'."), PhysAnimComponentInternal::ExpectedPhysicsAssetPath, *PhysicsAssetPath);
		return false;
	}

	TArray<FName> MissingBodies;
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		if (SkeletalMesh->GetBoneIndex(BoneName) == INDEX_NONE || SkeletalMesh->GetBodyInstance(BoneName) == nullptr)
		{
			MissingBodies.Add(BoneName);
		}
	}

	if (MissingBodies.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Missing required physics bodies: %s"), *PhysAnimComponentInternal::JoinNames(MissingBodies));
		return false;
	}

	return true;
}


bool UPhysAnimComponent::ValidateRuntimePhysicsControl(FString& OutError) const
{
	const UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Physics Control component was not resolved.");
		return false;
	}

	TArray<FName> MissingControls;
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		if (!PhysicsControl->GetControlExists(ControlName))
		{
			MissingControls.Add(ControlName);
		}
	}

	TArray<FName> MissingModifiers;
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			MissingModifiers.Add(ModifierName);
		}
	}

	if (MissingControls.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Missing required runtime controls: %s"), *PhysAnimComponentInternal::JoinNames(MissingControls));
		return false;
	}

	if (MissingModifiers.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Missing required runtime body modifiers: %s"), *PhysAnimComponentInternal::JoinNames(MissingModifiers));
		return false;
	}

	return true;
}


bool UPhysAnimComponent::ValidatePoseSearchIntegration(FString& OutError)
{
	const UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	if (!LocalAnimInstance)
	{
		OutError = TEXT("AnimInstance was not resolved.");
		return false;
	}

	const FString AnimClassPath = GetPathNameSafe(LocalAnimInstance->GetClass());
	if (AnimClassPath != PhysAnimComponentInternal::ExpectedAnimBlueprintPath)
	{
		OutError = FString::Printf(TEXT("Expected AnimBlueprint '%s' but found '%s'."), PhysAnimComponentInternal::ExpectedAnimBlueprintPath, *AnimClassPath);
		return false;
	}

	if (UPoseSearchLibrary::FindPoseHistoryNode(PhysAnimComponentInternal::PoseHistoryName, LocalAnimInstance) == nullptr)
	{
		OutError = TEXT("PoseHistory_Stage1 was not found on the live AnimInstance.");
		return false;
	}

	LoadedPoseSearchDatabase = LoadObject<UPoseSearchDatabase>(nullptr, PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath);
	if (!LoadedPoseSearchDatabase)
	{
		OutError = FString::Printf(TEXT("Failed to load PoseSearch database '%s'."), PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath);
		return false;
	}

	if (GetPathNameSafe(LoadedPoseSearchDatabase) != PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath)
	{
		OutError = FString::Printf(
			TEXT("Expected PoseSearch database '%s' but found '%s'."),
			PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath,
			*GetPathNameSafe(LoadedPoseSearchDatabase));
		return false;
	}

	if (GetPathNameSafe(LoadedPoseSearchDatabase->Schema.Get()) != PhysAnimComponentInternal::ExpectedPoseSearchSchemaPath)
	{
		OutError = FString::Printf(
			TEXT("Expected PoseSearch schema '%s' but found '%s'."),
			PhysAnimComponentInternal::ExpectedPoseSearchSchemaPath,
			*GetPathNameSafe(LoadedPoseSearchDatabase->Schema.Get()));
		return false;
	}

	return true;
}

