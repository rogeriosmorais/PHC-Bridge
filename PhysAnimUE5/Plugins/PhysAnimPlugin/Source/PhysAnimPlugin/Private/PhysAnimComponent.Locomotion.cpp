#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

namespace
{
	const TCHAR* GetLocomotionRequestStateName(EBridgeLocomotionRequestState State)
	{
		switch (State)
		{
		case EBridgeLocomotionRequestState::BalanceActiveStanding:
			return TEXT("BalanceActiveStanding");
		case EBridgeLocomotionRequestState::LocomotionRequested:
			return TEXT("LocomotionRequested");
		case EBridgeLocomotionRequestState::LocomotionRequestDenied:
			return TEXT("LocomotionRequestDenied");
		default:
			return TEXT("Unknown");
		}
	}
}

bool UPhysAnimComponent::ShouldUseBridgeOwnedMovementDrive(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	const bool bPhase1RootOn = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
	const bool bPhase1Settle = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
	if (!EffectiveSettings.bLockCharacterMovementUntilStartupReady ||
		EffectiveSettings.bRestoreCharacterMovementAfterStartupReady ||
		!EffectiveSettings.bEnableBridgeOwnedMovementWhileCharacterMovementLocked ||
		(RuntimeState != EPhysAnimRuntimeState::BridgeActive && !bPhase1Prepare && !bPhase1LateValidate && !bPhase1RootOn && !bPhase1Settle) ||
		((bPhase1Prepare || bPhase1LateValidate || bPhase1RootOn || bPhase1Settle) && BalanceReadyTransition.ShouldSuppressShell()) ||
		IsTransitionOwnedShellLocked() ||
		bStartupMovementLockActive)
	{
		return false;
	}

	const ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	return CharacterMovement && !CharacterMovement->IsComponentTickEnabled() && CharacterMovement->MovementMode == MOVE_None;
}


void UPhysAnimComponent::ResetBridgeLocomotionAuthorityState()
{
	BridgeIntentState = FBridgeIntentState();
	BridgeTrajectoryState = FBridgeTrajectoryState();
	BridgeShellState = FBridgeShellState();
	BridgeOwnedMovementLastWorldIntent = FVector::ZeroVector;
	BridgeOwnedMovementPlanarVelocityCmPerSecond = FVector::ZeroVector;
	BridgePoseSearchQueryVelocityCmPerSecond = FVector::ZeroVector;
	BridgePoseSearchTrajectory = FTransformTrajectory();
	BridgePoseSearchDesiredControllerYawLastUpdate = 0.0f;
	bBridgePoseSearchTrajectoryInitialized = false;
	BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	BridgeLocomotionStateEnterTimeSeconds = -1.0;
	BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
	ResetBridgeLocomotionRequestState();
}


void UPhysAnimComponent::ResetBridgeLocomotionRequestState()
{
	BridgeLocomotionRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
	BridgeLocomotionRequestReason.Reset();
	BridgeLocomotionRequestStateEnteredTimeSeconds = -1.0;
}


void UPhysAnimComponent::UpdateBridgeLocomotionGateTiming(const FPhysAnimStabilizationSettings& EffectiveSettings, double CurrentTimeSeconds)
{
	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds = -1.0f;
		return;
	}

	if (IsBridgeLocomotionEntryRequested(EffectiveSettings))
	{
		if (DistalLocomotionCompositionTimeSinceActiveIntentSeconds < 0.0f)
		{
			DistalLocomotionCompositionTimeSinceActiveIntentSeconds = static_cast<float>(CurrentTimeSeconds);
		}
	}
	else
	{
		DistalLocomotionCompositionTimeSinceActiveIntentSeconds = -1.0f;
	}
}


bool UPhysAnimComponent::EvaluateBridgeLocomotionGate(FString& OutReason) const
{
	OutReason.Reset();

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double IntentStartTimeSeconds = static_cast<double>(DistalLocomotionCompositionTimeSinceActiveIntentSeconds);
	const double IntentAgeSeconds =
		IntentStartTimeSeconds >= 0.0 ? (CurrentTimeSeconds - IntentStartTimeSeconds) : -1.0;
	const FPhysAnimRuntimeTerminationState& TerminationState = LiveRuntimeEvidenceTerminationState;
	const FPhysAnimRunArtifactSnapshot& Latest = TerminationState.LatestArtifact;
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = ActivatedStandingStabilityMetrics;
	const auto Deny = [&](const TCHAR* Reason) -> bool
	{
		OutReason = Reason;
		return false;
	};

	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		return Deny(TEXT("standing_inactive"));
	}

	if (!bEnableLiveRuntimeEvidenceProof)
	{
		return Deny(TEXT("proof_disabled"));
	}

	if (!bLiveRuntimeEvidenceProofComplete)
	{
		return Deny(TEXT("proof_incomplete"));
	}

	if (Latest.SupportMode == EPhysAnimSupportMode::Airborne ||
		Latest.SupportHullAreaCm2 <= 0.0f ||
		Latest.ActiveSupportSideCount < 1)
	{
		return Deny(TEXT("negative_support"));
	}

	if (!Latest.bPhysicsAssetContractValid || !Latest.bSkeletonAuditPassed)
	{
		return Deny(TEXT("support_audit_invalid"));
	}

	if (!Latest.bCapsuleContractPassed)
	{
		return Deny(TEXT("capsule_invalid"));
	}

	if (!Latest.bPhysicalContinuityValidatorPassed || Latest.bContinuityBookkeepingMismatch)
	{
		return Deny(TEXT("continuity_invalid"));
	}

	if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
	{
		return Deny(TEXT("terminal_reason_present"));
	}

	if (!IsLiveRuntimeEvidenceProofSatisfied())
	{
		return Deny(TEXT("proof_not_truthful"));
	}

	if (!Metrics.bHasSamples || Metrics.SampleCount <= 0)
	{
		return Deny(TEXT("stability_metrics_unavailable"));
	}

	if (Metrics.FailStopCount != 0)
	{
		return Deny(TEXT("stability_metrics_fail_stop"));
	}

	if (!FMath::IsFinite(Metrics.ActivationDurationSec) ||
		!FMath::IsFinite(Metrics.RootWorldPositionDriftCm) ||
		!FMath::IsFinite(Metrics.RootVerticalDriftCm) ||
		!FMath::IsFinite(Metrics.RootAngularDriftDeg) ||
		!FMath::IsFinite(Metrics.MaxBodyLinearSpeedCmPerSecond) ||
		!FMath::IsFinite(Metrics.MaxBodyAngularSpeedDegPerSecond) ||
		!FMath::IsFinite(Metrics.SupportHullAreaMinCm2) ||
		!FMath::IsFinite(Metrics.SupportHullAreaMeanCm2) ||
		!FMath::IsFinite(Metrics.SupportHullAreaMaxCm2) ||
		!FMath::IsFinite(Metrics.ActiveSupportSideCountMin) ||
		!FMath::IsFinite(Metrics.ActiveSupportSideCountMean) ||
		!FMath::IsFinite(Metrics.ActiveSupportSideCountMax))
	{
		return Deny(TEXT("stability_metrics_invalid"));
	}

	if (!IsBridgeLocomotionEntryRequested(EffectiveSettings))
	{
		return Deny(TEXT("intent_absent"));
	}

	if (IntentAgeSeconds < EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds)
	{
		return Deny(TEXT("intent_too_short"));
	}

	OutReason = TEXT("standing_active intent_stable");
	return true;
}


bool UPhysAnimComponent::CanEnterBridgeLocomotionGate(FString& OutReason) const
{
	OutReason.Reset();

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double IntentStartTimeSeconds = static_cast<double>(DistalLocomotionCompositionTimeSinceActiveIntentSeconds);
	const double IntentAgeSeconds =
		IntentStartTimeSeconds >= 0.0 ? (CurrentTimeSeconds - IntentStartTimeSeconds) : -1.0;
	const FPhysAnimRuntimeTerminationState& TerminationState = LiveRuntimeEvidenceTerminationState;
	const FPhysAnimRunArtifactSnapshot& Latest = TerminationState.LatestArtifact;
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = ActivatedStandingStabilityMetrics;
	const auto Deny = [&](const TCHAR* Reason) -> bool
	{
		OutReason = Reason;
		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimLocomotion] LOCOMOTION_GATE_DENIED reason=%s runtimeState=%s proofComplete=%d proofSatisfied=%d terminalReason=%d supportMode=%d supportHull=%.2f activeSides=%.0f intentAge=%.2f intentThreshold=%.2f"),
			*OutReason,
			GetRuntimeStateName(RuntimeState),
			bLiveRuntimeEvidenceProofComplete ? 1 : 0,
			IsLiveRuntimeEvidenceProofSatisfied() ? 1 : 0,
			static_cast<int32>(TerminationState.TerminalReason),
			static_cast<int32>(Latest.SupportMode),
			Latest.SupportHullAreaCm2,
			static_cast<double>(Latest.ActiveSupportSideCount),
			IntentAgeSeconds,
			EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds);
		return false;
	};

	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		return Deny(TEXT("standing_inactive"));
	}

	if (!bEnableLiveRuntimeEvidenceProof)
	{
		return Deny(TEXT("proof_disabled"));
	}

	if (!bLiveRuntimeEvidenceProofComplete)
	{
		return Deny(TEXT("proof_incomplete"));
	}

	if (Latest.SupportMode == EPhysAnimSupportMode::Airborne ||
		Latest.SupportHullAreaCm2 <= 0.0f ||
		Latest.ActiveSupportSideCount < 1)
	{
		return Deny(TEXT("negative_support"));
	}

	if (!Latest.bPhysicsAssetContractValid || !Latest.bSkeletonAuditPassed)
	{
		return Deny(TEXT("support_audit_invalid"));
	}

	if (!Latest.bCapsuleContractPassed)
	{
		return Deny(TEXT("capsule_invalid"));
	}

	if (!Latest.bPhysicalContinuityValidatorPassed || Latest.bContinuityBookkeepingMismatch)
	{
		return Deny(TEXT("continuity_invalid"));
	}

	if (TerminationState.TerminalReason != EPhysAnimTerminalReason::None)
	{
		return Deny(TEXT("terminal_reason_present"));
	}

	if (!IsLiveRuntimeEvidenceProofSatisfied())
	{
		return Deny(TEXT("proof_not_truthful"));
	}

	if (!Metrics.bHasSamples || Metrics.SampleCount <= 0)
	{
		return Deny(TEXT("stability_metrics_unavailable"));
	}

	if (Metrics.FailStopCount != 0)
	{
		return Deny(TEXT("stability_metrics_fail_stop"));
	}

	if (!FMath::IsFinite(Metrics.ActivationDurationSec) ||
		!FMath::IsFinite(Metrics.RootWorldPositionDriftCm) ||
		!FMath::IsFinite(Metrics.RootVerticalDriftCm) ||
		!FMath::IsFinite(Metrics.RootAngularDriftDeg) ||
		!FMath::IsFinite(Metrics.MaxBodyLinearSpeedCmPerSecond) ||
		!FMath::IsFinite(Metrics.MaxBodyAngularSpeedDegPerSecond) ||
		!FMath::IsFinite(Metrics.SupportHullAreaMinCm2) ||
		!FMath::IsFinite(Metrics.SupportHullAreaMeanCm2) ||
		!FMath::IsFinite(Metrics.SupportHullAreaMaxCm2) ||
		!FMath::IsFinite(Metrics.ActiveSupportSideCountMin) ||
		!FMath::IsFinite(Metrics.ActiveSupportSideCountMean) ||
		!FMath::IsFinite(Metrics.ActiveSupportSideCountMax))
	{
		return Deny(TEXT("stability_metrics_invalid"));
	}

	if (!IsBridgeLocomotionEntryRequested(EffectiveSettings))
	{
		return Deny(TEXT("intent_absent"));
	}

	if (IntentAgeSeconds < EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds)
	{
		return Deny(TEXT("intent_too_short"));
	}

	OutReason = TEXT("standing_active intent_stable");
	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimLocomotion] LOCOMOTION_GATE_ALLOWED reason=%s runtimeState=%s proofComplete=%d proofSatisfied=%d terminalReason=%d supportMode=%d supportHull=%.2f activeSides=%.0f intentAge=%.2f intentThreshold=%.2f"),
		*OutReason,
		GetRuntimeStateName(RuntimeState),
		1,
		1,
		static_cast<int32>(TerminationState.TerminalReason),
		static_cast<int32>(Latest.SupportMode),
		Latest.SupportHullAreaCm2,
		static_cast<double>(Latest.ActiveSupportSideCount),
		IntentAgeSeconds,
		EffectiveSettings.DistalLocomotionCompositionPolicyIntentGraceSeconds);
	return true;
}


void UPhysAnimComponent::UpdateBridgeLocomotionRequestState(double CurrentTimeSeconds)
{
	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		ResetBridgeLocomotionRequestState();
		return;
	}

	FString Reason;
	const bool bAllowed = EvaluateBridgeLocomotionGate(Reason);
	const EBridgeLocomotionRequestState NewState =
		bAllowed ? EBridgeLocomotionRequestState::LocomotionRequested : EBridgeLocomotionRequestState::LocomotionRequestDenied;
	if (BridgeLocomotionRequestState == NewState && BridgeLocomotionRequestReason == Reason)
	{
		return;
	}

	BridgeLocomotionRequestState = NewState;
	BridgeLocomotionRequestReason = Reason;
	BridgeLocomotionRequestStateEnteredTimeSeconds = CurrentTimeSeconds;

	const FPhysAnimRuntimeTerminationState& TerminationState = LiveRuntimeEvidenceTerminationState;
	const FPhysAnimRunArtifactSnapshot& Latest = TerminationState.LatestArtifact;
	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = ActivatedStandingStabilityMetrics;
	const TCHAR* const StateLabel = bAllowed ? TEXT("LOCOMOTION_REQUESTED") : TEXT("LOCOMOTION_REQUEST_DENIED");
	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimLocomotion] %s reason=%s runtimeState=%s requestState=%s proofComplete=%d proofSatisfied=%d terminalReason=%d supportMode=%d supportHull=%.2f activeSides=%.0f capsuleValid=%d continuityValid=%d intentMagnitude=%.2f intentAge=%.2f metricsSamples=%d"),
		StateLabel,
		*BridgeLocomotionRequestReason,
		GetRuntimeStateName(RuntimeState),
		GetLocomotionRequestStateName(BridgeLocomotionRequestState),
		bLiveRuntimeEvidenceProofComplete ? 1 : 0,
		IsLiveRuntimeEvidenceProofSatisfied() ? 1 : 0,
		static_cast<int32>(TerminationState.TerminalReason),
		static_cast<int32>(Latest.SupportMode),
		Latest.SupportHullAreaCm2,
		static_cast<double>(Latest.ActiveSupportSideCount),
		Latest.bCapsuleContractPassed ? 1 : 0,
		(Latest.bPhysicalContinuityValidatorPassed && !Latest.bContinuityBookkeepingMismatch) ? 1 : 0,
		BridgeIntentState.IntentMagnitude,
		GetBridgeLocomotionIntentAgeSeconds(),
		Metrics.SampleCount);
}


#if WITH_DEV_AUTOMATION_TESTS
void UPhysAnimComponent::TestOnlySetBridgeLocomotionGateIntent(float IntentMagnitude, double IntentAgeSeconds)
{
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	const float ClampedMagnitude = FMath::Clamp(IntentMagnitude, 0.0f, 1.0f);
	const FVector WorldDirection = ClampedMagnitude > KINDA_SMALL_NUMBER ? FVector::ForwardVector : FVector::ZeroVector;

	BridgeIntentState.WorldMoveDirection = WorldDirection;
	BridgeIntentState.LocalMoveDirection = WorldDirection;
	BridgeIntentState.IntentMagnitude = ClampedMagnitude;
	BridgeIntentState.DesiredSpeedCmPerSecond = ClampedMagnitude * FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementMaxPlanarSpeedCmPerSecond);
	BridgeIntentState.DesiredFacingYawDegrees = 0.0f;
	BridgeIntentState.bHasDesiredFacing = ClampedMagnitude > KINDA_SMALL_NUMBER;
	BridgeTrajectoryState.DesiredVelocityCmPerSecond = WorldDirection * BridgeIntentState.DesiredSpeedCmPerSecond;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = BridgeTrajectoryState.DesiredVelocityCmPerSecond;
	BridgeTrajectoryState.AcceptedVelocityCmPerSecond = BridgeTrajectoryState.DesiredVelocityCmPerSecond;
	BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	BridgeLocomotionStateEnterTimeSeconds = -1.0;
	BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
	BridgeOwnedMovementLastWorldIntent = WorldDirection;
	DistalLocomotionCompositionTimeSinceActiveIntentSeconds =
		IntentAgeSeconds >= 0.0
			? static_cast<float>((GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - IntentAgeSeconds)
			: -1.0f;
}


void UPhysAnimComponent::TestOnlySetBridgeLocomotionRequestEvidence(
	const FPhysAnimRuntimeTerminationState& EvidenceState,
	float IntentMagnitude,
	double IntentAgeSeconds)
{
	LiveRuntimeEvidenceTerminationState = EvidenceState;
	bLiveRuntimeEvidenceProofActive = true;
	bLiveRuntimeEvidenceProofComplete = true;
	LiveRuntimeEvidenceStandingSeconds = FMath::Max(LiveRuntimeEvidenceStandingSeconds, LiveRuntimeEvidenceStandingTargetSeconds);

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	const float ClampedMagnitude = FMath::Clamp(IntentMagnitude, 0.0f, 1.0f);
	const FVector WorldDirection = ClampedMagnitude > KINDA_SMALL_NUMBER ? FVector::ForwardVector : FVector::ZeroVector;

	BridgeIntentState.WorldMoveDirection = WorldDirection;
	BridgeIntentState.LocalMoveDirection = WorldDirection;
	BridgeIntentState.IntentMagnitude = ClampedMagnitude;
	BridgeIntentState.DesiredSpeedCmPerSecond = ClampedMagnitude * FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementMaxPlanarSpeedCmPerSecond);
	BridgeIntentState.DesiredFacingYawDegrees = 0.0f;
	BridgeIntentState.bHasDesiredFacing = ClampedMagnitude > KINDA_SMALL_NUMBER;
	BridgeTrajectoryState.DesiredVelocityCmPerSecond = WorldDirection * BridgeIntentState.DesiredSpeedCmPerSecond;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = BridgeTrajectoryState.DesiredVelocityCmPerSecond;
	BridgeTrajectoryState.AcceptedVelocityCmPerSecond = BridgeTrajectoryState.DesiredVelocityCmPerSecond;
	BridgeOwnedMovementLastWorldIntent = WorldDirection;
	DistalLocomotionCompositionTimeSinceActiveIntentSeconds =
		IntentAgeSeconds >= 0.0
			? static_cast<float>((GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - IntentAgeSeconds)
			: -1.0f;

	ResetBridgeLocomotionRequestState();
	UpdateBridgeLocomotionRequestState(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}
#endif


void UPhysAnimComponent::CaptureBridgeIntent(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	if (!ShouldUseBridgeOwnedMovementDrive(EffectiveSettings))
	{
		ResetBridgeLocomotionAuthorityState();
		return;
	}

	ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	APawn* const OwnerPawn = Cast<APawn>(GetOwner());
	if (!CharacterOwner || !OwnerPawn)
	{
		ResetBridgeLocomotionAuthorityState();
		return;
	}

	FVector WorldIntent = OwnerPawn->GetPendingMovementInputVector();
	if (WorldIntent.IsNearlyZero())
	{
		WorldIntent = OwnerPawn->GetLastMovementInputVector();
	}
	WorldIntent.Z = 0.0f;
	WorldIntent = WorldIntent.GetClampedToMaxSize(1.0f);

	if (WorldIntent.IsNearlyZero() &&
		(BridgeLocomotionAuthorityState == EBridgeLocomotionAuthorityState::StartupLocomotion ||
		 BridgeLocomotionAuthorityState == EBridgeLocomotionAuthorityState::Locomoting) &&
		!BridgeOwnedMovementLastWorldIntent.IsNearlyZero())
	{
		WorldIntent = BridgeOwnedMovementLastWorldIntent;
	}

	if (!WorldIntent.IsNearlyZero())
	{
		BridgeOwnedMovementLastWorldIntent = WorldIntent;
	}
	BridgeIntentState.WorldMoveDirection = WorldIntent.IsNearlyZero() ? FVector::ZeroVector : WorldIntent.GetSafeNormal2D();
	BridgeIntentState.LocalMoveDirection = CharacterOwner->GetActorRotation().UnrotateVector(BridgeIntentState.WorldMoveDirection);
	BridgeIntentState.LocalMoveDirection.Z = 0.0f;
	BridgeIntentState.IntentMagnitude = FMath::Clamp(WorldIntent.Size2D(), 0.0f, 1.0f);
	BridgeIntentState.DesiredSpeedCmPerSecond = BridgeIntentState.IntentMagnitude * FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementMaxPlanarSpeedCmPerSecond);
	BridgeTrajectoryState.DesiredVelocityCmPerSecond = BridgeIntentState.WorldMoveDirection * BridgeIntentState.DesiredSpeedCmPerSecond;
	BridgeTrajectoryState.DesiredVelocityCmPerSecond.Z = 0.0f;

	BridgeIntentState.bHasDesiredFacing = false;
	BridgeIntentState.DesiredFacingYawDegrees = CharacterOwner->GetActorRotation().Yaw;
	if (EffectiveSettings.bBridgeOwnedMovementUseControllerYaw)
	{
		if (const AController* const Controller = CharacterOwner->GetController())
		{
			BridgeIntentState.DesiredFacingYawDegrees = Controller->GetControlRotation().Yaw;
			BridgeIntentState.bHasDesiredFacing = true;
		}
	}
	else if (!BridgeIntentState.WorldMoveDirection.IsNearlyZero())
	{
		BridgeIntentState.DesiredFacingYawDegrees = BridgeIntentState.WorldMoveDirection.Rotation().Yaw;
		BridgeIntentState.bHasDesiredFacing = true;
	}
	else if (BridgeTrajectoryState.AcceptedVelocityCmPerSecond.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		BridgeIntentState.DesiredFacingYawDegrees = BridgeTrajectoryState.AcceptedVelocityCmPerSecond.Rotation().Yaw;
		BridgeIntentState.bHasDesiredFacing = true;
	}

	AActor* const OwnerActor = GetOwner();
	if (OwnerActor)
	{
		if (!BridgeShellState.bInitialized)
		{
			BridgeShellState.LastAcceptedActorLocation = OwnerActor->GetActorLocation();
			BridgeShellState.bInitialized = true;
		}
	}
}


void UPhysAnimComponent::ApplyBridgeOwnedMovementDrive(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	if (!ShouldUseBridgeOwnedMovementDrive(EffectiveSettings))
	{
		ResetBridgeLocomotionAuthorityState();
		return;
	}

	ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	UCapsuleComponent* const CapsuleComponent = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	AActor* const OwnerActor = GetOwner();
	if (!CharacterOwner || !CapsuleComponent || !OwnerActor)
	{
		ResetBridgeLocomotionAuthorityState();
		return;
	}

	const FVector DesiredVelocity = BridgeTrajectoryState.DesiredVelocityCmPerSecond;
	const float BlendRate = DesiredVelocity.IsNearlyZero()
		? FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementDecelerationCmPerSecondSq)
		: FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementAccelerationCmPerSecondSq);

	BridgeShellState.PendingPlanarVelocityCmPerSecond = FMath::VInterpConstantTo(
		BridgeShellState.PendingPlanarVelocityCmPerSecond,
		DesiredVelocity,
		DeltaTime,
		BlendRate);
	BridgeShellState.PendingPlanarVelocityCmPerSecond.Z = 0.0f;
	BridgeOwnedMovementPlanarVelocityCmPerSecond = BridgeShellState.PendingPlanarVelocityCmPerSecond;

	if (BridgeIntentState.bHasDesiredFacing)
	{
		FRotator TargetRotation(0.0f, BridgeIntentState.DesiredFacingYawDegrees, 0.0f);
		const FRotator NewRotation = FMath::RInterpTo(
			CharacterOwner->GetActorRotation(),
			TargetRotation,
			DeltaTime,
			FMath::Max(0.0f, EffectiveSettings.BridgeOwnedMovementRotationInterpSpeed));
		CharacterOwner->SetActorRotation(NewRotation);
	}

	const FVector StartLocation = OwnerActor->GetActorLocation();
	const FVector MoveDelta = BridgeShellState.PendingPlanarVelocityCmPerSecond * DeltaTime;
	BridgeShellState.bLastMoveBlocked = false;
	if (!MoveDelta.IsNearlyZero())
	{
		FHitResult Hit;
		CapsuleComponent->MoveComponent(MoveDelta, CapsuleComponent->GetComponentQuat(), true, &Hit, MOVECOMP_NoFlags, ETeleportType::None);
		if (Hit.IsValidBlockingHit())
		{
			BridgeShellState.bLastMoveBlocked = true;
			const FVector RemainingDelta = MoveDelta * FMath::Clamp(1.0f - Hit.Time, 0.0f, 1.0f);
			const FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, Hit.Normal);
			if (!SlideDelta.IsNearlyZero())
			{
				FHitResult SlideHit;
				CapsuleComponent->MoveComponent(SlideDelta, CapsuleComponent->GetComponentQuat(), true, &SlideHit, MOVECOMP_NoFlags, ETeleportType::None);
			}
		}
	}

	const FVector EndLocation = OwnerActor->GetActorLocation();
	BridgeShellState.AcceptedWorldDeltaCm = EndLocation - StartLocation;
	BridgeShellState.AcceptedPlanarVelocityCmPerSecond = DeltaTime > UE_SMALL_NUMBER
		? (BridgeShellState.AcceptedWorldDeltaCm / DeltaTime)
		: FVector::ZeroVector;
	BridgeShellState.AcceptedPlanarVelocityCmPerSecond.Z = 0.0f;
	BridgeShellState.LastAcceptedActorLocation = EndLocation;
	BridgeTrajectoryState.AcceptedVelocityCmPerSecond = BridgeShellState.AcceptedPlanarVelocityCmPerSecond;

	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	if (BridgeIntentState.IntentMagnitude > KINDA_SMALL_NUMBER)
	{
		if (LastBridgeOwnedMovementLogTimeSeconds < 0.0 || (CurrentTimeSeconds - LastBridgeOwnedMovementLogTimeSeconds) >= 1.0)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Log,
				TEXT("[PhysAnim] Bridge shell authority applying intent: world=(%.2f,%.2f) desiredSpeedCmPerSec=%.1f acceptedSpeedCmPerSec=%.1f"),
				BridgeIntentState.WorldMoveDirection.X,
				BridgeIntentState.WorldMoveDirection.Y,
				BridgeIntentState.DesiredSpeedCmPerSecond,
				BridgeShellState.AcceptedPlanarVelocityCmPerSecond.Size2D());
			LastBridgeOwnedMovementLogTimeSeconds = CurrentTimeSeconds;
		}
	}
	else if (LastBridgeOwnedMovementNoInputLogTimeSeconds < 0.0 || (CurrentTimeSeconds - LastBridgeOwnedMovementNoInputLogTimeSeconds) >= 2.0)
	{
		UE_LOG(LogPhysAnimBridge, Verbose, TEXT("[PhysAnim] Bridge shell authority idle with CharacterMovement suppressed."));
		LastBridgeOwnedMovementNoInputLogTimeSeconds = CurrentTimeSeconds;
	}
}


void UPhysAnimComponent::UpdateBridgeLocomotionAuthorityState(
	const FVector& QueryVelocity,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	double CurrentTimeSeconds)
{
	const bool bPhase1Prepare = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare;
	const bool bPhase1LateValidate = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate;
	const bool bPhase1RootOn = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn;
	const bool bPhase1Settle = RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle;
	if (IsTransitionOwnedShellLocked() ||
		((bPhase1Prepare || bPhase1LateValidate || bPhase1RootOn || bPhase1Settle) && BalanceReadyTransition.ShouldSuppressShell()) ||
		bPendingBalanceModeStartRequest)
	{
		ResetBridgeLocomotionAuthorityState();
		return;
	}

	const float PredictedSpeedCmPerSecond = QueryVelocity.Size2D();
	const float AcceptedSpeedCmPerSecond = BridgeTrajectoryState.AcceptedVelocityCmPerSecond.Size2D();
	const bool bEntryRequested =
		IsBridgeLocomotionEntryRequested(EffectiveSettings) ||
		PredictedSpeedCmPerSecond >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond;
	const bool bAcceptedSustain =
		AcceptedSpeedCmPerSecond >= EffectiveSettings.BridgePoseSearchSustainAcceptedSpeedThresholdCmPerSecond;

	switch (BridgeLocomotionAuthorityState)
	{
	case EBridgeLocomotionAuthorityState::Idle:
		if (bEntryRequested)
		{
			BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::StartupLocomotion;
			BridgeLocomotionStateEnterTimeSeconds = CurrentTimeSeconds;
			BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
		}
		break;

	case EBridgeLocomotionAuthorityState::StartupLocomotion:
		if (bAcceptedSustain)
		{
			BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Locomoting;
			BridgeLocomotionStateEnterTimeSeconds = CurrentTimeSeconds;
			BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
		}
		else
		{
			const double StartupElapsedSeconds =
				BridgeLocomotionStateEnterTimeSeconds >= 0.0 ? (CurrentTimeSeconds - BridgeLocomotionStateEnterTimeSeconds) : 0.0;
			if (!bEntryRequested && StartupElapsedSeconds >= EffectiveSettings.BridgePoseSearchStartupLocomotionSeconds)
			{
				BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
				BridgeLocomotionStateEnterTimeSeconds = CurrentTimeSeconds;
				BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
			}
			else if (bEntryRequested && StartupElapsedSeconds >= EffectiveSettings.BridgePoseSearchStartupLocomotionSeconds)
			{
				BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Locomoting;
				BridgeLocomotionStateEnterTimeSeconds = CurrentTimeSeconds;
				BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
			}
		}
		break;

	case EBridgeLocomotionAuthorityState::Locomoting:
		if (bEntryRequested || bAcceptedSustain)
		{
			BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
		}
		else
		{
			if (BridgeLocomotionExitHoldStartTimeSeconds < 0.0)
			{
				BridgeLocomotionExitHoldStartTimeSeconds = CurrentTimeSeconds;
			}
			else if ((CurrentTimeSeconds - BridgeLocomotionExitHoldStartTimeSeconds) >= EffectiveSettings.BridgePoseSearchExitHoldSeconds)
			{
				BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
				BridgeLocomotionStateEnterTimeSeconds = CurrentTimeSeconds;
				BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
			}
		}
		break;
	}
}


bool UPhysAnimComponent::IsBridgeLocomotionQueryActive() const
{
	return BridgeLocomotionAuthorityState != EBridgeLocomotionAuthorityState::Idle;
}


bool UPhysAnimComponent::IsBridgeLocomotionEntryRequested(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	float DirectIntentMagnitude = BridgeIntentState.IntentMagnitude;

	if (const APawn* const OwnerPawn = Cast<APawn>(GetOwner()))
	{
		FVector PendingInput = OwnerPawn->GetPendingMovementInputVector();
		PendingInput.Z = 0.0f;
		DirectIntentMagnitude = FMath::Max(DirectIntentMagnitude, FMath::Clamp(PendingInput.Size2D(), 0.0f, 1.0f));

		FVector LastInput = OwnerPawn->GetLastMovementInputVector();
		LastInput.Z = 0.0f;
		DirectIntentMagnitude = FMath::Max(DirectIntentMagnitude, FMath::Clamp(LastInput.Size2D(), 0.0f, 1.0f));
	}

	if (const ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (const UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			FVector CurrentAcceleration = CharacterMovement->GetCurrentAcceleration();
			CurrentAcceleration.Z = 0.0f;
			DirectIntentMagnitude = FMath::Max(DirectIntentMagnitude, CurrentAcceleration.IsNearlyZero() ? 0.0f : 1.0f);
		}
	}

	return
		DirectIntentMagnitude >= EffectiveSettings.BridgePoseSearchWalkIntentThreshold ||
		BridgeTrajectoryState.DesiredVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond ||
		BridgeTrajectoryState.QueryVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond;
}


bool UPhysAnimComponent::IsBridgePoseSearchIdleResult(const FPoseSearchBlueprintResult& SearchResult)
{
	if (SearchResult.SelectedAnim == nullptr)
	{
		return false;
	}

	const FString AssetName = SearchResult.SelectedAnim->GetName().ToLower();
	return AssetName.Contains(TEXT("mm_idle")) ||
		AssetName == TEXT("idle") ||
		AssetName.EndsWith(TEXT("_idle")) ||
		AssetName.Contains(TEXT("idle"));
}


bool UPhysAnimComponent::IsIdlePoseActive() const
{
	return LastValidPoseSearchResult.SelectedAnim == nullptr || IsBridgePoseSearchIdleResult(LastValidPoseSearchResult);
}


void UPhysAnimComponent::ResolveBridgePoseSearchQueryVelocity(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	FVector& OutQueryVelocity,
	float* OutIntentMagnitude) const
{
	const float IntentMagnitude = BridgeIntentState.IntentMagnitude;
	if (OutIntentMagnitude)
	{
		*OutIntentMagnitude = IntentMagnitude;
	}

	FVector QueryDirection = BridgeIntentState.WorldMoveDirection;
	if (!QueryDirection.IsNearlyZero())
	{
		QueryDirection = QueryDirection.GetSafeNormal2D();
	}
	else if (BridgeTrajectoryState.DesiredVelocityCmPerSecond.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		QueryDirection = BridgeTrajectoryState.DesiredVelocityCmPerSecond.GetSafeNormal2D();
	}
	else if (BridgeTrajectoryState.AcceptedVelocityCmPerSecond.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		QueryDirection = BridgeTrajectoryState.AcceptedVelocityCmPerSecond.GetSafeNormal2D();
	}

	const bool bLocomotionEntryRequested =
		IntentMagnitude >= EffectiveSettings.BridgePoseSearchWalkIntentThreshold ||
		BridgeTrajectoryState.DesiredVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond ||
		BridgeTrajectoryState.QueryVelocityCmPerSecond.Size2D() >= EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond;

	float QuerySpeedCmPerSecond = 0.0f;
	if (bLocomotionEntryRequested && !QueryDirection.IsNearlyZero())
	{
		if (EffectiveSettings.bBridgePoseSearchUseStabilizedWalkQuerySpeed)
		{
			const float MaxQuerySpeedCmPerSecond =
				FMath::Max(EffectiveSettings.BridgeOwnedMovementMaxPlanarSpeedCmPerSecond, EffectiveSettings.BridgePoseSearchStabilizedWalkSpeedCmPerSecond);
			QuerySpeedCmPerSecond = FMath::Clamp(EffectiveSettings.BridgePoseSearchStabilizedWalkSpeedCmPerSecond, 0.0f, MaxQuerySpeedCmPerSecond);
		}
		else
		{
			QuerySpeedCmPerSecond = FMath::Max(
				BridgeTrajectoryState.DesiredVelocityCmPerSecond.Size2D(),
				BridgeTrajectoryState.AcceptedVelocityCmPerSecond.Size2D());
		}
	}
	else
	{
		QuerySpeedCmPerSecond = BridgeTrajectoryState.AcceptedVelocityCmPerSecond.Size2D();
	}

	OutQueryVelocity = QueryDirection.IsNearlyZero() ? FVector::ZeroVector : (QueryDirection * QuerySpeedCmPerSecond);
	OutQueryVelocity.Z = 0.0f;
}


void UPhysAnimComponent::AdvanceBridgePoseSearchResultTime(FPoseSearchBlueprintResult& InOutSearchResult, float DeltaTimeSeconds) const
{
	if (InOutSearchResult.SelectedAnim == nullptr || DeltaTimeSeconds <= 0.0f)
	{
		return;
	}

	const UAnimationAsset* const AnimationAsset = Cast<UAnimationAsset>(InOutSearchResult.SelectedAnim);
	if (AnimationAsset == nullptr)
	{
		return;
	}

	const float PlayLength = AnimationAsset->GetPlayLength();
	if (PlayLength <= UE_SMALL_NUMBER)
	{
		return;
	}

	const float PlayRate = FMath::Max(InOutSearchResult.WantedPlayRate, 0.01f);
	const float AdvancedTime = InOutSearchResult.SelectedTime + (DeltaTimeSeconds * PlayRate);
	InOutSearchResult.SelectedTime = InOutSearchResult.bLoop
		? FMath::Fmod(AdvancedTime, PlayLength)
		: FMath::Clamp(AdvancedTime, 0.0f, PlayLength);
	InOutSearchResult.bIsContinuingPoseSearch = true;
}


bool UPhysAnimComponent::ShouldContinueBridgePoseSearchWalkSelection(
	const FVector& QueryVelocity,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	double CurrentTimeSeconds) const
{
	if (!bHasBridgePoseSearchLatchedWalkResult ||
		CurrentTimeSeconds > BridgePoseSearchWalkLatchExpireTimeSeconds)
	{
		return false;
	}

	const float QuerySpeedCmPerSecond = QueryVelocity.Size2D();
	if (QuerySpeedCmPerSecond < EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond)
	{
		return false;
	}

	if (FMath::Abs(QuerySpeedCmPerSecond - BridgePoseSearchLatchedQuerySpeedCmPerSecond) >
		EffectiveSettings.BridgePoseSearchContinuationMaxSpeedDeltaCmPerSecond)
	{
		return false;
	}

	const FVector QueryDirection = QuerySpeedCmPerSecond > UE_SMALL_NUMBER
		? QueryVelocity.GetSafeNormal2D()
		: FVector::ZeroVector;
	if (!QueryDirection.IsNearlyZero() && !BridgePoseSearchLatchedQueryDirection.IsNearlyZero())
	{
		const float ClampedDot = FMath::Clamp(FVector::DotProduct(QueryDirection, BridgePoseSearchLatchedQueryDirection), -1.0f, 1.0f);
		const float DirectionDeltaDegrees = FMath::RadiansToDegrees(FMath::Acos(ClampedDot));
		if (DirectionDeltaDegrees > EffectiveSettings.BridgePoseSearchContinuationMaxDirectionDeltaDegrees)
		{
			return false;
		}
	}

	return true;
}


void UPhysAnimComponent::ApplyBridgePoseSearchSelectionPolicy(
	FPoseSearchBlueprintResult& InOutSearchResult,
	float QueryDeltaTimeSeconds,
	const FVector& QueryVelocity,
	const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	const float QuerySpeedCmPerSecond = QueryVelocity.Size2D();
	const bool bCandidateIdle = IsBridgePoseSearchIdleResult(InOutSearchResult);
	const bool bCandidateLocomotion = InOutSearchResult.SelectedAnim != nullptr && !bCandidateIdle;
	const bool bShouldContinueWalk =
		ShouldContinueBridgePoseSearchWalkSelection(QueryVelocity, EffectiveSettings, CurrentTimeSeconds);
	const float AcceptedSpeedCmPerSecond = BridgeTrajectoryState.AcceptedVelocityCmPerSecond.Size2D();

	UpdateBridgeLocomotionAuthorityState(QueryVelocity, EffectiveSettings, CurrentTimeSeconds);
	const bool bLocomotionQueryActive = IsBridgeLocomotionQueryActive();

	if (bShouldContinueWalk)
	{
		const bool bSelectedDifferentAnimation =
			InOutSearchResult.SelectedAnim != nullptr &&
			BridgePoseSearchLatchedWalkResult.SelectedAnim != nullptr &&
			InOutSearchResult.SelectedAnim != BridgePoseSearchLatchedWalkResult.SelectedAnim;

		if (bCandidateIdle || bSelectedDifferentAnimation)
		{
			InOutSearchResult = BridgePoseSearchLatchedWalkResult;
			AdvanceBridgePoseSearchResultTime(InOutSearchResult, QueryDeltaTimeSeconds);
		}
	}
	else if (bCandidateIdle && bLocomotionQueryActive && bHasBridgePoseSearchLatchedWalkResult)
	{
		InOutSearchResult = BridgePoseSearchLatchedWalkResult;
		AdvanceBridgePoseSearchResultTime(InOutSearchResult, QueryDeltaTimeSeconds);
	}
	else if (bCandidateIdle && bLocomotionQueryActive &&
		LastValidPoseSearchResult.SelectedAnim != nullptr &&
		!IsBridgePoseSearchIdleResult(LastValidPoseSearchResult))
	{
		InOutSearchResult = LastValidPoseSearchResult;
		AdvanceBridgePoseSearchResultTime(InOutSearchResult, QueryDeltaTimeSeconds);
	}

	if (InOutSearchResult.SelectedAnim != nullptr && !IsBridgePoseSearchIdleResult(InOutSearchResult) &&
		(bLocomotionQueryActive || AcceptedSpeedCmPerSecond >= EffectiveSettings.BridgePoseSearchSustainAcceptedSpeedThresholdCmPerSecond))
	{
		BridgePoseSearchLatchedWalkResult = InOutSearchResult;
		BridgePoseSearchLatchedQueryDirection = QuerySpeedCmPerSecond > UE_SMALL_NUMBER
			? QueryVelocity.GetSafeNormal2D()
			: FVector::ZeroVector;
		BridgePoseSearchLatchedQuerySpeedCmPerSecond = QuerySpeedCmPerSecond;
		BridgePoseSearchWalkLatchExpireTimeSeconds =
			CurrentTimeSeconds + FMath::Max(0.0f, EffectiveSettings.BridgePoseSearchWalkContinuationSeconds);
		bHasBridgePoseSearchLatchedWalkResult = true;
	}
	else if (!bLocomotionQueryActive && AcceptedSpeedCmPerSecond < EffectiveSettings.BridgePoseSearchSustainAcceptedSpeedThresholdCmPerSecond)
	{
		BridgePoseSearchLatchedWalkResult = FPoseSearchBlueprintResult();
		BridgePoseSearchLatchedQueryDirection = FVector::ZeroVector;
		BridgePoseSearchLatchedQuerySpeedCmPerSecond = 0.0f;
		BridgePoseSearchWalkLatchExpireTimeSeconds = -1.0;
		bHasBridgePoseSearchLatchedWalkResult = false;
	}
}


bool UPhysAnimComponent::QueryPoseSearchWithBridgeTrajectory(FPoseSearchBlueprintResult& OutSearchResult, FString& OutError)
{
	OutSearchResult = FPoseSearchBlueprintResult();
	OutError.Reset();

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

	const FAnimNode_PoseSearchHistoryCollector_Base* const PoseHistoryNode =
		UPoseSearchLibrary::FindPoseHistoryNode(PhysAnimComponentInternal::PoseHistoryName, LocalAnimInstance);
	if (!PoseHistoryNode)
	{
		OutError = TEXT("PoseHistory_Stage1 was not found on the live AnimInstance.");
		return false;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	UpdateBridgePoseSearchTrajectory(FMath::Max(0.0001f, BridgeTrajectoryState.LastDeltaTimeSeconds), EffectiveSettings);

	FVector QueryVelocity = FVector::ZeroVector;
	ResolveBridgePoseSearchQueryVelocity(EffectiveSettings, QueryVelocity);
	BridgePoseSearchQueryVelocityCmPerSecond = QueryVelocity;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = QueryVelocity;

	FAnimNode_PoseSearchHistoryCollector_Base* const MutablePoseHistoryNode = const_cast<FAnimNode_PoseSearchHistoryCollector_Base*>(PoseHistoryNode);
	MutablePoseHistoryNode->GetPoseHistory().SetTrajectory(BridgeTrajectoryState.QueryTrajectory, 1.0f);

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

	TArray<const UObject*, TInlineAllocator<1>> AssetsToSearch;
	AssetsToSearch.Add(LoadedPoseSearchDatabase);

	FChooserEvaluationContext ChooserContext;
	ChooserContext.AddObjectParam(LocalAnimInstance);

	UE::PoseSearch::FSearchContext SearchContext(0.0f, FFloatInterval(0.0f, 0.0f), FPoseSearchEvent());
	SearchContext.AddRole(UE::PoseSearch::DefaultRole, &ChooserContext, &MutablePoseHistoryNode->GetPoseHistory());

	UE::PoseSearch::FSearchResults_Single SearchResults;
	UPoseSearchLibrary::MotionMatch(SearchContext, AssetsToSearch, ContinuingProperties, SearchResults);

	const UE::PoseSearch::FSearchResult SearchResult = SearchResults.GetBestResult();
	if (!SearchResult.IsValid())
	{
		OutError = TEXT("SearchContext MotionMatch returned no valid result.");
		return false;
	}

	OutSearchResult.InitFrom(SearchResult, 1.0f);
	ApplyBridgePoseSearchSelectionPolicy(
		OutSearchResult,
		FMath::Max(0.0001f, LastBridgePoseSearchDeltaTimeSeconds),
		QueryVelocity,
		EffectiveSettings);

	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
	if (LastBridgePoseSearchTrajectoryLogTimeSeconds < 0.0 || (CurrentTimeSeconds - LastBridgePoseSearchTrajectoryLogTimeSeconds) >= 1.0)
	{
		const FRotator ActorRotation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
		const FVector WorldIntent = BridgeIntentState.WorldMoveDirection * BridgeIntentState.IntentMagnitude;
		const FVector LocalIntent = BridgeIntentState.LocalMoveDirection * BridgeIntentState.IntentMagnitude;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Bridge predictor search selected '%s' from locomotion intent: local=(%.2f,%.2f) world=(%.2f,%.2f) actualSpeedCmPerSec=%.1f querySpeedCmPerSec=%.1f"),
			OutSearchResult.SelectedAnim ? *OutSearchResult.SelectedAnim->GetName() : TEXT("None"),
			LocalIntent.X,
			LocalIntent.Y,
			WorldIntent.X,
			WorldIntent.Y,
			BridgeTrajectoryState.AcceptedVelocityCmPerSecond.Size2D(),
			BridgeTrajectoryState.QueryVelocityCmPerSecond.Size2D());
		LastBridgePoseSearchTrajectoryLogTimeSeconds = CurrentTimeSeconds;
	}

	return true;
}


FVector UPhysAnimComponent::ResolveMovementSmokeLocalIntent(float ElapsedSeconds)
{
	if (ElapsedSeconds < 0.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 3.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 8.0f)
	{
		return FVector(1.0f, 0.0f, 0.0f);
	}
	if (ElapsedSeconds < 11.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 16.0f)
	{
		return FVector(0.0f, -1.0f, 0.0f);
	}
	if (ElapsedSeconds < 19.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 24.0f)
	{
		return FVector(0.0f, 1.0f, 0.0f);
	}
	if (ElapsedSeconds < 27.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < PhysAnimComponentInternal::MovementSmokeTimelineDurationSeconds)
	{
		return FVector(-1.0f, 0.0f, 0.0f);
	}

	return FVector::ZeroVector;
}


FName UPhysAnimComponent::ResolveMovementSmokePhaseName(float ElapsedSeconds)
{
	if (ElapsedSeconds < 0.0f)
	{
		return TEXT("Inactive");
	}
	if (ElapsedSeconds < 3.0f)
	{
		return TEXT("Idle_00");
	}
	if (ElapsedSeconds < 8.0f)
	{
		return TEXT("Forward");
	}
	if (ElapsedSeconds < 11.0f)
	{
		return TEXT("Idle_01");
	}
	if (ElapsedSeconds < 16.0f)
	{
		return TEXT("StrafeLeft");
	}
	if (ElapsedSeconds < 19.0f)
	{
		return TEXT("Idle_02");
	}
	if (ElapsedSeconds < 24.0f)
	{
		return TEXT("StrafeRight");
	}
	if (ElapsedSeconds < 27.0f)
	{
		return TEXT("Idle_03");
	}
	if (ElapsedSeconds < PhysAnimComponentInternal::MovementSmokeTimelineDurationSeconds)
	{
		return TEXT("Backward");
	}

	return TEXT("Complete");
}


float UPhysAnimComponent::GetMovementSmokeDurationSeconds()
{
	return PhysAnimComponentInternal::MovementSmokeTimelineDurationSeconds;
}


float UPhysAnimComponent::GetMovementSmokeTotalDurationSeconds(int32 NumLoops)
{
	return GetMovementSmokeDurationSeconds() * FMath::Max(NumLoops, 1);
}

