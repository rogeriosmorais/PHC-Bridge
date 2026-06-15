#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"
#include "PhysAnimPhase1AutoCalibSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "PhysicsControlComponent.h"

void UPhysAnimComponent::ProcessPendingDistalOwnershipChecks()
{
	if (!BalanceReadyTransition.IsDistalKinematicAccepted())
	{
		return;
	}

	USkeletalMeshComponent* const Mesh = GetMeshComponent();
	UPhysicsControlComponent* const PC = PhysicsControlComponent.Get();
	if (!Mesh || !PC)
	{
		return;
	}

	for (const FName BoneName : {TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r")})
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, ModifierName))
		{
			const EPhysicsMovementType ModifierType = Record->BodyModifier.ModifierData.MovementType;
			FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName);
			const bool bRawSimulating = BodyInstance ? BodyInstance->IsInstanceSimulatingPhysics() : false;
			
			if (ModifierType == EPhysicsMovementType::Simulated && !bRawSimulating)
			{
				static TMap<FName, bool> LoggedStaleOnce;
				if (!LoggedStaleOnce.Contains(BoneName))
				{
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("DISTAL_MODIFIER_RECORD_STALE bone=%s intended=Kinematic rawBody=Kinematic modifier=Simulated runtimeState=%s phase=%d"),
						*BoneName.ToString(),
						GetRuntimeStateName(RuntimeState),
						(int32)BalanceReadyTransition.GetPhase());
					LoggedStaleOnce.Add(BoneName, true);
				}
			}
		}
	}
}

void UPhysAnimComponent::TickRuntimeStateMachine(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	BalanceReadyTransition.Tick(DeltaTime, this, EffectiveSettings);

	// Authoritative state sync
	{
		EBalanceReadyTransitionPhase TransitionPhase = BalanceReadyTransition.GetPhase();
		if (TransitionPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
		{
			if (!IsBalanceActiveState(RuntimeState))
			{
				CompleteBalanceModeEntry();
			}
			TransitionPhase = BalanceReadyTransition.GetPhase();
		}

		const EPhysAnimRuntimeState MappedRuntimeState =
			UPhysAnimComponent::MapBalanceTransitionPhaseToRuntimeState(TransitionPhase);

		if (TransitionPhase == EBalanceReadyTransitionPhase::BRT_Inactive)
		{
			if (!IsBalanceActiveState(RuntimeState) &&
				RuntimeState != EPhysAnimRuntimeState::BalanceSafeDeny &&
				RuntimeState != EPhysAnimRuntimeState::LocomotionActiveShell &&
				RuntimeState != EPhysAnimRuntimeState::LocomotionActiveShellDenied)
			{
				TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
			}
		}
		else
		{
			TransitionRuntimeState(MappedRuntimeState);
		}
	}

	// Auto-trigger: when in BridgeActive with no pending request and no ongoing transition,
	// queue an "auto_trigger" balance start request so the state machine progresses toward
	// BalanceActive_Standing. This call site was previously missing, leaving the component
	// permanently stuck in BridgeActive. (S2-FIX-BALANCE-STARTUP-TICK-RACE-01)
	{
		UPhysAnimPhase1AutoCalibSubsystem* const AutoCalibSubsystem =
			GetWorld() ? GetWorld()->GetSubsystem<UPhysAnimPhase1AutoCalibSubsystem>() : nullptr;
		const bool bAutoCalibSubsystemActive = AutoCalibSubsystem && AutoCalibSubsystem->IsPhase1AutoCalibActive();

		if (ShouldAttemptAutoTriggeredBalanceStart(
				RuntimeState,
				bPendingBalanceModeStartRequest,
				BalanceReadyTransition.HasAnyInternalPhase(),
				bPhase1AutoCalibOwnsStartRequests,
				bAutoCalibSubsystemActive))
		{
			if (!ShouldDeferAutoTriggeredBalanceStartForRecoveryTelemetry(RecoveryPreEntryTelemetrySkipFrames))
			{
				QueueBalanceModeStartRequest(TEXT("auto_trigger"));
			}
			else
			{
				--RecoveryPreEntryTelemetrySkipFrames;
			}
		}

		TryStartPendingBalanceModeRequest(EffectiveSettings);
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		const EPhysAnimRuntimeState EvaluatedState = EvaluateBalanceActiveStanding();
		if (EvaluatedState != RuntimeState)
		{
			TransitionRuntimeState(EvaluatedState);
		}
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle)
	{
		if (ShouldExitStandingToSafeDeny(LiveRuntimeEvidenceTerminationState))
		{
			const FPhysAnimRunArtifactSnapshot& Latest = LiveRuntimeEvidenceTerminationState.LatestArtifact;
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] SETTLE_DENIED reason=PHASE3_ACTIVE_SUPPORT_FAILURE hull_area=%.1f gap=%.1f proxy_inside=%d proxy_drift=%.1f"),
				Latest.SupportHullAreaCm2,
				Latest.SupportGapTimerMs,
				Latest.ProxyInsideHull.IsSet() ? (Latest.ProxyInsideHull.GetValue() ? 1 : 0) : -1,
				Latest.ProxyOutsideHullDurationMs.IsSet() ? Latest.ProxyOutsideHullDurationMs.GetValue() : 0.0);
			TransitionRuntimeState(EPhysAnimRuntimeState::BalanceSafeDeny);
		}
	}
}

void UPhysAnimComponent::TickPolicyAndUpdateMetrics(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError)
{
	const float PolicyControlIntervalSeconds = ResolvePolicyControlIntervalSeconds(EffectiveSettings.PolicyControlRateHz);
	int32 ElapsedPolicySteps = 0;
	const bool bRunPolicyUpdateThisTick = AdvancePolicyControlAccumulator(
		DeltaTime,
		PolicyControlIntervalSeconds,
		PolicyUpdateAccumulatorSeconds,
		ElapsedPolicySteps);
	LastPolicyElapsedSteps = ElapsedPolicySteps;

	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] TICK_POLICY state=%s runPolicy=%d steps=%d acc=%.4f interval=%.4f dt=%.4f"),
		GetRuntimeStateName(RuntimeState), bRunPolicyUpdateThisTick ? 1 : 0, ElapsedPolicySteps, PolicyUpdateAccumulatorSeconds, PolicyControlIntervalSeconds, DeltaTime);

	if (bRunPolicyUpdateThisTick)
	{
		++PolicyControlTicksExecuted;
		PolicyControlTicksSkipped += FMath::Max(ElapsedPolicySteps - 1, 0);

		FPoseSearchBlueprintResult SearchResult;
		const double PoseSearchStartSeconds = FPlatformTime::Seconds();
		const bool bPoseSearchValid = QueryPoseSearch(SearchResult, OutError);
		RecordLiveRuntimeEvidencePoseSearchQueryResult(bPoseSearchValid, SearchResult.SelectedAnim ? SearchResult.SelectedAnim->GetName() : TEXT(""));

		if (bPoseSearchValid)
		{
			LastValidPoseSearchResult = SearchResult;
			ConsecutiveInvalidPoseSearchFrames = 0;
		}
		else
		{
			++ConsecutiveInvalidPoseSearchFrames;
			if (ConsecutiveInvalidPoseSearchFrames > 1 || LastValidPoseSearchResult.SelectedAnim == nullptr)
			{
				OutError = FString::Printf(TEXT("PoseSearch query was invalid for two consecutive policy ticks. %s"), *OutError);
				return;
			}
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] Reusing last valid PoseSearch result for one grace policy tick. Reason: %s"), *OutError);
			SearchResult = LastValidPoseSearchResult;
		}

		TArray<FPhysAnimBodySample> CurrentBodySamples;
		if (!GatherCurrentBodySamples(CurrentBodySamples, OutError))
		{
			return;
		}

		TArray<FPhysAnimFuturePoseSample> FuturePoseSamples;
		if (!SampleFuturePoses(SearchResult, FuturePoseSamples, OutError))
		{
			return;
		}

		FVector2D MimicTargetReferenceDataOffsetXY = FVector2D::ZeroVector;
		if (!ResolveMimicTargetReferenceDataOffset(SearchResult, MimicTargetReferenceDataOffsetXY, OutError))
		{
			return;
		}

		const float MimicTargetReferenceGroundHeight = ResolveSelfObservationGroundHeight(CurrentBodySamples);
		if (!PhysAnimBridge::BuildSelfObservation(CurrentBodySamples, MimicTargetReferenceGroundHeight, SelfObservationBuffer, OutError))
		{
			return;
		}

		TArray<FPhysAnimBodySample> MimicCurrentReferenceBodySamples;
		MakeMimicTargetCurrentReferenceBodySamples(CurrentBodySamples, MimicTargetReferenceDataOffsetXY, MimicTargetReferenceGroundHeight, MimicCurrentReferenceBodySamples);

		if (!PhysAnimBridge::BuildMimicTargetPoses(MimicCurrentReferenceBodySamples, FuturePoseSamples, MimicTargetPosesBuffer, OutError))
		{
			return;
		}

		if (!BuildTerrainObservation(CurrentBodySamples, TerrainBuffer, OutError))
		{
			return;
		}

		const int32 V0PlantReviewMode = PhysAnimComponentInternal::CVarPhysAnimV0PlantReviewMode.GetValueOnGameThread();
		const bool bV0PlantReviewStaticTargets = V0PlantReviewMode == 1 && RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
		if (!bV0PlantReviewStaticTargets && !RunInference(OutError))
		{
			return;
		}

		if (V0PlantReviewMode == 2 && RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
		{
			ActionOutputBuffer.Init(0.0f, PhysAnimBridge::NumActionFloats);
		}

		ConditionModelActions(EffectiveSettings, OutError);
	}

	ApplyControlTargets(
		bRunPolicyUpdateThisTick ? (PolicyControlIntervalSeconds * FMath::Max(ElapsedPolicySteps, 1)) : 0.0f,
		EffectiveSettings,
		bRunPolicyUpdateThisTick,
		OutError);
}

void UPhysAnimComponent::UpdateStartupMovementLockState(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	if (!bStartupMovementLockActive)
	{
		return;
	}

	const bool bInStableState = RuntimeState == EPhysAnimRuntimeState::BridgeActive || IsBalanceActiveState(RuntimeState);
	if (!bInStableState)
	{
		return;
	}

	if (!EffectiveSettings.bLockCharacterMovementUntilStartupReady)
	{
		ReleaseStartupMovementLock(true);
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Released startup movement lock because startup movement locking is disabled."));
		return;
	}

	const float PolicyInfluenceAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
	if (!EffectiveSettings.bDelayMovementUnlockUntilPolicySettled)
	{
		if (PolicyInfluenceAlpha > KINDA_SMALL_NUMBER)
		{
			ReleaseStartupMovementLock(EffectiveSettings.bRestoreCharacterMovementAfterStartupReady);
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Released startup movement lock after bridge became ready."));
		}
	}
	else if (PolicyInfluenceAlpha >= EffectiveSettings.PolicySettleMinInfluenceAlpha)
	{
		float ShellOffset, LinSpeed, AngSpeed;
		if (UpdatePolicySettleWindow(EffectiveSettings, ShellOffset, LinSpeed, AngSpeed))
		{
			ReleaseStartupMovementLock(EffectiveSettings.bRestoreCharacterMovementAfterStartupReady);
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Released startup movement lock after policy settled."));
		}
	}
}

void UPhysAnimComponent::HandleInitialPoseSearchWait(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError, FPoseSearchBlueprintResult& OutSearchResult)
{
	const bool bPoseSearchValid = QueryPoseSearch(OutSearchResult, OutError);
	RecordLiveRuntimeEvidencePoseSearchQueryResult(bPoseSearchValid, OutSearchResult.SelectedAnim ? OutSearchResult.SelectedAnim->GetName() : TEXT(""));

	if (!bPoseSearchValid)
	{
		const double WaitSeconds = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - InitialPoseSearchWaitStartTimeSeconds;
		if (IsInitialPoseSearchWaitTimedOut(WaitSeconds, PhysAnimComponentInternal::InitialPoseSearchWaitTimeoutSeconds))
		{
			FailStop(FString::Printf(TEXT("Initial PoseSearch result was never produced. %s"), *OutError));
		}
		return;
	}

	LastValidPoseSearchResult = OutSearchResult;
	ConsecutiveInvalidPoseSearchFrames = 0;

	if (EffectiveSettings.bLockCharacterMovementUntilStartupReady)
	{
		float LinSpeed, AngSpeed;
		if (!UpdateStartupQuietWindow(DeltaTime, EffectiveSettings, LinSpeed, AngSpeed))
		{
			return;
		}
		ReleaseStartupMovementLock(true);
	}

	// Activate physics ownership and the bringup ramp sequence.
	// bRequireLiveProofSatisfied=false because proof hasn't completed yet —
	// it runs during BridgeActive. Without this call, physics bodies stay
	// kinematic, bringup groups never unlock, PolicyInfluenceRampStartTimeSeconds
	// stays -1, and the BalanceReadyTransition preflight gate permanently blocks.
	// (S2-FIX-BALANCE-STARTUP-TICK-RACE-01)
	if (EffectiveSettings.bForceZeroActions)
	{
		TransitionRuntimeState(EPhysAnimRuntimeState::ReadyForActivation);
	}
	else
	{
		FString ActivationError;
		if (!ActivateBridgeFromReadyState(EffectiveSettings, TEXT("InitialPoseSearchSuccess"), ActivationError, false))
		{
			FailStop(FString::Printf(TEXT("Failed to activate bridge from ready state: %s"), *ActivationError));
		}
	}
}
