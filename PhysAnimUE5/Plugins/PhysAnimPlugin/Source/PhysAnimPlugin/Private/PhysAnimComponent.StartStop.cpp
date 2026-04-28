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
		SetComponentTickEnabled(false);
		TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
		StopBridgeTraceSession(TEXT("StartupBlocked"), TEXT("Bridge trace session stopped after startup block."));
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
		SetComponentTickEnabled(false);
		TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
		StopBridgeTraceSession(TEXT("StartupBlocked"), TEXT("Bridge trace session stopped after startup block."));
		return false;
	}

	const FPhysAnimStabilizationSettings EffectiveStartupSettings = ResolveEffectiveStabilizationSettings();
	if (EffectiveStartupSettings.bLockCharacterMovementUntilStartupReady)
	{
		ApplyStartupMovementLock();
	}
	ResetStartupQuietWindowState();
	ResetPolicySettleWindowState();

	bStartupReported = true;
	bEnableLiveRuntimeEvidenceProof = true;
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

