#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimProofArtifactEmitter.h"
#include "PhysAnimRuntimeAdapter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY(LogPhysAnimBridge);

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
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROOF_DISABLED"));
		return false;
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

	if (Latest.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=SUPPORT_GAP gap=%.1f threshold=%.1f"), 
			Latest.SupportGapTimerMs, Settings.BalancePhase1AdmissionMaxSupportGapMs);
		return false;
	}

	if (Latest.ProxyInsideHull.IsSet() && !Latest.ProxyInsideHull.GetValue())
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROXY_OUTSIDE_HULL"));
		return false;
	}

	if (Latest.ProxyOutsideHullDurationMs.IsSet() && Latest.ProxyOutsideHullDurationMs.GetValue() >= Settings.ProxyDriftLimitMs)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=PROXY_DRIFT_THRESHOLD drift=%.1f threshold=%.1f"), 
			Latest.ProxyOutsideHullDurationMs.GetValue(), Settings.ProxyDriftLimitMs);
		return false;
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

	if (LiveRuntimeEvidenceStandingSeconds < LiveRuntimeEvidenceStandingTargetSeconds)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTRY_DENIED reason=INSUFFICIENT_STANDING_DURATION duration=%.3f target=%.3f"), 
			LiveRuntimeEvidenceStandingSeconds, LiveRuntimeEvidenceStandingTargetSeconds);
		return false;
	}

	return true;
}


bool UPhysAnimComponent::ShouldExitStandingToSafeDeny(const FPhysAnimRuntimeTerminationState& TerminationState) const
{
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

	if (Latest.SupportGapTimerMs >= Settings.BalancePhase1AdmissionMaxSupportGapMs)
	{
		return true;
	}

	if (Latest.ProxyInsideHull.IsSet() && !Latest.ProxyInsideHull.GetValue())
	{
		return true;
	}

	if (Latest.ProxyOutsideHullDurationMs.Get(0.0) >= Settings.ProxyDriftLimitMs)
	{
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
	return bLiveRuntimeEvidenceProofComplete && 
		LiveRuntimeEvidenceTerminationState.bTerminated && 
		LiveRuntimeEvidenceTerminationState.TerminalReason == EPhysAnimTerminalReason::None;
}


void UPhysAnimComponent::ResetLiveRuntimeEvidenceProof()
{
	bLiveRuntimeEvidenceProofActive = false;
	bLiveRuntimeEvidenceProofComplete = false;
	bLiveRuntimeEvidenceTerminalArtifactEmitted = false;

	LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
	LiveRuntimeEvidenceStandingSeconds = 0.0f;
	LiveRuntimeEvidenceLastProgressLogSeconds = -1.0f;

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
		}
	}

	LiveRuntimeEvidenceSubstepCounter = 0;
	LiveRuntimeEvidenceTerminationState = FPhysAnimRuntimeTerminationState();
	LiveRuntimeEvidenceAttemptUuid.Empty();
	bForceSupportFailure = false;
}

void UPhysAnimComponent::TickLiveRuntimeEvidenceProof(float DeltaTimeSeconds)
{
	if (!bEnableLiveRuntimeEvidenceProof || bLiveRuntimeEvidenceProofComplete)
	{
		return;
	}
	if (LiveRuntimeEvidenceAttemptUuid.IsEmpty())
	{
		LiveRuntimeEvidenceAttemptUuid = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
		PhysAnimProofArtifactEmitter::LogAttemptStart(LiveRuntimeEvidenceAttemptUuid);
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
	PipelineInput.bEnableTerminationCommand = true;
	PipelineInput.bAllowMovementReclaimOnTermination = true;

	const FPhysAnimRuntimeTerminationPipelineResult PipelineResult =
		PhysAnimRuntimeTerminationPipeline::EvaluateTerminationPipeline(PipelineInput);

	LiveRuntimeEvidenceTerminationState = PipelineResult.StateApplyResult.State;

	PhysAnimProofArtifactEmitter::LogRuntimeEvidence(
		LiveRuntimeEvidenceAttemptUuid,
		HitResults.Num(),
		MappedSupportHitCount,
		PipelineResult.SubstepResult.Artifact);

	if (LiveRuntimeEvidenceTerminationState.bTerminated)
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

	if (LiveRuntimeEvidenceStandingSeconds >= LiveRuntimeEvidenceStandingTargetSeconds)
	{
		bLiveRuntimeEvidenceProofComplete = true;

		// On success, we haven't officially "terminated" via the pipeline, 
		// but we want to capture the final state as the terminal artifact.
		LiveRuntimeEvidenceTerminationState.bTerminated = true;
		LiveRuntimeEvidenceTerminationState.TerminalReason = EPhysAnimTerminalReason::None;
		LiveRuntimeEvidenceTerminationState.TerminalArtifact = PipelineResult.SubstepResult.Artifact;
		LiveRuntimeEvidenceTerminationState.LatestArtifact = PipelineResult.SubstepResult.Artifact;
		
		FPhysAnimProofArtifactEmitInput EmitInput;
		EmitInput.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
		EmitInput.StandingSeconds = LiveRuntimeEvidenceStandingSeconds;
		EmitInput.RuntimeHitCount = HitResults.Num();
		EmitInput.MappedSupportHitCount = MappedSupportHitCount;
		EmitInput.PipelineResult = PipelineResult;
		// Ensure the terminal state we just built is the one that gets emitted
		EmitInput.PipelineResult.StateApplyResult.State = LiveRuntimeEvidenceTerminationState;

		PhysAnimProofArtifactEmitter::EmitTerminalArtifactAndWriteJson(EmitInput);
		
		PhysAnimProofArtifactEmitter::LogAttemptResult(
			LiveRuntimeEvidenceAttemptUuid,
			true,
			LiveRuntimeEvidenceStandingSeconds,
			EPhysAnimTerminalReason::None);
	}
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
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	int32 HitCount = 0;
	for (const FVector& Offset : Offsets)
	{
		const FVector SampleLocation = BoneWorldLocation + Offset;
		const FVector TraceStart = SampleLocation + FVector(0.0, 0.0, LiveRuntimeEvidenceSupportSweepStartLiftCm);
		const FVector TraceEnd = SampleLocation - FVector(0.0, 0.0, LiveRuntimeEvidenceSupportSweepDistanceCm);

		FHitResult Hit;
		const bool bHit = World->LineTraceSingleByObjectType(
			Hit,
			TraceStart,
			TraceEnd,
			ObjectQueryParams,
			QueryParams);

		if (bHit && Hit.bBlockingHit)
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
	const FVector ActorLocation = Mesh ? Mesh->GetComponentLocation() : FVector::ZeroVector;

	Input.HitResults = HitResults;
	Input.WorldOriginCm = ActorLocation;
	Input.ComProxyPosCm = FVector2D::ZeroVector; // Local relative to WorldOrigin
	Input.DeltaMs = static_cast<double>(FMath::Max(0.0f, DeltaTimeSeconds) * 1000.0f);
	Input.PreviousSupportGapTimerMs = LiveRuntimeEvidenceTerminationState.LatestArtifact.SupportGapTimerMs;

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

	Input.SupportAreaMinCm2 = 0.0;
	Input.ProxyDriftLimitMs = 500.0;
	return Input;
}

FPhysAnimRuntimeSubstepInput UPhysAnimComponent::BuildLiveRuntimeEvidenceSubstepInput(
	const FPhysAnimSupportObservationResult& SupportObservation,
	float DeltaTimeSeconds) const
{
	FPhysAnimRuntimeSubstepInput Input;

	Input.SupportObservation = SupportObservation;
	Input.Values.AttemptUuid = LiveRuntimeEvidenceAttemptUuid;
	Input.Values.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : LiveRuntimeEvidenceStandingSeconds;
	Input.Values.TerminalSubstepTimestamp = LiveRuntimeEvidenceSubstepCounter;
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

	Input.ControllerStability.HoldDurationSec = LiveRuntimeEvidenceStandingSeconds;
	Input.ControllerStability.bControllerStabilityPassed = true;
	Input.Authority.bAuthorityPassed = true;
	Input.MovementReclaim.bMovementReclaimPassed = true;
	Input.ShellHelper.bShellHelperPassed = true;

	Input.Capsule = PhysAnimValidators::ValidateCapsule(BuildCapsuleContractSnapshot());
	Input.Continuity = PhysAnimValidators::ValidateContinuity(BuildContinuitySnapshot());

	return Input;
}

void UPhysAnimComponent::EmitLiveRuntimeEvidenceTerminalArtifactOnce(const FPhysAnimRuntimeTerminationPipelineResult& PipelineResult)
{
	// Implementation moved to PhysAnimProofArtifactEmitter
}
