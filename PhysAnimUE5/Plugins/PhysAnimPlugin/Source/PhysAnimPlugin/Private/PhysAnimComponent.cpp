#include "PhysAnimComponent.h"
#include "PhysAnimStage1InitializerComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimLogger.h"
#include "PhysAnimProofArtifactEmitter.h"
#include "PhysAnimRuntimeAdapter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY(LogPhysAnimBridge);

namespace
{
	static constexpr float Stage2AForwardWalkSpeedCmPerSecond = 60.0f;
	static constexpr float Stage2AMaxWalkDeltaPerTickCm = 6.0f;
	static constexpr int32 Stage2AMinPolicyActiveFramesBeforeWalk = 3;
	static constexpr const TCHAR* Stage2AWalkIntentName = TEXT("WalkForward");
	static constexpr const TCHAR* Stage2AMotionSourceName = TEXT("Stage2A_KinematicShell");
	static constexpr float Stage2APolicyLivenessTimeoutSeconds = 0.1f;

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

	bool TryCalculateRawSimBodyComWorldCm(const USkeletalMeshComponent& Mesh, FVector& OutComWorldCm)
	{
		const FName V0BodyNames[] =
		{
			PhysAnimBridge::GetRootBoneName(),
			TEXT("spine_01"),
			TEXT("spine_02"),
			TEXT("spine_03"),
			TEXT("thigh_l"),
			TEXT("thigh_r"),
			TEXT("foot_l"),
			TEXT("foot_r"),
			TEXT("ball_l"),
			TEXT("ball_r")
		};

		FVector WeightedPositionSum = FVector::ZeroVector;
		double TotalMassKg = 0.0;
		for (const FName& BodyName : V0BodyNames)
		{
			const FBodyInstance* const BodyInstance = Mesh.GetBodyInstance(BodyName);
			if (!BodyInstance || !BodyInstance->IsValidBodyInstance() || !BodyInstance->IsInstanceSimulatingPhysics())
			{
				continue;
			}

			const double BodyMassKg = FMath::Max(static_cast<double>(BodyInstance->GetBodyMass()), UE_SMALL_NUMBER);
			WeightedPositionSum += BodyInstance->GetUnrealWorldTransform().GetLocation() * BodyMassKg;
			TotalMassKg += BodyMassKg;
		}

		if (TotalMassKg <= 0.0)
		{
			return false;
		}

		OutComWorldCm = WeightedPositionSum / TotalMassKg;
		return true;
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


bool UPhysAnimComponent::IsStage1() const
{
	const AActor* const OwnerActor = GetOwner();
	return OwnerActor ? (OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>() != nullptr) : false;
}

UPhysAnimComponent::UPhysAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	ModelDataAsset = TSoftObjectPtr<UNNEModelData>(FSoftObjectPath(PhysAnimComponentInternal::DefaultModelPath));
	GenerateNewAttemptUuid();
	bEnableLiveRuntimeEvidenceProof = false;
	bV0PlantThighWorkDiagnosticEnabled = true;
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
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] ENTRY_ALLOWED reason=PROOF_DISABLED_BYPASS"));
		return true;
	}

	const bool bIsTransitioningToStanding = (RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle);

	if (!bIsTransitioningToStanding)
	{
		if (!bLiveRuntimeEvidenceProofComplete)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROOF_NOT_COMPLETE"));
			return false;
		}

		if (LiveRuntimeEvidenceTerminationState.bTerminated &&
			LiveRuntimeEvidenceTerminationState.TerminalReason != EPhysAnimTerminalReason::None)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=TERMINATED_IN_PROOF terminal_reason=%d"), 
				static_cast<int32>(LiveRuntimeEvidenceTerminationState.TerminalReason));
			return false;
		}

		const bool bProofSatisfied = IsLiveRuntimeEvidenceProofSatisfied();
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
			TEXT("[PhysAnim] Startup proof satisfaction evaluated satisfied=%d state=%s fresh=%d observed=%d deferredReason=%d enforceArmed=%d"),
			bProofSatisfied ? 1 : 0,
			GetRuntimeStateName(RuntimeState),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
			static_cast<int32>(StartupProofDeferredTerminalReason),
			bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0);

		if (!bProofSatisfied)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Warning,
				1.0f,
				TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROOF_NOT_TRUTHFUL startupObserved=%d startupArmed=%d startupArmSubstep=%lld startupDeferredReason=%d proxyHandoffArmed=%d proofComplete=%d"),
				bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
				bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0,
				static_cast<long long>(StartupProofVerificationHandoffArmedSubstep),
				static_cast<int32>(StartupProofDeferredTerminalReason),
				bLiveRuntimeEvidenceStartupProxySupportHandoffArmed ? 1 : 0,
				bLiveRuntimeEvidenceProofComplete ? 1 : 0);
			return false;
		}
	}

	const FPhysAnimRunArtifactSnapshot& Latest = LiveRuntimeEvidenceTerminationState.LatestArtifact;

	if (Latest.SupportMode == EPhysAnimSupportMode::Airborne)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=AIRBORNE_MODE"));
		return false;
	}

	if (Latest.ActiveSupportSideCount == 0)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=NO_SUPPORT_SIDES"));
		return false;
	}

	if (Latest.SupportHullAreaCm2 <= 0.0f)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=NO_SUPPORT_AREA"));
		return false;
	}

	const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();
	if (IsStage1())
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] ENTRY_ALLOWED reason=STAGE1_BYPASS"));
		return true;
	}

	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive && !bIsTransitioningToStanding && !IsBringUpGroupUnlocked(GetBringUpGroupCount() - 1))
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=BRING_UP_INCOMPLETE"));
		return false;
	}

	const bool bDeferredStartupProxyTerminalCurrentAttempt =
		IsDeferredStartupProxyTerminalStillCurrent(LiveRuntimeEvidenceTerminationState, LiveRuntimeEvidenceAttemptUuid);
	const bool bProxyOutsideHullDeferred =
		bLiveRuntimeEvidenceProofComplete &&
		(!bLiveRuntimeEvidenceStartupProxySupportHandoffArmed || bDeferredStartupProxyTerminalCurrentAttempt);

	if (Latest.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=SUPPORT_GAP gap=%.1f threshold=%.1f"), 
			Latest.SupportGapTimerMs, Settings.BalancePhase1AdmissionMaxSupportGapMs);
		return false;
	}

	if (Latest.ProxyInsideHull.IsSet() && !Latest.ProxyInsideHull.GetValue())
	{
		if (bProxyOutsideHullDeferred)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
				TEXT("[PhysAnim] Proxy outside hull deferred during standing entry state=%s"),
				GetRuntimeStateName(RuntimeState));
		}
		else
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
				TEXT("[PhysAnim] Proxy outside hull enforced after handoff armed state=%s"),
				GetRuntimeStateName(RuntimeState));
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROXY_OUTSIDE_HULL"));
			return false;
		}
	}

	if (Latest.ProxyOutsideHullDurationMs.IsSet() && Latest.ProxyOutsideHullDurationMs.GetValue() >= Settings.ProxyDriftLimitMs)
	{
		if (bProxyOutsideHullDeferred)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
				TEXT("[PhysAnim] Proxy outside hull deferred during standing entry state=%s"),
				GetRuntimeStateName(RuntimeState));
		}
		else
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
				TEXT("[PhysAnim] Proxy outside hull enforced after handoff armed state=%s"),
				GetRuntimeStateName(RuntimeState));
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROXY_DRIFT_THRESHOLD drift=%.1f threshold=%.1f"), 
				Latest.ProxyOutsideHullDurationMs.GetValue(), Settings.ProxyDriftLimitMs);
			return false;
		}
	}

	if (!Latest.bCapsuleContractPassed)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=CAPSULE_CONTRACT_FAILED"));
		return false;
	}

	if (!Latest.bPhysicalContinuityValidatorPassed)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=CONTINUITY_CONTRACT_FAILED"));
		return false;
	}

	if (Latest.bContinuityBookkeepingMismatch)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=CONTINUITY_BOOKKEEPING_MISMATCH"));
		return false;
	}

	if (!Latest.bPhysicsAssetContractValid || !Latest.bSkeletonAuditPassed)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=AUDIT_STATE_INCONSISTENT physicsAsset=%d skeletonAudit=%d"),
			Latest.bPhysicsAssetContractValid ? 1 : 0,
			Latest.bSkeletonAuditPassed ? 1 : 0);
		return false;
	}

	if (!bIsTransitioningToStanding)
	{
		if (!HasConsistentLiveRuntimeEvidenceArtifact(LiveRuntimeEvidenceTerminationState))
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=ARTIFACT_STATE_INCONSISTENT"));
			return false;
		}

		if (LiveRuntimeEvidenceStandingSeconds < LiveRuntimeEvidenceStandingTargetSeconds)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=INSUFFICIENT_STANDING_DURATION duration=%.3f target=%.3f"), 
				LiveRuntimeEvidenceStandingSeconds, LiveRuntimeEvidenceStandingTargetSeconds);
			return false;
		}
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
		PHYSANIM_LOG(
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
		PHYSANIM_LOG(
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
		!IsStage1() &&
		Latest.ProxyOutsideHullDurationMs.IsSet() &&
		Latest.ProxyOutsideHullDurationMs.GetValue() >= Settings.ProxyDriftLimitMs;
	if (bProxyOutsideHull)
	{
		if (!bLiveRuntimeEvidenceStartupProxySupportHandoffArmed || bDeferredStartupProxyTerminalCurrentAttempt)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
				TEXT("[PhysAnim] Proxy outside hull deferred during standing entry state=%s"),
				GetRuntimeStateName(RuntimeState));
			return false;
		}

		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
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
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] STANDING_ACTIVE_EVAL result=FailStopped reason=TERMINATED_IN_LOOP terminal_reason=%d"), 
			static_cast<int32>(LiveRuntimeEvidenceTerminationState.TerminalReason));
		return EPhysAnimRuntimeState::FailStopped;
	}

	if (ShouldExitStandingToSafeDeny(LiveRuntimeEvidenceTerminationState))
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] STANDING_ACTIVE_EVAL result=BalanceSafeDeny reason=PHASE3_ACTIVE_SUPPORT_FAILURE hull_area=%.1f gap=%.1f proxy_inside=%d proxy_drift=%.1f"),
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
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
			TEXT("[PhysAnim] Startup proof satisfaction evaluated satisfied=0 state=%s fresh=%d observed=%d deferredReason=%d enforceArmed=%d reason=proof_incomplete_or_terminated"),
			GetRuntimeStateName(RuntimeState),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
			static_cast<int32>(StartupProofDeferredTerminalReason),
			bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0);
		return false;
	}

	const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();

	const FPhysAnimRunArtifactSnapshot& SatisfactionArtifact =
		SelectStartupProofSatisfactionArtifact(LiveRuntimeEvidenceTerminationState);

	if (!SatisfactionArtifact.bPhysicsAssetContractValid ||
		!SatisfactionArtifact.bSkeletonAuditPassed ||
		!SatisfactionArtifact.bCapsuleContractPassed ||
		!HasStartupProofPhysicalContinuityEvidence(SatisfactionArtifact) ||
		SatisfactionArtifact.bContinuityBookkeepingMismatch)
	{
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
			TEXT("[PhysAnim] Startup proof satisfaction evaluated satisfied=0 state=%s fresh=%d observed=%d deferredReason=%d enforceArmed=%d reason=artifact_contract_invalid"),
			GetRuntimeStateName(RuntimeState),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved ? 1 : 0,
			static_cast<int32>(StartupProofDeferredTerminalReason),
			bLiveRuntimeEvidenceStartupVerificationHandoffArmed ? 1 : 0);
		return false;
	}

	if (SatisfactionArtifact.SupportMode == EPhysAnimSupportMode::Airborne ||
		SatisfactionArtifact.ActiveSupportSideCount <= 0 ||
		SatisfactionArtifact.SupportHullAreaCm2 <= 0.0f ||
		SatisfactionArtifact.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
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

const FPhysAnimRunArtifactSnapshot& UPhysAnimComponent::SelectStartupProofSatisfactionArtifact(
	const FPhysAnimRuntimeTerminationState& State)
{
	const FPhysAnimRunArtifactSnapshot& Terminal = State.TerminalArtifact;
	const FPhysAnimRunArtifactSnapshot& Latest = State.LatestArtifact;
	const bool bUseTerminalArtifact =
		!State.bTerminated &&
		State.TerminalReason == EPhysAnimTerminalReason::None &&
		!Terminal.AttemptUuid.IsEmpty() &&
		Terminal.AttemptUuid == Latest.AttemptUuid &&
		Terminal.TerminalReason == EPhysAnimTerminalReason::None &&
		Terminal.bTerminalFrameArtifactCaptured;

	return bUseTerminalArtifact ? Terminal : Latest;
}

bool UPhysAnimComponent::IsStartupProofSatisfactionStateAccepted(
	const FPhysAnimRuntimeTerminationState& State,
	const FPhysAnimStabilizationSettings& Settings)
{
	if (State.TerminalReason != EPhysAnimTerminalReason::None)
	{
		return false;
	}

	const FPhysAnimRunArtifactSnapshot& SatisfactionArtifact =
		SelectStartupProofSatisfactionArtifact(State);

	if (!SatisfactionArtifact.bPhysicsAssetContractValid ||
		!SatisfactionArtifact.bSkeletonAuditPassed ||
		!SatisfactionArtifact.bCapsuleContractPassed ||
		!HasStartupProofPhysicalContinuityEvidence(SatisfactionArtifact) ||
		SatisfactionArtifact.bContinuityBookkeepingMismatch)
	{
		return false;
	}

	if (SatisfactionArtifact.SupportMode == EPhysAnimSupportMode::Airborne ||
		SatisfactionArtifact.ActiveSupportSideCount <= 0 ||
		SatisfactionArtifact.SupportHullAreaCm2 <= 0.0f ||
		SatisfactionArtifact.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		return false;
	}

	return HasConsistentLiveRuntimeEvidenceArtifact(State);
}

#if !UE_BUILD_SHIPPING
bool UPhysAnimComponent::TestOnlyIsStartupProofSatisfactionStateAccepted(
	const FPhysAnimRuntimeTerminationState& State,
	const FPhysAnimStabilizationSettings& Settings)
{
	return IsStartupProofSatisfactionStateAccepted(State, Settings);
}
#endif

bool UPhysAnimComponent::HasStartupProofPhysicalContinuityEvidence(const FPhysAnimRunArtifactSnapshot& Artifact)
{
	return Artifact.bPhysicalContinuityValidatorPassed;
}

EPhysAnimTerminalReason UPhysAnimComponent::ResolveStartupPhysicalContinuityTerminalReason(
	const EPhysAnimRuntimeState RuntimeState,
	const FPhysAnimRunArtifactSnapshot& Artifact)
{
	if (!HasStartupProofPhysicalContinuityEvidence(Artifact) &&
		Artifact.RuntimeSimulatingBodyCount <= 0 &&
		(RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
			RuntimeState == EPhysAnimRuntimeState::ReadyForActivation))
	{
		return EPhysAnimTerminalReason::ActivationContinuousSimulationLost;
	}

	return EPhysAnimTerminalReason::ActivationContinuousSimulationLost;
}

bool UPhysAnimComponent::ShouldStartBridgePhysicsOwnershipForStartupProof(
	const EPhysAnimRuntimeState RuntimeState,
	const bool bPoseSearchValid,
	const bool bLiveProofEnabled,
	const bool bProofComplete,
	const bool bProofSatisfied,
	const bool bProofTerminated)
{
	return RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch &&
		bPoseSearchValid &&
		bLiveProofEnabled &&
		!bProofTerminated &&
		!bProofSatisfied &&
		!bProofComplete;
}


void UPhysAnimComponent::ResetLiveRuntimeEvidenceProof()
{
	bLiveRuntimeEvidenceProofActive = false;
	bLiveRuntimeEvidenceProofComplete = false;
	bLiveRuntimeEvidenceTerminalArtifactEmitted = false;
	bLiveRuntimeEvidenceProofShouldComplete = true;
	PhysAnimComponentInternal::ClearPhysicsTuningDiagnosticCaches(this);

	GenerateNewAttemptUuid();

	LiveRuntimeEvidenceStandingSeconds = 0.0f;
	ProductGateStandingWindow = FPhysAnimStandingWindowAccumulator();
	ProductGateBodyContinuity = FPhysAnimBodyContinuityAccumulator();
	LiveRuntimeEvidenceLastProgressLogSeconds = -1.0f;
	ActivatedStandingStabilityMetrics.ResetForNewAttempt();
	RecentInferenceSuccessRatio = 1.0f;
	RecentActionMagnitudeHistory.Reset();
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

FString UPhysAnimComponent::GenerateNewAttemptUuid()
{
	LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	FPhysAnimLogger::SetCurrentAttemptUuid(LiveRuntimeEvidenceAttemptUuid);
	return LiveRuntimeEvidenceAttemptUuid;
}

void UPhysAnimComponent::RecordLiveRuntimeEvidencePoseSearchQueryResult(const bool bPoseSearchValid, const FString& SelectedAnimName, double SelectedTime)
{
	++ActivatedStandingStabilityMetrics.PoseSearchQueryCount;
	if (bPoseSearchValid)
	{
		++ActivatedStandingStabilityMetrics.PoseSearchValidResultCount;
		ActivatedStandingStabilityMetrics.PoseSearchConsecutiveInvalidFrameCount = 0;
		if (!SelectedAnimName.IsEmpty())
		{
			ActivatedStandingStabilityMetrics.PoseSearchSelectedAnimationName = SelectedAnimName;
			ActivatedStandingStabilityMetrics.PoseSearchSelectedTime = SelectedTime;
		}
	}
	else
	{
		++ActivatedStandingStabilityMetrics.PoseSearchConsecutiveInvalidFrameCount;
	}
}

void UPhysAnimComponent::RecordLiveRuntimeEvidenceRendererFacingMotionSample(
	const bool bRendererFacingMotionActive,
	const double RootWorldPositionDriftCm)
{
	if (!bEnableLiveRuntimeEvidenceProof)
	{
		return;
	}

	++ActivatedStandingStabilityMetrics.RendererFacingMotionSampleCount;
	if (bRendererFacingMotionActive)
	{
		++ActivatedStandingStabilityMetrics.RendererFacingMotionActiveSampleCount;
	}

	ActivatedStandingStabilityMetrics.RendererFacingMotionMaxRootWorldPositionDriftCm = FMath::Max(
		ActivatedStandingStabilityMetrics.RendererFacingMotionMaxRootWorldPositionDriftCm,
		RootWorldPositionDriftCm);
}

void UPhysAnimComponent::TickLiveRuntimeEvidenceProof(float DeltaTimeSeconds)
{
	const bool bTerminalFailureLatched =
		LiveRuntimeEvidenceTerminationState.bTerminated &&
		LiveRuntimeEvidenceTerminationState.TerminalReason != EPhysAnimTerminalReason::None;
	const bool bStandingMeasurementActive =
		bEnableLiveRuntimeEvidenceProof &&
		!bTerminalFailureLatched &&
		RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
	const bool bEnteringStandingMeasurement =
		bStandingMeasurementActive && !ProductGateStandingWindow.bWasStanding;
	if (bEnteringStandingMeasurement)
	{
		ActivatedStandingStabilityMetrics.ResetForNewAttempt();
		ProductGateBodyContinuity = FPhysAnimBodyContinuityAccumulator();
		bActivatedStandingStabilityBaselineInitialized = false;
		ActivatedStandingStabilityBaselineRootLocationCm = FVector::ZeroVector;
		ActivatedStandingStabilityBaselineRootTiltDeg = 0.0f;
		ActivatedStandingStabilitySupportHullAreaSumCm2 = 0.0;
		ActivatedStandingStabilityActiveSupportSideCountSum = 0.0;
	}
	ProductGateStandingWindow.Advance(bStandingMeasurementActive, DeltaTimeSeconds);
	LiveRuntimeEvidenceStandingSeconds = static_cast<float>(ProductGateStandingWindow.ContinuousSeconds);
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
		RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
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
		GenerateNewAttemptUuid();
		PhysAnimProofArtifactEmitter::LogAttemptStart(LiveRuntimeEvidenceAttemptUuid);
	}
	const auto PopulateProductGateMetadata = [this](FPhysAnimProofArtifactEmitInput& EmitInput)
	{
		FParse::Value(FCommandLine::Get(), TEXT("PhysAnimGateNonce="), EmitInput.AttemptNonce);
		FParse::Value(FCommandLine::Get(), TEXT("PhysAnimSourceCommit="), EmitInput.SourceCommit);
		int32 SourceTreeDirty = 1;
		FParse::Value(FCommandLine::Get(), TEXT("PhysAnimSourceTreeDirty="), SourceTreeDirty);
		EmitInput.bSourceTreeDirty = SourceTreeDirty != 0;
		EmitInput.FinalRuntimeOutcome = GetRuntimeStateName(RuntimeState);
		EmitInput.StandingWindowSampleCount = ProductGateStandingWindow.SampleCount;
		EmitInput.StandingWindowMaxDeltaSec = ProductGateStandingWindow.MaxDeltaSeconds;
	};
	bLiveRuntimeEvidenceProofActive = true;
	// Reset the policy ramp only in the pre-activation proof window (before physics ownership
	// is taken). Once BridgeActive (or beyond), ActivateBridgeFromReadyState has legitimately
	// started the ramp and we must NOT clobber it — doing so permanently blocks the
	// BalanceReadyTransition preflight gate. (S2-FIX-BALANCE-STARTUP-TICK-RACE-01)
	const bool bPreActivationProofState =
		RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation;
	if (bPreActivationProofState && bEnableLiveRuntimeEvidenceProof && !bLiveRuntimeEvidenceProofComplete)
	{
		PolicyInfluenceRampStartTimeSeconds = -1.0;
	}
	if (LiveRuntimeEvidenceSubstepCounter == 0)
	{
		bLiveRuntimeEvidenceStartupEvidenceFresh = true;
		if (RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch)
		{
			if (!bLiveRuntimeEvidenceStartupWaitingForPoseSearchObserved)
			{
				NoteStartupProofWaitingForPoseSearchObserved();
			}
		}

		const bool bStartupEntryBridgeSatisfied = IsLiveRuntimeEvidenceProofSatisfied();
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Startup entry bridge evidence fresh=%d satisfied=%d state=%s"),
			bLiveRuntimeEvidenceStartupEvidenceFresh ? 1 : 0,
			bStartupEntryBridgeSatisfied ? 1 : 0,
			GetRuntimeStateName(RuntimeState));
	}

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
			RuntimeState == EPhysAnimRuntimeState::ReadyForActivation ||
			((RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
			  RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
			  RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
			  RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
			  RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle) &&
				bEnableLiveRuntimeEvidenceProof &&
				!bLiveRuntimeEvidenceProofComplete);
		const bool bStartupProxySupportHandoffDeferred =
			ShouldDeferStartupProxyTerminalForProof(
				RuntimeState,
				bEnableLiveRuntimeEvidenceProof,
				bLiveRuntimeEvidenceProofComplete,
				bLiveRuntimeEvidenceStartupProxySupportHandoffArmed,
				LiveRuntimeEvidenceTerminationState.TerminalReason);
		if (bStartupProxySupportHandoffDeferred)
		{
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
				TEXT("[PhysAnim] Startup proof deferred terminal reason=ActivationProxyOutsideSupportRegion phase=%s"),
				GetRuntimeStateName(RuntimeState));
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
				TEXT("[PhysAnim] Startup proxy terminal recorded reason=PROXY_OUTSIDE_HULL state=%s"),
				GetRuntimeStateName(RuntimeState));
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
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
			PHYSANIM_LOG_RATE_LIMITED(
				LogPhysAnimBridge,
				Verbose,
				1.0f,
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
			PHYSANIM_LOG(LogPhysAnimBridge, Verbose, TEXT("[PhysAnimProof] Startup support evidence sample ignored before activation proof was initialized or refreshed."));
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

			FPhysAnimRunArtifactSnapshot FailureArtifact = PipelineResult.SubstepResult.Artifact;
			FailureArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			FailureArtifact.bTerminalFrameArtifactCaptured = true;

			LiveRuntimeEvidenceTerminationState.bTerminated = true;
			LiveRuntimeEvidenceTerminationState.TerminalReason = FailureArtifact.TerminalReason;
			LiveRuntimeEvidenceTerminationState.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			LiveRuntimeEvidenceTerminationState.bTerminalFrameArtifactCaptured = true;
			LiveRuntimeEvidenceTerminationState.TerminalArtifact = FailureArtifact;
			LiveRuntimeEvidenceTerminationState.LatestArtifact = FailureArtifact;

			FPhysAnimProofArtifactEmitInput EmitInput;
			EmitInput.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
			EmitInput.StandingSeconds = LiveRuntimeEvidenceStandingSeconds;
			EmitInput.RuntimeHitCount = HitResults.Num();
			EmitInput.MappedSupportHitCount = MappedSupportHitCount;
			EmitInput.PipelineResult = PipelineResult;
			EmitInput.PipelineResult.StateApplyResult.State = LiveRuntimeEvidenceTerminationState;
			PopulateProductGateMetadata(EmitInput);

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
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Standing entry accepted proxy handoff arming pending state=%s"),
			GetRuntimeStateName(RuntimeState));

		PHYSANIM_LOG(
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
			PHYSANIM_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Standing entry accepted proxy handoff arming pending state=%s"),
				GetRuntimeStateName(RuntimeState));

			PHYSANIM_LOG(
				LogPhysAnimBridge,
				Verbose,
				TEXT("[PhysAnim] Proxy handoff armed state=%s"),
				GetRuntimeStateName(RuntimeState));
		}
		else if (bStandingSupportFresh && bStandingProxyOutsideHull)
		{
			PHYSANIM_LOG(
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
		FPhysAnimRunArtifactSnapshot CompletionArtifact = PipelineResult.SubstepResult.Artifact;
		if (!HasStartupProofPhysicalContinuityEvidence(CompletionArtifact))
		{
			bLiveRuntimeEvidenceProofComplete = true;
			CompletionArtifact.TerminalReason =
				ResolveStartupPhysicalContinuityTerminalReason(RuntimeState, CompletionArtifact);
			CompletionArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
			CompletionArtifact.bTerminalFrameArtifactCaptured = true;
			CompletionArtifact.bPhysicalContinuityValidatorPassed = false;

			LiveRuntimeEvidenceTerminationState.bTerminated = true;
			LiveRuntimeEvidenceTerminationState.TerminalReason = CompletionArtifact.TerminalReason;
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
			EmitInput.PipelineResult.StateApplyResult.State = LiveRuntimeEvidenceTerminationState;
			PopulateProductGateMetadata(EmitInput);

			PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(EmitInput);
			bLiveRuntimeEvidenceTerminalArtifactEmitted = true;

			PhysAnimProofArtifactEmitter::LogAttemptResult(
				LiveRuntimeEvidenceAttemptUuid,
				false,
				LiveRuntimeEvidenceStandingSeconds,
				CompletionArtifact.TerminalReason);

			PHYSANIM_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnim] Startup proof rejected without physical continuity evidence state=%s simBodies=%d continuity=%d"),
				GetRuntimeStateName(RuntimeState),
				CompletionArtifact.RuntimeSimulatingBodyCount,
				CompletionArtifact.bPhysicalContinuityValidatorPassed ? 1 : 0);
			return;
		}

		bLiveRuntimeEvidenceProofComplete = true;
		PHYSANIM_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] Active-standing measurement window complete state=%s"),
			GetRuntimeStateName(RuntimeState));

		CompletionArtifact.TerminalReason = EPhysAnimTerminalReason::None;
		CompletionArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
		CompletionArtifact.bTerminalFrameArtifactCaptured = true;

		LiveRuntimeEvidenceTerminationState.bTerminated = false;
		LiveRuntimeEvidenceTerminationState.TerminalReason = EPhysAnimTerminalReason::None;
		LiveRuntimeEvidenceTerminationState.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
		LiveRuntimeEvidenceTerminationState.bTerminalFrameArtifactCaptured = true;
		LiveRuntimeEvidenceTerminationState.TerminalArtifact = CompletionArtifact;
		LiveRuntimeEvidenceTerminationState.LatestArtifact = CompletionArtifact;
		
		PhysAnimProofArtifactEmitter::LogAttemptResult(
			LiveRuntimeEvidenceAttemptUuid,
			true,
			LiveRuntimeEvidenceStandingSeconds,
			EPhysAnimTerminalReason::None);

	}

	// Emit the terminal artifact for Stage 2A after a 0.3s metric gathering window
	if (bLiveRuntimeEvidenceProofComplete && 
		!bLiveRuntimeEvidenceTerminalArtifactEmitted &&
		LiveRuntimeEvidenceStandingSeconds >= (LiveRuntimeEvidenceStandingTargetSeconds + 0.35f))
	{
		FPhysAnimRunArtifactSnapshot CompletionArtifact = PipelineResult.SubstepResult.Artifact;
		CompletionArtifact.TerminalReason = EPhysAnimTerminalReason::None;
		CompletionArtifact.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
		CompletionArtifact.bTerminalFrameArtifactCaptured = true;

		LiveRuntimeEvidenceTerminationState.TerminalArtifact = CompletionArtifact;
		LiveRuntimeEvidenceTerminationState.LatestArtifact = CompletionArtifact;

		FPhysAnimProofArtifactEmitInput EmitInput;
		EmitInput.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
		EmitInput.StandingSeconds = LiveRuntimeEvidenceStandingSeconds;
		EmitInput.RuntimeHitCount = HitResults.Num();
		EmitInput.MappedSupportHitCount = MappedSupportHitCount;
		EmitInput.PipelineResult = PipelineResult;
		EmitInput.PipelineResult.StateApplyResult.State = LiveRuntimeEvidenceTerminationState;
		PopulateProductGateMetadata(EmitInput);

		PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(EmitInput);
		bLiveRuntimeEvidenceTerminalArtifactEmitted = true;
	}
}

void UPhysAnimComponent::UpdateActivatedStandingStabilityMetrics(float DeltaTimeSeconds)
{
	{
		const USkeletalMeshComponent* const Mesh = MeshComponent.Get();
		const FBodyInstance* const PelvisBody = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
		const bool bPelvisSim = PelvisBody ? PelvisBody->IsInstanceSimulatingPhysics() : false;
		const bool bPelvisAwake = PelvisBody ? PelvisBody->IsInstanceAwake() : false;
		PHYSANIM_LOG_RATE_LIMITED(LogTemp, Verbose, 1.0f, TEXT("[PhysAnimV0] METRICS_TICK state=%s t=%.3f PelvisSim=%d PelvisAwake=%d"), GetRuntimeStateName(RuntimeState), ActivatedStandingStabilityMetrics.ActivationDurationSec, bPelvisSim ? 1 : 0, bPelvisAwake ? 1 : 0);
	}
	if (!bV0PlantThighWorkDiagnosticEnabled &&
		!((IsBalanceActiveState(RuntimeState) || IsBalanceEntryState(RuntimeState)) && bLiveRuntimeEvidenceProofActive) &&
		!(bEnableLiveRuntimeEvidenceProof && RuntimeStateOwnsBridgePhysics(RuntimeState) && bLiveRuntimeEvidenceProofActive))
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
	const float CurrentRootTiltDeg = ResolvePhase1Uprightness(
		Mesh, OwnerActor, PhysAnimBridge::GetRootBoneName(),
		bHasNeutralPelvisActorRelativeRotation, NeutralPelvisActorRelativeRotation, TiltSource);
	const FVector CurrentRootLocationCm = LastRuntimeInstabilityDiagnostics.RootLocationCm;
	const double CurrentSupportHullAreaCm2 = LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportHullAreaCm2;
	const double CurrentActiveSupportSideCount = static_cast<double>(LiveRuntimeEvidenceTerminationState.LatestArtifact.ActiveSupportSideCount);
	const double CurrentSampleTimeSec = ActivatedStandingStabilityMetrics.ActivationDurationSec + FMath::Max(0.0f, DeltaTimeSeconds);

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
	ActivatedStandingStabilityMetrics.bPhysicsControlComponentAvailable = PhysicsControlComponent.IsValid();
	ActivatedStandingStabilityMetrics.ControlledBodyCount = PhysAnimBridge::GetControlledBoneNames().Num();

	ActivatedStandingStabilityMetrics.SampleCount++;

	const double CurrentRootWorldPositionDriftCm = FVector::Dist(CurrentRootLocationCm, ActivatedStandingStabilityBaselineRootLocationCm);
	const double CurrentRootVerticalDriftCm = FMath::Abs(CurrentRootLocationCm.Z - ActivatedStandingStabilityBaselineRootLocationCm.Z);
	const double CurrentRootAngularDriftDeg = FMath::Abs(CurrentRootTiltDeg - ActivatedStandingStabilityBaselineRootTiltDeg);
	if (ActivatedStandingStabilityMetrics.FirstSupportFailureTimeSec < 0.0 &&
		(CurrentSupportHullAreaCm2 <= UE_SMALL_NUMBER || CurrentActiveSupportSideCount < 1.0))
	{
		ActivatedStandingStabilityMetrics.FirstSupportFailureTimeSec = CurrentSampleTimeSec;
	}

	ActivatedStandingStabilityMetrics.RootWorldPositionDriftCm = FMath::Max(ActivatedStandingStabilityMetrics.RootWorldPositionDriftCm, CurrentRootWorldPositionDriftCm);
	ActivatedStandingStabilityMetrics.RootVerticalDriftCm = FMath::Max(ActivatedStandingStabilityMetrics.RootVerticalDriftCm, CurrentRootVerticalDriftCm);
	ActivatedStandingStabilityMetrics.RootAngularDriftDeg = FMath::Max(ActivatedStandingStabilityMetrics.RootAngularDriftDeg, CurrentRootAngularDriftDeg);
	ActivatedStandingStabilityMetrics.MaxRootTiltDeg = FMath::Max(
		ActivatedStandingStabilityMetrics.MaxRootTiltDeg,
		static_cast<double>(FMath::Abs(CurrentRootTiltDeg)));
	RecordLiveRuntimeEvidenceRendererFacingMotionSample(
		CurrentRootWorldPositionDriftCm > UE_SMALL_NUMBER,
		CurrentRootWorldPositionDriftCm);
	int32 DirectBodySampleCount = 0;
	int32 DirectSimulatingBodyCount = 0;
	int32 ExcludedRequiredBodySimulatingCount = 0;
	int32 FrameCriticalBodyValidMask = 0;
	int32 FrameCriticalBodySimulatingMask = 0;
	int32 FrameSupportBodyValidMask = 0;
	int32 FrameSupportBodySimulatingMask = 0;
	bool bAnyNonzeroBodyVelocity = false;

	const FName CriticalBodyNames[] =
	{
		PhysAnimBridge::GetRootBoneName(),
		TEXT("spine_01"),
		TEXT("spine_02"),
		TEXT("spine_03"),
		TEXT("thigh_l"),
		TEXT("thigh_r")
	};
	const FName SupportBodyNames[] =
	{
		TEXT("foot_l"),
		TEXT("foot_r"),
		TEXT("ball_l"),
		TEXT("ball_r")
	};
	const auto GetNamedBodyIndex = [](const FName& BoneName, const FName* BodyNames, int32 NumBodyNames) -> int32
	{
		for (int32 Index = 0; Index < NumBodyNames; ++Index)
		{
			if (BoneName == BodyNames[Index])
			{
				return Index;
			}
		}
		return INDEX_NONE;
	};

	for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		if (FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
		{
			const bool bSimulating = BodyInstance->IsInstanceSimulatingPhysics();
			++DirectBodySampleCount;

			const int32 CriticalBodyIndex = GetNamedBodyIndex(BoneName, CriticalBodyNames, UE_ARRAY_COUNT(CriticalBodyNames));
			const int32 SupportBodyIndex = GetNamedBodyIndex(BoneName, SupportBodyNames, UE_ARRAY_COUNT(SupportBodyNames));

			if (CriticalBodyIndex != INDEX_NONE)
			{
				FrameCriticalBodyValidMask |= (1 << CriticalBodyIndex);
				ActivatedStandingStabilityMetrics.CriticalBodyValidMask |= (1 << CriticalBodyIndex);
				if (bSimulating)
				{
					FrameCriticalBodySimulatingMask |= (1 << CriticalBodyIndex);
					ActivatedStandingStabilityMetrics.CriticalBodySimulatingMask |= (1 << CriticalBodyIndex);
				}
			}

			if (SupportBodyIndex != INDEX_NONE)
			{
				FrameSupportBodyValidMask |= (1 << SupportBodyIndex);
				ActivatedStandingStabilityMetrics.SupportBodyValidMask |= (1 << SupportBodyIndex);
				if (bSimulating)
				{
					FrameSupportBodySimulatingMask |= (1 << SupportBodyIndex);
					ActivatedStandingStabilityMetrics.SupportBodySimulatingMask |= (1 << SupportBodyIndex);
				}
			}

			if (bSimulating)
			{
				++DirectSimulatingBodyCount;

				if (CriticalBodyIndex == INDEX_NONE && SupportBodyIndex == INDEX_NONE)
				{
					++ExcludedRequiredBodySimulatingCount;
				}
			}

			const double LinearSpeedCmPerSecond = static_cast<double>(BodyInstance->GetUnrealWorldVelocity().Size());
			const double AngularSpeedDegPerSecond = static_cast<double>(FMath::RadiansToDegrees(BodyInstance->GetUnrealWorldAngularVelocityInRadians().Size()));
			constexpr double DiagnosticLinearThresholdCmPerSecond = 1200.0;
			constexpr double DiagnosticAngularThresholdDegPerSecond = 3600.0;

			if (ActivatedStandingStabilityMetrics.FirstLinearThresholdTimeSec < 0.0 &&
				LinearSpeedCmPerSecond >= DiagnosticLinearThresholdCmPerSecond)
			{
				ActivatedStandingStabilityMetrics.FirstLinearThresholdTimeSec = CurrentSampleTimeSec;
				ActivatedStandingStabilityMetrics.FirstLinearThresholdBodyName = BoneName;
			}

			if (ActivatedStandingStabilityMetrics.FirstAngularThresholdTimeSec < 0.0 &&
				AngularSpeedDegPerSecond >= DiagnosticAngularThresholdDegPerSecond)
			{
				ActivatedStandingStabilityMetrics.FirstAngularThresholdTimeSec = CurrentSampleTimeSec;
				ActivatedStandingStabilityMetrics.FirstAngularThresholdBodyName = BoneName;
			}

			// Work calculation for thighs during critical window (0.00s - 0.30s)
			if (bV0PlantThighWorkDiagnosticEnabled && CurrentSampleTimeSec >= 0.00 && CurrentSampleTimeSec <= 0.30)
			{
				if (BoneName == TEXT("thigh_l") || BoneName == TEXT("thigh_r"))
				{
					if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
					{
						const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
						FPhysicsControlTarget ControlTarget;
						FPhysicsControlData ControlData;
						FPhysicsControlMultiplier ControlMultiplier;
						
						if (PhysicsControl->GetControlTarget(ControlName, ControlTarget) &&
							PhysicsControl->GetControlData(ControlName, ControlData) &&
							PhysicsControl->GetControlMultiplier(ControlName, ControlMultiplier))
						{
							const bool bIsSimulating = bSimulating; // Capture for logging
							const FVector AngularVelocity = BodyInstance->GetUnrealWorldAngularVelocityInRadians();
							const FQuat CurrentRotation = BodyInstance->GetUnrealWorldTransform().GetRotation();
							const FQuat TargetRotation = FQuat(ControlTarget.TargetOrientation);
							
							const FQuat ErrorQuat = TargetRotation * CurrentRotation.Inverse();
							const FVector ErrorAxis = ErrorQuat.GetRotationAxis();
							const float ErrorAngle = ErrorQuat.GetAngle();

							const FVector Torque = (ControlData.AngularStrength * ControlMultiplier.AngularStrengthMultiplier * ErrorAxis * ErrorAngle) -
								(ControlData.AngularDampingRatio * ControlMultiplier.AngularDampingRatioMultiplier * AngularVelocity);

							const float TorqueDotVel = FVector::DotProduct(Torque, AngularVelocity);
							// Apply a proxy inertia scalar (e.g. 10.0f) since PhysicsControl strength acts as acceleration
							const float WorkIncrement = TorqueDotVel * DeltaTimeSeconds * 10.0f;
							const double AbsWorkIncrement = static_cast<double>(FMath::Abs(WorkIncrement));
							
							if (CurrentSampleTimeSec < 0.05)
							{
								ActivatedStandingStabilityMetrics.ThighBaselineWork += AbsWorkIncrement;
							}
							else
							{
								ActivatedStandingStabilityMetrics.ThighActivationWork += AbsWorkIncrement;
								
								if (TorqueDotVel > 0.0f)
								{
									ActivatedStandingStabilityMetrics.ThighPositiveWorkAccumulated += static_cast<double>(WorkIncrement);
								}
								else
								{
									ActivatedStandingStabilityMetrics.ThighNegativeWorkAccumulated += static_cast<double>(FMath::Abs(WorkIncrement));
								}
							}

							ActivatedStandingStabilityMetrics.ThighNetWork = ActivatedStandingStabilityMetrics.ThighActivationWork;

							// AC-2: snapshot thigh control state and pose-seeded flag on first window entry (thigh_l only)
							if (BoneName == TEXT("thigh_l") && ActivatedStandingStabilityMetrics.PoseTargetsSeededAtWindowEntry < 0 && CurrentSampleTimeSec >= 0.05)
							{
								ActivatedStandingStabilityMetrics.ThighAngularStrengthAtWindowEntry =
									ControlData.AngularStrength * ControlMultiplier.AngularStrengthMultiplier;
								ActivatedStandingStabilityMetrics.ThighAngularDampingAtWindowEntry =
									ControlData.AngularDampingRatio * ControlMultiplier.AngularDampingRatioMultiplier;
								ActivatedStandingStabilityMetrics.ThighLinearStrengthAtWindowEntry =
									(ControlData.LinearStrength * ControlMultiplier.LinearStrengthMultiplier).Size();
								const bool bPrevSeeded = GetPreviousControlTargetRotationsForDiagnostics().Num() > 0;
								const bool bBlendSeeded = GetPolicyBlendStartControlTargetRotationsForDiagnostics().Num() > 0;
								ActivatedStandingStabilityMetrics.PoseTargetsSeededAtWindowEntry = (bPrevSeeded && bBlendSeeded) ? 1 : 0;
							}

							PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimV0] WORK_DIAG bone=%s t=%.3f dW=%.6f dot=%.4f sim=%d"), 
								*BoneName.ToString(), CurrentSampleTimeSec, WorkIncrement, TorqueDotVel, bIsSimulating ? 1 : 0);
						}
						else
						{
							PHYSANIM_LOG_RATE_LIMITED(LogTemp, Warning, 1.0f, TEXT("[PhysAnimV0] DEBUG_WORK Missing data for %s (PC=%d)"), *BoneName.ToString(), PhysicsControl != nullptr);
						}
					}
				}
			}

			// Velocity snapshots at specific timestamps
			const auto UpdateSnapshot = [&](double TargetT, double& MetricField)
			{
				if (FMath::Abs(CurrentSampleTimeSec - TargetT) < (DeltaTimeSeconds * 0.51))
				{
					MetricField = FMath::Max(MetricField, AngularSpeedDegPerSecond);
				}
			};

			UpdateSnapshot(0.05, ActivatedStandingStabilityMetrics.MaxAngVel005s);
			UpdateSnapshot(0.10, ActivatedStandingStabilityMetrics.MaxAngVel010s);
			UpdateSnapshot(0.15, ActivatedStandingStabilityMetrics.MaxAngVel015s);
			UpdateSnapshot(0.20, ActivatedStandingStabilityMetrics.MaxAngVel020s);
			UpdateSnapshot(0.30, ActivatedStandingStabilityMetrics.MaxAngVel030s);
			UpdateSnapshot(0.60, ActivatedStandingStabilityMetrics.MaxAngVel060s);
			UpdateSnapshot(1.00, ActivatedStandingStabilityMetrics.MaxAngVel100s);

			if (BoneName == TEXT("spine_01"))
			{
				ActivatedStandingStabilityMetrics.Spine01MaxLinearSpeedCmPerSecond = FMath::Max(
					ActivatedStandingStabilityMetrics.Spine01MaxLinearSpeedCmPerSecond,
					LinearSpeedCmPerSecond);
				ActivatedStandingStabilityMetrics.Spine01MaxAngularSpeedDegPerSecond = FMath::Max(
					ActivatedStandingStabilityMetrics.Spine01MaxAngularSpeedDegPerSecond,
					AngularSpeedDegPerSecond);
			}
			else if (BoneName == TEXT("spine_03"))
			{
				ActivatedStandingStabilityMetrics.Spine03MaxLinearSpeedCmPerSecond = FMath::Max(
					ActivatedStandingStabilityMetrics.Spine03MaxLinearSpeedCmPerSecond,
					LinearSpeedCmPerSecond);
				ActivatedStandingStabilityMetrics.Spine03MaxAngularSpeedDegPerSecond = FMath::Max(
					ActivatedStandingStabilityMetrics.Spine03MaxAngularSpeedDegPerSecond,
					AngularSpeedDegPerSecond);
			}

			if (ActivatedStandingStabilityMetrics.FirstMajorSpineSpikeTimeSec < 0.0 &&
				(BoneName == TEXT("spine_01") || BoneName == TEXT("spine_03")) &&
				(LinearSpeedCmPerSecond >= 1200.0 || AngularSpeedDegPerSecond >= 3600.0))
			{
				ActivatedStandingStabilityMetrics.FirstMajorSpineSpikeTimeSec = CurrentSampleTimeSec;
				ActivatedStandingStabilityMetrics.FirstMajorSpineSpikeBodyName = BoneName;
				const FTransform BodyTransform = BodyInstance->GetUnrealWorldTransform();
				const FVector BodyLocation = BodyTransform.GetLocation();
				const FRotator BodyRotation = BodyTransform.GetRotation().Rotator();
				PHYSANIM_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimV0] FIRST_MAJOR_SPINE_SPIKE bone=%s activationT=%.3f runtimeState=%s lin=%.2f angDeg=%.2f thighWorkP=%.4f sim=%d awake=%d"),
					*BoneName.ToString(),
					CurrentSampleTimeSec,
					GetRuntimeStateName(RuntimeState),
					LinearSpeedCmPerSecond,
					AngularSpeedDegPerSecond,
					ActivatedStandingStabilityMetrics.ThighPositiveWorkAccumulated,
					bSimulating ? 1 : 0,
					BodyInstance->IsInstanceAwake() ? 1 : 0);
			}

			if (LinearSpeedCmPerSecond > ActivatedStandingStabilityMetrics.MaxBodyLinearSpeedCmPerSecond)
			{
				ActivatedStandingStabilityMetrics.MaxBodyLinearSpeedCmPerSecond = LinearSpeedCmPerSecond;
				ActivatedStandingStabilityMetrics.MaxBodyLinearSpeedBodyName = BoneName;
			}
			if (AngularSpeedDegPerSecond > ActivatedStandingStabilityMetrics.MaxBodyAngularSpeedDegPerSecond)
			{
				ActivatedStandingStabilityMetrics.MaxBodyAngularSpeedDegPerSecond = AngularSpeedDegPerSecond;
				ActivatedStandingStabilityMetrics.MaxBodyAngularSpeedBodyName = BoneName;
			}
			bAnyNonzeroBodyVelocity = bAnyNonzeroBodyVelocity || LinearSpeedCmPerSecond > 0.1 || AngularSpeedDegPerSecond > 0.1;
		}
	}

	ActivatedStandingStabilityMetrics.BodyTelemetrySampleCount += DirectBodySampleCount;
	ActivatedStandingStabilityMetrics.SimulatingBodyCountMax = FMath::Max(
		ActivatedStandingStabilityMetrics.SimulatingBodyCountMax,
		DirectSimulatingBodyCount);
	ActivatedStandingStabilityMetrics.ExcludedRequiredBodySimulatingCountMax = FMath::Max(
		ActivatedStandingStabilityMetrics.ExcludedRequiredBodySimulatingCountMax,
		ExcludedRequiredBodySimulatingCount);
	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		ProductGateBodyContinuity.RecordSample(
			DirectSimulatingBodyCount,
			FrameCriticalBodyValidMask,
			FrameCriticalBodySimulatingMask,
			FrameSupportBodyValidMask,
			FrameSupportBodySimulatingMask);
	}
	if (bAnyNonzeroBodyVelocity)
	{
		++ActivatedStandingStabilityMetrics.BodyVelocityNonZeroSampleCount;
	}

	ActivatedStandingStabilityMetrics.ActivationDurationSec = CurrentSampleTimeSec;

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
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation already applied once."));
		return false;
	}

	if (RuntimeState != EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation blocked by runtime state=%s"), GetRuntimeStateName(RuntimeState));
		return false;
	}

	if (!bLiveRuntimeEvidenceProofComplete || !IsLiveRuntimeEvidenceProofSatisfied())
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation blocked by proof state complete=%d satisfied=%d"),
			bLiveRuntimeEvidenceProofComplete ? 1 : 0,
			IsLiveRuntimeEvidenceProofSatisfied() ? 1 : 0);
		return false;
	}

	const FPhysAnimActivatedStandingStabilityMetrics& Metrics = GetActivatedStandingStabilityMetrics();
	if (!Metrics.bHasSamples || Metrics.SupportHullAreaMinCm2 <= 0.0 || Metrics.ActiveSupportSideCountMin < 1.0)
	{
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation blocked by unstable metrics samples=%d supportHullMin=%.2f activeSidesMin=%.2f"),
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
		PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Activated standing perturbation requires a raw-simulating pelvis impulse; no actor offset fallback was applied."));
		return false;
	}

	bActivatedStandingPerturbationApplied = true;

	PHYSANIM_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimBalance] Activated standing perturbation applied direction=%d magnitude=%d runtimeState=%s"),
		static_cast<int32>(Direction),
		static_cast<int32>(Magnitude),
		GetRuntimeStateName(RuntimeState));
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPhysAnimComponent::HasProductSupportContactForTesting() const
{
	TArray<FHitResult> HitResults;
	int32 MappedSupportHitCount = 0;
	return CaptureLiveRuntimeEvidenceHitResults(HitResults, MappedSupportHitCount) &&
		MappedSupportHitCount > 0;
}
#endif

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

	const int32 BeforeLeftBall = OutHitResults.Num();
	if (CaptureLiveRuntimeEvidenceHitResultForBody(TEXT("ball_l"), OutHitResults))
	{
		OutMappedSupportHitCount += OutHitResults.Num() - BeforeLeftBall;
	}

	const int32 BeforeRightBall = OutHitResults.Num();
	if (CaptureLiveRuntimeEvidenceHitResultForBody(TEXT("ball_r"), OutHitResults))
	{
		OutMappedSupportHitCount += OutHitResults.Num() - BeforeRightBall;
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
		PHYSANIM_LOG(LogTemp, Warning, TEXT("[PhysAnimBalance] ALL_SAMPLES_FAILED body=%s location=(%.1f,%.1f,%.1f) radius=%.1f"),
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
	FVector ProxyWorldCm = Capsule
		? Capsule->GetComponentLocation()
		: (OwnerActor ? OwnerActor->GetActorLocation() : RebaseOriginCm);
	if (Mesh &&
		(RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing ||
			ShouldUseRawSimComProxyForStartupProof(
				RuntimeState,
				bEnableLiveRuntimeEvidenceProof,
				bLiveRuntimeEvidenceProofComplete)))
	{
		(void)TryCalculateRawSimBodyComWorldCm(*Mesh, ProxyWorldCm);
	}
	const FPhysAnimStabilizationSettings Settings = ResolveEffectiveStabilizationSettings();

	Input.HitResults = HitResults;
	Input.WorldOriginCm = RebaseOriginCm;
	Input.ComProxyPosCm = FVector2D(ProxyWorldCm.X - RebaseOriginCm.X, ProxyWorldCm.Y - RebaseOriginCm.Y);
	Input.bRequireWorldStatic = bLiveRuntimeEvidenceWorldStaticOnly;
	Input.DeltaMs = static_cast<double>(FMath::Max(0.0f, DeltaTimeSeconds) * 1000.0f);
	Input.PreviousSupportGapTimerMs = LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportGapTimerMs;
	Input.SupportGapMaxMs = Settings.BalancePhase1AdmissionMaxSupportGapMs;
	Input.ProxyDriftLimitMs = Settings.ProxyDriftLimitMs;
	Input.SupportAreaMinCm2 = (GetAttachedLoadMassKg() <= 0.01f) ? 25.0 : 50.0;

	if (LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.IsSet())
	{
		Input.PreviousProxyOutsideHullDurationMs =
			LiveRuntimeEvidenceTerminationState.LatestArtifact.ProxyOutsideHullDurationMs.GetValue();
	}

	FPhysAnimSupportBodyMapping LeftMapping;
	LeftMapping.BodyName = LiveRuntimeEvidenceLeftSupportBodyName;
	LeftMapping.SupportSide = EPhysAnimSupportSide::Left;
	Input.SupportBodies.Add(LeftMapping);

	FPhysAnimSupportBodyMapping LeftBallMapping;
	LeftBallMapping.BodyName = TEXT("ball_l");
	LeftBallMapping.SupportSide = EPhysAnimSupportSide::Left;
	Input.SupportBodies.Add(LeftBallMapping);

	FPhysAnimSupportBodyMapping RightMapping;
	RightMapping.BodyName = LiveRuntimeEvidenceRightSupportBodyName;
	RightMapping.SupportSide = EPhysAnimSupportSide::Right;
	Input.SupportBodies.Add(RightMapping);

	FPhysAnimSupportBodyMapping RightBallMapping;
	RightBallMapping.BodyName = TEXT("ball_r");
	RightBallMapping.SupportSide = EPhysAnimSupportSide::Right;
	Input.SupportBodies.Add(RightBallMapping);

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
	const FPhysAnimCapsuleContractSnapshot CapsuleSnapshot = BuildCapsuleContractSnapshot();
	const FPhysAnimContinuitySnapshot ContinuitySnapshot = BuildContinuitySnapshot(DeltaTimeSeconds);
	const FPhysAnimCapsuleContractValidationResult CapsuleValidation = PhysAnimValidators::ValidateCapsule(CapsuleSnapshot);
	const FPhysAnimContinuityValidationResult ContinuityValidation = PhysAnimValidators::ValidateContinuity(ContinuitySnapshot);
	FString PlantAuditError;
	FPhysAnimPlantContractSnapshotCaptureInput PlantCaptureInput;
	PlantCaptureInput.SkeletalMeshComponent = const_cast<USkeletalMeshComponent*>(Mesh);
	PlantCaptureInput.ExpectedPhysicsAssetPath = PhysAnimComponentInternal::ExpectedPhysicsAssetPath;
	PlantCaptureInput.bSkeletonAuditPassed = ValidateRequiredBodies(PlantAuditError);
	const FPhysAnimPlantContractSnapshot PlantSnapshot = PhysAnimRuntimeAdapter::CapturePlantContractSnapshot(PlantCaptureInput);
	const FPhysAnimPlantContractValidationResult PlantValidation = PhysAnimValidators::ValidatePlant(PlantSnapshot);

	Input.SupportObservation = SupportObservation;
	Input.Plant = PlantValidation;
	Input.Values.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
	Input.Values.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : LiveRuntimeEvidenceStandingSeconds;
	Input.Values.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
	Input.Values.CapsuleWorldPosCm = Character ? Character->GetActorLocation() : FVector::ZeroVector;
	Input.Values.HoldDurationSec = LiveRuntimeEvidenceStandingSeconds;
	Input.Values.BalanceActiveStandingContinuousSec = ProductGateStandingWindow.ContinuousSeconds;
	Input.Values.BalanceActiveStandingExitCount = ProductGateStandingWindow.ExitCount;
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
	Input.Values.PolicyInferenceAttemptCount = ActivatedStandingStabilityMetrics.PolicyInferenceAttemptCount;
	Input.Values.PolicyInferenceFailureCount = ActivatedStandingStabilityMetrics.PolicyInferenceFailureCount;
	Input.Values.ThighNetWork = ActivatedStandingStabilityMetrics.ThighNetWork;
	Input.Values.ThighBaselineWork = ActivatedStandingStabilityMetrics.ThighBaselineWork;
	Input.Values.ThighActivationWork = ActivatedStandingStabilityMetrics.ThighActivationWork;
	Input.Values.ActionMagnitudeVariance = ActivatedStandingStabilityMetrics.ActionMagnitudeVariance;
	Input.Values.PolicyInferenceLatencyMsMax = ActivatedStandingStabilityMetrics.PolicyInferenceLatencyMsMax;
	Input.Values.bPolicyModelLoaded = ActivatedStandingStabilityMetrics.bPolicyModelLoaded;
	Input.Values.PolicyRuntimeName = ActivatedStandingStabilityMetrics.PolicyRuntimeName;
	Input.Values.PolicyModelName = ActivatedStandingStabilityMetrics.PolicyModelName;
	Input.Values.bPolicyInputBuffersFinite = ActivatedStandingStabilityMetrics.bPolicyInputBuffersFinite;
	Input.Values.PolicyActionSampleCount = ActivatedStandingStabilityMetrics.PolicyActionSampleCount;
	Input.Values.PolicyActionRawMeanAbsMax = ActivatedStandingStabilityMetrics.PolicyActionRawMeanAbsMax;
	Input.Values.PolicyActionConditionedMeanAbsMax = ActivatedStandingStabilityMetrics.PolicyActionConditionedMeanAbsMax;
	Input.Values.PolicyActionClampedFloatMax = ActivatedStandingStabilityMetrics.PolicyActionClampedFloatMax;
	Input.Values.bPhysicsControlComponentAvailable = ActivatedStandingStabilityMetrics.bPhysicsControlComponentAvailable;
	Input.Values.ControlledBodyCount = ActivatedStandingStabilityMetrics.ControlledBodyCount;
	Input.Values.ControlTargetSampleCount = ActivatedStandingStabilityMetrics.ControlTargetSampleCount;
	Input.Values.ControlTargetNormalWrites = ActivatedStandingStabilityMetrics.ControlTargetNormalWrites;
	Input.Values.ControlTargetTotalWrites = ActivatedStandingStabilityMetrics.ControlTargetTotalWrites;
	Input.Values.ControlTargetMaxDeltaDeg = ActivatedStandingStabilityMetrics.ControlTargetMaxDeltaDeg;
	Input.Values.ControlTargetMeanDeltaDegMax = ActivatedStandingStabilityMetrics.ControlTargetMeanDeltaDegMax;
	Input.Values.ControlTargetMaxRawPolicyOffsetDeg = ActivatedStandingStabilityMetrics.ControlTargetMaxRawPolicyOffsetDeg;
	Input.Values.ControlTargetMeanRawPolicyOffsetDegMax = ActivatedStandingStabilityMetrics.ControlTargetMeanRawPolicyOffsetDegMax;
	Input.Values.PoseSearchQueryCount = ActivatedStandingStabilityMetrics.PoseSearchQueryCount;
	Input.Values.PoseSearchValidResultCount = ActivatedStandingStabilityMetrics.PoseSearchValidResultCount;
	Input.Values.PoseSearchSelectedAnimationName = ActivatedStandingStabilityMetrics.PoseSearchSelectedAnimationName;
	Input.Values.PoseSearchSelectedTime = ActivatedStandingStabilityMetrics.PoseSearchSelectedTime;
	Input.Values.PoseSearchConsecutiveInvalidFrameCount = ActivatedStandingStabilityMetrics.PoseSearchConsecutiveInvalidFrameCount;
	Input.Values.RendererFacingMotionSampleCount = ActivatedStandingStabilityMetrics.RendererFacingMotionSampleCount;
	Input.Values.RendererFacingMotionActiveSampleCount = ActivatedStandingStabilityMetrics.RendererFacingMotionActiveSampleCount;
	Input.Values.RendererFacingMotionMaxRootWorldPositionDriftCm =
		ActivatedStandingStabilityMetrics.RendererFacingMotionMaxRootWorldPositionDriftCm;
	Input.Values.RendererFacingMotionMaxMeshWorldPositionDriftCm =
		ActivatedStandingStabilityMetrics.RendererFacingMotionMaxMeshWorldPositionDriftCm;
	Input.Values.RendererFacingMotionMaxRootYawDeltaDeg =
		ActivatedStandingStabilityMetrics.RendererFacingMotionMaxRootYawDeltaDeg;
	Input.Values.RendererFacingMotionMaxBodyDeltaCm =
		ActivatedStandingStabilityMetrics.RendererFacingMotionMaxBodyDeltaCm;
	Input.Values.RendererFacingMotionMaxBodyDeltaDeg =
		ActivatedStandingStabilityMetrics.RendererFacingMotionMaxBodyDeltaDeg;
	Input.Values.bRendererFacingMotionUsedNullRhi =
		!FApp::CanEverRender();
	Input.Values.RuntimeBodySampleCount = ProductGateBodyContinuity.SampleCount;
	Input.Values.RuntimeSimulatingBodyCount = ActivatedStandingStabilityMetrics.SimulatingBodyCountMax;
	Input.Values.RuntimeMinSimulatingBodyCount = ProductGateBodyContinuity.MinSimulatingBodyCount;
	Input.Values.CriticalBodyValidMask = ActivatedStandingStabilityMetrics.CriticalBodyValidMask;
	Input.Values.CriticalBodySimulatingMask = ActivatedStandingStabilityMetrics.CriticalBodySimulatingMask;
	Input.Values.SupportBodyValidMask = ActivatedStandingStabilityMetrics.SupportBodyValidMask;
	Input.Values.SupportBodySimulatingMask = ActivatedStandingStabilityMetrics.SupportBodySimulatingMask;
	Input.Values.CriticalBodyValidAllFramesMask = ProductGateBodyContinuity.CriticalBodyValidAllFramesMask;
	Input.Values.CriticalBodySimulatingAllFramesMask = ProductGateBodyContinuity.CriticalBodySimulatingAllFramesMask;
	Input.Values.SupportBodyValidAllFramesMask = ProductGateBodyContinuity.SupportBodyValidAllFramesMask;
	Input.Values.SupportBodySimulatingAllFramesMask = ProductGateBodyContinuity.SupportBodySimulatingAllFramesMask;
	Input.Values.RuntimeMaxBodyLinearSpeedCmPerSecond = ActivatedStandingStabilityMetrics.MaxBodyLinearSpeedCmPerSecond;
	Input.Values.RuntimeMaxBodyAngularSpeedDegPerSecond = ActivatedStandingStabilityMetrics.MaxBodyAngularSpeedDegPerSecond;
	Input.Values.bPhysicalPerturbationApplied = ActivatedStandingStabilityMetrics.bPhysicalPerturbationApplied;
	Input.Values.PerturbationMeasuredDeltaVCmPerSecond = ActivatedStandingStabilityMetrics.PerturbationMeasuredDeltaVCmPerSecond;
	Input.Values.ThighBaselineWork = ActivatedStandingStabilityMetrics.ThighBaselineWork;
	Input.Values.ThighActivationWork = ActivatedStandingStabilityMetrics.ThighActivationWork;
	Input.Values.ThighNetWork = ActivatedStandingStabilityMetrics.ThighNetWork;

	// Stage 2A Locomotion Telemetry
	const FBodyInstance* const RootBody = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
	Input.Values.RootMode = RootBody && RootBody->IsInstanceSimulatingPhysics()
		? TEXT("ChaosSimulated")
		: TEXT("Kinematic");
	Input.Values.LocomotionIntent = ResolveBridgeLocomotionIntentName();
	Input.Values.bPolicyOutputActive = IsStage2APolicyOutputActive();
	Input.Values.CapsuleVelocity = Character ? Character->GetVelocity() : FVector::ZeroVector;
	Input.Values.ShellDivergencePeak = ActivatedStandingStabilityMetrics.RendererFacingMotionMaxRootWorldPositionDriftCm;

	FPhysAnimControllerStabilitySnapshot ControllerStabilitySnapshot;
	ControllerStabilitySnapshot.TargetDiscontinuityDeg = ActivatedStandingStabilityMetrics.ControlTargetMaxDeltaDeg;
	ControllerStabilitySnapshot.MaxRootTiltDeg = ActivatedStandingStabilityMetrics.MaxRootTiltDeg;
	ControllerStabilitySnapshot.PeakAngularSpeedDegPerSec = ActivatedStandingStabilityMetrics.MaxBodyAngularSpeedDegPerSecond;
	ControllerStabilitySnapshot.MaxBodyMismatchDeg = ActivatedStandingStabilityMetrics.ControlTargetMaxDeltaDeg;
	ControllerStabilitySnapshot.RmsMismatchDeg = ActivatedStandingStabilityMetrics.ControlTargetMeanDeltaDegMax;
	ControllerStabilitySnapshot.MismatchDurationMs =
		ControllerStabilitySnapshot.MaxBodyMismatchDeg > 25.0 || ControllerStabilitySnapshot.RmsMismatchDeg > 15.0
			? ProductGateStandingWindow.ContinuousSeconds * 1000.0
			: 0.0;
	ControllerStabilitySnapshot.ControllerGainScale = LiveRuntimeEvidenceTerminationState.LatestArtifact.ControllerGainScale;
	ControllerStabilitySnapshot.ControllerDampingRatio = LiveRuntimeEvidenceTerminationState.LatestArtifact.ControllerDampingRatio;
	ControllerStabilitySnapshot.bControllerGainDampingValid =
		FMath::IsFinite(ControllerStabilitySnapshot.ControllerGainScale) &&
		FMath::IsFinite(ControllerStabilitySnapshot.ControllerDampingRatio);
	ControllerStabilitySnapshot.HoldDurationSec = ProductGateStandingWindow.ContinuousSeconds;
	Input.ControllerStability = PhysAnimValidators::ValidateControllerStability(ControllerStabilitySnapshot);

	FPhysAnimAuthoritySnapshot AuthoritySnapshot;
	AuthoritySnapshot.AuthorityConflictCount =
		RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing &&
		(CapsuleSnapshot.bCmcIsActive || CapsuleSnapshot.bCmcTickEnabled || !CapsuleSnapshot.bCmcUpdatedComponentIsNull)
			? 1
			: 0;
	Input.Authority = PhysAnimValidators::ValidateAuthority(AuthoritySnapshot);

	FPhysAnimMovementReclaimSnapshot MovementReclaimSnapshot;
	MovementReclaimSnapshot.MovementReclaimCount = LiveRuntimeEvidenceTerminationState.bMovementReclaimRequested ? 1 : 0;
	Input.MovementReclaim = PhysAnimValidators::ValidateMovementReclaim(MovementReclaimSnapshot);

	FPhysAnimShellHelperSnapshot ShellHelperSnapshot;
	ShellHelperSnapshot.ShellHelperUsedCount = LiveRuntimeEvidenceTerminationState.LatestArtifact.ShellHelperUsedCount;
	Input.ShellHelper = PhysAnimValidators::ValidateShellHelper(ShellHelperSnapshot);

	Input.Capsule = CapsuleValidation;
	Input.Continuity = ContinuityValidation;

	if (bEnableLiveRuntimeEvidenceProof)
	{
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
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
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
			TEXT("PhysAnimProof: LiveCapsule capsule_valid=%d collision=%s cmc_active=%d cmc_tick=%d lock_delta=%.2f"),
			CapsuleValidation.bCapsuleContractPassed ? 1 : 0,
			ToCapsuleCollisionStateName(CapsuleValidation.CapsuleCollisionEnabled),
			CharacterMovement && CharacterMovement->IsActive() ? 1 : 0,
			CharacterMovement && CharacterMovement->IsComponentTickEnabled() ? 1 : 0,
			CapsuleValidation.CapsuleLockDeltaCm);
		PHYSANIM_LOG_RATE_LIMITED(
			LogPhysAnimBridge,
			Verbose,
			1.0f,
			TEXT("PhysAnimProof: LiveContinuity continuity_valid=%d pelvis_sleep_ms=%.2f bookkeeping_mismatch=%d"),
			ContinuityValidation.bPhysicalContinuityValidatorPassed ? 1 : 0,
			ContinuityValidation.PelvisSleepDurationMs,
			ContinuityValidation.bContinuityBookkeepingMismatch ? 1 : 0);
		if (!PlantValidation.bPhysicsAssetContractValid || !PlantValidation.bSkeletonAuditPassed)
		{
			PHYSANIM_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] LiveProof plant audit failed: %s"), *PlantAuditError);
		}
	}

	return Input;
}

void UPhysAnimComponent::EmitLiveRuntimeEvidenceTerminalArtifactOnce(const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult)
{
	// Implementation moved to PhysAnimProofArtifactEmitter
}

bool UPhysAnimComponent::IsStage2APolicyOutputActive() const
{
	const double CurrentTimeSeconds = GetPhysAnimClockTime();
	return PolicyControlTicksExecuted > 0
		&& LastPolicyControlUpdateTimeSeconds >= 0.0
		&& CurrentTimeSeconds >= LastPolicyControlUpdateTimeSeconds
		&& (CurrentTimeSeconds - LastPolicyControlUpdateTimeSeconds) <= Stage2APolicyLivenessTimeoutSeconds;
}

bool UPhysAnimComponent::IsStage2AShellRootLocked() const
{
	return BalanceTransitionShellAuthorityMode == EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked
		&& BridgeShellState.bInitialized;
}

const TCHAR* UPhysAnimComponent::Stage2ATerminalStateToString(EStage2ALocomotionTerminalState TerminalState)
{
	switch (TerminalState)
	{
	case EStage2ALocomotionTerminalState::Allowed: return TEXT("Allowed");
	case EStage2ALocomotionTerminalState::Denied_NotBalanceActiveStanding: return TEXT("Denied_NotBalanceActiveStanding");
	case EStage2ALocomotionTerminalState::Denied_PolicyOutputInactive: return TEXT("Denied_PolicyOutputInactive");
	case EStage2ALocomotionTerminalState::Denied_SimRootAttempted: return TEXT("Denied_SimRootAttempted");
	case EStage2ALocomotionTerminalState::Denied_ShellRootUnlocked: return TEXT("Denied_ShellRootUnlocked");
	case EStage2ALocomotionTerminalState::Denied_Phase3ThresholdRelaxed: return TEXT("Denied_Phase3ThresholdRelaxed");
	default: return TEXT("NotEvaluated");
	}
}

EStage2ALocomotionTerminalState UPhysAnimComponent::EvaluateStage2ALocomotionRequestGate() const
{
	const bool bInValidState = (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing || 
	                            (RuntimeState == EPhysAnimRuntimeState::LocomotionActiveShell && 
	                             Stage2ALocomotionRequestState == EBridgeLocomotionRequestState::LocomotionRequested));
	if (!bInValidState) { return EStage2ALocomotionTerminalState::Denied_NotBalanceActiveStanding; }

	if (!IsStage2APolicyOutputActive()) { return EStage2ALocomotionTerminalState::Denied_PolicyOutputInactive; }
	if (bStage2APhase3SimRootAttempted) { return EStage2ALocomotionTerminalState::Denied_SimRootAttempted; }
	if (!IsStage2AShellRootLocked()) { return EStage2ALocomotionTerminalState::Denied_ShellRootUnlocked; }
	return EStage2ALocomotionTerminalState::Allowed;
}

bool UPhysAnimComponent::TryOpenStage2ALocomotionRequestGate(const TCHAR* RequestReason)
{
	Stage2ALastLocomotionTerminalState = EvaluateStage2ALocomotionRequestGate();
	const bool bAllowed = Stage2ALastLocomotionTerminalState == EStage2ALocomotionTerminalState::Allowed;
	Stage2ALocomotionRequestState = bAllowed ? EBridgeLocomotionRequestState::LocomotionRequested : EBridgeLocomotionRequestState::LocomotionRequestDenied;
	BridgeLocomotionAuthorityState = bAllowed ? EBridgeLocomotionAuthorityState::StartupLocomotion : EBridgeLocomotionAuthorityState::Idle;
	if (bAllowed)
	{
		TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShell);
	}
	else if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShellDenied);
	}
	EmitStage2ALocomotionTelemetry(TEXT("RequestOnly"), TEXT("Stage2A_KinematicShell"), Stage2ALastLocomotionTerminalState);
	return bAllowed;
}

bool UPhysAnimComponent::TryActivateStage2AWalkIntent(float DeltaTime)
{
	Stage2ALastLocomotionTerminalState = EvaluateStage2ALocomotionRequestGate();
	if (Stage2ALastLocomotionTerminalState != EStage2ALocomotionTerminalState::Allowed)
	{
		bStage2AWalkIntentActive = false;
		BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
		
		// Clear stale intent/trajectory state on denial
		BridgeIntentState = FBridgeIntentState();
		BridgeTrajectoryState = FBridgeTrajectoryState();

		EmitStage2ALocomotionTelemetry(Stage2AWalkIntentName, Stage2AMotionSourceName, Stage2ALastLocomotionTerminalState);
		return false;
	}

	if (Stage2AConsecutivePolicyActiveFrames < Stage2AMinPolicyActiveFramesBeforeWalk)
	{
		bStage2AWalkIntentActive = false;
		Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::Denied_PolicyOutputInactive;
		BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
		
		// Clear stale intent/trajectory state on denial
		BridgeIntentState = FBridgeIntentState();
		BridgeTrajectoryState = FBridgeTrajectoryState();

		TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShellDenied);
		EmitStage2ALocomotionTelemetry(Stage2AWalkIntentName, Stage2AMotionSourceName, Stage2ALastLocomotionTerminalState);
		return false;
	}

	AActor* const OwnerActor = GetOwner();
	FVector WorldMoveDirection = OwnerActor ? OwnerActor->GetActorForwardVector() : FVector::ForwardVector;
	WorldMoveDirection.Z = 0.0f;
	if (!WorldMoveDirection.Normalize())
	{
		WorldMoveDirection = FVector::ForwardVector;
	}

	BridgeIntentState.ActiveIntent = Stage2AWalkIntentName;
	BridgeIntentState.WorldMoveDirection = WorldMoveDirection;
	BridgeIntentState.LocalMoveDirection = FVector::ForwardVector;
	BridgeIntentState.IntentMagnitude = 1.0f;
	BridgeIntentState.DesiredSpeedCmPerSecond = Stage2AForwardWalkSpeedCmPerSecond;
	BridgeIntentState.bHasDesiredFacing = false;

	BridgeTrajectoryState.MotionSource = Stage2AMotionSourceName;
	BridgeTrajectoryState.DesiredVelocityCmPerSecond = WorldMoveDirection * Stage2AForwardWalkSpeedCmPerSecond;
	BridgeTrajectoryState.AcceptedVelocityCmPerSecond = FVector::ZeroVector;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = BridgeTrajectoryState.DesiredVelocityCmPerSecond;
	BridgeTrajectoryState.LastDeltaTimeSeconds = DeltaTime;
	BridgeTrajectoryState.bInitialized = true;

	Stage2ALastWalkDeltaCm = BuildStage2AWalkDeltaCm(DeltaTime);
	ApplyStage2AKinematicShellWalkDelta(Stage2ALastWalkDeltaCm);

	const bool bAcceptedMovement = !BridgeShellState.AcceptedWorldDeltaCm.IsNearlyZero();
	bStage2AWalkIntentActive = bAcceptedMovement;
	
	if (bAcceptedMovement)
	{
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequested;
		BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Locomoting;
		Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::Allowed;
		TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShell);
		BridgeTrajectoryState.AcceptedVelocityCmPerSecond = BridgeShellState.AcceptedPlanarVelocityCmPerSecond;
	}
	else
	{
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
		BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::StartupLocomotion;
		Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::Denied_ShellRootUnlocked;
		TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShellDenied);

		// Clear stale intent/trajectory state on denial
		BridgeIntentState = FBridgeIntentState();
		BridgeTrajectoryState = FBridgeTrajectoryState();
	}

	EmitStage2ALocomotionTelemetry(Stage2AWalkIntentName, Stage2AMotionSourceName, Stage2ALastLocomotionTerminalState);

	return bAcceptedMovement;
}

bool UPhysAnimComponent::TryActivateStage2ATurnIntent(float DeltaTime, float YawDeltaDegrees)
{
	const TCHAR* IntentName = (YawDeltaDegrees >= 0.0f) ? TEXT("TurnLeft") : TEXT("TurnRight");
	Stage2ALastLocomotionTerminalState = EvaluateStage2ALocomotionRequestGate();
	if (Stage2ALastLocomotionTerminalState != EStage2ALocomotionTerminalState::Allowed)
	{
		BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
		
		// Clear stale intent/trajectory state on denial
		BridgeIntentState = FBridgeIntentState();
		BridgeTrajectoryState = FBridgeTrajectoryState();

		EmitStage2ALocomotionTelemetry(IntentName, Stage2AMotionSourceName, Stage2ALastLocomotionTerminalState);
		return false;
	}

	if (Stage2AConsecutivePolicyActiveFrames < Stage2AMinPolicyActiveFramesBeforeWalk)
	{
		Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::Denied_PolicyOutputInactive;
		BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
		
		// Clear stale intent/trajectory state on denial
		BridgeIntentState = FBridgeIntentState();
		BridgeTrajectoryState = FBridgeTrajectoryState();

		TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShellDenied);
		EmitStage2ALocomotionTelemetry(IntentName, Stage2AMotionSourceName, Stage2ALastLocomotionTerminalState);
		return false;
	}

	BridgeIntentState.ActiveIntent = IntentName;
	BridgeIntentState.WorldMoveDirection = FVector::ZeroVector;
	BridgeIntentState.LocalMoveDirection = FVector::ZeroVector;
	BridgeIntentState.IntentMagnitude = 1.0f;
	BridgeIntentState.DesiredSpeedCmPerSecond = 0.0f;
	BridgeIntentState.bHasDesiredFacing = true;
	
	BridgeTrajectoryState.MotionSource = Stage2AMotionSourceName;
	BridgeTrajectoryState.DesiredVelocityCmPerSecond = FVector::ZeroVector;
	BridgeTrajectoryState.AcceptedVelocityCmPerSecond = FVector::ZeroVector;
	BridgeTrajectoryState.QueryVelocityCmPerSecond = FVector::ZeroVector;
	BridgeTrajectoryState.LastDeltaTimeSeconds = DeltaTime;
	BridgeTrajectoryState.bInitialized = true;

	AActor* const OwnerActor = GetOwner();
	const FRotator CurrentRotation = OwnerActor ? OwnerActor->GetActorRotation() : FRotator::ZeroRotator;
	const FRotator TargetRotation = FRotator(CurrentRotation.Pitch, CurrentRotation.Yaw + YawDeltaDegrees, CurrentRotation.Roll);

	BridgeIntentState.DesiredFacingYawDegrees = TargetRotation.Yaw;

	bool bRotationAccepted = true;
	if (OwnerActor)
	{
		bRotationAccepted = OwnerActor->SetActorLocationAndRotation(OwnerActor->GetActorLocation(), TargetRotation, true);
	}

	if (!bRotationAccepted)
	{
		Stage2ALastTurnYawDeltaDegrees = 0.0f;
		BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::StartupLocomotion;
		Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequestDenied;
		Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::Denied_ShellRootUnlocked;
		TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShellDenied);
		
		// Clear stale intent/trajectory state on denial
		BridgeIntentState = FBridgeIntentState();
		BridgeTrajectoryState = FBridgeTrajectoryState();

		EmitStage2ALocomotionTelemetry(IntentName, Stage2AMotionSourceName, Stage2ALastLocomotionTerminalState);
		return false;
	}

	Stage2ALastTurnYawDeltaDegrees = YawDeltaDegrees;
	bStage2AWalkIntentActive = false;
	BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Locomoting;
	Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequested;
	Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::Allowed;
	TransitionRuntimeState(EPhysAnimRuntimeState::LocomotionActiveShell);

	EmitStage2ALocomotionTelemetry(IntentName, Stage2AMotionSourceName, Stage2ALastLocomotionTerminalState);

	return true;
}

FVector UPhysAnimComponent::BuildStage2AWalkDeltaCm(float DeltaTime) const
{
	if (DeltaTime <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	const AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FVector::ZeroVector;
	}

	FVector Forward = OwnerActor->GetActorForwardVector();
	Forward.Z = 0.0f;
	if (!Forward.Normalize())
	{
		return FVector::ZeroVector;
	}

	const float RawDelta = Stage2AForwardWalkSpeedCmPerSecond * DeltaTime;
	const float ClampedDelta = FMath::Clamp(RawDelta, 0.0f, Stage2AMaxWalkDeltaPerTickCm);
	return Forward * ClampedDelta;
}

void UPhysAnimComponent::ApplyStage2AKinematicShellWalkDelta(const FVector& DeltaCm)
{
	BridgeShellState.RequestedWorldDeltaCm = DeltaCm;
	Stage2ALastWalkDeltaCm = DeltaCm;
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor || DeltaCm.IsNearlyZero())
	{
		BridgeShellState.AcceptedWorldDeltaCm = FVector::ZeroVector;
		BridgeShellState.AcceptedPlanarVelocityCmPerSecond = FVector::ZeroVector;
		BridgeShellState.bLastMoveBlocked = true;
		return;
	}

	BridgeShellState.bLastMoveBlocked = false;
	const FVector OldLocation = OwnerActor->GetActorLocation();
	const FVector NewLocation = OldLocation + DeltaCm;

	const bool bMoveSuccess = OwnerActor->SetActorLocation(NewLocation, true);

	if (!bMoveSuccess)
	{
		BridgeShellState.bLastMoveBlocked = true;
		BridgeShellState.AcceptedWorldDeltaCm = FVector::ZeroVector;
		BridgeShellState.AcceptedPlanarVelocityCmPerSecond = FVector::ZeroVector;
		return;
	}

	const FVector AcceptedDeltaCm = OwnerActor->GetActorLocation() - OldLocation;
	BridgeShellState.LastAcceptedActorLocation = OwnerActor->GetActorLocation();
	BridgeShellState.AcceptedWorldDeltaCm = AcceptedDeltaCm;
	BridgeShellState.bInitialized = true;
	BridgeShellState.bLastMoveBlocked = AcceptedDeltaCm.IsNearlyZero();

	const float DeltaTimeSeconds = FMath::Max(BridgeTrajectoryState.LastDeltaTimeSeconds, 0.001f);
	BridgeShellState.AcceptedPlanarVelocityCmPerSecond = FVector(AcceptedDeltaCm.X, AcceptedDeltaCm.Y, 0.0f) / DeltaTimeSeconds;
}

void UPhysAnimComponent::EmitStage2ALocomotionTelemetry(const TCHAR* LocomotionIntent, const TCHAR* CapsuleOrShellMotionSource, EStage2ALocomotionTerminalState TerminalState)
{
	LastStage2ALocomotionTelemetryLine = FString::Printf(
		TEXT("root_mode=Stage1_KinematicRoot locomotion_intent=%s policy_output_active=%s capsule_or_shell_motion_source=%s phase3_simroot_attempted=%s terminal_state=%s"),
		LocomotionIntent,
		IsStage2APolicyOutputActive() ? TEXT("true") : TEXT("false"),
		CapsuleOrShellMotionSource,
		bStage2APhase3SimRootAttempted ? TEXT("true") : TEXT("false"),
		Stage2ATerminalStateToString(TerminalState));
	PHYSANIM_LOG(LogPhysAnimBridge, Display, TEXT("%s"), *LastStage2ALocomotionTelemetryLine);
}

double UPhysAnimComponent::GetPhysAnimClockTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
}

void UPhysAnimComponent::HardenStage2ALocomotionState()
{
	if (Stage2ALocomotionRequestState == EBridgeLocomotionRequestState::LocomotionRequested)
	{
		if (!IsStage2APolicyOutputActive())
		{
			Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
		}
	}
}

float UPhysAnimComponent::GetAttachedLoadMassKg() const
{
	USkeletalMeshComponent* const SkeletalMesh = GetMeshComponent();
	if (!SkeletalMesh)
	{
		return 0.0f;
	}

	TArray<USceneComponent*> Children;
	SkeletalMesh->GetChildrenComponents(true, Children);

	float TotalMass = 0.0f;
	for (USceneComponent* Child : Children)
	{
		if (UPhysicsConstraintComponent* Constraint = Cast<UPhysicsConstraintComponent>(Child))
		{
			UPrimitiveComponent* Comp1 = Constraint->OverrideComponent1.IsValid() ? Constraint->OverrideComponent1.Get() : nullptr;
			UPrimitiveComponent* Comp2 = Constraint->OverrideComponent2.IsValid() ? Constraint->OverrideComponent2.Get() : nullptr;

			if (Comp1 && Comp1 != SkeletalMesh && Comp1->IsSimulatingPhysics())
			{
				TotalMass += Comp1->GetMass();
			}
			if (Comp2 && Comp2 != SkeletalMesh && Comp2->IsSimulatingPhysics())
			{
				TotalMass += Comp2->GetMass();
			}
		}
	}

	return TotalMass;
}
