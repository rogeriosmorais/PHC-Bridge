#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

FPhysAnimRuntimeTerminationPipelineResult UPhysAnimComponent::BuildProofFailureFailStopRoutingResult(
	const FPhysAnimRuntimeTerminationState& PreviousState,
	const FPhysAnimRunArtifactSnapshot& Artifact,
	const EPhysAnimTerminalReason TerminalReason,
	const int64 TerminalSubstepTimestamp)
{
	FPhysAnimRuntimeProofFailureFailStopRoutingInput RoutingInput;
	RoutingInput.PreviousState = PreviousState;
	RoutingInput.Artifact = Artifact;
	RoutingInput.TerminalReason = TerminalReason;
	RoutingInput.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
	return PhysAnimRuntimeTerminationPipeline::EvaluateProofFailureFailStopRouting(RoutingInput);
}

void UPhysAnimComponent::ResetStabilizationRuntimeState()
{
	BalanceReadyTransition.Cancel(this);
	ConditionedActionBuffer.Reset();
	PreviousConditionedActionBuffer.Reset();
	PreviousActionOutputBuffer.Reset();
	FirstPolicyInferenceSnapshot.Reset();
#if WITH_DEV_AUTOMATION_TESTS
	FirstActiveStandingPolicyInferenceSnapshot.Reset();
#endif
	PreviousControlTargetRotations.Reset();
	PolicyNeutralControlTargetRotations.Reset();
	LastActionDiagnostics = {};
	LastControlTargetDiagnostics = {};
	LastBodyModifierResetRequestCount = 0;
	RuntimeInstabilityState = {};
	LastRuntimeInstabilityDiagnostics = {};
	RecoveryPreEntryTelemetrySkipFrames = 0;
	ClearPublishedBalanceTransitionFailureReason();
	SimulationHandoffAlpha = 0.0f;
	bLastAppliedSimulationHandoffSettled = false;
	LastAppliedControlAuthorityAlpha = -1.0f;
	BridgeStartTimeSeconds = 0.0;
	SimulationHandoffCompletedTimeSeconds = -1.0;
	PolicyInfluenceRampStartTimeSeconds = -1.0;
	SetStartupBringUpFrozenByBalanceEntry(false, TEXT("reset_stabilization_runtime"));
	HighestUnlockedBringUpGroupIndex = INDEX_NONE;
	BringUpGroupStableAccumulatedSeconds = 0.0f;
	BringUpGroupActivationTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	BringUpGroupControlRampStartTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	PendingBodyModifierCachedResetNames.Reset();
	LastRuntimeDiagnosticsLogTimeSeconds = -1.0;
	PolicyUpdateAccumulatorSeconds = -1.0f;
	LastPolicyElapsedSteps = 0;
	PolicyControlTicksExecuted = 0;
	PolicyControlTicksSkipped = 0;
	LastPolicyControlUpdateTimeSeconds = -1.0;
	bDistalLocomotionCompositionModeActive = false;
	DistalLocomotionCompositionTimeAboveEnterSeconds = 0.0f;
	DistalLocomotionCompositionTimeBelowExitSeconds = 0.0f;
	DistalLocomotionCompositionTimeSinceActiveIntentSeconds = -1.0f;
	LastMovementSmokeLocalIntent = FVector::ZeroVector;
	LastMovementSmokeWorldIntent = FVector::ZeroVector;
	LastMovementSmokeOwnerVelocityCmPerSecond = FVector::ZeroVector;
	ResetBridgeLocomotionAuthorityState();
	LastBridgePoseSearchDeltaTimeSeconds = 1.0f / 30.0f;
	LastBridgePoseSearchTrajectoryLogTimeSeconds = -1.0;
	BridgePoseSearchLatchedWalkResult = FPoseSearchBlueprintResult();
	BridgePoseSearchLatchedQueryDirection = FVector::ZeroVector;
	BridgePoseSearchLatchedQuerySpeedCmPerSecond = 0.0f;
	BridgePoseSearchWalkLatchExpireTimeSeconds = -1.0;
	bHasBridgePoseSearchLatchedWalkResult = false;
	bBridgePoseSearchTrajectoryInitialized = false;
	MovementSmokeStartLocation = FVector::ZeroVector;
	ShellCouplingReferenceRootLocalOffsetCm = FVector::ZeroVector;
	LastMovementSmokePhaseName = NAME_None;
	bMovementSmokeScriptStarted = false;
	bMovementSmokeCompletionLogged = false;
	bHasShellCouplingReferenceRootLocalOffset = false;
	PresentationPerturbationOverrideEndTimeSeconds = -1.0;
	bLastAppliedPresentationRootSimulationEnabled = false;
	StabilizationStressTestStartTimeSeconds = -1.0;
	bStabilizationStressTestCompletionLogged = false;
	StabilizationStressTestFirstAngularSpikeTimeSeconds = -1.0;
	StabilizationStressTestFirstLinearSpikeTimeSeconds = -1.0;
	StabilizationStressTestFirstInstabilitySignTimeSeconds = -1.0;
	StabilizationStressTestFirstAngularSpikeMultiplier = 1.0f;
	StabilizationStressTestFirstLinearSpikeMultiplier = 1.0f;
	StabilizationStressTestFirstInstabilityMultiplier = 1.0f;
	StabilizationStressTestFirstAngularSpikeBoneName = NAME_None;
	StabilizationStressTestFirstLinearSpikeBoneName = NAME_None;
	StabilizationStressTestBaselineActorLocation = FVector::ZeroVector;
	StabilizationStressTestBaselineSpineLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineHeadLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineLeftFootLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineRightFootLocalOffset = FVector::ZeroVector;
	OriginalBodyMassScales.Reset();
	bHasSavedBodyMassScales = false;
	PolicyBlendStartControlTargetRotations.Reset();
	bPolicyTargetsAppliedLastFrame = false;
	bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = false;
	LastAppliedStabilizationSettings = {};
	bPendingBalanceModeStartRequest = false;
	bPendingBalanceModeStartAttemptIssued = false;
	PendingBalanceModeStartReason.Reset();
	PendingBalanceModeRequestTimeSeconds = -1.0;
	bKineticGateActiveLastFrame = false;
}


void UPhysAnimComponent::FailStop(const FString& Reason)
{
	TRACE_BOOKMARK(TEXT("PhysAnim FailStop: %s"), *Reason);
	TRACE_COUNTER_ADD(COUNTER_PhysAnim_FailStopCount, 1);

	if (PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() > 0 &&
		StabilizationStressTestStartTimeSeconds >= 0.0)
	{
		const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : StabilizationStressTestStartTimeSeconds;
		const double ElapsedSinceStartSeconds = CurrentTimeSeconds - StabilizationStressTestStartTimeSeconds;
		const double CascadeSeconds =
			(StabilizationStressTestFirstInstabilitySignTimeSeconds >= 0.0)
				? (CurrentTimeSeconds - StabilizationStressTestFirstInstabilitySignTimeSeconds)
				: -1.0;
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Error,
			1.0f,
			TEXT("[PhysAnim] Stabilization stress-test collapse: multiplier=%.2f elapsed=%.2fs firstAngularSpike=%s:%.2f firstLinearSpike=%s:%.2f firstInstability=%.2f onsetToCollapse=%.2fs"),
			ResolveStabilizationStressTestMultiplier(),
			ElapsedSinceStartSeconds,
			*StabilizationStressTestFirstAngularSpikeBoneName.ToString(),
			StabilizationStressTestFirstAngularSpikeMultiplier,
			*StabilizationStressTestFirstLinearSpikeBoneName.ToString(),
			StabilizationStressTestFirstLinearSpikeMultiplier,
			StabilizationStressTestFirstInstabilityMultiplier,
			CascadeSeconds);
	}

	LogBridgeStateSnapshot(TEXT("FailStop"));
	FString ProductRunRoot;
	const bool bProductRun = FParse::Value(
		FCommandLine::Get(),
		TEXT("PhysAnimProductRunRoot="),
		ProductRunRoot);
	if (bProductRun)
	{
		// Product fail-stop is behavioral evidence. Keep it in the raw UE log without
		// turning a valid captured failure into an automation-infrastructure failure.
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] Fail-stop: %s"), *Reason);
	}
	else
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Fail-stop: %s"), *Reason);
	}
	EmitBridgeTraceEvent(TEXT("fail_stop"), TEXT("Bridge entered fail-stop."), Reason);
	const bool bStandingActivationFailure = IsStandingActivationRuntimeState(RuntimeState);
	if (!bStandingActivationFailure)
	{
		DeactivateRuntimePhysicsControl(TEXT("FailStop"));
		ResetBridgePhysicsState();
	}
	TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
	StopBridgeTraceSession(TEXT("FailStop"), TEXT("Bridge trace session stopped after fail-stop."));
	SetComponentTickEnabled(false);
	if (!bStandingActivationFailure)
	{
		ResetStabilizationRuntimeState();
	}
}

#if !UE_BUILD_SHIPPING
void UPhysAnimComponent::TriggerProofFailureFailStopRoutingForTesting()
{
	const FString ProofFailStopReason =
		FString::Printf(TEXT("Proof failed during activation wait: %d"), static_cast<int32>(RuntimeState));
	const FPhysAnimRuntimeTerminationPipelineResult ProofFailureRoutingResult =
		BuildProofFailureFailStopRoutingResult(
			LiveRuntimeEvidenceTerminationState,
			LiveRuntimeEvidenceTerminationState.TerminalArtifact,
			StartupProofDeferredTerminalReason != EPhysAnimTerminalReason::None
				? StartupProofDeferredTerminalReason
				: LiveRuntimeEvidenceTerminationState.TerminalReason,
			LiveRuntimeEvidenceSubstepCounter);
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("PhysAnimProof: TerminalArtifact"));
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("PhysAnimProof: AttemptResult"));
	PHYSANIM_LOG_RATE_LIMITED(
		LogPhysAnimBridge,
		Error,
		1.0f,
		TEXT("[PhysAnim] Startup entry bridge terminal enforced reason=ActivationSupportFailure state=%s"),
		GetRuntimeStateName(RuntimeState));
	PHYSANIM_LOG_RATE_LIMITED(
		LogPhysAnimBridge,
		Error,
		1.0f,
		TEXT("[PhysAnim] Proof failure routed through fail-stop helper reason=%s state=%s"),
		*ProofFailStopReason,
		GetRuntimeStateName(RuntimeState));
	FailStop(ProofFailStopReason);
	PHYSANIM_LOG_RATE_LIMITED(
		LogPhysAnimBridge,
		Error,
		1.0f,
		TEXT("[PhysAnim] Proof failure fail-stop side effects complete reason=%s"),
		*ProofFailStopReason);
	PHYSANIM_LOG_RATE_LIMITED(
		LogPhysAnimBridge,
		Error,
		1.0f,
		TEXT("[PhysAnim] Proof failure terminal reason preserved reason=%d"),
		static_cast<int32>(ProofFailureRoutingResult.StateApplyResult.State.TerminalReason));
}
#endif


void UPhysAnimComponent::SetStartupBringUpFrozenByBalanceEntry(bool bFrozen, const FString& InReason)
{
	if (bStartupBringUpFrozenByBalanceEntry == bFrozen)
	{
		return;
	}

	bStartupBringUpFrozenByBalanceEntry = bFrozen;

	if (bFrozen)
	{
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Log,
			1.0f,
			TEXT("PHASE1_FREEZE_ACQUIRE reason=%s"),
			*InReason);
	}
	else
	{
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Log,
			1.0f,
			TEXT("PHASE1_FREEZE_RELEASE reason=%s phase=%s"),
			*InReason,
			GetRuntimeStateName(RuntimeState));
	}
}


void UPhysAnimComponent::TransitionRuntimeState(EPhysAnimRuntimeState NewState)
{
	if (RuntimeState == NewState)
	{
		return;
	}

	const TCHAR* const PreviousStateName = GetRuntimeStateName(RuntimeState);
	const TCHAR* const NewStateName = GetRuntimeStateName(NewState);

	// Smoke-facing contract filtering: Phase 1/2 is Pending, Phase 3 is LateValidate.
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] State Transition: %s -> %s"), PreviousStateName, NewStateName);
	
	if (NewState != EPhysAnimRuntimeState::LocomotionActiveShell && 
	    NewState != EPhysAnimRuntimeState::LocomotionActiveShellDenied)
	{
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
	}

	RuntimeState = NewState;
	if (NewState == EPhysAnimRuntimeState::WaitingForPoseSearch)
	{
		NoteStartupProofWaitingForPoseSearchObserved();
	}
	EmitBridgeTraceEvent(
		TEXT("runtime_state_transition"),
		FString::Printf(TEXT("Runtime state: %s -> %s"), PreviousStateName, NewStateName),
		FString(),
		PreviousStateName,
		NewStateName);

	UpdateBridgeStatusIndicator(60.0f);
}
