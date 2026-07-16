#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"

float UPhysAnimComponent::CalculateSimulationHandoffAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (EffectiveSettings.bForceZeroActions)
	{
		return 0.0f;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	const double ElapsedSeconds = FMath::Max(CurrentTimeSeconds - BridgeStartTimeSeconds, 0.0);
	const double HandoffDurationSeconds = FMath::Max(static_cast<double>(EffectiveSettings.StartupRampSeconds), UE_DOUBLE_SMALL_NUMBER);
	return FMath::Clamp(static_cast<float>(ElapsedSeconds / HandoffDurationSeconds), 0.0f, 1.0f);
}


float UPhysAnimComponent::CalculateCurrentControlAuthorityAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (EffectiveSettings.bForceZeroActions)
	{
		return 0.0f;
	}

	float MaxControlAuthorityAlpha = 0.0f;
	for (int32 GroupIndex = 0; GroupIndex <= HighestUnlockedBringUpGroupIndex; ++GroupIndex)
	{
		MaxControlAuthorityAlpha = FMath::Max(
			MaxControlAuthorityAlpha,
			CalculateBringUpGroupControlAuthorityAlpha(GroupIndex, EffectiveSettings));
	}

	return MaxControlAuthorityAlpha;
}


float UPhysAnimComponent::CalculateCurrentPolicyInfluenceAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (IsCausalPolicyControlRuntimeState(RuntimeState))
	{
		return StandingActivation.GetStatus().LinearBlendAlpha;
	}

	if (PolicyInfluenceRampStartTimeSeconds < 0.0)
	{
		return 0.0f;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	const float ElapsedSincePolicyRampStartSeconds = static_cast<float>(
		FMath::Max(CurrentTimeSeconds - PolicyInfluenceRampStartTimeSeconds, 0.0));
	return CalculatePolicyInfluenceAlpha(
		false,
		true,
		ElapsedSincePolicyRampStartSeconds,
		EffectiveSettings.StartupRampSeconds);
}


bool UPhysAnimComponent::IsPresentationPerturbationOverrideActive() const
{
	const UWorld* const World = GetWorld();
	return World &&
		PresentationPerturbationOverrideEndTimeSeconds >= 0.0 &&
		World->GetTimeSeconds() < PresentationPerturbationOverrideEndTimeSeconds;
}


void UPhysAnimComponent::UnlockBringUpGroup(int32 GroupIndex, const TCHAR* Context)
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	const bool bFrozen = bStartupBringUpFrozenByBalanceEntry && !bPhase1Prepare && !bPhase1LateValidate;

	if (bPhase1Prepare || bPhase1LateValidate || bFrozen ||
		GroupIndex < 0 ||
		GroupIndex >= GetBringUpGroupCount() ||
		GroupIndex <= HighestUnlockedBringUpGroupIndex)
	{
		return;
	}

	// No blocking for indices >= 2 during LateValidate/Prepare if we want distal bring-up

	if (!BringUpGroupActivationTimeSeconds.IsValidIndex(GroupIndex))
	{
		BringUpGroupActivationTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	}
	if (!BringUpGroupControlRampStartTimeSeconds.IsValidIndex(GroupIndex))
	{
		BringUpGroupControlRampStartTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	}
	if (!BringUpGroupAlphaActiveLogged.IsValidIndex(GroupIndex))
	{
		BringUpGroupAlphaActiveLogged.Init(false, GetBringUpGroupCount());
	}

	const double ActivationTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	BringUpGroupActivationTimeSeconds[GroupIndex] = ActivationTimeSeconds;
	const bool bDelayControlRamp = ShouldDelayBringUpGroupControlRamp(GroupIndex, GetBringUpGroupCount());
	const bool bStartControlRampImmediately = ShouldStartBringUpGroupControlRamp(
		false,
		true,
		bDelayControlRamp,
		false,
		bStartupBringUpFrozenByBalanceEntry);
	BringUpGroupControlRampStartTimeSeconds[GroupIndex] =
		bStartControlRampImmediately ? ActivationTimeSeconds : -1.0;
	HighestUnlockedBringUpGroupIndex = GroupIndex;
	BringUpGroupStableAccumulatedSeconds = 0.0f;

	TArray<FString> GroupBoneNames;
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		if (ResolveBringUpGroupIndex(BoneName) != GroupIndex)
		{
			continue;
		}

		GroupBoneNames.Add(BoneName.ToString());

		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!PendingBodyModifierCachedResetNames.Contains(ModifierName))
		{
			if (IsBalanceActiveState(RuntimeState) && CalculateCurrentPolicyInfluenceAlpha(ResolveEffectiveStabilizationSettings()) > 0.0f)
			{
				const FString ViolationReason = FString::Printf(TEXT("bodyPromotionViolation:%s"), *BoneName.ToString());
				PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnimBalance] STATE MACHINE VIOLATION: Body promotion for '%s' requested after policy influence has begun. Failing and stopping mode."),
					*BoneName.ToString());
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
						PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("PHASE1_UPPER_BODY_RESET_READD_SUPPRESSED bone=%s source=unlockGroup"), *BoneName.ToString());
					}
				}
				else
				{
					PendingBodyModifierCachedResetNames.Add(ModifierName);
				}
			}
		}
	}

	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Stabilization bring-up unlocked group %d/%d [%s] context=%s"),
		GroupIndex + 1,
		GetBringUpGroupCount(),
		*FString::Join(GroupBoneNames, TEXT(", ")),
		Context);
}



void UPhysAnimComponent::ApplyStartupMovementLock()
{
	ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	if (!CharacterMovement)
	{
		bStartupMovementLockActive = false;
		return;
	}

	if (!bHasSavedStartupMovementLockState)
	{
		StartupMovementLockOriginalMode = CharacterMovement->MovementMode;
		StartupMovementLockOriginalCustomMovementMode = CharacterMovement->CustomMovementMode;
		bStartupMovementLockOriginalTickEnabled = CharacterMovement->IsComponentTickEnabled();
		bHasSavedStartupMovementLockState = true;
	}

	CharacterMovement->StopMovementImmediately();
	CharacterMovement->DisableMovement();
	CharacterMovement->SetComponentTickEnabled(false);

	if (IsBalanceActiveState(RuntimeState))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (!bHasSavedCapsuleCollisionState)
			{
				OriginalCapsuleCollisionEnabled = CapsuleComponent->GetCollisionEnabled();
				bHasSavedCapsuleCollisionState = true;
			}
			CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	bStartupMovementLockActive = true;
}


void UPhysAnimComponent::ReleaseStartupMovementLock(bool bRestoreCharacterMovement)
{
	if (!bStartupMovementLockActive && !bHasSavedStartupMovementLockState)
	{
		return;
	}

	ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	if (CharacterMovement && bHasSavedStartupMovementLockState && bRestoreCharacterMovement)
	{
		CharacterMovement->SetComponentTickEnabled(bStartupMovementLockOriginalTickEnabled);
		CharacterMovement->SetMovementMode(static_cast<EMovementMode>(StartupMovementLockOriginalMode), StartupMovementLockOriginalCustomMovementMode);
		bHasSavedStartupMovementLockState = false;
	}

	if (CharacterOwner && bHasSavedCapsuleCollisionState && bRestoreCharacterMovement)
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			CapsuleComponent->SetCollisionEnabled(OriginalCapsuleCollisionEnabled);
			bHasSavedCapsuleCollisionState = false;
		}
	}

	bStartupMovementLockActive = false;
	if (bRestoreCharacterMovement)
	{
		bHasSavedStartupMovementLockState = false;
	}
}


void UPhysAnimComponent::ResetStartupQuietWindowState()
{
	StartupQuietWindowAccumulatedSeconds = 0.0;
	bHasLastStartupQuietActorRotation = false;
	LastStartupQuietActorRotation = FRotator::ZeroRotator;
	LastStartupQuietGateLogTimeSeconds = -1.0;
}


void UPhysAnimComponent::ResetPolicySettleWindowState()
{
	PolicySettleWindowAccumulatedSeconds = 0.0;
	LastPolicySettleGateLogTimeSeconds = -1.0;
}


bool UPhysAnimComponent::UpdateStartupQuietWindow(
	float DeltaTime,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	float& OutLinearSpeedCmPerSecond,
	float& OutAngularSpeedDegPerSecond)
{
	OutLinearSpeedCmPerSecond = 0.0f;
	OutAngularSpeedDegPerSecond = 0.0f;

	if (!EffectiveSettings.bLockCharacterMovementUntilStartupReady)
	{
		return true;
	}

	if (EffectiveSettings.StartupQuietRequiredSeconds <= 0.0f)
	{
		return true;
	}

	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return true;
	}

	OutLinearSpeedCmPerSecond = OwnerActor->GetVelocity().Size();

	const FRotator CurrentActorRotation = OwnerActor->GetActorRotation();
	if (bHasLastStartupQuietActorRotation && DeltaTime > SMALL_NUMBER)
	{
		const FRotator DeltaRotation = (CurrentActorRotation - LastStartupQuietActorRotation).GetNormalized();
		OutAngularSpeedDegPerSecond =
			FMath::Max3(
				FMath::Abs(DeltaRotation.Roll),
				FMath::Abs(DeltaRotation.Pitch),
				FMath::Abs(DeltaRotation.Yaw)) / DeltaTime;
	}

	LastStartupQuietActorRotation = CurrentActorRotation;
	bHasLastStartupQuietActorRotation = true;

	const bool bLocomotionEntryRequested = IsBridgeLocomotionEntryRequested(EffectiveSettings);
	const bool bWithinQuietWindow =
		bLocomotionEntryRequested ||
		(
			OutLinearSpeedCmPerSecond <= EffectiveSettings.StartupQuietLinearSpeedThresholdCmPerSecond &&
			OutAngularSpeedDegPerSecond <= EffectiveSettings.StartupQuietAngularSpeedThresholdDegPerSec
		);

	if (bWithinQuietWindow)
	{
		StartupQuietWindowAccumulatedSeconds += DeltaTime;
	}
	else
	{
		StartupQuietWindowAccumulatedSeconds = 0.0;
	}

	return StartupQuietWindowAccumulatedSeconds >= EffectiveSettings.StartupQuietRequiredSeconds;
}


bool UPhysAnimComponent::UpdatePolicySettleWindow(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	float& OutShellOffsetCm,
	float& OutRootLinearSpeedCmPerSecond,
	float& OutRootAngularSpeedDegPerSecond)
{
	OutShellOffsetCm = 0.0f;
	OutRootLinearSpeedCmPerSecond = LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond;
	OutRootAngularSpeedDegPerSecond = LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond;

	if (!EffectiveSettings.bLockCharacterMovementUntilStartupReady || !EffectiveSettings.bDelayMovementUnlockUntilPolicySettled)
	{
		return true;
	}

	if (EffectiveSettings.PolicySettleRequiredSeconds <= 0.0f)
	{
		return true;
	}

	AActor* const OwnerActor = GetOwner();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (OwnerActor && SkeletalMesh)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		const FVector OwnerLocationCm = OwnerActor->GetActorLocation();
		const FVector RootLocationCm = SkeletalMesh->GetBoneLocation(RootBoneName, EBoneSpaces::WorldSpace);
		if (!bHasShellCouplingReferenceRootLocalOffset)
		{
			ShellCouplingReferenceRootLocalOffsetCm = RootLocationCm - OwnerLocationCm;
			bHasShellCouplingReferenceRootLocalOffset = true;
		}
		OutShellOffsetCm = ResolveShellCouplingPlanarOffsetDeltaCm(
			OwnerLocationCm,
			RootLocationCm,
			ShellCouplingReferenceRootLocalOffsetCm);
	}

	const bool bWithinQuietWindow =
		OutShellOffsetCm <= EffectiveSettings.PolicySettleMaxShellOffsetCm &&
		OutRootLinearSpeedCmPerSecond <= EffectiveSettings.PolicySettleMaxRootLinearSpeedCmPerSecond &&
		OutRootAngularSpeedDegPerSecond <= EffectiveSettings.PolicySettleMaxRootAngularSpeedDegPerSec;

	const UWorld* const World = GetWorld();
	const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.0f;
	if (bWithinQuietWindow)
	{
		PolicySettleWindowAccumulatedSeconds += DeltaSeconds;
	}
	else
	{
		PolicySettleWindowAccumulatedSeconds = 0.0;
	}

	return PolicySettleWindowAccumulatedSeconds >= EffectiveSettings.PolicySettleRequiredSeconds;
}


bool UPhysAnimComponent::HandlePrePolicyShellRecovery(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	if (!EffectiveSettings.bEnablePrePolicyShellRecovery)
	{
		return false;
	}

	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive)
	{
		return false;
	}

	if (PolicyInfluenceRampStartTimeSeconds >= 0.0 || CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings) > KINDA_SMALL_NUMBER)
	{
		return false;
	}

	AActor* const OwnerActor = GetOwner();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!OwnerActor || !SkeletalMesh || !PhysicsControl)
	{
		return false;
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (SkeletalMesh->GetBoneIndex(RootBoneName) == INDEX_NONE)
	{
		return false;
	}

	const FVector OwnerLocationCm = OwnerActor->GetActorLocation();
	const FVector RootLocationCm = SkeletalMesh->GetBoneLocation(RootBoneName, EBoneSpaces::WorldSpace);

	if (!bHasShellCouplingReferenceRootLocalOffset)
	{
		ShellCouplingReferenceRootLocalOffsetCm = RootLocationCm - OwnerLocationCm;
		bHasShellCouplingReferenceRootLocalOffset = true;
	}

	const float ShellPlanarOffsetDeltaCm = ResolveShellCouplingPlanarOffsetDeltaCm(
		OwnerLocationCm,
		RootLocationCm,
		ShellCouplingReferenceRootLocalOffsetCm);
	const float RootAngularSpeedDegPerSecond = LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond;

	if (ShellPlanarOffsetDeltaCm < EffectiveSettings.PrePolicyShellRecoveryOffsetThresholdCm &&
		RootAngularSpeedDegPerSecond < EffectiveSettings.PrePolicyShellRecoveryRootAngularSpeedThresholdDegPerSec)
	{
		return false;
	}

	FString RecoveryError;
	const bool bSeeded = SeedControlTargetsFromCurrentPose(0.0f, RecoveryError);

	TArray<FName> ModifierNamesToReset;
	ModifierNamesToReset.Reserve(PhysAnimBridge::GetRequiredBodyModifierBoneNames().Num());
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		if (BoneName == RootBoneName)
		{
			continue;
		}

		const int32 BringUpGroupIndex = ResolveBringUpGroupIndex(BoneName);
		if (!IsBringUpGroupUnlocked(BringUpGroupIndex))
		{
			continue;
		}

		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			ModifierNamesToReset.Add(ModifierName);
		}
	}

	if (!ModifierNamesToReset.IsEmpty())
	{
		PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(
			ModifierNamesToReset,
			EResetToCachedTargetBehavior::ResetDuringUpdateControls,
			true,
			false);
	}

	BringUpGroupStableAccumulatedSeconds = 0.0f;

	const double CurrentRecoveryTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (LastPrePolicyShellRecoveryLogTimeSeconds < 0.0 || (CurrentRecoveryTimeSeconds - LastPrePolicyShellRecoveryLogTimeSeconds) >= 0.5)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] Pre-policy shell recovery triggered: shellOffsetCm=%.1f rootAngDegPerSec=%.1f seeded=%s modifiersReset=%d error=%s"),
			ShellPlanarOffsetDeltaCm,
			RootAngularSpeedDegPerSecond,
			bSeeded ? TEXT("true") : TEXT("false"),
			ModifierNamesToReset.Num(),
			RecoveryError.IsEmpty() ? TEXT("None") : *RecoveryError);
		LastPrePolicyShellRecoveryLogTimeSeconds = CurrentRecoveryTimeSeconds;
	}

	return true;
}


void UPhysAnimComponent::AdvanceBringUpState(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	const bool bFrozen = bStartupBringUpFrozenByBalanceEntry && !bPhase1Prepare && !bPhase1LateValidate;

	if (bPhase1Prepare || bPhase1LateValidate)
	{
		return;
	}

	if (EffectiveSettings.bForceZeroActions || !IsBringUpGroupUnlocked(0) || bFrozen)
	{
		return;
	}

	/* 
	   Allow bring-up to continue during BalancePerturbationMode 
	   so we don't get stuck if the user starts the test during ramp-up.
	*/
	// if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery) { return; }

	if (HandlePrePolicyShellRecovery(EffectiveSettings))
	{
		return;
	}

	const bool bWithinBodyVelocityBounds =
		LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond <= EffectiveSettings.MaxRootLinearSpeedCmPerSecond &&
		LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond <= EffectiveSettings.MaxRootAngularSpeedDegPerSecond;
	const bool bWithinRootBounds =
		LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm <= EffectiveSettings.MaxRootHeightDeltaCm &&
		LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond <= EffectiveSettings.MaxRootLinearSpeedCmPerSecond &&
		LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond <= EffectiveSettings.MaxRootAngularSpeedDegPerSecond;
	if (!bWithinBodyVelocityBounds || !bWithinRootBounds)
	{
		BringUpGroupStableAccumulatedSeconds = 0.0f;
		return;
	}

	BringUpGroupStableAccumulatedSeconds += DeltaTime;
	float StableDwellSeconds = 0.10f; // Rapid unlock during transition.
	
	if (GStrictPhase1Certification != 0)
	{
		StableDwellSeconds = 0.25f;
	}

	if (BringUpGroupStableAccumulatedSeconds < StableDwellSeconds)
	{
		return;
	}

	if (BringUpGroupStableAccumulatedSeconds < 0.25f - KINDA_SMALL_NUMBER)
	{
		BalanceReadyTransition.Audit.bUsedDwellShortcut = true;
	}

	const int32 CoreFinalGroupIndex = FMath::Min(1, GetBringUpGroupCount() - 1);
	if (HighestUnlockedBringUpGroupIndex >= CoreFinalGroupIndex)
	{
		if (BringUpGroupControlRampStartTimeSeconds.IsValidIndex(CoreFinalGroupIndex) &&
			BringUpGroupControlRampStartTimeSeconds[CoreFinalGroupIndex] < 0.0 &&
			ShouldStartBringUpGroupControlRamp(
				EffectiveSettings.bForceZeroActions,
				true,
				ShouldDelayBringUpGroupControlRamp(CoreFinalGroupIndex, GetBringUpGroupCount()),
				true,
				bStartupBringUpFrozenByBalanceEntry))
		{
			const double WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
			const float CurrentAngularVelocity = LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond;
			
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] STATE FLIP - BEFORE FINAL RAMP: time=%.4f handoffAlpha=%.4f unlockedGroup=%d distalComposition=%s rootAngVel=%.2f"),
				WorldTime,
				SimulationHandoffAlpha,
				HighestUnlockedBringUpGroupIndex,
				bDistalLocomotionCompositionModeActive ? TEXT("On") : TEXT("Off"),
				CurrentAngularVelocity);

			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] PHASE1_BRINGUP_RAMP_ARMED group=%d"), CoreFinalGroupIndex);

			BringUpGroupControlRampStartTimeSeconds[CoreFinalGroupIndex] = WorldTime;
			BringUpGroupStableAccumulatedSeconds = 0.0f;
			
			const float AngularVelocity = LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond;
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Stabilization final-group control ramp enabled for group %d/%d [hand_l, hand_r]. PelvisAngularVelocity=%.2f deg/s. PendingResets=%d"),
				CoreFinalGroupIndex + 1,
				GetBringUpGroupCount(),
				AngularVelocity,
				PendingBodyModifierCachedResetNames.Num());

			// Violation class: If the act of enabling the final ramp causes a massive root spike, 
			// it means our bridge setup itself is explosive.
			if (IsBalanceActiveState(RuntimeState) && AngularVelocity > EffectiveSettings.MaxRootAngularSpeedDegPerSecond)
			{
				PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnimBalance] STATE MACHINE VIOLATION: Ramp enable caused large root angular spike (%.2f > %.2f). reason=rampEnableRootSpike"),
					AngularVelocity,
					EffectiveSettings.MaxRootAngularSpeedDegPerSecond);
				FinalizeBalanceScenario(false, TEXT("rampEnableRootSpike"));
				StopBalancePerturbationMode();
			}

			return;
		}
		
		if (PolicyInfluenceRampStartTimeSeconds < 0.0 &&
			ShouldStartPolicyInfluenceRamp(
				RuntimeState,
				EffectiveSettings.bForceZeroActions,
				true,
				IsBringUpGroupControlRampActive(CoreFinalGroupIndex),
				true,
				bStartupBringUpFrozenByBalanceEntry) &&
			RuntimeState != EPhysAnimRuntimeState::BalanceSafeDeny)
		{
			PolicyInfluenceRampStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
			BringUpGroupStableAccumulatedSeconds = 0.0f;
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Stabilization policy influence ramp enabled after final-group control settle."));
			return;
		}
	}

	const int32 MaxConfiguredAutoUnlockGroup =
		EffectiveSettings.MaxAutoUnlockBringUpGroup >= 0
			? FMath::Min(EffectiveSettings.MaxAutoUnlockBringUpGroup, GetBringUpGroupCount() - 1)
			: (GetBringUpGroupCount() - 1);
	const int32 PhaseAwareMaxAutoUnlockGroup =
		(IsBalanceActiveState(RuntimeState))
			? MaxConfiguredAutoUnlockGroup
			: FMath::Min(MaxConfiguredAutoUnlockGroup, 1);
	if (HighestUnlockedBringUpGroupIndex < PhaseAwareMaxAutoUnlockGroup)
	{
		UnlockBringUpGroup(HighestUnlockedBringUpGroupIndex + 1, TEXT("StableRuntimeWindow"));
		return;
	}

	// Start delayed ramps for ANY unlocked groups if they are still pending
	if (IsBringUpGroupUnlocked(HighestUnlockedBringUpGroupIndex) && !IsBringUpGroupControlRampActive(HighestUnlockedBringUpGroupIndex))
	{
		const double WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
		BringUpGroupControlRampStartTimeSeconds[HighestUnlockedBringUpGroupIndex] = WorldTime;
		BringUpGroupStableAccumulatedSeconds = 0.0f;

		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] PHASE1_BRINGUP_RAMP_ARMED group=%d"), HighestUnlockedBringUpGroupIndex);
		return;
	}

	if (IsBringUpGroupUnlocked(GetBringUpGroupCount() - 1))
	{
		if (BalanceTransitionShellAuthorityMode == EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked)
		{
			if (!bTransitionOwnedShellReferenceReanchored)
			{
				ReanchorShellCouplingReferenceToCurrentRoot(TEXT("final_bring_up_unlock"));
			}
		}
	}
}


bool UPhysAnimComponent::AreAllBringUpGroupsUnlocked() const
{
	return HighestUnlockedBringUpGroupIndex >= (GetBringUpGroupCount() - 1);
}


bool UPhysAnimComponent::IsBringUpGroupUnlocked(int32 GroupIndex) const
{
	if (GroupIndex < 0) return false;

	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;

	if ((bPhase1Prepare || bPhase1LateValidate) && GroupIndex > HighestUnlockedBringUpGroupIndex)
	{
		// During fixed-baseline Phase 1, we pretend higher groups are unlocked 
		// to satisfy transition settlement while and ApplyRuntimeControlTuning keeps them kinematic.
		return true;
	}

	return GroupIndex <= HighestUnlockedBringUpGroupIndex;
}


bool UPhysAnimComponent::IsBringUpGroupControlRampActive(int32 GroupIndex) const
{
	if (GroupIndex < 0) return false;

	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;

	if ((bPhase1Prepare || bPhase1LateValidate) && GroupIndex > HighestUnlockedBringUpGroupIndex)
	{
		return IsBringUpGroupControlRampActive(HighestUnlockedBringUpGroupIndex);
	}

	return BringUpGroupControlRampStartTimeSeconds.IsValidIndex(GroupIndex) &&
		BringUpGroupControlRampStartTimeSeconds[GroupIndex] >= 0.0;
}


bool UPhysAnimComponent::IsBoneInUnlockedBringUpGroup(FName BoneName) const
{
	return IsBringUpGroupUnlocked(ResolveBringUpGroupIndex(BoneName));
}


float UPhysAnimComponent::CalculateBringUpGroupControlAuthorityAlpha(
	int32 GroupIndex,
	const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;

	if ((bPhase1Prepare || bPhase1LateValidate) && GroupIndex > HighestUnlockedBringUpGroupIndex)
	{
		return CalculateBringUpGroupControlAuthorityAlpha(HighestUnlockedBringUpGroupIndex, EffectiveSettings);
	}

	if (!IsBringUpGroupUnlocked(GroupIndex) ||
		!BringUpGroupControlRampStartTimeSeconds.IsValidIndex(GroupIndex) ||
		BringUpGroupControlRampStartTimeSeconds[GroupIndex] < 0.0)
	{
		return 0.0f;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	const float ElapsedSinceGroupControlRampStartSeconds = static_cast<float>(
		FMath::Max(CurrentTimeSeconds - BringUpGroupControlRampStartTimeSeconds[GroupIndex], 0.0));
	const float Alpha = CalculateControlAuthorityAlpha(
		EffectiveSettings.bForceZeroActions,
		true,
		ElapsedSinceGroupControlRampStartSeconds,
		EffectiveSettings.StartupRampSeconds);

	if (Alpha > 0.0f && BringUpGroupAlphaActiveLogged.IsValidIndex(GroupIndex) && BringUpGroupAlphaActiveLogged[GroupIndex] == 0)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] PHASE1_BRINGUP_ALPHA_ACTIVE group=%d alpha=%.4f"), GroupIndex, Alpha);
		BringUpGroupAlphaActiveLogged[GroupIndex] = 1;
	}

	return Alpha;
}


int32 UPhysAnimComponent::ResolveBringUpGroupIndex(FName BoneName)
{
	if (BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03") ||
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("thigh_r"))
	{
		return 0;
	}

	if (BoneName == TEXT("calf_l") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("foot_r") ||
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("ball_r"))
	{
		return 1;
	}

	if (BoneName == TEXT("clavicle_l") ||
		BoneName == TEXT("clavicle_r") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("lowerarm_r"))
	{
		return 2;
	}

	if (BoneName == TEXT("neck_01") ||
		BoneName == TEXT("head"))
	{
		return 3;
	}

	if (BoneName == TEXT("hand_l") ||
		BoneName == TEXT("hand_r"))
	{
		return 4;
	}

	return INDEX_NONE;
}


int32 UPhysAnimComponent::GetBringUpGroupCount()
{
	return PhysAnimComponentInternal::NumBringUpGroups;
}


bool UPhysAnimComponent::ShouldDelayBringUpGroupControlRamp(int32 GroupIndex, int32 NumBringUpGroups)
{
	return NumBringUpGroups >= 5 && GroupIndex >= (NumBringUpGroups - 3);
}


bool UPhysAnimComponent::ShouldStartBringUpGroupControlRamp(
	bool bForceZeroActions,
	bool bBringUpGroupUnlocked,
	bool bDelayBringUpGroupControlRamp,
	bool bPostUnlockSettleComplete,
	bool bStartupBringUpFrozenByBalanceEntry)
{
	if (bStartupBringUpFrozenByBalanceEntry || bForceZeroActions || !bBringUpGroupUnlocked)
	{
		return false;
	}

	return !bDelayBringUpGroupControlRamp || bPostUnlockSettleComplete;
}


bool UPhysAnimComponent::ShouldStartPolicyInfluenceRamp(
	EPhysAnimRuntimeState RuntimeState,
	bool bForceZeroActions,
	bool bAllBringUpGroupsUnlocked,
	bool bFinalBringUpGroupControlRampActive,
	bool bPostFinalGroupControlSettleComplete,
	bool bStartupBringUpFrozenByBalanceEntry)
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;

	if ((bPhase1Prepare || bPhase1LateValidate))
	{
		// During fixed-baseline Phase 1, we allow the policy ramp to start 
		// if the baseline is ready, even if overall bring-up is frozen and "absolute" all groups aren't unlocked.
		return bFinalBringUpGroupControlRampActive && bPostFinalGroupControlSettleComplete;
	}

	if (bStartupBringUpFrozenByBalanceEntry ||
		bForceZeroActions ||
		!bAllBringUpGroupsUnlocked ||
		!bFinalBringUpGroupControlRampActive)
	{
		return false;
	}

	return bPostFinalGroupControlSettleComplete;
}

