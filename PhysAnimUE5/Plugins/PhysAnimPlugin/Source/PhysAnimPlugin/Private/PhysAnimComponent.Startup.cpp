#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"

bool UPhysAnimComponent::BeginStartupTPoseCapture(FString& OutError)
{
	if (!TPoseReference)
	{
		OutError = TEXT("PhysAnimComponent requires a valid TPoseReference animation to align bone axes. Please assign a 1-frame T-Pose AnimSequence.");
		return false;
	}

	if (!ResolveRuntimeContext(OutError))
	{
		return false;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved for live T-pose capture.");
		return false;
	}

	SaveStartupAnimationState(SkeletalMesh);
	ForceTPoseReferenceOntoMesh(SkeletalMesh, TPoseReference);
	bPendingStartupRestPoseCapture = true;
	SetComponentTickEnabled(true);
	PrimaryComponentTick.SetTickFunctionEnable(true);
	return true;
}


bool UPhysAnimComponent::FinalizeStartupTPoseCaptureAndStartBridge(FString& OutError)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved while finalizing live T-pose capture.");
		return false;
	}

	ForceTPoseReferenceOntoMesh(SkeletalMesh, TPoseReference);
	CacheRestPoses(TPoseReference);
	if (CachedSmplObservationRestComponentTransforms.IsEmpty())
	{
		OutError = TEXT("Live T-pose capture did not populate any cached rest transforms.");
		return false;
	}

	if (bRunStartupTPoseIdentityCheck)
	{
		LogTPoseIdentityCheck();
	}

	RestoreStartupAnimationState(SkeletalMesh);

	bPendingStartupRestPoseCapture = false;

	return StartBridge();
}


void UPhysAnimComponent::SaveStartupAnimationState(USkeletalMeshComponent* SkeletalMesh)
{
	if (!SkeletalMesh)
	{
		bHasSavedStartupAnimationState = false;
		return;
	}

	SavedStartupAnimationMode = static_cast<uint8>(SkeletalMesh->GetAnimationMode());
	SavedStartupAnimClass = SkeletalMesh->GetAnimClass();
	SavedStartupAnimationAsset = nullptr;
	if (static_cast<EAnimationMode::Type>(SavedStartupAnimationMode) == EAnimationMode::AnimationSingleNode)
	{
		if (UAnimSingleNodeInstance* SingleNodeInstance = SkeletalMesh->GetSingleNodeInstance())
		{
			SavedStartupAnimationAsset = SingleNodeInstance->GetCurrentAsset();
		}
	}
	bHasSavedStartupAnimationState = true;
}


void UPhysAnimComponent::RestoreStartupAnimationState(USkeletalMeshComponent* SkeletalMesh)
{
	if (!SkeletalMesh || !bHasSavedStartupAnimationState)
	{
		return;
	}

	const EAnimationMode::Type SavedMode = static_cast<EAnimationMode::Type>(SavedStartupAnimationMode);
	SkeletalMesh->SetAnimationMode(SavedMode);

	if (SavedMode == EAnimationMode::AnimationBlueprint)
	{
		SkeletalMesh->SetAnimInstanceClass(SavedStartupAnimClass.Get());
	}
	else if (SavedMode == EAnimationMode::AnimationSingleNode && SavedStartupAnimationAsset)
	{
		SkeletalMesh->SetAnimation(SavedStartupAnimationAsset);
		SkeletalMesh->PlayAnimation(SavedStartupAnimationAsset, false);
	}

	SkeletalMesh->TickAnimation(0.0f, false);
	SkeletalMesh->RefreshBoneTransforms();
	SkeletalMesh->UpdateComponentToWorld();

	bHasSavedStartupAnimationState = false;
	SavedStartupAnimClass = nullptr;
	SavedStartupAnimationAsset = nullptr;
}


void UPhysAnimComponent::LogTPoseIdentityCheck() const
{
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] TPose identity check skipped: skeletal mesh component was not resolved."));
		return;
	}

	if (CachedSmplObservationRestComponentTransforms.IsEmpty())
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] TPose identity check skipped: cached rest transforms are empty."));
		return;
	}

	static const TSet<FName> DebugBones =
	{
		TEXT("clavicle_l"),
		TEXT("upperarm_l"),
		TEXT("lowerarm_l"),
		TEXT("hand_l"),
		TEXT("clavicle_r"),
		TEXT("upperarm_r"),
		TEXT("lowerarm_r"),
		TEXT("hand_r")
	};

	const TArray<FName>& BoneNames = PhysAnimBridge::GetSmplObservationBoneNames();
	const FTransform MeshComponentTransform = SkeletalMesh->GetComponentTransform();

	PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] === TPose identity check start ==="));

	for (int32 BoneIndex = 0; BoneIndex < BoneNames.Num(); ++BoneIndex)
	{
		const FName& BoneName = BoneNames[BoneIndex];
		if (!DebugBones.Contains(BoneName))
		{
			continue;
		}

		if (SkeletalMesh->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] TPose identity bone=%s skipped: missing on skeletal mesh."), *BoneName.ToString());
			continue;
		}

		const FTransform BoneWorldTransform = SkeletalMesh->GetBoneTransform(BoneName, RTS_World);
		FTransform CurrentComponentTransform = BoneWorldTransform.GetRelativeTransform(MeshComponentTransform);
		CurrentComponentTransform.NormalizeRotation();

		const FTransform& RestComponentTransform = CachedSmplObservationRestComponentTransforms[BoneIndex];

		const FQuat QuaternionDelta =
			(CurrentComponentTransform.GetRotation().GetNormalized() *
			 RestComponentTransform.GetRotation().Inverse()).GetNormalized();

		const FMatrix CurrentComponentMatrix = CurrentComponentTransform.ToMatrixNoScale();
		const FMatrix RestComponentMatrix = RestComponentTransform.ToMatrixNoScale();
		const FMatrix MatrixDelta = CurrentComponentMatrix * RestComponentMatrix.InverseFast();
		const FQuat MatrixDeltaQuat = FQuat(MatrixDelta).GetNormalized();

		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnim] TPose identity bone=%s quat_err_deg=%.3f matrix_err_deg=%.3f current_q=(%.5f, %.5f, %.5f, %.5f) rest_q=(%.5f, %.5f, %.5f, %.5f)"),
			*BoneName.ToString(),
			QuaternionAngularErrorDegrees(QuaternionDelta),
			QuaternionAngularErrorDegrees(MatrixDeltaQuat),
			CurrentComponentTransform.GetRotation().X,
			CurrentComponentTransform.GetRotation().Y,
			CurrentComponentTransform.GetRotation().Z,
			CurrentComponentTransform.GetRotation().W,
			RestComponentTransform.GetRotation().X,
			RestComponentTransform.GetRotation().Y,
			RestComponentTransform.GetRotation().Z,
			RestComponentTransform.GetRotation().W);
	}

	PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] === TPose identity check end ==="));
}


void UPhysAnimComponent::CacheRestPoses(UAnimSequence* TPoseAnim)
{
	CachedSmplObservationRestComponentTransforms.Reset();

	const USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!TPoseAnim)
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[%s] CacheRestPoses called without a valid TPose AnimSequence."), *GetName());
		return;
	}

	if (!Mesh || !Mesh->GetSkeletalMeshAsset())
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[%s] CacheRestPoses could not access skeletal mesh asset."), *GetName());
		return;
	}

	const TArray<FName>& BoneNames = PhysAnimBridge::GetSmplObservationBoneNames();
	CachedSmplObservationRestComponentTransforms.Reserve(BoneNames.Num());

	const FTransform MeshComponentTransform = Mesh->GetComponentTransform();

	for (const FName& BoneName : BoneNames)
	{
		if (Mesh->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[%s] Could not find bone '%s' for live rest pose caching."), *GetName(), *BoneName.ToString());
			CachedSmplObservationRestComponentTransforms.Add(FTransform::Identity);
			continue;
		}

		FTransform BoneWorldTransform = Mesh->GetBoneTransform(BoneName, RTS_World);
		FTransform BoneComponentTransform = BoneWorldTransform.GetRelativeTransform(MeshComponentTransform);
		BoneComponentTransform.NormalizeRotation();
		CachedSmplObservationRestComponentTransforms.Add(BoneComponentTransform);
	}
}

