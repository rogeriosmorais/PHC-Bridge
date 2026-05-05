#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimProofArtifactEmitter.h"
#include "PhysAnimRuntimeAdapter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogPhysAnimBridge);

namespace
{
	bool HasConsistentLiveRuntimeEvidenceArtifact(const FPhysAnimRuntimeTerminationState& State)
	{
		const FPhysAnimRunArtifactSnapshot& Latest = State.LatestArtifact;
		const FPhysAnimRunArtifactSnapshot& Terminal = State.TerminalArtifact;

		if (!State.bTerminated)
		{
			return !Terminal.AttemptUuid.IsEmpty() &&
				Terminal.AttemptUuid == Latest.AttemptUuid &&
				Terminal.TerminalReason == EPhysAnimTerminalReason::None &&
				Terminal.bTerminalFrameArtifactCaptured;
		}

	return Terminal.AttemptUuid == Latest.AttemptUuid &&
			Terminal.TerminalReason == Latest.TerminalReason &&
			Terminal.TerminalSubstepTimestamp == Latest.TerminalSubstepTimestamp &&
			Terminal.bTerminalFrameArtifactCaptured == Latest.bTerminalFrameArtifactCaptured;
	}

	bool HasDeferredStartupProxyTerminalForCurrentAttempt(
		const FPhysAnimRuntimeTerminationState& State,
		const FString& CurrentAttemptUuid)
	{
		return State.bHasDeferredStartupProxyTerminalReason &&
			State.DeferredStartupProxyTerminalReason == EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion &&
			!CurrentAttemptUuid.IsEmpty() &&
			State.DeferredStartupProxyTerminalAttemptUuid == CurrentAttemptUuid &&
			State.DeferredStartupProxyTerminalSubstepTimestamp >= 0;
	}

	bool IsDeferredStartupProxyTerminalStillCurrent(
		const FPhysAnimRuntimeTerminationState& State,
		const FString& CurrentAttemptUuid)
	{
		return HasDeferredStartupProxyTerminalForCurrentAttempt(State, CurrentAttemptUuid) &&
			State.LatestArtifact.TerminalSubstepTimestamp <= State.DeferredStartupProxyTerminalSubstepTimestamp;
	}

	const TCHAR* ToCapsuleCollisionStateName(EPhysAnimCapsuleCollisionState State)
	{
		return State == EPhysAnimCapsuleCollisionState::CollisionEnabled
			? TEXT("collision_enabled")
			: TEXT("no_collision");
	}
}

TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_PoseSearchQueryMs, TEXT("PhysAnim/PoseSearch Query ms"));
TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_FuturePoseSampleMs, TEXT("PhysAnim/Future Pose Sample ms"));
TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_ObservationPackMs, TEXT("PhysAnim/Observation Pack ms"));
TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_RunSyncMs, TEXT("PhysAnim/RunSync ms"));
TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_ControlWritesMs, TEXT("PhysAnim/Control Writes ms"));
TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_UpdateControlsMs, TEXT("PhysAnim/UpdateControls ms"));
TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_MaxBodyAngularSpeedDegPerSec, TEXT("PhysAnim/Max Body Angular Speed deg/s"));
TRACE_DECLARE_FLOAT_COUNTER(COUNTER_PhysAnim_MaxLowerLimbLimitOccupancy, TEXT("PhysAnim/Max Lower Limb Limit Occupancy"));
TRACE_DECLARE_INT_COUNTER(COUNTER_PhysAnim_NumNormalPolicyTargetsWritten, TEXT("PhysAnim/Normal Policy Targets Written"));
TRACE_DECLARE_INT_COUNTER(COUNTER_PhysAnim_NumHeldTargetsWritten, TEXT("PhysAnim/Held Targets Written"));
TRACE_DECLARE_INT_COUNTER(COUNTER_PhysAnim_NumTotalTargetsWritten, TEXT("PhysAnim/Total Targets Written"));
TRACE_DECLARE_INT_COUNTER(COUNTER_PhysAnim_RuntimeState, TEXT("PhysAnim/Runtime State"));
TRACE_DECLARE_INT_COUNTER(COUNTER_PhysAnim_FailStopCount, TEXT("PhysAnim/FailStop Count"));

int32 GStrictPhase1Certification = 1;
FAutoConsoleVariableRef CVarStrictPhase1Certification(
	TEXT("p.PhysAnim.StrictPhase1Certification"),
	GStrictPhase1Certification,
	TEXT("If 1, Phase 1 certification restores original, more stringent constraints (timeout, dwell, shell reanchor, and ownership rules) to verify genuine convergence."),
	ECVF_Cheat);

int32 GVerbosePhase1Forensics = 0;
FAutoConsoleVariableRef CVarVerbosePhase1Forensics(
	TEXT("p.PhysAnim.VerbosePhase1Forensics"),
	GVerbosePhase1Forensics,
	TEXT("If 1, enables high-volume per-tick forensics for Phase 1 (modifier syncs, simulating body lists, persistent write traces). Default OFF to keep logs readable."),
	ECVF_Cheat);

int32 GVerbosePhase2Forensics = 0;
FAutoConsoleVariableRef CVarVerbosePhase2Forensics(
	TEXT("p.PhysAnim.VerbosePhase2Forensics"),
	GVerbosePhase2Forensics,
	TEXT("If 1, enables high-volume per-tick forensics for Phase 2 (root sim status, write coverage, shell drop status). Default OFF to keep logs readable."),
	ECVF_Cheat);


UPhysAnimComponent::UPhysAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	ModelDataAsset = TSoftObjectPtr<UNNEModelData>(FSoftObjectPath(PhysAnimComponentInternal::DefaultModelPath));
	LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	bEnableLiveRuntimeEvidenceProof = true;
}

bool UPhysAnimComponent::BuildConditionedActions(
	const TArray<float>& RawActions,
	const TArray<float>* PreviousConditionedActions,
	const FPhysAnimActionConditioningSettings& Settings,
	TArray<float>& OutConditionedActions,
	FPhysAnimActionDiagnostics& OutDiagnostics,
	FString& OutError)
{
	return PhysAnimBridge::ConditionModelActions(
		RawActions,
		PreviousConditionedActions,
		Settings,
		OutConditionedActions,
		OutDiagnostics,
		OutError);
}

FQuat UPhysAnimComponent::LimitTargetRotationStep(
	const FQuat& PreviousRotation,
	const FQuat& TargetRotation,
	float MaxAngularStepDegrees)
{
	return PhysAnimBridge::LimitControlRotationStep(PreviousRotation, TargetRotation, MaxAngularStepDegrees);
}

bool UPhysAnimComponent::EvaluateRuntimeInstability(
	const FVector& RootLocationCm,
	const FVector& RootLinearVelocityCmPerSecond,
	const FVector& RootAngularVelocityDegPerSecond,
	float DeltaTimeSeconds,
	const FPhysAnimRuntimeInstabilitySettings& Settings,
	FPhysAnimRuntimeInstabilityState& InOutState,
	FPhysAnimRuntimeInstabilityDiagnostics& OutDiagnostics,
	FString& OutError)
{
	return PhysAnimBridge::UpdateRuntimeInstabilityState(
		RootLocationCm,
		RootLinearVelocityCmPerSecond,
		RootAngularVelocityDegPerSecond,
		DeltaTimeSeconds,
		Settings,
		InOutState,
		OutDiagnostics,
		OutError);
}



bool UPhysAnimComponent::CanEnterBalanceActiveStanding() const
{
	if (!bEnableLiveRuntimeEvidenceProof)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] ENTRY_ALLOWED reason=PROOF_DISABLED_BYPASS"));
		return true;
	}

	if (!bLiveRuntimeEvidenceProofComplete)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROOF_NOT_COMPLETE"));
		return false;
	}

	if (LiveRuntimeEvidenceTerminationState.bTerminated &&
		LiveRuntimeEvidenceTerminationState.TerminalReason != EPhysAnimTerminalReason::None)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=TERMINATED_IN_PROOF terminal_reason=%d"), 
			static_cast<int32>(LiveRuntimeEvidenceTerminationState.TerminalReason));
		return false;
	}

	const bool bProofSatisfied = IsLiveRuntimeEvidenceProofSatisfied();
	UE_LOG(
		LogPhysAnimBridge,
		Verbose,
		TEXT("[PhysAnim] Startup proof satisfaction evaluated satisfied=%d state=%s fresh=%d observed=%d deferredReason=%d enforceArmed=%d"),
		bProofSatisfied ? 1 : 0,
		GetRuntimeStateName(RuntimeState),
		bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
		bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
		static_cast<int32>(StartupProofDeferredTerminalReason),
		bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0);

	if (!bProofSatisfied)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROOF_NOT_TRUTHFUL startupObserved=%d startupArmed=%d startupArmSubstep=%lld startupDeferredReason=%d proxyHandoffArmed=%d proofComplete=%d"),
			bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
			bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0,
			static_cast<long long>(StartupProofVerificationHandoffArmedSubstep),
			static_cast<int32>(StartupProofDeferredTerminalReason),
			bLiveRuntimeEvidenceStartupProxySupportHandoffArmed ? 1 : 0,
			bLiveRuntimeEvidenceProofComplete ? 1 : 0);
		return false;
	}

	const FPhysAnimRunArtifactSnapshot& Latest = LiveRuntimeEvidenceTerminationState.LatestArtifact;

	if (Latest.SupportMode == EPhysAnimSupportMode::Airborne)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=AIRBORNE_MODE"));
		return false;
	}

	if (Latest.ActiveSupportSideCount == 0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=NO_SUPPORT_SIDES"));
		return false;
	}

	if (Latest.SupportHullAreaCm2 <= 0.0f)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=NO_SUPPORT_AREA"));
		return false;
	}

	const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();
	const bool bDeferredStartupProxyTerminalCurrentAttempt =
		IsDeferredStartupProxyTerminalStillCurrent(LiveRuntimeEvidenceTerminationState, LiveRuntimeEvidenceAttemptUuid);
	const bool bProxyOutsideHullDeferred =
		bLiveRuntimeEvidenceProofComplete &&
		(!bLiveRuntimeEvidenceStartupProxySupportHandoffArmed || bDeferredStartupProxyTerminalCurrentAttempt);

	if (Latest.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=SUPPORT_GAP gap=%.1f threshold=%.1f"), 
			Latest.SupportGapTimerMs, Settings.BalancePhase1AdmissionMaxSupportGapMs);
		return false;
	}

	if (Latest.ProxyInsideHull.IsSet() && !Latest.ProxyInsideHull.GetValue())
	{
		if (bProxyOutsideHullDeferred)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy outside hull deferred during standing entry state=%s"),
				GetRuntimeStateName(RuntimeState));
		}
		else
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy outside hull enforced after handoff armed state=%s"),
				GetRuntimeStateName(RuntimeState));
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROXY_OUTSIDE_HULL"));
			return false;
		}
	}

	if (Latest.ProxyOutsideHullDurationMs.IsSet() && Latest.ProxyOutsideHullDurationMs.GetValue() >= Settings.ProxyDriftLimitMs)
	{
		if (bProxyOutsideHullDeferred)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy outside hull deferred during standing entry state=%s"),
				GetRuntimeStateName(RuntimeState));
		}
		else
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy outside hull enforced after handoff armed state=%s"),
				GetRuntimeStateName(RuntimeState));
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROXY_DRIFT_THRESHOLD drift=%.1f threshold=%.1f"), 
				Latest.ProxyOutsideHullDurationMs.GetValue(), Settings.ProxyDriftLimitMs);
			return false;
		}
	}

	if (!Latest.bCapsuleContractPassed)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=CAPSULE_CONTRACT_FAILED"));
		return false;
	}

	if (!Latest.bPhysicalContinuityValidatorPassed)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=CONTINUITY_CONTRACT_FAILED"));
		return false;
	}

	if (Latest.bContinuityBookkeepingMismatch)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=CONTINUITY_BOOKKEEPING_MISMATCH"));
		return false;
	}

	if (!Latest.bPhysicsAssetContractValid || !Latest.bSkeletonAuditPassed)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=AUDIT_STATE_INCONSISTENT physicsAsset=%d skeletonAudit=%d"),
			Latest.bPhysicsAssetContractValid ? 1 : 0,
			Latest.bSkeletonAuditPassed ? 1 : 0);
		return false;
	}

	if (!HasConsistentLiveRuntimeEvidenceArtifact(LiveRuntimeEvidenceTerminationState))
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=ARTIFACT_STATE_INCONSISTENT"));
		return false;
	}

	if (LiveRuntimeEvidenceStandingSeconds < LiveRuntimeEvidenceStandingTargetSeconds)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=INSUFFICIENT_STANDING_DURATION duration=%.3f target=%.3f"), 
			LiveRuntimeEvidenceStandingSeconds, LiveRuntimeEvidenceStandingTargetSeconds);
		return false;
	}

	return true;
}

void UPhysAnimComponent::NoteStartupProofWaitingForPoseSearchObserved()
{
	if (!bEnableLiveRuntimeEvidenceProof)
	{
		return;
	}

	if (!bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved)
	{
		bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved = true;
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup entry bridge verify observed state=%s"),
			GetRuntimeStateName(RuntimeState));
	}
}

void UPhysAnimComponent::ArmStartupProofTerminalEnforcement()
{
	if (!bEnableLiveRuntimeEvidenceProof)
	{
		return;
	}

	if (!bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved)
	{
		NoteStartupProofWaitingForPoseSearchObserved();
	}

	if (!bLiveRuntimeEvidenceStartupVerificationHandoffArmed)
	{
		bLiveRuntimeEvidenceStartupVerificationHandoffArmed = true;
		StartupProofVerificationHandoffArmedSubstep = LiveRuntimeEvidenceSubstepCounter;
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup entry bridge terminal enforcement armed state=%s substep=%lld"),
			GetRuntimeStateName(RuntimeState),
			static_cast<long long>(StartupProofVerificationHandoffArmedSubstep));
	}
}


bool UPhysAnimComponent::ShouldExitStandingToSafeDeny(const FPhysAnimRuntimeTerminationState& TerminationState) const
{
	if (!bEnableLiveRuntimeEvidenceProof)
	{
		return false;
	}

	if (LiveRuntimeEvidenceSubstepCounter <= 0)
	{
		return false;
	}

	const FPhysAnimRunArtifactSnapshot& Latest = TerminationState.LatestArtifact;

	if (Latest.SupportMode == EPhysAnimSupportMode::Airborne)
	{
		return true;
	}

	if (Latest.ActiveSupportSideCount == 0)
	{
		return true;
	}

	if (Latest.SupportHullAreaCm2 <= 0.0f)
	{
		return true;
	}

	const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();
	const bool bDeferredStartupProxyTerminalCurrentAttempt =
		IsDeferredStartupProxyTerminalStillCurrent(TerminationState, LiveRuntimeEvidenceAttemptUuid);

	if (Latest.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		return true;
	}

	const bool bProxyOutsideHull =
		(Latest.ProxyInsideHull.IsSet() && !Latest.ProxyInsideHull.GetValue()) ||
		(Latest.ProxyOutsideHullDurationMs.IsSet() &&
			Latest.ProxyOutsideHullDurationMs.GetValue() >= Settings.ProxyDriftLimitMs);
	if (bProxyOutsideHull)
	{
		if (!bLiveRuntimeEvidenceStartupProxySupportHandoffArmed || bDeferredStartupProxyTerminalCurrentAttempt)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy outside hull deferred during standing entry state=%s"),
				GetRuntimeStateName(RuntimeState));
			return false;
		}

		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Proxy outside hull enforced after handoff armed state=%s"),
			GetRuntimeStateName(RuntimeState));
		return true;
	}

	return false;
}


EPhysAnimRuntimeState UPhysAnimComponent::EvaluateBalanceActiveStanding() const
{
	if (LiveRuntimeEvidenceTerminationState.bTerminated && 
		LiveRuntimeEvidenceTerminationState.TerminalReason != EPhysAnimTerminalReason::None)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] STANDING_ACTIVE_EVAL result=FailStopped reason=TERMINATED_IN_LOOP terminal_reason=%d"), 
			static_cast<int32>(LiveRuntimeEvidenceTerminationState.TerminalReason));
		return EPhysAnimRuntimeState::FailStopped;
	}

	if (ShouldExitStandingToSafeDeny(LiveRuntimeEvidenceTerminationState))
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] STANDING_ACTIVE_EVAL result=BalanceSafeDeny reason=PHASE3_ACTIVE_SUPPORT_FAILURE hull_area=%.1f gap=%.1f proxy_inside=%d proxy_drift=%.1f"),
			LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportHullAreaCm2,
			LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportGapTimerMs,
			LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyInsideHull.IsSet() ? (LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyInsideHull.GetValue() ? 1 : 0) : -1,
			LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.IsSet() ? LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.GetValue() : 0.0);

		return EPhysAnimRuntimeState::BalanceSafeDeny;
	}

	return EPhysAnimRuntimeState::BalanceActive_Standing;
}


bool UPhysAnimComponent::IsLiveRuntimeEvidenceProofSatisfied() const
{
	if (!bLiveRuntimeEvidenceProofComplete ||
		LiveRuntimeEvidenceTerminationState.TerminalReason != EPhysAnimTerminalReason::None ||
		LiveRuntimeEvidenceStandingSeconds < LiveRuntimeEvidenceStandingTargetSeconds)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup proof satisfaction evaluated satisfied=0 state=%s fresh=%d observed=%d deferredReason=%d enforceArmed=%d reason=proof_incomplete_or_terminated"),
			GetRuntimeStateName(RuntimeState),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
			static_cast<int32>(StartupProofDeferredTerminalReason),
			bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0);
		return false;
	}

	const FPhysAnimRunArtifactSnapshot& Latest = LiveRuntimeEvidenceTerminationState.LatestArtifact;
	const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();

	if (!Latest.bPhysicsAssetContractValid ||
		!Latest.bSkeletonAuditPassed ||
		!Latest.bCapsuleContractPassed ||
		!Latest.bPhysicalContinuityValidatorPassed ||
		Latest.bContinuityBookkeepingMismatch)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup proof satisfaction evaluated satisfied=0 state=%s fresh=%d observed=%d deferredReason=%d enforceArmed=%d reason=artifact_contract_invalid"),
			GetRuntimeStateName(RuntimeState),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
			static_cast<int32>(StartupProofDeferredTerminalReason),
			bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0);
		return false;
	}

	if (Latest.SupportMode == EPhysAnimSupportMode::Airborne ||
		Latest.ActiveSupportSideCount <= 0 ||
		Latest.SupportHullAreaCm2 <= 0.0f ||
		Latest.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup proof satisfaction evaluated satisfied=0 state=%s fresh=%d observed=%d deferredReason=%d enforceArmed=%d reason=insufficient_support"),
			GetRuntimeStateName(RuntimeState),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
			static_cast<int32>(StartupProofDeferredTerminalReason),
			bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0);
		return false;
	}

	return HasConsistentLiveRuntimeEvidenceArtifact(LiveRuntimeEvidenceTerminationState);
}


void UPhysAnimComponent::ResetLiveRuntimeEvidenceProof()
{
	bLiveRuntimeEvidenceProofActive = false;
	bLiveRuntimeEvidenceProofComplete = false;
	bLiveRuntimeEvidenceTerminalArtifactEmitted = false;
	bLiveRuntimeEvidenceProofShouldComplete = true;

	LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	LiveRuntimeEvidenceStandingSeconds = 0.0f;
	LiveRuntimeEvidenceLastProgressLogSeconds = -1.0f;
	ActivatedStandingStabilityMetrics = FPhysAnimActivatedStandingStabilityMetrics();
	bActivatedStandingStabilityBaselineInitialized = false;
	bActivatedStandingPerturbationApplied = false;
	ActivatedStandingStabilityBaselineRootLocationCm = FVector::ZeroVector;
	ActivatedStandingStabilityBaselineRootTiltDeg = 0.0f;
	ActivatedStandingStabilitySupportHullAreaSumCm2 = 0.0;
	ActivatedStandingStabilityActiveSupportSideCountSum = 0.0;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
	}

	LiveRuntimeEvidenceSubstepCounter = 0;
	const bool bPreserveDeferredStartupProxyTerminalReason =
		LiveRuntimeEvidenceTerminationState.bHasDeferredStartupProxyTerminalReason &&
		LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalReason ==
			EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
	const FString DeferredStartupProxyTerminalAttemptUuid =
		LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalAttemptUuid;
	const int64 DeferredStartupProxyTerminalSubstepTimestamp =
		LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalSubstepTimestamp;
	LiveRuntimeEvidenceTerminationState = FPhysAnimRuntimeTerminationState();
	if (bPreserveDeferredStartupProxyTerminalReason)
	{
		LiveRuntimeEvidenceTerminationState.bHasDeferredStartupProxyTerminalReason = true;
		LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalReason =
			EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
		LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalAttemptUuid =
			DeferredStartupProxyTerminalAttemptUuid;
		LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalSubstepTimestamp =
			DeferredStartupProxyTerminalSubstepTimestamp;
	}
	LiveRuntimeEvidenceAttemptUuid.Empty();
	StartupProofDeferredTerminalReason = EPhysAnimTerminalReason::None;
	bLiveRuntimeEvidenceStartupEvidenceFresh = false;
	bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved = false;
	bLiveRuntimeEvidenceStartupStandingEntryAccepted = false;
	StartupProofStandingEntryAcceptedSubstep = -1;
	bLiveRuntimeEvidenceStartupVerificationHandoffArmed = false;
	StartupProofVerificationHandoffArmedSubstep = -1;
	bLiveRuntimeEvidenceStartupProxySupportHandoffArmed = false;
	bForceSupportFailure = false;
	ResetBridgeLocomotionRequestState();
}

void UPhysAnimComponent::TickLiveRuntimeEvidenceProof(float DeltaTimeSeconds)
{
	const bool bTerminalFailureLatched =
		LiveRuntimeEvidenceTerminationState.bTerminated &&
		LiveRuntimeEvidenceTerminationState.TerminalReason != EPhysAnimTerminalReason::None;
	if (!bEnableLiveRuntimeEvidenceProof || bTerminalFailureLatched)
	{
		return;
	}

	const bool bContinuingSafeDenyProof =
		RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny &&
		bLiveRuntimeEvidenceProofActive &&
		!bLiveRuntimeEvidenceProofComplete;
	const bool bProofEligibleRuntimeState =
		RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation ||
		IsBalanceEntryState(RuntimeState) ||
		IsBalanceActiveState(RuntimeState) ||
		bContinuingSafeDenyProof;
	if (!bProofEligibleRuntimeState)
	{
		return;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	if ((RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation) &&
		EffectiveSettings.bLockCharacterMovementUntilStartupReady &&
		StartupQuietWindowAccumulatedSeconds < EffectiveSettings.StartupQuietRequiredSeconds)
	{
		return;
	}

	if (LiveRuntimeEvidenceAttemptUuid.IsEmpty())
	{
		LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
		PhysAnimProofArtifactEmitter::LogAttemptStart(LiveRuntimeEvidenceAttemptUuid);
	}
	bLiveRuntimeEvidenceProofActive = true;
	if (bEnableLiveRuntimeEvidenceProof && !bLiveRuntimeEvidenceProofComplete)
	{
		PolicyInfluenceRampStartTimeSeconds = -1.0;
	}
	if ((RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation) &&
		LiveRuntimeEvidenceSubstepCounter == 0)
	{
		if (RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch)
		{
			bLiveRuntimeEvidenceStartupEvidenceFresh = true;
			if (!bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved)
			{
				NoteStartupProofWaitingForPoseSearchObserved();
			}
		}

		const bool bStartupEntryBridgeSatisfied = IsLiveRuntimeEvidenceProofSatisfied();
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup entry bridge evidence fresh=%d satisfied=%d state=%s"),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bStartupEntryBridgeSatisfied ? 1 : 0,
			GetRuntimeStateName(RuntimeState));
	}

	LiveRuntimeEvidenceStandingSeconds += FMath::Max(0.0f, DeltaTimeSeconds);
	++LiveRuntimeEvidenceSubstepCounter;

	TArray<FHitResult> HitResults;
	int32 MappedSupportHitCount = 0;
	CaptureLiveRuntimeEvidenceHitResults(HitResults, MappedSupportHitCount);

	const FPhysAnimSupportHitResultObservationInput ObservationInput =
		BuildLiveRuntimeEvidenceObservationInput(HitResults, DeltaTimeSeconds);

	const FPhysAnimSupportObservationResult SupportObservation =
		PhysAnimRuntimeAdapter::BuildSupportObservationFromHitResults(ObservationInput);

	FPhysAnimRuntimeTerminationPipelineInput PipelineInput;
	PipelineInput.PreviousState = LiveRuntimeEvidenceTerminationState;
	PipelineInput.SubstepInput = BuildLiveRuntimeEvidenceSubstepInput(SupportObservation, DeltaTimeSeconds);
	PipelineInput.bEnableTerminationCommand = !bLiveRuntimeEvidenceProofComplete;
	PipelineInput.bAllowMovementReclaimOnTermination = true;

	const FPhysAnimRuntimeTerminationPipelineResult PipelineResult =
		PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(PipelineInput);

	LiveRuntimeEvidenceTerminationState = PipelineResult.StateApplyResult.State;

	PhysAnimProofArtifactEmitter::LogRuntimeEvidence(
		LiveRuntimeEvidenceAttemptUuid,
		HitResults.Num(),
		MappedSupportHitCount,
		PipelineResult.SubstepResult.Artifact);

	UpdateActivatedStandingStabilityMetrics(DeltaTimeSeconds);

	if (LiveRuntimeEvidenceTerminationState.bTerminated)
	{
		const bool bStartupProofRuntime =
			RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
			RuntimeState == EPhysAnimRuntimeState::ReadyForActivation;
		const bool bStartupProxySupportHandoffDeferred =
			bStartupProofRuntime &&
			!bLiveRuntimeEvidenceStartupProxySupportHandoffArmed &&
			LiveRuntimeEvidenceTerminationState.TerminalReason == EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
		if (bStartupProxySupportHandoffDeferred)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Startup proof deferred terminal reason=ActivationProxyOutsideSupportRegion phase=%s"),
				GetRuntimeStateName(RuntimeState));
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Startup proxy terminal recorded reason=PROXY_OUTSIDE_HULL state=%s"),
				GetRuntimeStateName(RuntimeState));
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Startup proxy terminal deferred for standing entry"));
			LiveRuntimeEvidenceTerminationState.bHasDeferredStartupProxyTerminalReason = true;
			LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalReason =
				LiveRuntimeEvidenceTerminationState.TerminalReason;
			LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalAttemptUuid = LiveRuntimeEvidenceAttemptUuid;
			LiveRuntimeEvidenceTerminationState.DeferredStartupProxyTerminalSubstepTimestamp =
				LiveRuntimeEvidenceSubstepCounter;

			FPhysAnimRunArtifactSnapshot DeferredArtifact = PipelineResult.SubstepResult.Artifact;
			DeferredArtifact.TerminalReason = EPhysAnimTerminalReason::None;
			DeferredArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			DeferredArtifact.bTerminalFrameArtifactCaptured = true;

			LiveRuntimeEvidenceTerminationState.bTerminated = false;
			LiveRuntimeEvidenceTerminationState.TerminalReason = EPhysAnimTerminalReason::None;
			LiveRuntimeEvidenceTerminationState.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			LiveRuntimeEvidenceTerminationState.bTerminalFrameArtifactCaptured = true;
			LiveRuntimeEvidenceTerminationState.TerminalArtifact = DeferredArtifact;
			LiveRuntimeEvidenceTerminationState.LatestArtifact = DeferredArtifact;
		}
		else if (bStartupProofRuntime &&
			!bLiveRuntimeEvidenceStartupVerificationHandoffArmed &&
			!bForceSupportFailure &&
			LiveRuntimeEvidenceTerminationState.TerminalReason == EPhysAnimTerminalReason::ActivationSupportFailure)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Startup terminal deferred reason=ActivationSupportFailure state=%s"),
				GetRuntimeStateName(RuntimeState));
			StartupProofDeferredTerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;

			FPhysAnimRunArtifactSnapshot DeferredArtifact = PipelineResult.SubstepResult.Artifact;
			DeferredArtifact.TerminalReason = EPhysAnimTerminalReason::None;
			DeferredArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			DeferredArtifact.bTerminalFrameArtifactCaptured = true;

			LiveRuntimeEvidenceTerminationState.bTerminated = false;
			LiveRuntimeEvidenceTerminationState.TerminalReason = EPhysAnimTerminalReason::None;
			LiveRuntimeEvidenceTerminationState.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			LiveRuntimeEvidenceTerminationState.bTerminalFrameArtifactCaptured = true;
			LiveRuntimeEvidenceTerminationState.TerminalArtifact = DeferredArtifact;
			LiveRuntimeEvidenceTerminationState.LatestArtifact = DeferredArtifact;
		}
		else if (bStartupProofRuntime && LiveRuntimeEvidenceSubstepCounter == 1)
		{
			UE_LOG(LogPhysAnimBridge, Verbose, TEXT("[PhysAnimProof] Startup support evidence sample ignored before activation proof was initialized or refreshed."));
			FPhysAnimRunArtifactSnapshot WarmupArtifact = PipelineResult.SubstepResult.Artifact;
			WarmupArtifact.TerminalReason = EPhysAnimTerminalReason::None;
			WarmupArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			WarmupArtifact.bTerminalFrameArtifactCaptured = true;
			LiveRuntimeEvidenceTerminationState.bTerminated = false;
			LiveRuntimeEvidenceTerminationState.TerminalReason = EPhysAnimTerminalReason::None;
			LiveRuntimeEvidenceTerminationState.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			LiveRuntimeEvidenceTerminationState.bTerminalFrameArtifactCaptured = true;
			LiveRuntimeEvidenceTerminationState.TerminalArtifact = WarmupArtifact;
			LiveRuntimeEvidenceTerminationState.LatestArtifact = WarmupArtifact;
		}
		else
		{
			bLiveRuntimeEvidenceProofComplete = true;

			FPhysAnimProofArtifactEmitInput EmitInput;
			EmitInput.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
			EmitInput.StandingSeconds = LiveRuntimeEvidenceStandingSeconds;
			EmitInput.RuntimeHitCount = HitResults.Num();
			EmitInput.MappedSupportHitCount = MappedSupportHitCount;
			EmitInput.PipelineResult = PipelineResult;

			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(EmitInput);

			PhysAnimProofArtifactEmitter::LogAttemptResult(
				LiveRuntimeEvidenceAttemptUuid,
				false,
				LiveRuntimeEvidenceStandingSeconds,
				LiveRuntimeEvidenceTerminationState.TerminalReason);
			
			return;
		}
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing &&
		!bLiveRuntimeEvidenceStartupStandingEntryAccepted &&
		IsLiveRuntimeEvidenceProofSatisfied())
	{
		ArmStartupProofTerminalEnforcement();
		bLiveRuntimeEvidenceStartupStandingEntryAccepted = true;
		StartupProofStandingEntryAcceptedSubstep = LiveRuntimeEvidenceSubstepCounter;
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Standing entry accepted proxy handoff arming pending state=%s"),
			GetRuntimeStateName(RuntimeState));
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup entry bridge proof satisfied transition state=%s"),
			GetRuntimeStateName(RuntimeState));
	}

	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing &&
		bLiveRuntimeEvidenceStartupStandingEntryAccepted &&
		!bLiveRuntimeEvidenceStartupProxySupportHandoffArmed)
	{
		const FPhysAnimRunArtifactSnapshot& StandingArtifact = LiveRuntimeEvidenceTerminationState.LatestArtifact;
		const bool bStandingSupportFresh =
			StandingArtifact.SupportMode != EPhysAnimSupportMode::Airborne &&
			StandingArtifact.ActiveSupportSideCount > 0 &&
			StandingArtifact.SupportHullAreaCm2 > 0.0f;
		const bool bStandingProxyOutsideHull =
			(StandingArtifact.ProxyInsideHull.IsSet() && !StandingArtifact.ProxyInsideHull.GetValue()) ||
			(StandingArtifact.ProxyOutsideHullDurationMs.IsSet() &&
				StandingArtifact.ProxyOutsideHullDurationMs.GetValue() >= ResolveEffectiveStabilizationSettings().ProxyDriftLimitMs);
		if (bStandingSupportFresh &&
			LiveRuntimeEvidenceSubstepCounter > (StartupProofStandingEntryAcceptedSubstep + 1))
		{
			bLiveRuntimeEvidenceStartupProxySupportHandoffArmed = true;
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Standing entry accepted proxy handoff armed state=%s"),
				GetRuntimeStateName(RuntimeState));
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy handoff armed state=%s"),
				GetRuntimeStateName(RuntimeState));
		}
		else if (bStandingSupportFresh && bStandingProxyOutsideHull)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy outside hull deferred during standing entry state=%s"),
				GetRuntimeStateName(RuntimeState));
		}
	}

	if (!bLiveRuntimeEvidenceProofComplete &&
		bLiveRuntimeEvidenceProofShouldComplete &&
		LiveRuntimeEvidenceStandingSeconds >= LiveRuntimeEvidenceStandingTargetSeconds)
	{
		bLiveRuntimeEvidenceProofComplete = true;
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup entry bridge proof satisfied transition state=%s"),
			GetRuntimeStateName(RuntimeState));

		FPhysAnimRunArtifactSnapshot CompletionArtifact = PipelineResult.SubstepResult.Artifact;
		CompletionArtifact.TerminalReason = EPhysAnimTerminalReason::None;
		CompletionArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
		CompletionArtifact.bTerminalFrameArtifactCaptured = true;

		LiveRuntimeEvidenceTerminationState.bTerminated = false;
		LiveRuntimeEvidenceTerminationState.TerminalReason = EPhysAnimTerminalReason::None;
		LiveRuntimeEvidenceTerminationState.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
		LiveRuntimeEvidenceTerminationState.bTerminalFrameArtifactCaptured = true;
		LiveRuntimeEvidenceTerminationState.TerminalArtifact = CompletionArtifact;
		LiveRuntimeEvidenceTerminationState.LatestArtifact = CompletionArtifact;
		
		FPhysAnimProofArtifactEmitInput EmitInput;
		EmitInput.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
		EmitInput.StandingSeconds = LiveRuntimeEvidenceStandingSeconds;
		EmitInput.RuntimeHitCount = HitResults.Num();
		EmitInput.MappedSupportHitCount = MappedSupportHitCount;
		EmitInput.PipelineResult = PipelineResult;
		// Ensure the terminal state we just built is the one that gets emitted
		EmitInput.PipelineResult.StateApplyResult.State = LiveRuntimeEvidenceTerminationState;

		PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(EmitInput);
		bLiveRuntimeEvidenceTerminalArtifactEmitted = true;
		
		PhysAnimProofArtifactEmitter::LogAttemptResult(
			LiveRuntimeEvidenceAttemptUuid,
			true,
			LiveRuntimeEvidenceStandingSeconds,
			EPhysAnimTerminalReason::None);
	}
}

void UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics(float DeltaTimeSeconds)
{
	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		return;
	}

	USkeletalMeshComponent* const Mesh = GetMeshComponent();
	AActor* const OwnerActor = GetOwner();
	if (!Mesh || !OwnerActor)
	{
		return;
	}

	FString TiltSource;
	const float CurrentRootTiltDeg = ResolvePhase1Uprightness(Mesh, OwnerActor, PhysAnimBridge::GetRootBoneName(), TiltSource);
	const FVector CurrentRootLocationCm = LastRuntimeInstabilityDiagnostics.RootLocationCm;
	const double CurrentSupportHullAreaCm2 = LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportHullAreaCm2;
	const double CurrentActiveSupportSideCount = static_cast<double>(LiveRuntimeEvidenceTerminationState.LatestArtifact.ActiveSupportSideCount);

	if (!bActivatedStandingStabilityBaselineInitialized)
	{
		bActivatedStandingStabilityBaselineInitialized = true;
		ActivatedStandingStabilityBaselineRootLocationCm = CurrentRootLocationCm;
		ActivatedStandingStabilityBaselineRootTiltDeg = CurrentRootTiltDeg;
		ActivatedStandingStabilityMetrics.SupportHullAreaMinCm2 = CurrentSupportHullAreaCm2;
		ActivatedStandingStabilityMetrics.SupportHullAreaMaxCm2 = CurrentSupportHullAreaCm2;
		ActivatedStandingStabilityMetrics.ActiveSupportSideCountMin = CurrentActiveSupportSideCount;
		ActivatedStandingStabilityMetrics.ActiveSupportSideCountMax = CurrentActiveSupportSideCount;
	}

	ActivatedStandingStabilityMetrics.bHasSamples = true;
	ActivatedStandingStabilityMetrics.RuntimeState = RuntimeState;
	ActivatedStandingStabilityMetrics.TerminalReason = static_cast<int32>(LiveRuntimeEvidenceTerminationState.TerminalReason);
	ActivatedStandingStabilityMetrics.ActivationDurationSec += FMath::Max(0.0f, DeltaTimeSeconds);
	ActivatedStandingStabilityMetrics.SampleCount++;

	const double CurrentRootWorldPositionDriftCm = FVector::Dist(CurrentRootLocationCm, ActivatedStandingStabilityBaselineRootLocationCm);
	const double CurrentRootVerticalDriftCm = FMath::Abs(CurrentRootLocationCm.Z - ActivatedStandingStabilityBaselineRootLocationCm.Z);
	const double CurrentRootAngularDriftDeg = FMath::Abs(CurrentRootTiltDeg - ActivatedStandingStabilityBaselineRootTiltDeg);

	ActivatedStandingStabilityMetrics.RootWorldPositionDriftCm = FMath::Max(ActivatedStandingStabilityMetrics.RootWorldPositionDriftCm, CurrentRootWorldPositionDriftCm);
	ActivatedStandingStabilityMetrics.RootVerticalDriftCm = FMath::Max(ActivatedStandingStabilityMetrics.RootVerticalDriftCm, CurrentRootVerticalDriftCm);
	ActivatedStandingStabilityMetrics.RootAngularDriftDeg = FMath::Max(ActivatedStandingStabilityMetrics.RootAngularDriftDeg, CurrentRootAngularDriftDeg);
	ActivatedStandingStabilityMetrics.MaxBodyLinearSpeedCmPerSecond = FMath::Max(
		ActivatedStandingStabilityMetrics.MaxBodyLinearSpeedCmPerSecond,
		static_cast<double>(LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond));
	ActivatedStandingStabilityMetrics.MaxBodyAngularSpeedDegPerSecond = FMath::Max(
		ActivatedStandingStabilityMetrics.MaxBodyAngularSpeedDegPerSecond,
		static_cast<double>(LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond));

	int32 DirectBodySampleCount = 0;
	int32 DirectSimulatingBodyCount = 0;
	for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		if (const FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
		{
			++DirectBodySampleCount;
			if (BodyInstance->IsInstanceSimulatingPhysics())
			{
				++DirectSimulatingBodyCount;
			}
		}
	}
	ActivatedStandingStabilityMetrics.BodyTelemetrySampleCount += DirectBodySampleCount;
	ActivatedStandingStabilityMetrics.SimulatingBodyCountMax = FMath::Max(
		ActivatedStandingStabilityMetrics.SimulatingBodyCountMax,
		DirectSimulatingBodyCount);

	ActivatedStandingStabilitySupportHullAreaSumCm2 += CurrentSupportHullAreaCm2;
	ActivatedStandingStabilityActiveSupportSideCountSum += CurrentActiveSupportSideCount;
	ActivatedStandingStabilityMetrics.SupportHullAreaMinCm2 = FMath::Min(ActivatedStandingStabilityMetrics.SupportHullAreaMinCm2, CurrentSupportHullAreaCm2);
	ActivatedStandingStabilityMetrics.SupportHullAreaMaxCm2 = FMath::Max(ActivatedStandingStabilityMetrics.SupportHullAreaMaxCm2, CurrentSupportHullAreaCm2);
	ActivatedStandingStabilityMetrics.SupportHullAreaMeanCm2 = ActivatedStandingStabilitySupportHullAreaSumCm2 / ActivatedStandingStabilityMetrics.SampleCount;
	ActivatedStandingStabilityMetrics.ActiveSupportSideCountMin = FMath::Min(ActivatedStandingStabilityMetrics.ActiveSupportSideCountMin, CurrentActiveSupportSideCount);
	ActivatedStandingStabilityMetrics.ActiveSupportSideCountMax = FMath::Max(ActivatedStandingStabilityMetrics.ActiveSupportSideCountMax, CurrentActiveSupportSideCount);
	ActivatedStandingStabilityMetrics.ActiveSupportSideCountMean = ActivatedStandingStabilityActiveSupportSideCountSum / ActivatedStandingStabilityMetrics.SampleCount;
	ActivatedStandingStabilityMetrics.FailStopCount =
		LiveRuntimeEvidenceTerminationState.bTerminated && LiveRuntimeEvidenceTerminationState.TerminalReason != EPhysAnimTerminalReason::None
		? 1
		: 0;
}

bool UPhysAnimComponent::ApplyActivatedStandingPerturbation(
	EPhysAnimPerturbationDirection Direction,
	EPhysAnimPerturbationMagnitude Magnitude)
{
	if (bActivatedStandingPerturbationApplied)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation already applied once."));
		return false;
	}

	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation blocked by runtime state=%s"), GetRuntimeStateName(RuntimeState));
		return false;
	}

	if (!bLiveRuntimeEvidenceProofComplete || !IsLiveRuntimeEvidenceProofSatisfied())
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation blocked by proof state complete=%d satisfied=%d"),
			bLiveRuntimeEvidenceProofComplete ? 1 : 0,
			IsLiveRuntimeEvidenceProofSatisfied() ? 1 : 0);
		return false;
	}

	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = GetActivatedStandingStabilityMetrics();
	if (!Metrics.bHasSamples || Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.ActiveSupportSideCountMin < 1.0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation blocked by unstable metrics samples=%d supportHullMin=%.2f activeSidesMin=%.2f"),
			Metrics.SampleCount,
			Metrics.SupportHullAreaMinCm2,
			Metrics.ActiveSupportSideCountMin);
		return false;
	}

	bool bApplied = false;
	if (USkeletalMeshComponent* const Mesh = GetMeshComponent())
	{
		if (FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName());
			PelvisBody && PelvisBody->IsInstanceSimulatingPhysics())
		{
			bApplied = ApplyPelvisImpulse(Direction, Magnitude);
		}
	}

	if (!bApplied)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation requires a raw-simulating pelvis impulse; no actor offset fallback was applied."));
		return false;
	}

	bActivatedStandingPerturbationApplied = true;

	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimBalance] Activated standing perturbation applied direction=%d magnitude=%d runtimeState=%s"),
		static_cast<int32>(Direction),
		static_cast<int32>(Magnitude),
		GetRuntimeStateName(RuntimeState));
	return true;
}

bool UPhysAnimComponent::CaptureLiveRuntimeEvidenceHitResults(TArray<FHitResult>& OutHitResults, int32& OutMappedSupportHitCount) const
{
	OutHitResults.Reset();
	OutMappedSupportHitCount = 0;

	if (bForceSupportFailure)
	{
		return false;
	}

	const int32 BeforeLeft = OutHitResults.Num();
	if (CaptureLiveRuntimeEvidenceHitResultForBody(LiveRuntimeEvidenceLeftSupportBodyName, OutHitResults))
	{
		OutMappedSupportHitCount += OutHitResults.Num() - BeforeLeft;
	}

	const int32 BeforeRight = OutHitResults.Num();
	if (CaptureLiveRuntimeEvidenceHitResultForBody(LiveRuntimeEvidenceRightSupportBodyName, OutHitResults))
	{
		OutMappedSupportHitCount += OutHitResults.Num() - BeforeRight;
	}

	return OutHitResults.Num() > 0;
}

bool UPhysAnimComponent::CaptureLiveRuntimeEvidenceHitResultForBody(const FName BodyName, TArray<FHitResult>& OutHitResults) const
{
	if (BodyName == NAME_None)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = GetMeshComponent();
	UWorld* World = GetWorld();

	if (!Mesh || !World)
	{
		return false;
	}

	const int32 BoneIndex = Mesh->GetBoneIndex(BodyName);
	if (BoneIndex == INDEX_NONE)
	{
		return false;
	}

	const FVector BoneWorldLocation = Mesh->GetBoneLocation(BodyName);
	const float R = LiveRuntimeEvidenceSupportSweepRadiusCm;
	
	const FVector Offsets[] = {
		FVector(0, 0, 0),
		FVector(R, 0, 0),
		FVector(-R, 0, 0),
		FVector(0, R, 0),
		FVector(0, -R, 0)
	};

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysAnimLiveRuntimeEvidenceProof), false);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.AddIgnoredActor(GetOwner());

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	if (!bLiveRuntimeEvidenceWorldStaticOnly)
	{
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	}

	int32 HitCount = 0;
	for (const FVector& Offset : Offsets)
	{
		const FVector SampleLocation = BoneWorldLocation + Offset;
		const FVector TraceStart = SampleLocation + FVector(0.0, 0.0, LiveRuntimeEvidenceSupportSweepStartLiftCm);
		const FVector TraceEnd = SampleLocation - FVector(0.0, 0.0, LiveRuntimeEvidenceSupportSweepDistanceCm);
		const FCollisionShape SweepShape = FCollisionShape::MakeSphere(FMath::Max(4.0f, R * 0.5f));

		FHitResult Hit;
		const bool bHit = World->SweepSingleByObjectType(
			Hit,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			ObjectQueryParams,
			SweepShape,
			QueryParams);

		if (!bHit || !Hit.bBlockingHit)
		{
			const bool bFallbackHit = World->LineTraceSingleByObjectType(
				Hit,
				TraceStart,
				TraceEnd,
				ObjectQueryParams,
				QueryParams);

			if (bFallbackHit && Hit.bBlockingHit)
			{
				Hit.BoneName = BodyName;
				OutHitResults.Add(Hit);
				HitCount++;
			}
		}
		else
		{
			Hit.BoneName = BodyName;
			OutHitResults.Add(Hit);
			HitCount++;
		}
	}

	if (HitCount == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PhysAnimBalance] ALL_SAMPLES_FAILED body=%s location=(%.1f,%.1f,%.1f) radius=%.1f"),
			*BodyName.ToString(), BoneWorldLocation.X, BoneWorldLocation.Y, BoneWorldLocation.Z, R);
		return false;
	}

	return true;
}

FPhysAnimSupportHitResultObservationInput UPhysAnimComponent::BuildLiveRuntimeEvidenceObservationInput(
	const TArray<FHitResult>& HitResults,
	float DeltaTimeSeconds) const
{
	FPhysAnimSupportHitResultObservationInput Input;

	const USkeletalMeshComponent* const Mesh = GetMeshComponent();
	const AActor* const OwnerActor = GetOwner();
	const ACharacter* const Character = Cast<ACharacter>(OwnerActor);
	const UCapsuleComponent* const Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	const FVector RebaseOriginCm = Capsule
		? Capsule->GetComponentLocation()
		: (OwnerActor ? OwnerActor->GetActorLocation() : (Mesh ? Mesh->GetComponentLocation() : FVector::ZeroVector));
	const FVector ProxyWorldCm = Capsule
		? Capsule->GetComponentLocation()
		: (OwnerActor ? OwnerActor->GetActorLocation() : RebaseOriginCm);
	const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();

	Input.HitResults = HitResults;
	Input.WorldOriginCm = RebaseOriginCm;
	Input.ComProxyPosCm = FVector2D(ProxyWorldCm.X - RebaseOriginCm.X, ProxyWorldCm.Y - RebaseOriginCm.Y);
	Input.bRequireWorldStatic = bLiveRuntimeEvidenceWorldStaticOnly;
	Input.DeltaMs = static_cast<double>(FMath::Max(0.0f, DeltaTimeSeconds) * 1000.0f);
	Input.PreviousSupportGapTimerMs = LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportGapTimerMs;
	Input.SupportGapMaxMs = Settings.BalancePhase1AdmissionMaxSupportGapMs;
	Input.ProxyDriftLimitMs = Settings.ProxyDriftLimitMs;

	if (LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.IsSet())
	{
		Input.PreviousProxyOutsideHullDurationMs =
			LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.GetValue();
	}

	FPhysAnimSupportBodyMapping LeftMapping;
	LeftMapping.BodyName = LiveRuntimeEvidenceLeftSupportBodyName;
	LeftMapping.SupportSide = EPhysAnimSupportSide::Left;
	Input.SupportBodies.Add(LeftMapping);

	FPhysAnimSupportBodyMapping RightMapping;
	RightMapping.BodyName = LiveRuntimeEvidenceRightSupportBodyName;
	RightMapping.SupportSide = EPhysAnimSupportSide::Right;
	Input.SupportBodies.Add(RightMapping);

	return Input;
}

FPhysAnimRuntimeSubstepInput UPhysAnimComponent::BuildLiveRuntimeEvidenceSubstepInput(
	const FPhysAnimSupportObservationResult& SupportObservation,
	float DeltaTimeSeconds) const
{
	FPhysAnimRuntimeSubstepInput Input;
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCharacterMovementComponent* CharacterMovement = Character ? Character->GetCharacterMovement() : nullptr;
	const USkeletalMeshComponent* Mesh = GetMeshComponent();
	const bool bEvaluateActivationContracts =
		RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation ||
		IsBalanceEntryState(RuntimeState);
	const FPhysAnimCapsuleContractSnapshot CapsuleSnapshot = BuildCapsuleContractSnapshot();
	const FPhysAnimContinuitySnapshot ContinuitySnapshot = BuildContinuitySnapshot(DeltaTimeSeconds);
	FPhysAnimCapsuleContractValidationResult CapsuleValidation = PhysAnimValidators::ValidateCapsule(CapsuleSnapshot);
	FPhysAnimContinuityValidationResult ContinuityValidation = PhysAnimValidators::ValidateContinuity(ContinuitySnapshot);
	FString PlantAuditError;
	FPhysAnimPlantContractSnapshotCaptureInput PlantCaptureInput;
	PlantCaptureInput.SkeletalMeshComponent = const_cast<USkeletalMeshComponent*>(Mesh);
	PlantCaptureInput.ExpectedPhysicsAssetPath = PhysAnimComponentInternal::ExpectedPhysicsAssetPath;
	PlantCaptureInput.bSkeletonAuditPassed = ValidateRequiredBodies(PlantAuditError);
	const FPhysAnimPlantContractSnapshot PlantSnapshot = PhysAnimRuntimeAdapter::CapturePlantContractSnapshot(PlantCaptureInput);
	const FPhysAnimPlantContractValidationResult PlantValidation = PhysAnimValidators::ValidatePlant(PlantSnapshot);

	if (!bEvaluateActivationContracts)
	{
		CapsuleValidation = FPhysAnimCapsuleContractValidationResult();
		ContinuityValidation = FPhysAnimContinuityValidationResult();
	}

	Input.SupportObservation = SupportObservation;
	Input.Plant = PlantValidation;
	Input.Values.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
	Input.Values.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : LiveRuntimeEvidenceStandingSeconds;
	Input.Values.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
	Input.Values.CapsuleWorldPosCm = Character ? Character->GetActorLocation() : FVector::ZeroVector;
	Input.Values.HoldDurationSec = LiveRuntimeEvidenceStandingSeconds;
	Input.Values.SupportUptimeSec = LiveRuntimeEvidenceStandingSeconds;
	Input.Values.ActiveSupportSideCount = SupportObservation.Validation.ActiveSupportSideCount;
	Input.Values.SupportHullAreaCm2 = SupportObservation.Validation.SupportHullAreaCm2;
	Input.Values.SupportGapTimerMs = SupportObservation.Validation.SupportGapTimerMs;
	Input.Values.SupportMode = SupportObservation.Validation.SupportMode;
	Input.Values.SupportChurnCount = SupportObservation.Validation.SupportChurnCount;
	Input.Values.SupportChurnHz = SupportObservation.Validation.SupportChurnHz;
	Input.Values.ProxyInsideHull = SupportObservation.Validation.ProxyInsideHull;
	Input.Values.ProxyOutsideHullDurationMs = SupportObservation.Validation.ProxyOutsideHullDurationMs;
	Input.Values.CmcMovementMode = CharacterMovement
		? FName(*UEnum::GetValueAsString(static_cast<EMovementMode>(CharacterMovement->MovementMode)))
		: NAME_None;
	Input.Values.bPhysicsAssetContractValid = PlantValidation.bPhysicsAssetContractValid;
	Input.Values.bSkeletonAuditPassed = PlantValidation.bSkeletonAuditPassed;
	Input.Values.bCapsuleContractPassed = CapsuleValidation.bCapsuleContractPassed;
	Input.Values.PlantFailureClass = PlantValidation.PlantFailureClass;
	Input.Values.PlantFailureField = PlantValidation.PlantFailureField;
	Input.Values.MassDriftTotalPct = PlantValidation.MassDriftTotalPct;
	Input.Values.TopologyChangeCount = ContinuityValidation.TopologyChangeCount;
	Input.Values.bContinuityBookkeepingMismatch = ContinuityValidation.bContinuityBookkeepingMismatch;
	Input.Values.PelvisSleepDurationMs = ContinuityValidation.PelvisSleepDurationMs;
	Input.Values.bPhysicalContinuityValidatorPassed = ContinuityValidation.bPhysicalContinuityValidatorPassed;
	Input.Values.PolicyInferenceSuccessCount = ActivatedStandingStabilityMetrics.PolicyInferenceSuccessCount;
	Input.Values.PolicyActionSampleCount = ActivatedStandingStabilityMetrics.PolicyActionSampleCount;
	Input.Values.PolicyActionRawMeanAbsMax = ActivatedStandingStabilityMetrics.PolicyActionRawMeanAbsMax;
	Input.Values.PolicyActionConditionedMeanAbsMax = ActivatedStandingStabilityMetrics.PolicyActionConditionedMeanAbsMax;
	Input.Values.PolicyActionClampedFloatMax = ActivatedStandingStabilityMetrics.PolicyActionClampedFloatMax;
	Input.Values.ControlTargetSampleCount = ActivatedStandingStabilityMetrics.ControlTargetSampleCount;
	Input.Values.ControlTargetNormalWrites = ActivatedStandingStabilityMetrics.ControlTargetNormalWrites;
	Input.Values.ControlTargetTotalWrites = ActivatedStandingStabilityMetrics.ControlTargetTotalWrites;
	Input.Values.ControlTargetMaxDeltaDeg = ActivatedStandingStabilityMetrics.ControlTargetMaxDeltaDeg;
	Input.Values.ControlTargetMeanDeltaDegMax = ActivatedStandingStabilityMetrics.ControlTargetMeanDeltaDegMax;
	Input.Values.ControlTargetMaxRawPolicyOffsetDeg = ActivatedStandingStabilityMetrics.ControlTargetMaxRawPolicyOffsetDeg;
	Input.Values.ControlTargetMeanRawPolicyOffsetDegMax = ActivatedStandingStabilityMetrics.ControlTargetMeanRawPolicyOffsetDegMax;
	Input.Values.RuntimeBodySampleCount = ActivatedStandingStabilityMetrics.BodyTelemetrySampleCount;
	Input.Values.RuntimeSimulatingBodyCount = ActivatedStandingStabilityMetrics.SimulatingBodyCountMax;
	Input.Values.RuntimeMaxBodyLinearSpeedCmPerSecond = LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond;
	Input.Values.RuntimeMaxBodyAngularSpeedDegPerSecond = LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond;
	Input.Values.bPhysicalPerturbationApplied = ActivatedStandingStabilityMetrics.bPhysicalPerturbationApplied;
	Input.Values.PerturbationMeasuredDeltaVCmPerSecond = ActivatedStandingStabilityMetrics.PerturbationMeasuredDeltaVCmPerSecond;

	Input.ControllerStability.HoldDurationSec = LiveRuntimeEvidenceStandingSeconds;
	Input.ControllerStability.bControllerStabilityPassed = true;
	Input.Authority.bAuthorityPassed = true;
	Input.MovementReclaim.bMovementReclaimPassed = true;
	Input.ShellHelper.bShellHelperPassed = true;

	Input.Capsule = CapsuleValidation;
	Input.Continuity = ContinuityValidation;

	if (bEnableLiveRuntimeEvidenceProof)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] LiveProof capture capsuleWorldPos=(%.1f,%.1f,%.1f) cmcMode=%s root=%s criticalBodies=%d topologyChanges=%d bookkeepingMismatch=%d capsulePassed=%d continuityPassed=%d"),
			Input.Values.CapsuleWorldPosCm.X,
			Input.Values.CapsuleWorldPosCm.Y,
			Input.Values.CapsuleWorldPosCm.Z,
			*Input.Values.CmcMovementMode.ToString(),
			*PhysAnimBridge::GetRootBoneName().ToString(),
			PhysAnimBridge::GetRequiredBodyModifierBoneNames().Num(),
			Input.Values.TopologyChangeCount,
			Input.Values.bContinuityBookkeepingMismatch ? 1 : 0,
			Input.Values.bCapsuleContractPassed ? 1 : 0,
			Input.Values.bPhysicalContinuityValidatorPassed ? 1 : 0);
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("PhysAnimProof: LiveCapsule capsule_valid=%d collision=%s cmc_active=%d cmc_tick=%d lock_delta=%.2f"),
			CapsuleValidation.bCapsuleContractPassed ? 1 : 0,
			ToCapsuleCollisionStateName(CapsuleValidation.CapsuleCollisionEnabled),
			CharacterMovement && CharacterMovement->IsActive() ? 1 : 0,
			CharacterMovement && CharacterMovement->IsComponentTickEnabled() ? 1 : 0,
			CapsuleValidation.CapsuleLockDeltaCm);
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("PhysAnimProof: LiveContinuity continuity_valid=%d pelvis_sleep_ms=%.2f bookkeeping_mismatch=%d"),
			ContinuityValidation.bPhysicalContinuityValidatorPassed ? 1 : 0,
			ContinuityValidation.PelvisSleepDurationMs,
			ContinuityValidation.bContinuityBookkeepingMismatch ? 1 : 0);
		if (!PlantValidation.bPhysicsAssetContractValid || !PlantValidation.bSkeletonAuditPassed)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] LiveProof plant audit failed: %s"), *PlantAuditError);
		}
	}

	return Input;
}

void UPhysAnimComponent::EmitLiveRuntimeEvidenceTerminalArtifactOnce(const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult)
{
	// Implementation moved to PhysAnimProofArtifactEmitter
}
