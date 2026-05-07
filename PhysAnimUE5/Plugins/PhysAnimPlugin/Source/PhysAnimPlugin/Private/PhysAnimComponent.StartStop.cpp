#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::StartBridge()
{
	if (bPendingStartupRestPoseCapture)
	{
		return true;
	}

	if (RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
		RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation)
	{
		return true;
	}

	StartBridgeTraceSession();

	FString Error;
	if (!ResolveRuntimeContext(Error))
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Startup blocked: %s"), *Error);
		EmitBridgeTraceEvent(TEXT("startup_blocked"), TEXT("Runtime context resolution failed during startup."), Error);
		FailStop(FString::Printf(TEXT("Startup blocked: %s"), *Error));
		return false;
	}

	DeactivateRuntimePhysicsControl(TEXT("StartupReset"));

	if (!ValidateRequiredBodies(Error) ||
		!ValidatePhysicsControlAuthoring(Error) ||
		!ValidatePoseSearchIntegration(Error) ||
		!InitializeModel(Error))
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Startup blocked: %s"), *Error);
		EmitBridgeTraceEvent(TEXT("startup_blocked"), TEXT("Startup validation failed before bridge activation."), Error);
		FailStop(FString::Printf(TEXT("Startup blocked: %s"), *Error));
		return false;
	}

	const FPhysAnimStabilizationSettings EffectiveStartupSettings = ResolveEffectiveStabilizationSettings();
	if (EffectiveStartupSettings.bLockCharacterMovementUntilStartupReady)
	{
		ApplyStartupMovementLock();
	}
	ResetStartupQuietWindowState();
	ResetPolicySettleWindowState();

	const bool bRequestedLiveRuntimeEvidenceProof = bEnableLiveRuntimeEvidenceProof;
	const bool bRequestedForceSupportFailure = bForceSupportFailure;
	const bool bRequestedProofShouldComplete = bLiveRuntimeEvidenceProofShouldComplete;
	ResetLiveRuntimeEvidenceProof();
	bLiveRuntimeEvidenceStartupEvidenceFresh = false;
	bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved = false;
	bLiveRuntimeEvidenceStartupStandingEntryAccepted = false;
	StartupProofStandingEntryAcceptedSubstep = -1;
	bLiveRuntimeEvidenceStartupVerificationHandoffArmed = false;
	bLiveRuntimeEvidenceStartupProxySupportHandoffArmed = false;
	bLiveRuntimeEvidenceProofShouldComplete = bRequestedProofShouldComplete;
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Proxy handoff reset state=%s"), GetRuntimeStateName(RuntimeState));
	bEnableLiveRuntimeEvidenceProof = bRequestedLiveRuntimeEvidenceProof;
	bForceSupportFailure = bRequestedForceSupportFailure;

	bStartupReported = true;
	SetComponentTickEnabled(true);
	TransitionRuntimeState(EPhysAnimRuntimeState::RuntimeReady);
	TransitionRuntimeState(EPhysAnimRuntimeState::WaitingForPoseSearch);
	InitialPoseSearchWaitStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ConsecutiveInvalidPoseSearchFrames = 0;
	LastValidPoseSearchResult = FPoseSearchBlueprintResult();
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Startup pending initial PoseSearch result before taking physics ownership."));
	UpdateBridgeStatusIndicator(1.0f);
	return true;
}


void UPhysAnimComponent::StopBridge()
{
	ReleaseStartupMovementLock(true);
	DeactivateRuntimePhysicsControl(TEXT("StopBridge"));
	ResetBridgePhysicsState();
	TransitionRuntimeState(EPhysAnimRuntimeState::Uninitialized);
	StopBridgeTraceSession(TEXT("StopBridge"), TEXT("Bridge stopped."));
	UpdateBridgeStatusIndicator(5.0f);
	SetComponentTickEnabled(false);
	bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved = false;
	bLiveRuntimeEvidenceStartupEvidenceFresh = false;
	bLiveRuntimeEvidenceStartupStandingEntryAccepted = false;
	StartupProofStandingEntryAcceptedSubstep = -1;
	bLiveRuntimeEvidenceStartupVerificationHandoffArmed = false;
	StartupProofVerificationHandoffArmedSubstep = -1;
	bLiveRuntimeEvidenceStartupProxySupportHandoffArmed = false;
	StartupProofDeferredTerminalReason = EPhysAnimTerminalReason::None;
	bLiveRuntimeEvidenceProofShouldComplete = true;
	UE_LOG(LogPhysAnimBridge, Display, TEXT("[PhysAnimV0] THIGH_WORK_DIAGNOSTIC positiveWork=%.6f negativeWork=%.6f"), 
		ActivatedStandingStabilityMetrics.ThighPositiveWorkAccumulated, 
		ActivatedStandingStabilityMetrics.ThighNegativeWorkAccumulated);

	ConsecutiveInvalidPoseSearchFrames = 0;
	LastValidPoseSearchResult = FPoseSearchBlueprintResult();
	ResetStabilizationRuntimeState();
}


bool UPhysAnimComponent::IsReadyForScriptedPresentation() const
{
	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive)
	{
		return false;
	}

	if (!AreAllBringUpGroupsUnlocked())
	{
		return false;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	return CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings) >= (1.0f - KINDA_SMALL_NUMBER);
}


void UPhysAnimComponent::SetPresentationPerturbationOverrideSeconds(float DurationSeconds)
{
	if (DurationSeconds <= 0.0f)
	{
		ClearPresentationPerturbationOverride();
		return;
	}

	const UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	PresentationPerturbationOverrideEndTimeSeconds =
		FMath::Max(PresentationPerturbationOverrideEndTimeSeconds, World->GetTimeSeconds() + DurationSeconds);
}


void UPhysAnimComponent::ClearPresentationPerturbationOverride()
{
	PresentationPerturbationOverrideEndTimeSeconds = -1.0;
}

