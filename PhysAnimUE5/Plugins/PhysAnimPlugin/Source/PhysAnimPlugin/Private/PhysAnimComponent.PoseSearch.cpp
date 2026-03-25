#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::QueryPoseSearch(FPoseSearchBlueprintResult& OutSearchResult, FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysAnimComponent_QueryPoseSearch);

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

	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
	{
		if (bHasBalanceIdlePoseSearchResult && BalanceIdlePoseSearchResult.SelectedAnim != nullptr)
		{
			OutSearchResult = BalanceIdlePoseSearchResult;
			AdvanceBridgePoseSearchResultTime(OutSearchResult, FMath::Max(LastBridgePoseSearchDeltaTimeSeconds, 1.0f / 60.0f));
			BalanceIdlePoseSearchResult = OutSearchResult;
			return true;
		}

		TArray<UObject*> AssetsToSearch;
		AssetsToSearch.Add(LoadedPoseSearchDatabase);
		FPoseSearchContinuingProperties ContinuingProperties;
		FPoseSearchFutureProperties FutureProperties;
		UPoseSearchLibrary::MotionMatch(
			LocalAnimInstance,
			AssetsToSearch,
			PhysAnimComponentInternal::PoseHistoryName,
			ContinuingProperties,
			FutureProperties,
			OutSearchResult);
		if (OutSearchResult.SelectedAnim == nullptr)
		{
			OutError = TEXT("Balance mode requires a valid idle PoseSearch result.");
			return false;
		}

		if (!IsBridgePoseSearchIdleResult(OutSearchResult))
		{
			OutError = FString::Printf(
				TEXT("Balance mode rejected non-idle PoseSearch result: %s"),
				*GetNameSafe(OutSearchResult.SelectedAnim));
			return false;
		}

		BalanceIdlePoseSearchResult = OutSearchResult;
		bHasBalanceIdlePoseSearchResult = true;
		return true;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	if (ShouldUseBridgeOwnedMovementDrive(EffectiveSettings))
	{
		FString TrajectoryError;
		if (QueryPoseSearchWithBridgeTrajectory(OutSearchResult, TrajectoryError))
		{
			return true;
		}

		OutError = FString::Printf(TEXT("Bridge trajectory query failed: %s"), *TrajectoryError);
	}

	TArray<UObject*> AssetsToSearch;
	AssetsToSearch.Add(LoadedPoseSearchDatabase);

	const bool bStartupOrEntryLocomotionRequested =
		IsBridgeLocomotionEntryRequested(EffectiveSettings) ||
		BridgeLocomotionAuthorityState == EBridgeLocomotionAuthorityState::StartupLocomotion ||
		BridgeLocomotionAuthorityState == EBridgeLocomotionAuthorityState::Locomoting;

	const bool bLastValidWasIdle =
		LastValidPoseSearchResult.SelectedAnim != nullptr &&
		IsBridgePoseSearchIdleResult(LastValidPoseSearchResult);

	FPoseSearchContinuingProperties ContinuingProperties;
	if (LastValidPoseSearchResult.SelectedAnim != nullptr &&
		!(bStartupOrEntryLocomotionRequested && bLastValidWasIdle))
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
		if (OutError.IsEmpty())
		{
			OutError = TEXT("UPoseSearchLibrary::MotionMatch returned no selected animation.");
		}
		return false;
	}

	return true;
}

bool UPhysAnimComponent::SampleFuturePoses(
	const FPoseSearchBlueprintResult& SearchResult,
	TArray<FPhysAnimFuturePoseSample>& OutFutureSamples,
	FString& OutError) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysAnimComponent_SampleFuturePoses);

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

	const TArray<FName>& BoneNames = PhysAnimBridge::GetSmplObservationBoneNames();

	for (const float FutureOffset : FutureOffsets)
	{
		FPoseSearchAssetSamplerInput SamplerInput;
		SamplerInput.Animation = AnimationAsset;
		SamplerInput.AnimationTime = FMath::Clamp(SearchResult.SelectedTime + FutureOffset, 0.0f, AnimationLength);
		SamplerInput.bMirrored = SearchResult.bIsMirrored;
		SamplerInput.BlendParameters = SearchResult.BlendParameters;
		SamplerInput.RootTransformOrigin = FTransform::Identity;

		FPoseSearchAssetSamplerPose SampledPose = UPoseSearchAssetSamplerLibrary::SamplePose(LocalAnimInstance, SamplerInput);

		FPhysAnimFuturePoseSample FutureSample;
		FutureSample.FutureTimeSeconds = PhysAnimBridge::ResolveFutureTargetTimeSeconds(
			SearchResult.SelectedTime,
			FutureOffset,
			AnimationLength);
		FutureSample.BodyTransforms.Reserve(PhysAnimBridge::NumSmplBodies);

		for (int32 i = 0; i < BoneNames.Num(); ++i)
		{
			const FName& BoneName = BoneNames[i];
			const FTransform WorldTransform =
				UPoseSearchAssetSamplerLibrary::GetTransformByName(SampledPose, BoneName, EPoseSearchAssetSamplerSpace::World);

			FTransform CorrectedTransform = WorldTransform;
			CorrectedTransform.NormalizeRotation();

			FQuat CorrectedRotation = CorrectedTransform.GetRotation().GetNormalized();
			if (CachedSmplObservationRestComponentTransforms.IsValidIndex(i))
			{
				const FMatrix SampledComponentMatrix = CorrectedTransform.ToMatrixNoScale();
				const FMatrix RestComponentMatrix = CachedSmplObservationRestComponentTransforms[i].ToMatrixNoScale();
				const FMatrix CorrectedComponentMatrix = SampledComponentMatrix * RestComponentMatrix.InverseFast();
				CorrectedRotation = FQuat(CorrectedComponentMatrix).GetNormalized();
			}

			FutureSample.BodyTransforms.Add(FTransform(
				PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CorrectedRotation),
				PhysAnimBridge::UeWorldPositionToProtoRuntime(WorldTransform.GetLocation()),
				WorldTransform.GetScale3D()));
		}

		OutFutureSamples.Add(MoveTemp(FutureSample));
	}

	return true;
}


bool UPhysAnimComponent::ResolveMimicTargetReferenceDataOffset(
	const FPoseSearchBlueprintResult& SearchResult,
	FVector2D& OutDataOffsetXY,
	FString& OutError) const
{
	const UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!LocalAnimInstance || !SkeletalMesh)
	{
		OutError = TEXT("Mimic target reference alignment requires both the AnimInstance and skeletal mesh.");
		return false;
	}

	const UAnimationAsset* const AnimationAsset = Cast<UAnimationAsset>(SearchResult.SelectedAnim);
	if (!AnimationAsset)
	{
		OutError = TEXT("PoseSearch result did not return a UAnimationAsset.");
		return false;
	}

	const float AnimationTime = FMath::Clamp(SearchResult.SelectedTime, 0.0f, AnimationAsset->GetPlayLength());

	FPoseSearchAssetSamplerInput WorldSamplerInput;
	WorldSamplerInput.Animation = AnimationAsset;
	WorldSamplerInput.AnimationTime = AnimationTime;
	WorldSamplerInput.bMirrored = SearchResult.bIsMirrored;
	WorldSamplerInput.BlendParameters = SearchResult.BlendParameters;
	WorldSamplerInput.RootTransformOrigin = SkeletalMesh->GetComponentTransform();

	FPoseSearchAssetSamplerInput DataSamplerInput = WorldSamplerInput;
	DataSamplerInput.RootTransformOrigin = FTransform::Identity;

	FPoseSearchAssetSamplerPose WorldPose = UPoseSearchAssetSamplerLibrary::SamplePose(LocalAnimInstance, WorldSamplerInput);
	FPoseSearchAssetSamplerPose DataPose = UPoseSearchAssetSamplerLibrary::SamplePose(LocalAnimInstance, DataSamplerInput);

	const FTransform WorldRootTransform =
		UPoseSearchAssetSamplerLibrary::GetTransformByName(WorldPose, PhysAnimBridge::GetRootBoneName(), EPoseSearchAssetSamplerSpace::World);
	const FTransform DataRootTransform =
		UPoseSearchAssetSamplerLibrary::GetTransformByName(DataPose, PhysAnimBridge::GetRootBoneName(), EPoseSearchAssetSamplerSpace::World);

	OutDataOffsetXY = ResolveMimicTargetReferenceDataOffsetXY(
		PhysAnimBridge::UeWorldPositionToProtoRuntime(WorldRootTransform.GetLocation()),
		PhysAnimBridge::UeWorldPositionToProtoRuntime(DataRootTransform.GetLocation()));
	return true;
}

