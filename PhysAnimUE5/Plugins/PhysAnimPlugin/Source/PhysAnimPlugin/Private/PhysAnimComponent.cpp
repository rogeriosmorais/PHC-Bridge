#include "PhysAnimComponent.h"

#include "PhysAnimBridge.h"
#include "PhysAnimStage1InitializerComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Logging/LogMacros.h"
#include "NNEStatus.h"
#include "PhysicsControlActor.h"
#include "PhysicsControlComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PoseSearch/PoseSearchAssetSamplerLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PhysicsEngine/BodyInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

namespace PhysAnimComponentInternal
{
	const FName PoseHistoryName(TEXT("PoseHistory_Stage1"));
	const TCHAR* ExpectedMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* ExpectedPhysicsAssetPath = TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin");
	const TCHAR* ExpectedAnimBlueprintPath = TEXT("/Game/Characters/Mannequins/Animations/ABP_PhysAnim.ABP_PhysAnim_C");
	const TCHAR* ExpectedPoseSearchDatabasePath = TEXT("/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion.PSDB_Stage1_Locomotion");
	const TCHAR* ExpectedPoseSearchSchemaPath = TEXT("/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion.PSS_Stage1_Locomotion");
	const TCHAR* DefaultModelPath = TEXT("/Game/NNEModels/phc_policy.phc_policy");
	const TCHAR* PreferredGpuRuntime = TEXT("NNERuntimeORTDml");
	const TCHAR* FallbackCpuRuntime = TEXT("NNERuntimeORTCpu");

	FString JoinNames(const TArray<FName>& Names)
	{
		TArray<FString> Strings;
		Strings.Reserve(Names.Num());
		for (const FName Name : Names)
		{
			Strings.Add(Name.ToString());
		}
		return FString::Join(Strings, TEXT(", "));
	}
}

UPhysAnimComponent::UPhysAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	ModelDataAsset = TSoftObjectPtr<UNNEModelData>(FSoftObjectPath(PhysAnimComponentInternal::DefaultModelPath));
}

void UPhysAnimComponent::BeginPlay()
{
	Super::BeginPlay();
	StartBridge();
}

void UPhysAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBridge();
	Super::EndPlay(EndPlayReason);
}

void UPhysAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bBridgeActive)
	{
		return;
	}

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	if (!PhysicsControl || !SkeletalMesh || !LocalAnimInstance)
	{
		FailStop(TEXT("Runtime context became invalid after startup."));
		return;
	}

	PhysicsControl->UpdateTargetCaches(DeltaTime);
	PhysicsControl->GetCachedBoneTransforms(SkeletalMesh, PhysAnimBridge::GetControlledBoneNames());

	FString TickError;
	FPoseSearchBlueprintResult SearchResult;
	if (QueryPoseSearch(SearchResult, TickError))
	{
		LastValidPoseSearchResult = SearchResult;
		ConsecutiveInvalidPoseSearchFrames = 0;
	}
	else
	{
		++ConsecutiveInvalidPoseSearchFrames;
		if (ConsecutiveInvalidPoseSearchFrames > 1 || LastValidPoseSearchResult.SelectedAnim == nullptr)
		{
			FailStop(FString::Printf(TEXT("PoseSearch query was invalid for two consecutive ticks. %s"), *TickError));
			return;
		}

		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] Reusing last valid PoseSearch result for one grace tick. Reason: %s"), *TickError);
		SearchResult = LastValidPoseSearchResult;
	}

	TArray<FPhysAnimBodySample> CurrentBodySamples;
	if (!GatherCurrentBodySamples(CurrentBodySamples, TickError))
	{
		FailStop(TickError);
		return;
	}

	TArray<FPhysAnimFuturePoseSample> FuturePoseSamples;
	if (!SampleFuturePoses(SearchResult, FuturePoseSamples, TickError))
	{
		FailStop(TickError);
		return;
	}

	if (!PhysAnimBridge::BuildSelfObservation(CurrentBodySamples, 0.0f, SelfObservationBuffer, TickError))
	{
		FailStop(TickError);
		return;
	}

	if (!PhysAnimBridge::BuildMimicTargetPoses(CurrentBodySamples, FuturePoseSamples, MimicTargetPosesBuffer, TickError))
	{
		FailStop(TickError);
		return;
	}

	PhysAnimBridge::BuildZeroTerrain(TerrainBuffer);

	if (!RunInference(TickError))
	{
		FailStop(TickError);
		return;
	}

	ApplyControlTargets(DeltaTime, TickError);
	if (!TickError.IsEmpty())
	{
		FailStop(TickError);
		return;
	}

	PhysicsControl->UpdateControls(DeltaTime);
}

bool UPhysAnimComponent::StartBridge()
{
	if (bBridgeActive)
	{
		return true;
	}

	FString Error;
	if (!ResolveRuntimeContext(Error) ||
		!ValidateRequiredBodies(Error) ||
		!EnsurePreauthoredPhysicsControl(Error) ||
		!ValidatePoseSearchIntegration(Error) ||
		!InitializeModel(Error))
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Startup blocked: %s"), *Error);
		SetComponentTickEnabled(false);
		bBridgeActive = false;
		return false;
	}

	bBridgeActive = true;
	bStartupReported = true;
	SetComponentTickEnabled(true);

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Startup success. Runtime=%s Model=%s"),
		*ActiveRuntimeName,
		*GetPathNameSafe(LoadedModelData));
	return true;
}

bool UPhysAnimComponent::EnsurePreauthoredPhysicsControl(FString& OutError)
{
	FString ValidationError;
	if (ValidatePreauthoredPhysicsControl(ValidationError))
	{
		return true;
	}

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!PhysicsControl || !OwnerActor)
	{
		OutError = ValidationError.IsEmpty() ? TEXT("Physics Control creation requires both the owning actor and Physics Control component.") : ValidationError;
		return false;
	}

	if (UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
	{
		Stage1Initializer->CreateControls(PhysicsControl);
		return ValidatePreauthoredPhysicsControl(OutError);
	}

	if (UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
	{
		Initializer->CreateControls(PhysicsControl);
		return ValidatePreauthoredPhysicsControl(OutError);
	}

	OutError = ValidationError;
	return false;
}

void UPhysAnimComponent::StopBridge()
{
	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		const TArray<FName> BodyModifierNames = PhysicsControl->GetAllBodyModifierNames();
		if (BodyModifierNames.Num() > 0)
		{
			PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(BodyModifierNames);
		}
		PhysicsControl->SetCachedBoneVelocitiesToZero();
	}

	bBridgeActive = false;
	SetComponentTickEnabled(false);
	ConsecutiveInvalidPoseSearchFrames = 0;
	LastValidPoseSearchResult = FPoseSearchBlueprintResult();
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

bool UPhysAnimComponent::ValidatePreauthoredPhysicsControl(FString& OutError) const
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
		OutError = FString::Printf(TEXT("Missing required pre-authored controls: %s"), *PhysAnimComponentInternal::JoinNames(MissingControls));
		return false;
	}

	if (MissingModifiers.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Missing required pre-authored body modifiers: %s"), *PhysAnimComponentInternal::JoinNames(MissingModifiers));
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

bool UPhysAnimComponent::QueryPoseSearch(FPoseSearchBlueprintResult& OutSearchResult, FString& OutError)
{
	UAnimInstance* const LocalAnimInstance = AnimInstance.Get();
	if (!LocalAnimInstance)
	{
		OutError = TEXT("AnimInstance was not resolved.");
		return false;
	}

	if (!LoadedPoseSearchDatabase)
	{
		OutError = TEXT("PoseSearch database was not loaded.");
		return false;
	}

	TArray<UObject*> AssetsToSearch;
	AssetsToSearch.Add(LoadedPoseSearchDatabase);

	FPoseSearchContinuingProperties ContinuingProperties;
	if (LastValidPoseSearchResult.SelectedAnim != nullptr)
	{
		ContinuingProperties.InitFrom(LastValidPoseSearchResult, EPoseSearchInterruptMode::DoNotInterrupt);
	}

	FPoseSearchFutureProperties FutureProperties;
	OutSearchResult = FPoseSearchBlueprintResult();
	UPoseSearchLibrary::MotionMatch(
		LocalAnimInstance,
		AssetsToSearch,
		PhysAnimComponentInternal::PoseHistoryName,
		ContinuingProperties,
		FutureProperties,
		OutSearchResult);

	if (OutSearchResult.SelectedAnim == nullptr)
	{
		OutError = TEXT("UPoseSearchLibrary::MotionMatch returned no selected animation.");
		return false;
	}

	return true;
}

bool UPhysAnimComponent::InitializeModel(FString& OutError)
{
	LoadedModelData = ModelDataAsset.LoadSynchronous();
	if (!LoadedModelData)
	{
		OutError = FString::Printf(TEXT("Failed to load model asset '%s'."), *ModelDataAsset.ToSoftObjectPath().ToString());
		return false;
	}

	RuntimeGPU = UE::NNE::GetRuntime<INNERuntimeGPU>(PhysAnimComponentInternal::PreferredGpuRuntime);
	if (RuntimeGPU.IsValid() && RuntimeGPU->CanCreateModelGPU(LoadedModelData) == UE::NNE::EResultStatus::Ok)
	{
		ModelGPU = RuntimeGPU->CreateModelGPU(LoadedModelData);
		if (ModelGPU.IsValid())
		{
			ModelInstanceGPU = ModelGPU->CreateModelInstanceGPU();
		}
		if (ModelInstanceGPU.IsValid())
		{
			ActiveRuntimeName = PhysAnimComponentInternal::PreferredGpuRuntime;
			return ValidateModelDescriptorContract(OutError);
		}
	}

	RuntimeCPU = UE::NNE::GetRuntime<INNERuntimeCPU>(PhysAnimComponentInternal::FallbackCpuRuntime);
	if (RuntimeCPU.IsValid() && RuntimeCPU->CanCreateModelCPU(LoadedModelData) == UE::NNE::EResultStatus::Ok)
	{
		ModelCPU = RuntimeCPU->CreateModelCPU(LoadedModelData);
		if (ModelCPU.IsValid())
		{
			ModelInstanceCPU = ModelCPU->CreateModelInstanceCPU();
		}
		if (ModelInstanceCPU.IsValid())
		{
			ActiveRuntimeName = PhysAnimComponentInternal::FallbackCpuRuntime;
			return ValidateModelDescriptorContract(OutError);
		}
	}

	OutError = TEXT("Could not create an NNE model instance from NNERuntimeORTDml or NNERuntimeORTCpu.");
	return false;
}

bool UPhysAnimComponent::ValidateModelDescriptorContract(FString& OutError)
{
	UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync();
	if (!ModelInstance)
	{
		OutError = TEXT("No active model instance exists.");
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorDesc> InputDescsView = ModelInstance->GetInputTensorDescs();
	TArray<UE::NNE::FTensorDesc> InputDescs;
	InputDescs.Append(InputDescsView.GetData(), InputDescsView.Num());

	if (!PhysAnimBridge::BuildInputTensorIndexMap(InputDescs, TensorIndexMap, OutError))
	{
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorDesc> OutputDescs = ModelInstance->GetOutputTensorDescs();
	if (OutputDescs.Num() != 1)
	{
		OutError = FString::Printf(TEXT("Expected exactly one output tensor but found %d."), OutputDescs.Num());
		return false;
	}

	SelfObservationBuffer.Init(0.0f, PhysAnimBridge::SelfObsSize);
	MimicTargetPosesBuffer.Init(0.0f, PhysAnimBridge::MimicTargetPosesSize);
	TerrainBuffer.Init(0.0f, PhysAnimBridge::TerrainSize);
	ActionOutputBuffer.Init(0.0f, PhysAnimBridge::NumActionFloats);

	TArray<UE::NNE::FTensorShape> InputShapes;
	InputShapes.SetNum(InputDescs.Num());
	InputShapes[TensorIndexMap.SelfObs] = UE::NNE::FTensorShape::Make({1u, static_cast<uint32>(PhysAnimBridge::SelfObsSize)});
	InputShapes[TensorIndexMap.MimicTargetPoses] = UE::NNE::FTensorShape::Make({1u, static_cast<uint32>(PhysAnimBridge::MimicTargetPosesSize)});
	InputShapes[TensorIndexMap.Terrain] = UE::NNE::FTensorShape::Make({1u, static_cast<uint32>(PhysAnimBridge::TerrainSize)});

	if (ModelInstance->SetInputTensorShapes(InputShapes) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("SetInputTensorShapes failed for the locked Stage 1 tensor contract.");
		return false;
	}

	InputBindings.SetNum(InputDescs.Num());
	InputBindings[TensorIndexMap.SelfObs] = { SelfObservationBuffer.GetData(), static_cast<uint64>(SelfObservationBuffer.Num() * sizeof(float)) };
	InputBindings[TensorIndexMap.MimicTargetPoses] = { MimicTargetPosesBuffer.GetData(), static_cast<uint64>(MimicTargetPosesBuffer.Num() * sizeof(float)) };
	InputBindings[TensorIndexMap.Terrain] = { TerrainBuffer.GetData(), static_cast<uint64>(TerrainBuffer.Num() * sizeof(float)) };

	OutputBindings.SetNum(1);
	OutputBindings[0] = { ActionOutputBuffer.GetData(), static_cast<uint64>(ActionOutputBuffer.Num() * sizeof(float)) };
	return true;
}

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

	for (const FName BoneName : PhysAnimBridge::GetSmplObservationBoneNames())
	{
		if (SkeletalMesh->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Missing observation bone '%s' on the skeletal mesh."), *BoneName.ToString());
			return false;
		}

		const FTransform BoneWorldTransform = SkeletalMesh->GetBoneTransform(BoneName, RTS_World);

		FPhysAnimBodySample BodySample;
		BodySample.Position = PhysAnimBridge::UeVectorToSmpl(BoneWorldTransform.GetLocation());
		BodySample.Rotation = PhysAnimBridge::UeQuaternionToSmpl(BoneWorldTransform.GetRotation());
		USkeletalMeshComponent* const MutableMesh = const_cast<USkeletalMeshComponent*>(SkeletalMesh);
		BodySample.LinearVelocity = PhysAnimBridge::UeVectorToSmpl(MutableMesh->GetPhysicsLinearVelocity(BoneName));
		BodySample.AngularVelocity = PhysAnimBridge::UeVectorToSmpl(MutableMesh->GetPhysicsAngularVelocityInRadians(BoneName));
		OutBodySamples.Add(BodySample);
	}

	return true;
}

bool UPhysAnimComponent::SampleFuturePoses(
	const FPoseSearchBlueprintResult& SearchResult,
	TArray<FPhysAnimFuturePoseSample>& OutFutureSamples,
	FString& OutError) const
{
	const UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!LocalAnimInstance || !SkeletalMesh)
	{
		OutError = TEXT("Pose sampling requires both the AnimInstance and skeletal mesh.");
		return false;
	}

	const UAnimationAsset* const AnimationAsset = Cast<UAnimationAsset>(SearchResult.SelectedAnim);
	if (!AnimationAsset)
	{
		OutError = TEXT("PoseSearch result did not return a UAnimationAsset.");
		return false;
	}

	const float AnimationLength = AnimationAsset->GetPlayLength();
	const TArray<float> FutureOffsets = PhysAnimBridge::BuildFutureSampleTimeSchedule();

	OutFutureSamples.Reset();
	OutFutureSamples.Reserve(FutureOffsets.Num());

	for (const float FutureOffset : FutureOffsets)
	{
		FPoseSearchAssetSamplerInput SamplerInput;
		SamplerInput.Animation = AnimationAsset;
		SamplerInput.AnimationTime = FMath::Clamp(SearchResult.SelectedTime + FutureOffset, 0.0f, AnimationLength);
		SamplerInput.bMirrored = SearchResult.bIsMirrored;
		SamplerInput.BlendParameters = SearchResult.BlendParameters;
		SamplerInput.RootTransformOrigin = SkeletalMesh->GetComponentTransform();

		FPoseSearchAssetSamplerPose SampledPose = UPoseSearchAssetSamplerLibrary::SamplePose(LocalAnimInstance, SamplerInput);

		FPhysAnimFuturePoseSample FutureSample;
		FutureSample.FutureTimeSeconds = FutureOffset;
		FutureSample.BodyTransforms.Reserve(PhysAnimBridge::NumSmplBodies);

		for (const FName BoneName : PhysAnimBridge::GetSmplObservationBoneNames())
		{
			const FTransform WorldTransform =
				UPoseSearchAssetSamplerLibrary::GetTransformByName(SampledPose, BoneName, EPoseSearchAssetSamplerSpace::World);
			FutureSample.BodyTransforms.Add(FTransform(
				PhysAnimBridge::UeQuaternionToSmpl(WorldTransform.GetRotation()),
				PhysAnimBridge::UeVectorToSmpl(WorldTransform.GetLocation()),
				WorldTransform.GetScale3D()));
		}

		OutFutureSamples.Add(MoveTemp(FutureSample));
	}

	return true;
}

bool UPhysAnimComponent::RunInference(FString& OutError)
{
	UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync();
	if (!ModelInstance)
	{
		OutError = TEXT("No active model instance exists.");
		return false;
	}

	for (const float Value : SelfObservationBuffer)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("self_obs contained NaN or Inf.");
			return false;
		}
	}

	for (const float Value : MimicTargetPosesBuffer)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("mimic_target_poses contained NaN or Inf.");
			return false;
		}
	}

	if (ModelInstance->RunSync(InputBindings, OutputBindings) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("RunSync failed.");
		return false;
	}

	for (const float Value : ActionOutputBuffer)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("Model action output contained NaN or Inf.");
			return false;
		}
	}

	return true;
}

void UPhysAnimComponent::ApplyControlTargets(float DeltaTime, FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Physics Control component was not resolved.");
		return;
	}

	TMap<FName, FQuat> ControlRotations;
	if (!PhysAnimBridge::ConvertModelActionsToControlRotations(ActionOutputBuffer, ControlRotations, OutError))
	{
		return;
	}

	for (const TPair<FName, FQuat>& Pair : ControlRotations)
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(Pair.Key);
		if (!PhysicsControl->GetControlExists(ControlName))
		{
			OutError = FString::Printf(TEXT("Missing required control '%s' during target write."), *ControlName.ToString());
			return;
		}

		PhysicsControl->SetControlTargetOrientation(
			ControlName,
			Pair.Value.Rotator(),
			DeltaTime,
			true,
			false,
			true,
			false);
	}
}

void UPhysAnimComponent::FailStop(const FString& Reason)
{
	UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Fail-stop: %s"), *Reason);

	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		const TArray<FName> BodyModifierNames = PhysicsControl->GetAllBodyModifierNames();
		if (BodyModifierNames.Num() > 0)
		{
			PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(BodyModifierNames);
		}
		PhysicsControl->SetCachedBoneVelocitiesToZero();
	}

	bBridgeActive = false;
	SetComponentTickEnabled(false);
}

UE::NNE::IModelInstanceRunSync* UPhysAnimComponent::GetModelInstanceRunSync() const
{
	if (ModelInstanceGPU.IsValid())
	{
		return ModelInstanceGPU.Get();
	}

	if (ModelInstanceCPU.IsValid())
	{
		return ModelInstanceCPU.Get();
	}

	return nullptr;
}

TConstArrayView<UE::NNE::FTensorDesc> UPhysAnimComponent::GetInputTensorDescs() const
{
	if (const UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync())
	{
		return ModelInstance->GetInputTensorDescs();
	}

	return {};
}

TConstArrayView<UE::NNE::FTensorDesc> UPhysAnimComponent::GetOutputTensorDescs() const
{
	if (const UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync())
	{
		return ModelInstance->GetOutputTensorDescs();
	}

	return {};
}
