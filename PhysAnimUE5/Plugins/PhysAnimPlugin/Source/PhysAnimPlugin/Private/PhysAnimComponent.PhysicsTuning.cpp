#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimBalanceReadyTransitionPrivate.h"

bool UPhysAnimComponent::ActivateRuntimePhysicsControl(FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!PhysicsControl || !OwnerActor)
	{
		OutError = TEXT("Runtime Physics Control activation requires both the owning actor and Physics Control component.");
		return false;
	}

	DeactivateRuntimePhysicsControl(TEXT("ActivateRuntimePhysicsControl"));

	if (UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
	{
		Stage1Initializer->CreateControls(PhysicsControl);
	}
	else if (UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
	{
		Initializer->CreateControls(PhysicsControl);
	}
	else
	{
		OutError = TEXT("Owning actor is missing a runtime Physics Control initializer.");
		return false;
	}

	if (!ValidateRuntimePhysicsControl(OutError))
	{
		return false;
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime operator activation: controls=%d bodyModifiers=%d"),
		PhysicsControl->GetAllControlNames().Num(),
		PhysicsControl->GetAllBodyModifierNames().Num());
	return true;
}


void UPhysAnimComponent::DeactivateRuntimePhysicsControl(const TCHAR* Context)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		return;
	}

	const TArray<FName> ControlNames = PhysicsControl->GetAllControlNames();
	const TArray<FName> BodyModifierNames = PhysicsControl->GetAllBodyModifierNames();
	if (ControlNames.Num() == 0 && BodyModifierNames.Num() == 0)
	{
		return;
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime operator deactivation[%s]: controls=%d bodyModifiers=%d"),
		Context,
		ControlNames.Num(),
		BodyModifierNames.Num());

	if (BodyModifierNames.Num() > 0)
	{
		PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(BodyModifierNames);
		PhysicsControl->SetCachedBoneVelocitiesToZero();
		PhysicsControl->DestroyBodyModifiers(BodyModifierNames, true, false);
	}

	if (ControlNames.Num() > 0)
	{
		PhysicsControl->DestroyControls(ControlNames, true, false);
	}
}


void UPhysAnimComponent::ActivateBridgePhysicsState(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	if (!bHasSavedMeshCollisionState)
	{
		OriginalMeshCollisionProfileName = SkeletalMesh->GetCollisionProfileName();
		OriginalMeshCollisionEnabled = SkeletalMesh->GetCollisionEnabled();
		OriginalMeshPawnResponse = SkeletalMesh->GetCollisionResponseToChannel(ECC_Pawn);
		bHasSavedMeshCollisionState = true;
	}

	SkeletalMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	SkeletalMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SkeletalMesh->SetSimulatePhysics(true);

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		(void)RootBoneName;
	}

	ApplyTrainingAlignedToeLimitPolicy(EffectiveSettings);
	ApplyTrainingAlignedSpineLimitPolicy(EffectiveSettings);
	SkeletalMesh->RecreatePhysicsState();
	SkeletalMesh->SetEnablePhysicsBlending(true);
	SkeletalMesh->WakeAllRigidBodies();
	ApplyTrainingAlignedMassScales(EffectiveSettings);

	const bool bPreserveGameplayShell = ShouldPreserveGameplayShellDuringBridgeActive(
		IsMovementSmokeModeEnabled(),
		PhysAnimComponentInternal::CVarPhysAnimAllowCharacterMovementInBridgeActive.GetValueOnGameThread() != 0);
	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (!bHasSavedCapsuleCollisionState)
			{
				OriginalCapsuleCollisionEnabled = CapsuleComponent->GetCollisionEnabled();
				bHasSavedCapsuleCollisionState = true;
			}

			if (!bPreserveGameplayShell)
			{
				CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			if (!bHasSavedCharacterMovementState)
			{
				OriginalCharacterMovementMode = CharacterMovement->MovementMode;
				OriginalCharacterCustomMovementMode = CharacterMovement->CustomMovementMode;
				bOriginalCharacterMovementTickEnabled = CharacterMovement->IsComponentTickEnabled();
				bHasSavedCharacterMovementState = true;
			}

			if (!bPreserveGameplayShell)
			{
				CharacterMovement->DisableMovement();
				CharacterMovement->SetComponentTickEnabled(false);
			}
			else
			{
				CharacterMovement->SetComponentTickEnabled(true);
				if (CharacterMovement->MovementMode == MOVE_None)
				{
					CharacterMovement->SetMovementMode(MOVE_Walking);
				}
			}
		}
	}

	if (bPreserveGameplayShell)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] BridgeActive preserving capsule collision and CharacterMovement during bridge ownership."));
	}
}


void UPhysAnimComponent::ApplyTrainingAlignedMassScales(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	const bool bApplyMassPolicy = ShouldApplyTrainingAlignedMassScales(
		EffectiveSettings.bApplyTrainingAlignedMassScales,
		EffectiveSettings.TrainingAlignedMassScaleBlend);
	if (!bApplyMassPolicy)
	{
		return;
	}

	if (!bHasSavedBodyMassScales)
	{
		OriginalBodyMassScales.Reset();
		for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (!BodySetup)
			{
				continue;
			}

			OriginalBodyMassScales.Add(BodySetup->BoneName, SkeletalMesh->GetMassScale(BodySetup->BoneName));
		}
		bHasSavedBodyMassScales = OriginalBodyMassScales.Num() > 0;
	}

	int32 NumAdjustedBodies = 0;
	for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		const float MassScale =
			ResolveTrainingAlignedMassScaleForBone(
				BodySetup->BoneName,
				EffectiveSettings.TrainingAlignedMassScaleBlend);
		SkeletalMesh->SetMassScale(BodySetup->BoneName, MassScale);
		++NumAdjustedBodies;
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Applied training-aligned Manny mass scales: bodies=%d blend=%.2f"),
		NumAdjustedBodies,
		EffectiveSettings.TrainingAlignedMassScaleBlend);
}


void UPhysAnimComponent::ResetTrainingAlignedMassScales()
{
	if (!bHasSavedBodyMassScales)
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OriginalBodyMassScales.Reset();
		bHasSavedBodyMassScales = false;
		return;
	}

	for (const TPair<FName, float>& Pair : OriginalBodyMassScales)
	{
		SkeletalMesh->SetMassScale(Pair.Key, Pair.Value);
	}

	OriginalBodyMassScales.Reset();
	bHasSavedBodyMassScales = false;
}


void UPhysAnimComponent::ApplyTrainingAlignedToeLimitPolicy(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	if (!ShouldApplyTrainingAlignedToeLimitPolicy(
		EffectiveSettings.bApplyTrainingAlignedToeLimitPolicy,
		EffectiveSettings.TrainingAlignedToeLimitPolicyBlend))
	{
		return;
	}

	struct FToeConstraintTarget
	{
		FName ChildBoneName;
		FName ParentBoneName;
	};

	const TArray<FToeConstraintTarget> ToeConstraints =
	{
		{ TEXT("ball_l"), TEXT("foot_l") },
		{ TEXT("ball_r"), TEXT("foot_r") }
	};

	if (!bHasSavedToeConstraintLimits)
	{
		OriginalToeTwistMotions.Reset();
		OriginalToeSwing1Motions.Reset();
		OriginalToeSwing2Motions.Reset();
		OriginalToeTwistLimits.Reset();
		OriginalToeSwing1Limits.Reset();
		OriginalToeSwing2Limits.Reset();
	}

	int32 NumAdjustedToeConstraints = 0;
	const float ClampedBlendAlpha = FMath::Clamp(EffectiveSettings.TrainingAlignedToeLimitPolicyBlend, 0.0f, 1.0f);
	for (const FToeConstraintTarget& ToeConstraint : ToeConstraints)
	{
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ToeConstraint.ChildBoneName, ToeConstraint.ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		if (!bHasSavedToeConstraintLimits)
		{
			OriginalToeTwistMotions.Add(ToeConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularTwistMotion()));
			OriginalToeSwing1Motions.Add(ToeConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing1Motion()));
			OriginalToeSwing2Motions.Add(ToeConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing2Motion()));
			OriginalToeTwistLimits.Add(ToeConstraint.ChildBoneName, ConstraintInstance->GetAngularTwistLimit());
			OriginalToeSwing1Limits.Add(ToeConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing1Limit());
			OriginalToeSwing2Limits.Add(ToeConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing2Limit());
		}

		const float TargetTwistLimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularTwistLimit(), 20.0f, ClampedBlendAlpha);
		const float TargetSwing1LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing1Limit(), 20.0f, ClampedBlendAlpha);
		const float TargetSwing2LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing2Limit(), 20.0f, ClampedBlendAlpha);

		ConstraintInstance->SetAngularTwistLimit(ACM_Limited, TargetTwistLimitDegrees);
		ConstraintInstance->SetAngularSwing1Limit(ACM_Limited, TargetSwing1LimitDegrees);
		ConstraintInstance->SetAngularSwing2Limit(ACM_Limited, TargetSwing2LimitDegrees);
		++NumAdjustedToeConstraints;
	}

	bHasSavedToeConstraintLimits = OriginalToeTwistMotions.Num() > 0;
	if (NumAdjustedToeConstraints > 0)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Applied training-aligned toe operating limits: constraints=%d blend=%.2f"),
			NumAdjustedToeConstraints,
			EffectiveSettings.TrainingAlignedToeLimitPolicyBlend);
	}
}

void UPhysAnimComponent::ApplyTrainingAlignedSpineLimitPolicy(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	if (!ShouldApplyTrainingAlignedControlFamilyProfile(
		EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile,
		EffectiveSettings.TrainingAlignedControlFamilyProfileBlend))
	{
		return;
	}

	struct FSpineConstraintTarget
	{
		FName ChildBoneName;
		FName ParentBoneName;
	};

	const TArray<FSpineConstraintTarget> SpineConstraints =
	{
		{ TEXT("spine_02"), TEXT("spine_01") },
		{ TEXT("spine_03"), TEXT("spine_02") }
	};

	if (!bHasSavedSpineConstraintLimits)
	{
		OriginalSpineTwistMotions.Reset();
		OriginalSpineSwing1Motions.Reset();
		OriginalSpineSwing2Motions.Reset();
		OriginalSpineTwistLimits.Reset();
		OriginalSpineSwing1Limits.Reset();
		OriginalSpineSwing2Limits.Reset();
	}

	int32 NumAdjustedSpineConstraints = 0;
	const float ClampedBlendAlpha = FMath::Clamp(EffectiveSettings.TrainingAlignedControlFamilyProfileBlend, 0.0f, 1.0f);
	for (const FSpineConstraintTarget& SpineConstraint : SpineConstraints)
	{
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(SpineConstraint.ChildBoneName, SpineConstraint.ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		if (!bHasSavedSpineConstraintLimits)
		{
			OriginalSpineTwistMotions.Add(SpineConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularTwistMotion()));
			OriginalSpineSwing1Motions.Add(SpineConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing1Motion()));
			OriginalSpineSwing2Motions.Add(SpineConstraint.ChildBoneName, static_cast<uint8>(ConstraintInstance->GetAngularSwing2Motion()));
			OriginalSpineTwistLimits.Add(SpineConstraint.ChildBoneName, ConstraintInstance->GetAngularTwistLimit());
			OriginalSpineSwing1Limits.Add(SpineConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing1Limit());
			OriginalSpineSwing2Limits.Add(SpineConstraint.ChildBoneName, ConstraintInstance->GetAngularSwing2Limit());
		}

		const float TargetTwistLimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularTwistLimit(), 25.0f, ClampedBlendAlpha);
		const float TargetSwing1LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing1Limit(), 25.0f, ClampedBlendAlpha);
		const float TargetSwing2LimitDegrees = FMath::Lerp(ConstraintInstance->GetAngularSwing2Limit(), 25.0f, ClampedBlendAlpha);

		ConstraintInstance->SetAngularTwistLimit(ACM_Limited, TargetTwistLimitDegrees);
		ConstraintInstance->SetAngularSwing1Limit(ACM_Limited, TargetSwing1LimitDegrees);
		ConstraintInstance->SetAngularSwing2Limit(ACM_Limited, TargetSwing2LimitDegrees);
		++NumAdjustedSpineConstraints;
	}

	bHasSavedSpineConstraintLimits = OriginalSpineTwistMotions.Num() > 0;
	if (NumAdjustedSpineConstraints > 0)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Applied training-aligned spine operating limits: constraints=%d blend=%.2f"),
			NumAdjustedSpineConstraints,
			EffectiveSettings.TrainingAlignedControlFamilyProfileBlend);
	}
}


void UPhysAnimComponent::ResetTrainingAlignedToeLimitPolicy()
{
	if (!bHasSavedToeConstraintLimits)
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UPhysicsAsset* const PhysicsAsset = SkeletalMesh ? SkeletalMesh->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		OriginalToeTwistMotions.Reset();
		OriginalToeSwing1Motions.Reset();
		OriginalToeSwing2Motions.Reset();
		OriginalToeTwistLimits.Reset();
		OriginalToeSwing1Limits.Reset();
		OriginalToeSwing2Limits.Reset();
		bHasSavedToeConstraintLimits = false;
		return;
	}

	for (const TPair<FName, uint8>& Pair : OriginalToeTwistMotions)
	{
		const FName ChildBoneName = Pair.Key;
		const FName ParentBoneName = ChildBoneName == TEXT("ball_l") ? TEXT("foot_l") : TEXT("foot_r");
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		const uint8* const Swing1Motion = OriginalToeSwing1Motions.Find(ChildBoneName);
		const uint8* const Swing2Motion = OriginalToeSwing2Motions.Find(ChildBoneName);
		const float* const TwistLimit = OriginalToeTwistLimits.Find(ChildBoneName);
		const float* const Swing1Limit = OriginalToeSwing1Limits.Find(ChildBoneName);
		const float* const Swing2Limit = OriginalToeSwing2Limits.Find(ChildBoneName);
		if (!Swing1Motion || !Swing2Motion || !TwistLimit || !Swing1Limit || !Swing2Limit)
		{
			continue;
		}

		ConstraintInstance->SetAngularTwistLimit(static_cast<EAngularConstraintMotion>(Pair.Value), *TwistLimit);
		ConstraintInstance->SetAngularSwing1Limit(static_cast<EAngularConstraintMotion>(*Swing1Motion), *Swing1Limit);
		ConstraintInstance->SetAngularSwing2Limit(static_cast<EAngularConstraintMotion>(*Swing2Motion), *Swing2Limit);
	}

	OriginalToeTwistMotions.Reset();
	OriginalToeSwing1Motions.Reset();
	OriginalToeSwing2Motions.Reset();
	OriginalToeTwistLimits.Reset();
	OriginalToeSwing1Limits.Reset();
	OriginalToeSwing2Limits.Reset();
	bHasSavedToeConstraintLimits = false;
}

void UPhysAnimComponent::ResetTrainingAlignedSpineLimitPolicy()
{
	if (!bHasSavedSpineConstraintLimits)
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UPhysicsAsset* const PhysicsAsset = SkeletalMesh ? SkeletalMesh->GetPhysicsAsset() : nullptr;
	if (!PhysicsAsset)
	{
		OriginalSpineTwistMotions.Reset();
		OriginalSpineSwing1Motions.Reset();
		OriginalSpineSwing2Motions.Reset();
		OriginalSpineTwistLimits.Reset();
		OriginalSpineSwing1Limits.Reset();
		OriginalSpineSwing2Limits.Reset();
		bHasSavedSpineConstraintLimits = false;
		return;
	}

	for (const TPair<FName, uint8>& Pair : OriginalSpineTwistMotions)
	{
		const FName ChildBoneName = Pair.Key;
		const FName ParentBoneName = ChildBoneName == TEXT("spine_02") ? TEXT("spine_01") : TEXT("spine_02");
		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			continue;
		}

		UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		FConstraintInstance* const ConstraintInstance = ConstraintTemplate ? &ConstraintTemplate->DefaultInstance : nullptr;
		if (!ConstraintInstance)
		{
			continue;
		}

		const uint8* const Swing1Motion = OriginalSpineSwing1Motions.Find(ChildBoneName);
		const uint8* const Swing2Motion = OriginalSpineSwing2Motions.Find(ChildBoneName);
		const float* const TwistLimit = OriginalSpineTwistLimits.Find(ChildBoneName);
		const float* const Swing1Limit = OriginalSpineSwing1Limits.Find(ChildBoneName);
		const float* const Swing2Limit = OriginalSpineSwing2Limits.Find(ChildBoneName);
		if (!Swing1Motion || !Swing2Motion || !TwistLimit || !Swing1Limit || !Swing2Limit)
		{
			continue;
		}

		ConstraintInstance->SetAngularTwistLimit(static_cast<EAngularConstraintMotion>(Pair.Value), *TwistLimit);
		ConstraintInstance->SetAngularSwing1Limit(static_cast<EAngularConstraintMotion>(*Swing1Motion), *Swing1Limit);
		ConstraintInstance->SetAngularSwing2Limit(static_cast<EAngularConstraintMotion>(*Swing2Motion), *Swing2Limit);
	}

	OriginalSpineTwistMotions.Reset();
	OriginalSpineSwing1Motions.Reset();
	OriginalSpineSwing2Motions.Reset();
	OriginalSpineTwistLimits.Reset();
	OriginalSpineSwing1Limits.Reset();
	OriginalSpineSwing2Limits.Reset();
	bHasSavedSpineConstraintLimits = false;
}


void UPhysAnimComponent::ResetBridgePhysicsState()
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		bHasSavedMeshCollisionState = false;
		OriginalBodyMassScales.Reset();
		bHasSavedBodyMassScales = false;
		return;
	}

	ResetTrainingAlignedMassScales();
	ResetTrainingAlignedSpineLimitPolicy();
	ResetTrainingAlignedToeLimitPolicy();
	SkeletalMesh->SetSimulatePhysics(false);
	
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
	{
		(void)RootBoneName;
	}

	SkeletalMesh->SetEnablePhysicsBlending(false);
	if (bHasSavedMeshCollisionState)
	{
		SkeletalMesh->SetCollisionProfileName(OriginalMeshCollisionProfileName);
		SkeletalMesh->SetCollisionEnabled(OriginalMeshCollisionEnabled);
		SkeletalMesh->SetCollisionResponseToChannel(ECC_Pawn, OriginalMeshPawnResponse);
		bHasSavedMeshCollisionState = false;
	}

	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (bHasSavedCapsuleCollisionState)
			{
				CapsuleComponent->SetCollisionEnabled(OriginalCapsuleCollisionEnabled);
				bHasSavedCapsuleCollisionState = false;
			}
		}

		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			if (bHasSavedCharacterMovementState)
			{
				CharacterMovement->SetComponentTickEnabled(bOriginalCharacterMovementTickEnabled);
				CharacterMovement->SetMovementMode(static_cast<EMovementMode>(OriginalCharacterMovementMode), OriginalCharacterCustomMovementMode);
				bHasSavedCharacterMovementState = false;
			}
		}
	}
}


bool UPhysAnimComponent::IsInstabilityPrecursorActive() const
{
	return RuntimeInstabilityState.UnstableAccumulatedSeconds > 0.0f;
}


void UPhysAnimComponent::GetSimulatingBodies(TArray<FName>& OutBones) const
{
	OutBones.Reset();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName);
		if (BodyInstance && BodyInstance->IsInstanceSimulatingPhysics())
		{
			OutBones.Add(BoneName);
		}
	}
}


void UPhysAnimComponent::ApplyRuntimeControlTuning(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	const bool bPhase1Prepare = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare);
	const bool bPhase1LateValidate = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate);
	const bool bSimulationHandoffSettled = SimulationHandoffAlpha >= (1.0f - KINDA_SMALL_NUMBER);
	const bool bSimulationHandoffCompletedThisTick = bSimulationHandoffSettled && !bLastAppliedSimulationHandoffSettled;
	const bool bPresentationPerturbationOverrideActive = IsPresentationPerturbationOverrideActive();
	const bool bPolicyInfluenceActive = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings) > KINDA_SMALL_NUMBER;
	const bool bUseSkeletalAnimationTargetRepresentation =
		ShouldUseSkeletalAnimationTargetRepresentation(
			EffectiveSettings.bUseSkeletalAnimationTargets,
			bPolicyInfluenceActive);

	static uint64 LastFrameNumber = 0;
	static TMap<UPhysAnimComponent*, int32> CallCounts;
	const uint64 CurrentFrameNumber = GFrameNumber;
	if (LastFrameNumber != CurrentFrameNumber)
	{
		CallCounts.Empty();
		LastFrameNumber = CurrentFrameNumber;
	}
	int32& CallIndexRef = CallCounts.FindOrAdd(this);
	CallIndexRef++;
	const int32 CurrentCallIndex = CallIndexRef;

	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
	{
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn)
		{
			BalanceEntryRootOnFrameCount++;
		}
		else if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
		{
			BalanceEntrySettleFrameCount++;
		}

		if (CurrentCallIndex != 1)
		{
			UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_TUNING_CALL_AUDIT frame=%d callIndex=%d runtimeState=%s owner=%d actor=%s component=%s"),
				static_cast<int32>(CurrentFrameNumber),
				CurrentCallIndex,
				GetRuntimeStateName(RuntimeState),
				static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(BalanceReadyTransition.GetFailureReason())),
				*GetOwner()->GetName(),
				*GetName());
		}
	}

	const bool bAllowRootSim = ShouldAllowBalanceSimulation(EffectiveSettings);
	const bool bRootSimFlipFrame = bAllowRootSim && !bLastAppliedPresentationRootSimulationEnabled;
	if (bRootSimFlipFrame)
	{
		HipQuarantineTicksRemaining = 10;
	}
	const bool bHipQuarantineActiveThisFrame = HipQuarantineTicksRemaining > 0;
	bool bHipQuarantineReleasedThisFrame = false;
	if (!PhysicsControl)
	{
		return;
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny)
	{
		LastAppliedStabilizationSettings = EffectiveSettings;
		bLastAppliedSimulationHandoffSettled = bSimulationHandoffSettled;
		LastAppliedControlAuthorityAlpha = CalculateCurrentControlAuthorityAlpha(EffectiveSettings);
		return;
	}

	const float OwnerPlanarSpeedCmPerSec = [this]() -> float
	{
		const AActor* const OwnerActor = GetOwner();
		if (!OwnerActor)
		{
			return 0.0f;
		}

		const FVector OwnerVelocity = OwnerActor->GetVelocity();
		return FVector(OwnerVelocity.X, OwnerVelocity.Y, 0.0f).Size();
	}();
	const bool bHasActiveMovementIntent = [this]() -> bool
	{
		const APawn* const OwnerPawn = Cast<APawn>(GetOwner());
		const FVector PendingInput = OwnerPawn ? OwnerPawn->GetPendingMovementInputVector() : FVector::ZeroVector;
		const FVector LastInput = OwnerPawn ? OwnerPawn->GetLastMovementInputVector() : FVector::ZeroVector;
		const FVector PendingPlanarInput(PendingInput.X, PendingInput.Y, 0.0f);
		const FVector LastPlanarInput(LastInput.X, LastInput.Y, 0.0f);

		const ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
		const UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
		const FVector CurrentAcceleration = CharacterMovement ? CharacterMovement->GetCurrentAcceleration() : FVector::ZeroVector;
		const FVector PlanarAcceleration(CurrentAcceleration.X, CurrentAcceleration.Y, 0.0f);

		return PendingPlanarInput.SizeSquared() > UE_KINDA_SMALL_NUMBER ||
			LastPlanarInput.SizeSquared() > UE_KINDA_SMALL_NUMBER ||
			PlanarAcceleration.SizeSquared() > UE_KINDA_SMALL_NUMBER;
	}();
	const float RuntimeDeltaTimeSeconds =
		GetWorld() ? FMath::Max(0.0f, GetWorld()->GetDeltaSeconds()) : 0.0f;
	if (bHasActiveMovementIntent)
	{
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds = 0.0f;
	}
	else if (DistalLocomotionCompositionTimeSinceActiveIntentSeconds >= 0.0f)
	{
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds += RuntimeDeltaTimeSeconds;
	}
	const bool bHasRecentMovementIntent =
		bHasActiveMovementIntent ||
		(DistalLocomotionCompositionTimeSinceActiveIntentSeconds >= 0.0f &&
		 DistalLocomotionCompositionTimeSinceActiveIntentSeconds <= EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds);
	const bool bHasLocomotionEntrySignal =
		bHasRecentMovementIntent ||
		BridgeTrajectoryState.DesiredVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond ||
		BridgeTrajectoryState.QueryVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond;
	if (EffectiveSettings.bApplyTrainingAlignedDistalLocomotionCompositionPolicy)
	{
		const bool bPreviousDistalLocomotionCompositionModeActive = bDistalLocomotionCompositionModeActive;
		bDistalLocomotionCompositionModeActive =
			UpdateBinarySpeedModeWithIntentLatch(
				bDistalLocomotionCompositionModeActive,
				OwnerPlanarSpeedCmPerSec,
				bHasLocomotionEntrySignal,
				EffectiveSettings.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec,
				EffectiveSettings.DistalLocomotionCompositionPolicyExitSpeedCmPerSec,
				EffectiveSettings.DistalLocomotionCompositionPolicyEnterHoldSeconds,
				EffectiveSettings.DistalLocomotionCompositionPolicyExitHoldSeconds,
				RuntimeDeltaTimeSeconds,
				DistalLocomotionCompositionTimeAboveEnterSeconds,
				DistalLocomotionCompositionTimeBelowExitSeconds);
		if (bPreviousDistalLocomotionCompositionModeActive != bDistalLocomotionCompositionModeActive)
		{
			TRACE_BOOKMARK(
				TEXT("PhysAnim DistalCompositionMode %s speed=%.1f intent=%s"),
				bDistalLocomotionCompositionModeActive ? TEXT("On") : TEXT("Off"),
				OwnerPlanarSpeedCmPerSec,
				bHasActiveMovementIntent ? TEXT("true") : TEXT("false"));
		}
	}
	else
	{
		bDistalLocomotionCompositionModeActive = false;
		DistalLocomotionCompositionTimeAboveEnterSeconds = 0.0f;
		DistalLocomotionCompositionTimeBelowExitSeconds = 0.0f;
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds = -1.0f;
	}

	PhysicsControl->SetControlsInSetEnabled(TEXT("All"), false);
	PhysicsControl->SetControlsInSetUseSkeletalAnimation(
		TEXT("All"),
		bUseSkeletalAnimationTargetRepresentation,
		0.0f,
		0.0f);

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const int32 BringUpGroupIndex = ResolveBringUpGroupIndex(BoneName);
		bool bBringUpGroupUnlocked = IsBringUpGroupUnlocked(BringUpGroupIndex);
		float ControlAuthorityAlpha =
			CalculateBringUpGroupControlAuthorityAlpha(BringUpGroupIndex, EffectiveSettings);

		if (bPhase1Prepare || bPhase1LateValidate)
		{
			const int32 FinalGroupIndex = GetBringUpGroupCount() - 1;
			if (BringUpGroupIndex == 0 || BringUpGroupIndex == 1)
			{
				bBringUpGroupUnlocked = true;
				ControlAuthorityAlpha = 1.0f;
			}
			else if (bPhase1LateValidate && BringUpGroupIndex == FinalGroupIndex)
			{
				// Allow final bring-up group to ramp during LateValidate
				bBringUpGroupUnlocked = IsBringUpGroupUnlocked(BringUpGroupIndex);
				ControlAuthorityAlpha = CalculateBringUpGroupControlAuthorityAlpha(BringUpGroupIndex, EffectiveSettings);
			}
			else
			{
				bBringUpGroupUnlocked = false;
				ControlAuthorityAlpha = 0.0f;
			}
		}
		const bool bApplyTrainingAlignedControlProfile =
			ShouldApplyTrainingAlignedControlFamilyProfile(
				EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile,
				EffectiveSettings.TrainingAlignedControlFamilyProfileBlend);
		const float FamilyStrengthScale =
			bApplyTrainingAlignedControlProfile
				? ResolveTrainingAlignedControlStrengthScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedControlFamilyProfileBlend)
				: 1.0f;
		const float FamilyExtraDampingScale =
			bApplyTrainingAlignedControlProfile
				? ResolveTrainingAlignedControlExtraDampingScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedControlFamilyProfileBlend)
				: 1.0f;
		const bool bApplyTrainingAlignedLocomotionLowerLimbResponseProfile =
			ShouldApplyTrainingAlignedLocomotionLowerLimbResponsePolicy(
				EffectiveSettings.bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy,
				EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend,
				bDistalLocomotionCompositionModeActive);
		const float LocomotionLowerLimbDampingRatioScale =
			bApplyTrainingAlignedLocomotionLowerLimbResponseProfile
				? ResolveTrainingAlignedLocomotionLowerLimbDampingRatioScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend)
				: 1.0f;
		const float LocomotionLowerLimbExtraDampingScale =
			bApplyTrainingAlignedLocomotionLowerLimbResponseProfile
				? ResolveTrainingAlignedLocomotionLowerLimbExtraDampingScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend)
				: 1.0f;
		FPhysicsControlMultiplier ControlMultiplier;
		float HandoverEasing = 1.0f;
		if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
		{
			const double TimeSinceTransition = GetWorld() ? (GetWorld()->GetTimeSeconds() - BalanceScenarioStartTimeSeconds) : 0.0;
			if (TimeSinceTransition < 1.0)
			{
				HandoverEasing = FMath::Lerp(0.05f, 1.0f, FMath::Clamp(static_cast<float>(TimeSinceTransition / 1.0), 0.0f, 1.0f));
			}
		}

		ControlMultiplier.AngularStrengthMultiplier =
			EffectiveSettings.AngularStrengthMultiplier * FamilyStrengthScale * ControlAuthorityAlpha * HandoverEasing;
		ControlMultiplier.AngularStrengthMultiplier *= BalanceReadyTransition.GetProximalControlSoftAlpha(BoneName);
		ControlMultiplier.AngularDampingRatioMultiplier =
			EffectiveSettings.AngularDampingRatioMultiplier * LocomotionLowerLimbDampingRatioScale;
		ControlMultiplier.AngularExtraDampingMultiplier =
			EffectiveSettings.AngularExtraDampingMultiplier * FamilyExtraDampingScale * LocomotionLowerLimbExtraDampingScale *
			BalanceReadyTransition.GetTransitionExtraDampingMultiplier(EffectiveSettings);

		if (bHipQuarantineActiveThisFrame && (BoneName == "thigh_l" || BoneName == "thigh_r"))
		{
			ControlMultiplier.AngularStrengthMultiplier = 0.0f;
		}

		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		if (bDistalLocomotionCompositionModeActive &&
			ShouldForceExplicitOnlyDistalLocomotionTargetMode(BoneName))
		{
			PhysicsControl->SetControlUseSkeletalAnimation(
				ControlName,
				false,
				0.0f,
				0.0f,
				true,
				false);
		}
		PhysicsControl->SetControlMultiplier(
			ControlName,
			ControlMultiplier,
			bBringUpGroupUnlocked && !EffectiveSettings.bForceZeroActions,
			true,
			false);

		// Deep diagnostics for Balance Mode Final Ramp Enable
		if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery && BringUpGroupIndex == (GetBringUpGroupCount() - 1))
		{
			const double WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
			const bool bRampJustStarted = (BringUpGroupControlRampStartTimeSeconds.IsValidIndex(BringUpGroupIndex) && 
										  BringUpGroupControlRampStartTimeSeconds[BringUpGroupIndex] == WorldTime);
			
			if (bRampJustStarted)
			{
				const FTransform BoneTransform = MeshComponent->GetBoneTransform(MeshComponent->GetBoneIndex(BoneName));
				
				UE_LOG(
					LogPhysAnimBridge,
					Log,
					TEXT("[PhysAnimBalance] FINAL RAMP ENABLE DIAG: bone=%s alpha=%.4f easing=%.4f strength=%.2f useSkelAnim=%s loc=(%.1f, %.1f, %.1f) rot=(%.2f, %.2f, %.2f, %.2f)"),
					*BoneName.ToString(),
					ControlAuthorityAlpha,
					HandoverEasing,
					ControlMultiplier.AngularStrengthMultiplier,
					bUseSkeletalAnimationTargetRepresentation ? TEXT("true") : TEXT("false"),
					BoneTransform.GetLocation().X, BoneTransform.GetLocation().Y, BoneTransform.GetLocation().Z,
					BoneTransform.GetRotation().X, BoneTransform.GetRotation().Y, BoneTransform.GetRotation().Z, BoneTransform.GetRotation().W);
			}
		}
	}


	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
	{
		const FName PelvisName = PhysAnimBridge::GetRootBoneName();
		USkeletalMeshComponent* const Mesh = GetMeshComponent();
		if (Mesh)
		{
			if (const FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PelvisName))
			{
				const bool bActualSimulating = PelvisBody->IsInstanceSimulatingPhysics();
				if (!bActualSimulating)
				{
					const bool bIsFirstFailureTrigger = BalanceReadyTransition.GetFailureReason().IsEmpty();
					if (bIsFirstFailureTrigger && RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn)
					{
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PELVIS_SIM_CHECK_FAIL bone=%s pointer=%d [log]"), *PelvisName.ToString(), bActualSimulating ? 1 : 0);
					}
				}
			}
		}
	}

	// After the control loop, if the ramp just started, log the AFTER state.
	const int32 FinalGroupIndex = GetBringUpGroupCount() - 1;
	const double WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery && 
		BringUpGroupControlRampStartTimeSeconds.IsValidIndex(FinalGroupIndex) && 
		BringUpGroupControlRampStartTimeSeconds[FinalGroupIndex] == WorldTime &&
		WorldTime >= 0.0)
	{
		const float PolicyAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnimBalance] STATE FLIP - AFTER FINAL RAMP: time=%.4f policyAlpha=%.4f useSkelAnim=%s"),
			WorldTime,
			PolicyAlpha,
			bUseSkeletalAnimationTargetRepresentation ? TEXT("true") : TEXT("false"));
	}

	if ((bPhase1Prepare || bPhase1LateValidate || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn || RuntimeState == EPhysAnimRuntimeState::BridgeActive) && BalanceReadyTransition.IsDistalKinematicAccepted())
	{
		static bool bLoggedAuthoritativeWrite = false;
		if (!bLoggedAuthoritativeWrite)
		{
			UE_LOG(LogPhysAnimBridge, Log, TEXT("PHASE1_AUTHORITATIVE_PER_BONE_WRITE active=1 broadSetWriteBypassedForCriticalBones=1"));
			bLoggedAuthoritativeWrite = true;
		}

		// Broad sets for non-movement records are still fine
		PhysicsControl->SetBodyModifiersInSetPhysicsBlendWeight(TEXT("All"), 0.0f);
		PhysicsControl->SetBodyModifiersInSetCollisionType(TEXT("All"), ECollisionEnabled::NoCollision);
		PhysicsControl->SetBodyModifiersInSetUpdateKinematicFromSimulation(TEXT("All"), false);

		// But for movement type, we perform explicit per-bone writes to ensure the modifier record updates reliably.
		// We use bUpdateBody=true to ensure the engine state is updated immediately.
		for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			// Skip proximal bones (Group 0) during Phase 1 if they are NOT explicitly kept kinematic.
			// This prevents frame-internal thrash where we set it to Kinematic then Simulated immediately after.
			if ((bPhase1Prepare || bPhase1LateValidate) && ResolveBringUpGroupIndex(BoneName) == 0 && !BalanceReadyTransition.ShouldKeepBoneKinematic(BoneName, EffectiveSettings))
			{
				continue;
			}

			// NARROW GUARD: Do not drop the root during the Phase 2 guard window here.
			// Per-bone resolution (Phase 2 persistence guard) later in the function handles the final write.
			if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn && BoneName == PhysAnimBridge::GetRootBoneName())
			{
				continue;
			}

			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			TrackDistalModifierWrite(BoneName, EPhysicsMovementType::Kinematic, true, TEXT("ApplyRuntimeControlTuning_AuthoritativeDistalKin"));
			PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Kinematic, false, true);
		}
	}
	else
	{
		PhysicsControl->SetBodyModifiersInSetMovementType(TEXT("All"), EPhysicsMovementType::Kinematic);
		
		PhysicsControl->SetBodyModifiersInSetPhysicsBlendWeight(TEXT("All"), 0.0f);
		PhysicsControl->SetBodyModifiersInSetCollisionType(TEXT("All"), ECollisionEnabled::NoCollision);
		PhysicsControl->SetBodyModifiersInSetUpdateKinematicFromSimulation(TEXT("All"), false);
	}

	TrackDistalBoneOwnershipChange(TEXT("calf_r"), EPhysicsMovementType::Kinematic, TEXT("ApplyRuntimeControlTuning_SetAllKinematic"));
	TrackDistalBoneOwnershipChange(TEXT("foot_r"), EPhysicsMovementType::Kinematic, TEXT("ApplyRuntimeControlTuning_SetAllKinematic"));
	TrackDistalBoneOwnershipChange(TEXT("ball_r"), EPhysicsMovementType::Kinematic, TEXT("ApplyRuntimeControlTuning_SetAllKinematic"));

	// Use the pre-calculated value from the top of the function
	const bool bAllowRootBodyModifierSimulationInBalanceMode = bAllowRootSim;
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	USkeletalMeshComponent* const MeshComponentPtr = GetMeshComponent();
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			continue;
		}

		const FName RootBoneNameInternal = PhysAnimBridge::GetRootBoneName();
		const bool bIsRootBodyModifier = BoneName == RootBoneNameInternal;
		FTransform PelvisTransformPre = FTransform::Identity;
		if (bIsRootBodyModifier)
		{
			USkeletalMeshComponent* const Mesh = GetMeshComponent();
			PelvisTransformPre = Mesh ? Mesh->GetBoneTransform(Mesh->GetBoneIndex(RootBoneNameInternal)) : FTransform::Identity;
		}

		const int32 BringUpGroupIndex = ResolveBringUpGroupIndex(BoneName);
		const bool bPhase2RootOnGuardWindow =
			RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
		const bool bTransitionKeepsBoneKinematic =
			(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
				RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
				RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
				RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle ||
				RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny) &&
			BalanceReadyTransition.ShouldKeepBoneKinematic(BoneName, EffectiveSettings);
		const bool bTransitionOwnsRootOnThisTick =
			bIsRootBodyModifier &&
			bPhase2RootOnGuardWindow &&
			!bTransitionKeepsBoneKinematic &&
			RuntimeState != EPhysAnimRuntimeState::BalanceSafeDeny;
		const bool bPhase2RootAuthorityQuarantined =
			bIsRootBodyModifier &&
			bPhase2RootOnGuardWindow &&
			BalanceReadyTransition.IsPhase2RootAuthorityQuarantined();
		const bool bIsCertifiedRootOnPreservedBone =
			BoneName == TEXT("thigh_l") || BoneName == TEXT("thigh_r") ||
			BoneName == TEXT("spine_01") || BoneName == TEXT("spine_02") || BoneName == TEXT("spine_03");
		const bool bIsRootOnUpperSupportBone =
			BoneName == TEXT("neck_01") || BoneName == TEXT("clavicle_l") || BoneName == TEXT("clavicle_r");
		const bool bIsRootOnOrSettleSupportFollowWindow =
			(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
			 RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle) &&
			bIsRootOnUpperSupportBone;

		// During entry transition the component path keeps the root body modifier kinematic
		// until the transition explicitly enters Phase 2 root-on. Once Phase 2 owns the guard
		// window, later per-tick bring-up resolution must not demote the pelvis back off.
		const bool bAllowRootBodyModifierSimulation =
			bIsRootBodyModifier &&
			(bAllowRootBodyModifierSimulationInBalanceMode || bTransitionOwnsRootOnThisTick) &&
			RuntimeState != EPhysAnimRuntimeState::BalanceSafeDeny;
		
		if (bIsRootBodyModifier && bAllowRootBodyModifierSimulation)
		{
			// Resetting bLastAppliedPresentationRootSimulationEnabled happens at the END of the loop
		}

		bool bBringUpGroupUnlocked =
			bIsRootBodyModifier ? bAllowRootBodyModifierSimulation : IsBringUpGroupUnlocked(BringUpGroupIndex);

		if (bTransitionKeepsBoneKinematic)
		{
			bBringUpGroupUnlocked = false;
		}
		const bool bBodyModifierActivatedThisTick =
			(!bIsRootBodyModifier && bSimulationHandoffCompletedThisTick) ||
			(bIsRootBodyModifier && bAllowRootBodyModifierSimulation && !bLastAppliedPresentationRootSimulationEnabled);
		EPhysicsMovementType BodyModifierMovementType = EPhysicsMovementType::Kinematic;
		float BodyModifierPhysicsBlendWeight = 0.0f;
		bool bUpdateKinematicFromSimulation = false;
		ECollisionEnabled::Type BodyModifierCollisionType =
			ResolveBodyModifierCollisionType(
				RuntimeState,
				EffectiveSettings.bForceZeroActions,
				bSimulationHandoffSettled,
				bBringUpGroupUnlocked,
				bIsRootBodyModifier,
				bAllowRootBodyModifierSimulation);
		ResolveBodyModifierRuntimeMode(
			RuntimeState,
			EffectiveSettings.bForceZeroActions,
			bSimulationHandoffSettled,
			bBringUpGroupUnlocked,
			bIsRootBodyModifier,
			bAllowRootBodyModifierSimulation,
			BodyModifierMovementType,
			BodyModifierPhysicsBlendWeight,
			bUpdateKinematicFromSimulation);

		if (bPhase1Prepare || bPhase1LateValidate)
		{
			// Enforce Phase 1 topology (root=kin, proximal=sim, distal=sim/kin based on experiment, upper=kin)
			if (bIsRootBodyModifier || bTransitionKeepsBoneKinematic)
			{
				BodyModifierMovementType = EPhysicsMovementType::Kinematic;
				BodyModifierCollisionType = ECollisionEnabled::NoCollision;
				BodyModifierPhysicsBlendWeight = 0.0f;
				bUpdateKinematicFromSimulation = bIsRootBodyModifier;
			}
			else if (BringUpGroupIndex == 0 || BringUpGroupIndex == 1)
			{
				// Group 0 = Proximal (thighs + spine), Group 1 = Distal (calves + feet + balls)
				BodyModifierMovementType = EPhysicsMovementType::Simulated;
				BodyModifierCollisionType = ECollisionEnabled::QueryAndPhysics;
				BodyModifierPhysicsBlendWeight = 1.0f;
			}
			else
			{
				// Upper body (Groups 2, 3, 4)
				BodyModifierMovementType = EPhysicsMovementType::Kinematic;
				BodyModifierCollisionType = ECollisionEnabled::NoCollision;
				BodyModifierPhysicsBlendWeight = 0.0f;
			}
		}
		if (bPhase2RootAuthorityQuarantined && !bTransitionOwnsRootOnThisTick && !bLastAppliedPresentationRootSimulationEnabled)
		{
			BodyModifierMovementType = EPhysicsMovementType::Kinematic;
			BodyModifierPhysicsBlendWeight = 0.0f;
			BodyModifierCollisionType = ECollisionEnabled::NoCollision;
			if (!bIsRootBodyModifier)
			{
				bUpdateKinematicFromSimulation = false;
			}
		}

		const bool bRootOnApplicationTick =
			RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn &&
			!bTransitionKeepsBoneKinematic &&
			(bTransitionOwnsRootOnThisTick || (bIsCertifiedRootOnPreservedBone && bAllowRootBodyModifierSimulation));
		if (bRootOnApplicationTick && (bIsRootBodyModifier || bIsCertifiedRootOnPreservedBone))
		{
			BodyModifierMovementType = EPhysicsMovementType::Simulated;
			BodyModifierPhysicsBlendWeight = 1.0f;
			BodyModifierCollisionType = ECollisionEnabled::QueryAndPhysics;
			bUpdateKinematicFromSimulation = false;
		}
		else if (bIsRootOnOrSettleSupportFollowWindow && bTransitionKeepsBoneKinematic)
		{
			// Docs keep the upper body kinematic through RootOn/Settle, but the immediate
			// spine_03 support bones should follow live simulation instead of behaving like
			// rigid world anchors on the first dynamic root-on frame.
			BodyModifierMovementType = EPhysicsMovementType::Kinematic;
			BodyModifierPhysicsBlendWeight = 0.0f;
			BodyModifierCollisionType = ECollisionEnabled::NoCollision;
			bUpdateKinematicFromSimulation = true;
		}

		if (bIsRootBodyModifier)
		{
			const float RootSoftSimAlpha = BalanceReadyTransition.GetRootBodyModifierSoftSimAlpha();

			// NARROW GUARD: During the Phase 2 guard window, if the transition owns the root-on commitment,
			// or we are still in the quarantine window, we skip the soft-sim/collision suppression 
			// to ensure stable simulation bring-up.
			if (!bTransitionOwnsRootOnThisTick && !BalanceReadyTransition.IsPhase2RootAuthorityQuarantined())
			{
				BodyModifierPhysicsBlendWeight *= RootSoftSimAlpha;
				if (bPhase2RootOnGuardWindow && RootSoftSimAlpha < 1.0f)
				{
					BodyModifierCollisionType = ECollisionEnabled::NoCollision;
				}
			}
		}

		// Phase 2 Root Promotion Audit (One-Shot per Frame)
		const bool bIsRootTraceTargetState = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
		if (bIsRootBodyModifier && bIsRootTraceTargetState)
		{
			const int32 CurrentFrame = static_cast<int32>(GFrameNumber);
			
			// DROP CULPRIT: Trace if the pelvis was simulating but we are about to write it as kinematic
			if (bLastAppliedPresentationRootSimulationEnabled && BodyModifierMovementType == EPhysicsMovementType::Kinematic)
			{
				UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_ROOT_DROP_CULPRIT frame=%d bone=%s previousSim=1 requestedSim=%d quarantined=%d keepsKin=%d source=ApplyRuntimeControlTuning"),
					CurrentFrame, *BoneName.ToString(), 
					bAllowRootBodyModifierSimulation ? 1 : 0, 
					bPhase2RootAuthorityQuarantined ? 1 : 0,
					bTransitionKeepsBoneKinematic ? 1 : 0);
			}

			const int32 RawReadbackValue = -1;

			// Calculate TotalSimCount for the probe
			int32 TotalSimCount = 0;
			if (USkeletalMeshComponent* const Mesh = GetMeshComponent())
			{
				for (const FName& SimBone : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
				{
					if (const FBodyInstance* const BI = Mesh->GetBodyInstance(SimBone))
					{
						if (BI->IsInstanceSimulatingPhysics())
						{
							TotalSimCount++;
						}
					}
				}
			}

			static EPhysAnimRuntimeState LastAuditState = EPhysAnimRuntimeState::Uninitialized;
			const bool bRuntimeStateChanged = (RuntimeState != LastAuditState);

			static TMap<UPhysAnimComponent*, EPhysicsMovementType> LastRootMovementTypes;
			static TMap<UPhysAnimComponent*, bool> LastRootQuarantinedStates;
			static TMap<UPhysAnimComponent*, int32> LastRootRawReadbacks;

			const EPhysicsMovementType LastRootMovementType = LastRootMovementTypes.Contains(this) ? LastRootMovementTypes[this] : EPhysicsMovementType::Static;
			const bool LastRootQuarantined = LastRootQuarantinedStates.Contains(this) ? LastRootQuarantinedStates[this] : false;
			const int32 LastRootRawReadback = LastRootRawReadbacks.Contains(this) ? LastRootRawReadbacks[this] : -2;

			const bool bStateChanged = (BodyModifierMovementType != LastRootMovementType) ||
				(bPhase2RootAuthorityQuarantined != LastRootQuarantined) ||
				(RawReadbackValue != LastRootRawReadback);

			const bool bIsRootOn = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn);
			const bool bShouldEmitRootOnAudit = bIsRootOn && (BalanceEntryRootOnFrameCount <= 4 || bStateChanged);

			if (bShouldEmitRootOnAudit)
			{
				LastRootMovementTypes.FindOrAdd(this) = BodyModifierMovementType;
				LastRootQuarantinedStates.FindOrAdd(this) = bPhase2RootAuthorityQuarantined;
				LastRootRawReadbacks.FindOrAdd(this) = RawReadbackValue;
			}
		}

		if ((bPhase1Prepare || bPhase1LateValidate || RuntimeState == EPhysAnimRuntimeState::BridgeActive) &&
			(BoneName == TEXT("calf_r") || BoneName == TEXT("foot_r") || BoneName == TEXT("ball_r") ||
			 BoneName == TEXT("calf_l") || BoneName == TEXT("foot_l") || BoneName == TEXT("ball_l")) &&
			BalanceReadyTransition.IsDistalKinematicAccepted() &&
			BodyModifierMovementType == EPhysicsMovementType::Simulated)
		{
			// Explicit precedence rule: distal experiment wins
			BodyModifierMovementType = EPhysicsMovementType::Kinematic;
			BodyModifierCollisionType = ECollisionEnabled::NoCollision;
			BodyModifierPhysicsBlendWeight = 0.0f;
			bUpdateKinematicFromSimulation = false;
			
			// Telemetry when this safety override catches a conflicting re-promotion attempt
			if (!BalanceReadyTransition.LoggedSuppressedDistalBones.Contains(BoneName))
			{
				if (GVerbosePhase2Forensics != 0)
				{
					UE_LOG(LogPhysAnimBridge, Log, TEXT("DISTAL_SYNC_REPROMOTION_SUPPRESSED bone=%s phase=%s reason=PerBone_BodyModSync blockedBy=DistalOwnershipRule"), 
						*BoneName.ToString(), 
						GetRuntimeStateName(RuntimeState));
				}
				BalanceReadyTransition.LoggedSuppressedDistalBones.Add(BoneName);
			}

			if (bPhase1Prepare || bPhase1LateValidate)
			{
				if (GVerbosePhase1Forensics != 0)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_MODIFIER_SYNC_CORRECTED bone=%s phase=%s previousModifier=Simulated correctedModifier=Kinematic reason=AcceptedPhase1Topology"), 
						*BoneName.ToString(), 
						bPhase1Prepare ? TEXT("BalanceEntry_Prepare") : TEXT("BalanceEntry_LateValidate"));
				}
			}
		}

		const bool bRootBodyModLogStateChanged =
			bAllowRootBodyModifierSimulation != bLastAppliedPresentationRootSimulationEnabled ||
			bTransitionOwnsRootOnThisTick ||
			bTransitionKeepsBoneKinematic ||
			bBringUpGroupUnlocked ||
			bBodyModifierActivatedThisTick;
		if (bIsRootBodyModifier && bRootBodyModLogStateChanged)
		{
			if (GVerbosePhase2Forensics != 0 || RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn)
			{
				UE_LOG(
					LogPhysAnimBridge,
					Verbose,
					TEXT("[PhysAnimBalance] PELVIS_BODYMOD tickPhase=%d allowRootSim=%d transitionOwnsRootOn=%d transitionKeepKinematic=%d bringUpUnlocked=%d simHandoffSettled=%d movementType=%d collisionType=%d updateKinematicFromSimulation=%d bodyActivatedThisTick=%d lastAppliedRootSim=%d pendingResets=%d"),
					static_cast<int32>(RuntimeState),
					bAllowRootBodyModifierSimulation ? 1 : 0,
					bTransitionOwnsRootOnThisTick ? 1 : 0,
					bTransitionKeepsBoneKinematic ? 1 : 0,
					bBringUpGroupUnlocked ? 1 : 0,
					bSimulationHandoffSettled ? 1 : 0,
					static_cast<int32>(BodyModifierMovementType),
					static_cast<int32>(BodyModifierCollisionType),
					bUpdateKinematicFromSimulation ? 1 : 0,
					bBodyModifierActivatedThisTick ? 1 : 0,
					bLastAppliedPresentationRootSimulationEnabled ? 1 : 0,
					PendingBodyModifierCachedResetNames.Num());
			}
		}

		PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(
			ModifierName,
			bUpdateKinematicFromSimulation,
			false,
			false);
		TrackDistalModifierWrite(BoneName, BodyModifierMovementType, (bPhase1Prepare || bPhase1LateValidate || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn), TEXT("ApplyRuntimeControlTuning_PerBone_BodyModSync"));
		PhysicsControl->SetBodyModifierMovementType(ModifierName, BodyModifierMovementType, false, (bPhase1Prepare || bPhase1LateValidate || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn));
		

		if (bPhase1Prepare || bPhase1LateValidate)
		{
			if (BoneName == TEXT("spine_01") || BoneName == TEXT("calf_r"))
			{
				if (GVerbosePhase1Forensics != 0)
				{
					const USkeletalMeshComponent* const Mesh = GetMeshComponent();
					const FBodyInstance* const TargetBody = Mesh ? Mesh->GetBodyInstance(BoneName) : nullptr;
					const bool bRawSimulating = TargetBody ? TargetBody->IsInstanceSimulatingPhysics() : false;
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_MODIFIER_SYNC bone=%s movement=%s updateBody=1 rawSim=%d"),
						*BoneName.ToString(),
						UPhysAnimComponent::GetPhysicsMovementTypeName(BodyModifierMovementType),
						bRawSimulating ? 1 : 0);
				}
			}
		}

		TrackDistalBoneOwnershipChange(BoneName, BodyModifierMovementType, TEXT("ApplyRuntimeControlTuning_PerBone_BodyModSync"));
		PhysicsControl->SetBodyModifierMovementType(ModifierName, BodyModifierMovementType, false, (bPhase1Prepare || bPhase1LateValidate || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn));
		PhysicsControl->SetBodyModifierPhysicsBlendWeight(ModifierName, BodyModifierPhysicsBlendWeight, false, false);
		PhysicsControl->SetBodyModifierCollisionType(ModifierName, BodyModifierCollisionType, false, false);
		if (bIsRootBodyModifier && (bPhase1Prepare || bPhase1LateValidate))
		{
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				BodyModifierMovementType,
				BodyModifierPhysicsBlendWeight,
				BodyModifierCollisionType,
				bUpdateKinematicFromSimulation);
		}
		if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn &&
			(bIsRootBodyModifier || bIsCertifiedRootOnPreservedBone))
		{
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				BodyModifierMovementType,
				BodyModifierPhysicsBlendWeight,
				BodyModifierCollisionType,
				bUpdateKinematicFromSimulation);
		}
		if (bIsRootBodyModifier && bRootOnApplicationTick && BodyModifierMovementType == EPhysicsMovementType::Simulated)
		{
			PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(ModifierName, false, false, false);
			PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Simulated, false, true);
			ForceBodyModifierRecordState(
				PhysicsControl,
				ModifierName,
				EPhysicsMovementType::Simulated,
				1.0f,
				ECollisionEnabled::QueryAndPhysics,
				false);
		}

		const float CurrentPolicyAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);

		if (bIsRootBodyModifier)
		{
			bLastAppliedPresentationRootSimulationEnabled = bAllowRootBodyModifierSimulation;
		}

		const bool bShouldResetThisBone = ShouldResetBodyModifierToCachedBoneTransform(
				BoneName,
				RuntimeState,
				EffectiveSettings.bForceZeroActions,
				bBodyModifierActivatedThisTick,
				bBringUpGroupUnlocked,
				bIsRootBodyModifier,
				bAllowRootBodyModifierSimulation,
				CurrentPolicyAlpha,
				BalanceReadyTransition.IsDistalKinematicAccepted());

		if (bShouldResetThisBone &&
			!PendingBodyModifierCachedResetNames.Contains(ModifierName))
		{
			if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
			{
				if (bIsRootBodyModifier)
				{
					UE_LOG(
						LogPhysAnimBridge,
						Error,
						TEXT("[PhysAnimBalance] STATE MACHINE VIOLATION: Cached-target reset for pelvis/root '%s' requested in Balance Mode. Failing and stopping mode. reason=pelvisResetRequestedDuringBalance"),
						*BoneName.ToString());
					FinalizeBalanceScenario(false, TEXT("pelvisResetRequestedDuringBalance"));
					StopBalancePerturbationMode();
				}
				else if (CurrentPolicyAlpha > 0.0f)
				{
					const FString ViolationReason = FString::Printf(TEXT("bodyResetViolation:%s"), *BoneName.ToString());
					UE_LOG(
						LogPhysAnimBridge,
						Error,
						TEXT("[PhysAnimBalance] STATE MACHINE VIOLATION: Cached-target reset for '%s' requested after policy influence has begun (Alpha=%.2f). Failing and stopping mode."),
						*BoneName.ToString(), CurrentPolicyAlpha);
					FinalizeBalanceScenario(false, ViolationReason);
					StopBalancePerturbationMode();
				}
				else
				{
					if (BalanceTransitionSets::IsUpperBody(BoneName) && 
						(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate) &&
						BalanceReadyTransition.IsUpperBodyKinematicHoldActive())
					{
						if (GVerbosePhase1Forensics != 0)
						{
							UE_LOG(LogPhysAnimBridge, Log, TEXT("PHASE1_UPPER_BODY_RESET_READD_SUPPRESSED bone=%s source=recovery"), *BoneName.ToString());
						}
					}
					else
					{
						PendingBodyModifierCachedResetNames.Add(ModifierName);
					}
				}
			}
			else
			{
				if (BalanceTransitionSets::IsUpperBody(BoneName) && 
					(RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate) &&
					BalanceReadyTransition.IsUpperBodyKinematicHoldActive())
				{
					if (GVerbosePhase1Forensics != 0)
					{
						UE_LOG(LogPhysAnimBridge, Log, TEXT("PHASE1_UPPER_BODY_RESET_READD_SUPPRESSED bone=%s source=applyTuning"), *BoneName.ToString());
					}
				}
				else
				{
					PendingBodyModifierCachedResetNames.Add(ModifierName);
				}
			}
		}
	}

	if (bHipQuarantineActiveThisFrame)
	{
		if (HipQuarantineTicksRemaining > 0 && RuntimeState != EPhysAnimRuntimeState::FailStopped)
		{
			--HipQuarantineTicksRemaining;
			bHipQuarantineReleasedThisFrame = (HipQuarantineTicksRemaining == 0);
		}

		if (bHipQuarantineReleasedThisFrame)
		{
			if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
			{
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] HIP_QUARANTINE_RELEASED"));
			}
		}
	}


	LastAppliedStabilizationSettings = EffectiveSettings;
	bLastAppliedSimulationHandoffSettled = bSimulationHandoffSettled;
	LastAppliedControlAuthorityAlpha = CalculateCurrentControlAuthorityAlpha(EffectiveSettings);
	// bLastAppliedPresentationRootSimulationEnabled is now updated inside the loop for the root bone
}


void UPhysAnimComponent::ReconcilePhase1DistalModifierRecords(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!PhysicsControl || !Mesh)
	{
		return;
	}

	const FName DistalBones[] = { TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r") };
	for (const FName BoneName : DistalBones)
	{
		if (BalanceReadyTransition.IsDistalKinematicAccepted())
		{
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			EPhysicsMovementType ModifierMovementType = EPhysicsMovementType::Simulated;
			
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
			{
				ModifierMovementType = Record->BodyModifier.ModifierData.MovementType;
			}

			const FBodyInstance* BodyInst = Mesh->GetBodyInstance(BoneName);
			const bool bRawSimulating = BodyInst && BodyInst->IsValidBodyInstance() ? BodyInst->IsInstanceSimulatingPhysics() : false;

			if (ModifierMovementType != EPhysicsMovementType::Kinematic)
			{
				if (GVerbosePhase1Forensics != 0)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("PHASE1_DISTAL_RECORD_REPAIRED bone=%s prevModifier=%s rawBody=%s"),
						*BoneName.ToString(),
						GetPhysicsMovementTypeName(ModifierMovementType),
						bRawSimulating ? TEXT("Simulated") : TEXT("Kinematic"));
				}
				
				PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Kinematic);
				TrackDistalModifierWrite(BoneName, EPhysicsMovementType::Kinematic, false, TEXT("ReconcilePhase1DistalModifierRecords"));
			}
			else
			{
				if (GVerbosePhase1Forensics != 0)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("PHASE1_DISTAL_RECORD_ENTRY_STATE bone=%s modifier=%s rawBody=%s"),
						*BoneName.ToString(),
						GetPhysicsMovementTypeName(ModifierMovementType),
						bRawSimulating ? TEXT("Simulated") : TEXT("Kinematic"));
				}
			}
		}
	}
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
