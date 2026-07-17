#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimProtoMannyAdapter.h"

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

	if (ShouldUseBalanceIdlePoseSearchState(RuntimeState))
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
	if (ShouldUseBridgeTrajectoryPoseSearchState(RuntimeState) ||
		ShouldUseBridgeOwnedMovementDrive(EffectiveSettings))
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
	FString& OutError,
	PhysAnimBridge::FPhysAnimMimicFrameDiagnostics* OutMimicFrameDiagnostics) const
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
	TArray<FQuat> BindBoneRotations;
	BindBoneRotations.Reserve(CachedSmplObservationRestComponentTransforms.Num());
	for (const FTransform& RestTransform : CachedSmplObservationRestComponentTransforms)
	{
		BindBoneRotations.Add(RestTransform.GetRotation().GetNormalized());
	}

	for (int32 FutureIndex = 0; FutureIndex < FutureOffsets.Num(); ++FutureIndex)
	{
		const float FutureOffset = FutureOffsets[FutureIndex];
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
		TArray<FQuat> SampleBoneRotations;
		SampleBoneRotations.Reserve(PhysAnimBridge::NumSmplBodies);
		FQuat RootCanonicalRotation = FQuat::Identity;
		FQuat CorrectedRightFootRotation = FQuat::Identity;

		for (int32 i = 0; i < BoneNames.Num(); ++i)
		{
			const FName& BoneName = BoneNames[i];
			const FTransform WorldTransform =
				UPoseSearchAssetSamplerLibrary::GetTransformByName(SampledPose, BoneName, EPoseSearchAssetSamplerSpace::World);

			FTransform CorrectedTransform = WorldTransform;
			CorrectedTransform.NormalizeRotation();
			SampleBoneRotations.Add(CorrectedTransform.GetRotation().GetNormalized());

			FQuat CorrectedRotation = CorrectedTransform.GetRotation().GetNormalized();
			if (CachedSmplObservationRestComponentTransforms.IsValidIndex(i))
			{
				const FMatrix SampledComponentMatrix = CorrectedTransform.ToMatrixNoScale();
				const FMatrix RestComponentMatrix = CachedSmplObservationRestComponentTransforms[i].ToMatrixNoScale();
				const FMatrix CorrectedComponentMatrix = SampledComponentMatrix * RestComponentMatrix.InverseFast();
				CorrectedRotation = FQuat(CorrectedComponentMatrix).GetNormalized();
			}
			if (i == 0)
			{
				RootCanonicalRotation = CorrectedRotation;
			}
			else if (i == 7)
			{
				CorrectedRightFootRotation = CorrectedRotation;
			}

			FutureSample.BodyTransforms.Add(FTransform(
				PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CorrectedRotation),
				PhysAnimBridge::UeWorldPositionToProtoRuntime(WorldTransform.GetLocation()),
				WorldTransform.GetScale3D()));
		}

#if WITH_DEV_AUTOMATION_TESTS
		constexpr int32 RotationProbeFutureIndex = 2;
		constexpr int32 CanonicalRightFootBodyIndex = 7;
		if (OutMimicFrameDiagnostics &&
			FutureIndex == RotationProbeFutureIndex &&
			SampleBoneRotations.IsValidIndex(0) &&
			SampleBoneRotations.IsValidIndex(CanonicalRightFootBodyIndex))
		{
			OutMimicFrameDiagnostics->RawMannyProbeFutureRootRotation =
				SampleBoneRotations[0].GetNormalized();
			OutMimicFrameDiagnostics->RawMannyProbeFutureRightFootRotation =
				SampleBoneRotations[CanonicalRightFootBodyIndex].GetNormalized();
			OutMimicFrameDiagnostics->CorrectedMannyProbeFutureRootRotation =
				RootCanonicalRotation.GetNormalized();
			OutMimicFrameDiagnostics->CorrectedMannyProbeFutureRightFootRotation =
				CorrectedRightFootRotation.GetNormalized();
		}
#else
		(void)OutMimicFrameDiagnostics;
#endif

		TArray<FQuat> CanonicalGlobalRotations;
		FString AdapterError;
		if (!PhysAnimProtoMannyAdapter::BuildCanonicalSmplRotationsFromBonePose(
			RootCanonicalRotation,
			BindBoneRotations,
			SampleBoneRotations,
			CachedSmplObservationRestBodyComponentRotations,
			CanonicalGlobalRotations,
			AdapterError))
		{
			OutFutureSamples.Reset();
			OutError = FString::Printf(TEXT("Could not adapt PoseSearch Manny rotations to SMPL: %s"), *AdapterError);
			return false;
		}

		for (int32 BodyIndex = 0; BodyIndex < FutureSample.BodyTransforms.Num(); ++BodyIndex)
		{
			FutureSample.BodyTransforms[BodyIndex].SetRotation(
				PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CanonicalGlobalRotations[BodyIndex]));
		}

		OutFutureSamples.Add(MoveTemp(FutureSample));
	}

	return true;
}


bool UPhysAnimComponent::ResolveMimicTargetReferenceDataFrame(
	const FPoseSearchBlueprintResult& SearchResult,
	FTransform& OutWorldRoot,
	FTransform& OutDataRoot,
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
	if (!CachedSmplObservationRestComponentTransforms.IsValidIndex(0))
	{
		OutError = TEXT("Mimic target reference alignment requires the cached Manny root rest frame.");
		return false;
	}
	const FQuat RestRootComponentRotation =
		CachedSmplObservationRestComponentTransforms[0].GetRotation().GetNormalized();
	const FQuat CanonicalWorldRootRotation =
		PhysAnimProtoMannyAdapter::BuildCanonicalSmplRootRotationFromBonePose(
			FQuat::Identity,
			RestRootComponentRotation,
			WorldRootTransform.GetRotation());
	const FQuat CanonicalDataRootRotation =
		PhysAnimProtoMannyAdapter::BuildCanonicalSmplRootRotationFromBonePose(
			FQuat::Identity,
			RestRootComponentRotation,
			DataRootTransform.GetRotation());

	OutWorldRoot = FTransform(
		PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CanonicalWorldRootRotation),
		PhysAnimBridge::UeWorldPositionToProtoRuntime(WorldRootTransform.GetLocation()));
	OutDataRoot = FTransform(
		PhysAnimBridge::UeWorldQuaternionToProtoRuntime(CanonicalDataRootRotation),
		PhysAnimBridge::UeWorldPositionToProtoRuntime(DataRootTransform.GetLocation()));
	return true;
}

