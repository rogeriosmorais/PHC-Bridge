#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimRuntimeAdapter.h"

namespace
{
	constexpr float AutoBalancePreEntryMaxLowerLimbLimitOccupancy = 0.90f;
}

bool UPhysAnimComponent::ShouldAllowBalanceSimulation(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	return RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle ||
		IsBalanceActiveState(RuntimeState);
}

bool UPhysAnimComponent::ShouldRebaselineBridgeStateAfterTransitionFailure(const FString& FailureReason)
{
	if (FailureReason.IsEmpty())
	{
		return false;
	}

	switch (FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(FailureReason))
	{
	case EBalanceReadyConditionOwner::Phase2RootOnExecution:
	case EBalanceReadyConditionOwner::Phase2TopologyEnforcement:
	case EBalanceReadyConditionOwner::ShellAuthorityMaintenance:
	case EBalanceReadyConditionOwner::TransitionRecovery:
		return true;

	default:
		return false;
	}
}





void UPhysAnimComponent::PublishBalanceTransitionFailureReason(const FString& FailureReason)
{
	LastPublishedBalanceTransitionFailureReason = FailureReason;
}

void UPhysAnimComponent::ClearPublishedBalanceTransitionFailureReason()
{
	LastPublishedBalanceTransitionFailureReason.Reset();
}

void UPhysAnimComponent::RecoverBridgeActiveStateAfterBalanceTransitionFailure(const FString& FailureReason)
{
	PublishBalanceTransitionFailureReason(FailureReason);

	if (!ShouldRebaselineBridgeStateAfterTransitionFailure(FailureReason))
	{
		return;
	}

	RuntimeInstabilityState = {};
	LastRuntimeInstabilityDiagnostics = {};
	RecoveryPreEntryTelemetrySkipFrames = 1;
	ResetBridgeLocomotionAuthorityState();
	ReanchorShellCouplingReferenceToCurrentRoot(TEXT("transition_failure_recovery"));

	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimBalance] PHASE2_RECOVERY_REBASELINE reason=%s locomotionReset=1 watchdogReset=1 shellReferenceReanchored=1 preEntryTelemetryHoldFrames=%d"),
		*FailureReason,
		RecoveryPreEntryTelemetrySkipFrames);
}

bool UPhysAnimComponent::EvaluateBalancePerturbationRuntimeReadiness(
	EPhysAnimRuntimeState RuntimeState,
	int32 HighestUnlockedBringUpGroupIndex,
	int32 BringUpGroupCount,
	bool bFinalBringUpRampActive,
	float PolicyInfluenceAlpha,
	float PolicyInfluenceThreshold,
	bool bHasPendingBodyModifierCachedResets,
	bool bHasPelvisBody,
	bool bPelvisBodySimulating,
	FString* OutFailureReason)
{
	const auto SetFailure = [&](const TCHAR* Reason) -> bool
	{
		if (OutFailureReason)
		{
			*OutFailureReason = Reason;
		}
		return false;
	};
	if (OutFailureReason)
	{
		OutFailureReason->Reset();
	}

	if (!IsBalanceActiveState(RuntimeState))
	{
		return SetFailure(TEXT("invalidRuntimeState"));
	}

	const int32 CoreFinalBringUpGroupIndex = FMath::Min(1, BringUpGroupCount - 1);
	if (HighestUnlockedBringUpGroupIndex < CoreFinalBringUpGroupIndex)
	{
		return SetFailure(TEXT("bringUpIncomplete"));
	}

	if (!bFinalBringUpRampActive)
	{
		return SetFailure(TEXT("finalGroupRampInactive"));
	}

	if (PolicyInfluenceAlpha < PolicyInfluenceThreshold)
	{
		return SetFailure(TEXT("policyInfluenceBelowThreshold"));
	}

	if (bHasPendingBodyModifierCachedResets)
	{
		return SetFailure(TEXT("deferredResetsPending"));
	}

	if (!bHasPelvisBody)
	{
		return SetFailure(TEXT("pelvisBodyMissing"));
	}

	if (!bPelvisBodySimulating)
	{
		return SetFailure(TEXT("pelvisBodyNotSimulating"));
	}

	return true;
}


bool UPhysAnimComponent::IsBalancePerturbationRuntimeReady(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	float* OutPolicyInfluenceAlpha,
	FString* OutFailureReason) const
{
	const float PolicyInfluenceAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
	if (OutPolicyInfluenceAlpha)
	{
		*OutPolicyInfluenceAlpha = PolicyInfluenceAlpha;
	}

	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	FBodyInstance* const PelvisBody = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
	const int32 CoreFinalBringUpGroupIndex = FMath::Min(1, GetBringUpGroupCount() - 1);
	const bool bReady = EvaluateBalancePerturbationRuntimeReadiness(
		RuntimeState,
		HighestUnlockedBringUpGroupIndex,
		GetBringUpGroupCount(),
		IsBringUpGroupControlRampActive(CoreFinalBringUpGroupIndex),
		PolicyInfluenceAlpha,
		BalanceReadyPolicyInfluenceThreshold,
		!PendingBodyModifierCachedResetNames.IsEmpty(),
		PelvisBody != nullptr,
		PelvisBody && PelvisBody->IsInstanceSimulatingPhysics(),
		OutFailureReason);
	if (!bReady && IsBalanceActiveState(RuntimeState))
	{
		EPhysicsMovementType PelvisModifierMovementType = EPhysicsMovementType::Static;
		if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
		{
			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* const Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, PelvisModifierName))
			{
				PelvisModifierMovementType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		int32 TotalSimCount = 0;
		if (Mesh)
		{
			for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
			{
				if (const FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
				{
					if (BodyInstance->IsInstanceSimulatingPhysics())
					{
						++TotalSimCount;
					}
				}
			}
		}

		static TMap<const UPhysAnimComponent*, double> LastRecoveryFailureLogTimes;
		const double WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		double& LastRecoveryFailureLogTimeSeconds = LastRecoveryFailureLogTimes.FindOrAdd(this);
		if (LastRecoveryFailureLogTimeSeconds <= 0.0 || (WorldTimeSeconds - LastRecoveryFailureLogTimeSeconds) >= 0.25)
		{
			const TCHAR* const FailureReasonText =
				(OutFailureReason && !OutFailureReason->IsEmpty()) ? **OutFailureReason : TEXT("unknown");
			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnimBalance] RECOVERY_READY_FAIL runtimeState=%s reason=%s pelvisRawSim=%d pelvisModifier=%s pendingResets=%d policyAlpha=%.2f simCount=%d"),
				GetRuntimeStateName(RuntimeState),
				FailureReasonText,
				PelvisBody && PelvisBody->IsInstanceSimulatingPhysics() ? 1 : 0,
				GetPhysicsMovementTypeName(PelvisModifierMovementType),
				PendingBodyModifierCachedResetNames.Num(),
				PolicyInfluenceAlpha,
				TotalSimCount);
			LastRecoveryFailureLogTimeSeconds = WorldTimeSeconds;
		}
	}

	return bReady;
}


bool UPhysAnimComponent::IsBalanceScenarioQuietEnough(
	const FVector& PelvisLinearVelocity,
	const FVector& PelvisAngularVelocityDegPerSec,
	float TiltDeg,
	bool bIdlePoseActive,
	bool bNoLocomotionStateActive) const
{
	return PelvisLinearVelocity.Size() <= BalanceQuietLinearSpeedThresholdCmPerSec &&
		PelvisAngularVelocityDegPerSec.Size() <= BalanceQuietTiltThresholdDeg * 2.0f && // Use tilt threshold as proxy for angular speed
		TiltDeg <= BalanceQuietTiltThresholdDeg &&
		bIdlePoseActive &&
		bNoLocomotionStateActive;
}


bool UPhysAnimComponent::IsBalanceEntryState(EPhysAnimRuntimeState State)
{
	return (State == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
			State == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
			State == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
			State == EPhysAnimRuntimeState::BalanceEntry_Settle);
}

EPhysAnimRuntimeState UPhysAnimComponent::GetPublicBalanceEntryRuntimeState() const
{
	return RuntimeState;
}


bool UPhysAnimComponent::TryGetPublicBalanceEntryRuntimeState(EPhysAnimRuntimeState& OutState) const
{
	OutState = GetPublicBalanceEntryRuntimeState();
	return IsBalanceEntryState(OutState);
}


bool UPhysAnimComponent::IsBalanceActiveState(EPhysAnimRuntimeState State)
{
	return State == EPhysAnimRuntimeState::BalanceActive_Standing ||
		State == EPhysAnimRuntimeState::BalanceActive_Recovery ||
		State == EPhysAnimRuntimeState::LocomotionActiveShell ||
		State == EPhysAnimRuntimeState::LocomotionActiveShellDenied;
}


bool UPhysAnimComponent::IsLocomotionActiveShellState(EPhysAnimRuntimeState State)
{
	return State == EPhysAnimRuntimeState::LocomotionActiveShell ||
		State == EPhysAnimRuntimeState::LocomotionActiveShellDenied;
}


void UPhysAnimComponent::ResetBalanceScenarioQuietGate(const FString& Reason)
{
	UWorld* const World = GetWorld();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	AActor* const OwnerActor = GetOwner();
	const double WorldTime = World ? World->GetTimeSeconds() : 0.0;
	const FName PelvisName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* const PelvisBody = Mesh ? Mesh->GetBodyInstance(PelvisName) : nullptr;
	const FTransform PelvisTransform = PelvisBody
		? PelvisBody->GetUnrealWorldTransform()
		: (Mesh ? Mesh->GetBoneTransform(Mesh->GetBoneIndex(PelvisName)) : FTransform::Identity);

	if (BalanceScenarios.IsValidIndex(ActiveBalanceScenarioIndex))
	{
		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimBalance] [%d/%d %s] READY_ABORT: %s. Resetting quiet gate."),
			ActiveBalanceScenarioIndex + 1,
			BalanceScenarios.Num(),
			*BalanceScenarios[ActiveBalanceScenarioIndex].Name,
			*Reason);
	}

	bBalanceScenarioAwaitingStableWindow = true;
	BalanceScenarioStartTimeSeconds = WorldTime;
	BalanceScenarioStableWindowStartTimeSeconds = WorldTime;
	BalanceScenarioQuietWindowAccumulatedSeconds = 0.0;
	BalanceScenarioRecoveryStableAccumulatedSeconds = 0.0;
	LastBalanceStabilizationLogTimeSeconds = -1.0;
	LastBalanceScenarioImpactTimeSeconds = -1.0;
	BalanceScenarioImpactPelvisLinearVelPre = FVector::ZeroVector;
	BalanceScenarioImpactPelvisLinearVelPost = FVector::ZeroVector;
	BalanceScenarioImpactPelvisAngularVelPre = FVector::ZeroVector;
	BalanceScenarioImpactPelvisAngularVelPost = FVector::ZeroVector;
	BalanceScenarioPeakPelvisVel = 0.0f;
	BalanceScenarioPeakPelvisTilt = 0.0f;
	BalanceScenarioPeakPelvisAngularSpeed = 0.0f;
	BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
	BalanceScenarioPeakActorDisplacementCm = 0.0f;

	if (OwnerActor)
	{
		BalanceScenarioStartActorLocation = OwnerActor->GetActorLocation();
	}
	BalanceScenarioStartPelvisLocation = PelvisTransform.GetLocation();
	BalanceScenarioStartPelvisRotation = PelvisTransform.GetRotation();
}


void UPhysAnimComponent::UpdateBalancePerturbation(float DeltaTime)
{
	if (!BalanceScenarios.IsValidIndex(ActiveBalanceScenarioIndex))
	{
		return;
	}

	UWorld* const World = GetWorld();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!World || !Mesh || !OwnerActor)
	{
		return;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	FPhysAnimBalanceScenario& Scenario = BalanceScenarios[ActiveBalanceScenarioIndex];
	const double WorldTime = World->GetTimeSeconds();
	const double Elapsed = BalanceScenarioStartTimeSeconds >= 0.0 ? (WorldTime - BalanceScenarioStartTimeSeconds) : 0.0;
	const FName PelvisName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PelvisName);
	const FTransform PelvisTransform = PelvisBody
		? PelvisBody->GetUnrealWorldTransform()
		: Mesh->GetBoneTransform(Mesh->GetBoneIndex(PelvisName));
	const FVector PelvisLinearVelocity = PelvisBody
		? PelvisBody->GetUnrealWorldVelocity()
		: Mesh->GetPhysicsLinearVelocity(PelvisName);
	const FVector PelvisAngularVelocityDegPerSec = PelvisBody
		? FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians())
		: Mesh->GetPhysicsAngularVelocityInDegrees(PelvisName);
	const float PelvisSpeed = PelvisLinearVelocity.Size();
	const float PelvisAngularSpeed = PelvisAngularVelocityDegPerSec.Size();
	const float TiltDeg = FMath::RadiansToDegrees(BalanceScenarioStartPelvisRotation.AngularDistance(PelvisTransform.GetRotation()));
	const bool bIdlePoseActive =
		LastValidPoseSearchResult.SelectedAnim == nullptr ||
		IsBridgePoseSearchIdleResult(LastValidPoseSearchResult);
	const bool bNoLocomotionStateActive = BridgeLocomotionAuthorityState == EBridgeLocomotionAuthorityState::Idle;
	float PolicyInfluenceAlpha = 0.0f;
	FString RuntimeReadyReason;
	const bool bRuntimeReady = IsBalancePerturbationRuntimeReady(
		EffectiveSettings,
		&PolicyInfluenceAlpha,
		&RuntimeReadyReason);
	const bool bQuietEnough = bRuntimeReady &&
		IsBalanceScenarioQuietEnough(
			PelvisLinearVelocity,
			PelvisAngularVelocityDegPerSec,
			TiltDeg,
			bIdlePoseActive,
			bNoLocomotionStateActive);

	if (Elapsed > PhysAnimComponentInternal::BalanceModeTotalTimeoutSeconds)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimBalance] Scenario watchdog timeout reached (%.1fs). Stopping mode."),
			Elapsed);
		StopBalancePerturbationMode();
		return;
	}

	if (bBalanceScenarioAwaitingStableWindow)
	{
		// Treat any new deferred cached-target reset scheduled after simulated settle has begun as a settle reset event.
		// This forces the quiet accumulation to restart so bridge setup pops don't contaminate the baseline.
		if (!PendingBodyModifierCachedResetNames.IsEmpty() && BalanceScenarioQuietWindowAccumulatedSeconds > 0.0)
		{
			ResetBalanceScenarioQuietGate(TEXT("promotionDuringSettle"));
			return;
		}

		BalanceScenarioQuietWindowAccumulatedSeconds = bQuietEnough
			? (BalanceScenarioQuietWindowAccumulatedSeconds + DeltaTime)
			: 0.0;

		if (LastBalanceStabilizationLogTimeSeconds < 0.0 || (WorldTime - LastBalanceStabilizationLogTimeSeconds) >= 1.0)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Log,
				TEXT("[PhysAnimBalance] [%d/%d %s] QUIET_GATE: bridgeReady=%s reason=%s policyAlpha=%.2f/%.2f speed=%.1f/%.1f tilt=%.1f/%.1f idlePose=%s locomotionState=%s quiet=%.2f/%.2fs"),
				ActiveBalanceScenarioIndex + 1,
				BalanceScenarios.Num(),
				*Scenario.Name,
				bRuntimeReady ? TEXT("true") : TEXT("false"),
				bRuntimeReady ? TEXT("ready") : *RuntimeReadyReason,
				PolicyInfluenceAlpha,
				BalanceReadyPolicyInfluenceThreshold,
				PelvisSpeed,
				BalanceQuietLinearSpeedThresholdCmPerSec,
				TiltDeg,
				BalanceQuietTiltThresholdDeg,
				bIdlePoseActive ? TEXT("true") : TEXT("false"),
				bNoLocomotionStateActive ? TEXT("idle") : TEXT("active"),
				BalanceScenarioQuietWindowAccumulatedSeconds,
				BalanceQuietWindowRequiredSeconds);
			LastBalanceStabilizationLogTimeSeconds = WorldTime;
		}

		if (BalanceScenarioQuietWindowAccumulatedSeconds >= BalanceQuietWindowRequiredSeconds)
		{
			// Transition to READY without resetting the scenario timer.
			// This ensures the handover ramp finished during the quiet window and stays at 1.0 during the test.
			bBalanceScenarioAwaitingStableWindow = false;
			BalanceScenarioStableWindowStartTimeSeconds = WorldTime;
			BalanceScenarioRecoveryStableAccumulatedSeconds = 0.0;
			LastBalanceScenarioImpactTimeSeconds = -1.0;
			BalanceScenarioPeakPelvisVel = 0.0f;
			BalanceScenarioPeakPelvisTilt = 0.0f;
			BalanceScenarioPeakPelvisAngularSpeed = 0.0f;
			BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
			BalanceScenarioPeakActorDisplacementCm = 0.0f;
			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnimBalance] [%d/%d %s] READY: quietWindow=%.2fs triggerDelay=%.2fs policyAlpha=%.2f."),
				ActiveBalanceScenarioIndex + 1,
				BalanceScenarios.Num(),
				*Scenario.Name,
				BalanceScenarioQuietWindowAccumulatedSeconds,
				Scenario.TriggerDelaySeconds,
				PolicyInfluenceAlpha);
		}
		return;
	}

	if (!Scenario.bTriggered)
	{
		if (!bQuietEnough)
		{
			FString ResetReason = bRuntimeReady
				? FString::Printf(
					TEXT("preTriggerDisturbance speed=%.1f tilt=%.1f idlePose=%s locomotion=%s"),
					PelvisSpeed,
					TiltDeg,
					bIdlePoseActive ? TEXT("true") : TEXT("false"),
					bNoLocomotionStateActive ? TEXT("idle") : TEXT("active"))
				: FString::Printf(
					TEXT("runtimeNotReady=%s policyAlpha=%.2f"),
					*RuntimeReadyReason,
					PolicyInfluenceAlpha);
			ResetBalanceScenarioQuietGate(ResetReason);
			return;
		}

		if (Elapsed < Scenario.TriggerDelaySeconds)
		{
			return;
		}

		BalanceScenarioStartActorLocation = OwnerActor->GetActorLocation();
		BalanceScenarioStartPelvisLocation = PelvisTransform.GetLocation();
		BalanceScenarioStartPelvisRotation = PelvisTransform.GetRotation();
		BalanceScenarioImpactPelvisLinearVelPre = PelvisLinearVelocity;
		BalanceScenarioImpactPelvisLinearVelPost = PelvisLinearVelocity;
		BalanceScenarioImpactPelvisAngularVelPre = PelvisAngularVelocityDegPerSec;
		BalanceScenarioImpactPelvisAngularVelPost = PelvisAngularVelocityDegPerSec;
		LastBalanceScenarioImpactTimeSeconds = WorldTime;
		BalanceScenarioRecoveryStableAccumulatedSeconds = 0.0;
		BalanceScenarioPeakPelvisVel = PelvisSpeed;
		BalanceScenarioPeakPelvisTilt = 0.0f;
		BalanceScenarioPeakPelvisAngularSpeed = PelvisAngularSpeed;
		BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
		BalanceScenarioPeakActorDisplacementCm = 0.0f;

		if (!Scenario.Name.Contains(TEXT("NoPush")))
		{
			if (!ApplyPelvisImpulse(Scenario.Direction, Scenario.Magnitude))
			{
				FinalizeBalanceScenario(false, TEXT("INVALID_PERTURBATION"));
				return;
			}
		}
		else
		{
			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnimBalance] [%d/%d %s] TRIGGER: skipped (NoPush baseline)."),
				ActiveBalanceScenarioIndex + 1,
				BalanceScenarios.Num(),
				*Scenario.Name);
		}

		Scenario.bTriggered = true;
		return;
	}

	if (LastBalanceScenarioImpactTimeSeconds < 0.0)
	{
		LastBalanceScenarioImpactTimeSeconds = WorldTime;
	}

	const double RecoveryElapsedSeconds = WorldTime - LastBalanceScenarioImpactTimeSeconds;
	const float PelvisDisplacementCm = FVector::Dist(PelvisTransform.GetLocation(), BalanceScenarioStartPelvisLocation);
	const float ActorDisplacementCm = FVector::Dist2D(OwnerActor->GetActorLocation(), BalanceScenarioStartActorLocation);
	const float HeightErrorCm = FMath::Abs(PelvisTransform.GetLocation().Z - BalanceScenarioStartPelvisLocation.Z);
	BalanceScenarioPeakPelvisVel = FMath::Max(BalanceScenarioPeakPelvisVel, PelvisSpeed);
	BalanceScenarioPeakPelvisTilt = FMath::Max(BalanceScenarioPeakPelvisTilt, TiltDeg);
	BalanceScenarioPeakPelvisAngularSpeed = FMath::Max(BalanceScenarioPeakPelvisAngularSpeed, PelvisAngularSpeed);
	BalanceScenarioPeakPelvisDisplacementCm = FMath::Max(BalanceScenarioPeakPelvisDisplacementCm, PelvisDisplacementCm);
	BalanceScenarioPeakActorDisplacementCm = FMath::Max(BalanceScenarioPeakActorDisplacementCm, ActorDisplacementCm);

	const bool bLocomotionEntryDetected =
		LastValidPoseSearchResult.SelectedAnim != nullptr &&
		!IsBridgePoseSearchIdleResult(LastValidPoseSearchResult);
	if (bLocomotionEntryDetected)
	{
		FinalizeBalanceScenario(false, TEXT("LOCOMOTION_ENTRY"));
		return;
	}

	if (PelvisTransform.GetLocation().Z < (BalanceScenarioStartPelvisLocation.Z - BalanceFallHeightThresholdCm))
	{
		FinalizeBalanceScenario(false, TEXT("FALL"));
		return;
	}

	const bool bRecoveredNow =
		PelvisSpeed <= BalanceRecoveryVelocityThresholdCmPerSec &&
		TiltDeg <= BalanceRecoveryTiltThresholdDeg &&
		HeightErrorCm <= BalanceRecoveryHeightToleranceCm;
	BalanceScenarioRecoveryStableAccumulatedSeconds = bRecoveredNow
		? (BalanceScenarioRecoveryStableAccumulatedSeconds + DeltaTime)
		: 0.0;

	if (BalanceScenarioRecoveryStableAccumulatedSeconds >= BalanceRecoveryStableHoldSeconds)
	{
		FinalizeBalanceScenario(true, TEXT("RECOVERED"));
		return;
	}

	if (RecoveryElapsedSeconds >= Scenario.RecoveryTimeoutSeconds)
	{
		FinalizeBalanceScenario(false, TEXT("TIMEOUT"));
	}
}


bool UPhysAnimComponent::ApplyPelvisImpulse(EPhysAnimPerturbationDirection Direction, EPhysAnimPerturbationMagnitude Magnitude)
{
	UWorld* const World = GetWorld();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	if (!World || !Mesh)
	{
		return false;
	}

	const FName PelvisName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PelvisName);
	if (!PelvisBody || !PelvisBody->IsInstanceSimulatingPhysics())
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] FAILED: pelvis body not found or not simulating."));
		return false;
	}

	FVector ShoveDirection = FVector::ZeroVector;
	switch (Direction)
	{
	case EPhysAnimPerturbationDirection::Forward:
		ShoveDirection = FVector(1.0f, 0.0f, 0.0f);
		break;
	case EPhysAnimPerturbationDirection::Backward:
		ShoveDirection = FVector(-1.0f, 0.0f, 0.0f);
		break;
	case EPhysAnimPerturbationDirection::Left:
		ShoveDirection = FVector(0.0f, -1.0f, 0.0f);
		break;
	case EPhysAnimPerturbationDirection::Right:
		ShoveDirection = FVector(0.0f, 1.0f, 0.0f);
		break;
	}

	float TargetDeltaVCmPerSec = 0.0f;
	switch (Magnitude)
	{
	case EPhysAnimPerturbationMagnitude::Small:
		TargetDeltaVCmPerSec = PhysAnimComponentInternal::BalanceTargetDeltaVSmall;
		break;
	case EPhysAnimPerturbationMagnitude::Medium:
		TargetDeltaVCmPerSec = PhysAnimComponentInternal::BalanceTargetDeltaVMedium;
		break;
	case EPhysAnimPerturbationMagnitude::Large:
		TargetDeltaVCmPerSec = PhysAnimComponentInternal::BalanceTargetDeltaVLarge;
		break;
	}

	const float PelvisMassKg = PelvisBody->GetBodyMass();
	const float ImpulseMagnitude = PelvisMassKg * TargetDeltaVCmPerSec;
	const FVector ImpulseVector = ShoveDirection * ImpulseMagnitude;

	BalanceScenarioImpactPelvisLinearVelPre = PelvisBody->GetUnrealWorldVelocity();
	BalanceScenarioImpactPelvisAngularVelPre = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());
	PelvisBody->AddImpulse(ImpulseVector, false);
	BalanceScenarioImpactPelvisLinearVelPost = PelvisBody->GetUnrealWorldVelocity();
	BalanceScenarioImpactPelvisAngularVelPost = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());
	LastBalanceScenarioImpactTimeSeconds = World->GetTimeSeconds();

	const float MeasuredDeltaV = (BalanceScenarioImpactPelvisLinearVelPost - BalanceScenarioImpactPelvisLinearVelPre).Size();
	const bool bValidResponse = MeasuredDeltaV >= BalanceResponseVelocityThresholdCmPerSec;
	ActivatedStandingStabilityMetrics.PerturbationMeasuredDeltaVCmPerSecond = FMath::Max(
		ActivatedStandingStabilityMetrics.PerturbationMeasuredDeltaVCmPerSecond,
		static_cast<double>(MeasuredDeltaV));
	ActivatedStandingStabilityMetrics.bPhysicalPerturbationApplied = true;

	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimBalance] TRIGGER: scenario=%s impulse=(%.1f,%.1f,%.1f) mass=%.2fkg targetDv=%.1f preLin=(%.1f,%.1f,%.1f) postLin=(%.1f,%.1f,%.1f) preAng=(%.1f,%.1f,%.1f) postAng=(%.1f,%.1f,%.1f) measuredDv=%.1f valid=%s"),
		BalanceScenarios.IsValidIndex(ActiveBalanceScenarioIndex) ? *BalanceScenarios[ActiveBalanceScenarioIndex].Name : TEXT("Unknown"),
		ImpulseVector.X,
		ImpulseVector.Y,
		ImpulseVector.Z,
		PelvisMassKg,
		TargetDeltaVCmPerSec,
		BalanceScenarioImpactPelvisLinearVelPre.X,
		BalanceScenarioImpactPelvisLinearVelPre.Y,
		BalanceScenarioImpactPelvisLinearVelPre.Z,
		BalanceScenarioImpactPelvisLinearVelPost.X,
		BalanceScenarioImpactPelvisLinearVelPost.Y,
		BalanceScenarioImpactPelvisLinearVelPost.Z,
		BalanceScenarioImpactPelvisAngularVelPre.X,
		BalanceScenarioImpactPelvisAngularVelPre.Y,
		BalanceScenarioImpactPelvisAngularVelPre.Z,
		BalanceScenarioImpactPelvisAngularVelPost.X,
		BalanceScenarioImpactPelvisAngularVelPost.Y,
		BalanceScenarioImpactPelvisAngularVelPost.Z,
		MeasuredDeltaV,
		bValidResponse ? TEXT("true") : TEXT("false"));
	return true;
}


void UPhysAnimComponent::FinalizeBalanceScenario(bool bSuccess, const FString& Reason)
{
	if (!BalanceScenarios.IsValidIndex(ActiveBalanceScenarioIndex))
	{
		return;
	}

	FPhysAnimBalanceScenario& Scenario = BalanceScenarios[ActiveBalanceScenarioIndex];
	Scenario.bCompleted = true;

	UWorld* const World = GetWorld();
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	AActor* const OwnerActor = GetOwner();
	const FName PelvisName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* const PelvisBody = Mesh ? Mesh->GetBodyInstance(PelvisName) : nullptr;
	const FTransform PelvisTransform = PelvisBody
		? PelvisBody->GetUnrealWorldTransform()
		: (Mesh ? Mesh->GetBoneTransform(Mesh->GetBoneIndex(PelvisName)) : FTransform::Identity);

	const double Duration = (World && LastBalanceScenarioImpactTimeSeconds >= 0.0)
		? (World->GetTimeSeconds() - LastBalanceScenarioImpactTimeSeconds)
		: 0.0;
	const float MeasuredDeltaV = (BalanceScenarioImpactPelvisLinearVelPost - BalanceScenarioImpactPelvisLinearVelPre).Size();
	const bool bNoPushScenario = Scenario.Name.Contains(TEXT("NoPush"));
	const bool bInvalidPerturbation = !bNoPushScenario && MeasuredDeltaV < BalanceResponseVelocityThresholdCmPerSec;
	const bool bContaminated = BalanceScenarioPeakActorDisplacementCm > BalanceShellContaminationDisplacementCm;
	const float FinalHeightErrorCm = FMath::Abs(PelvisTransform.GetLocation().Z - BalanceScenarioStartPelvisLocation.Z);

	FString FinalStatus = bSuccess ? TEXT("PASSED") : TEXT("FAILED");
	if (bInvalidPerturbation)
	{
		FinalStatus = TEXT("INVALID");
	}
	else if (bContaminated)
	{FinalStatus = TEXT("CONTAMINATED");
	}

	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimBalance] [%d/%d %s] RESULT: %s recoveryTime=%.2fs measuredDv=%.1f peakPelvisVel=%.1f peakPelvisAng=%.1f peakTilt=%.1f peakPelvisDisp=%.1f peakActorDisp=%.1f finalHeightErr=%.1f thresholds[response=%.1f recoveryVel=%.1f recoveryTilt=%.1f recoveryHeight=%.1f contam=%.1f] reason=%s"),
		ActiveBalanceScenarioIndex + 1,
		BalanceScenarios.Num(),
		*Scenario.Name,
		*FinalStatus,
		Duration,
		MeasuredDeltaV,
		BalanceScenarioPeakPelvisVel,
		BalanceScenarioPeakPelvisAngularSpeed,
		BalanceScenarioPeakPelvisTilt,
		BalanceScenarioPeakPelvisDisplacementCm,
		BalanceScenarioPeakActorDisplacementCm,
		FinalHeightErrorCm,
		BalanceResponseVelocityThresholdCmPerSec,
		BalanceRecoveryVelocityThresholdCmPerSec,
		BalanceRecoveryTiltThresholdDeg,
		BalanceRecoveryHeightToleranceCm,
		BalanceShellContaminationDisplacementCm,
		*Reason);

	++ActiveBalanceScenarioIndex;
	if (ActiveBalanceScenarioIndex < BalanceScenarios.Num())
	{
		BalanceScenarioStartTimeSeconds = World ? World->GetTimeSeconds() : 0.0;
		bBalanceScenarioAwaitingStableWindow = true;
		BalanceScenarioStableWindowStartTimeSeconds = BalanceScenarioStartTimeSeconds;
		BalanceScenarioQuietWindowAccumulatedSeconds = 0.0;
		BalanceScenarioRecoveryStableAccumulatedSeconds = 0.0;
		LastBalanceStabilizationLogTimeSeconds = -1.0;
		LastBalanceScenarioImpactTimeSeconds = -1.0;
		BalanceScenarioPeakPelvisVel = 0.0f;
		BalanceScenarioPeakPelvisTilt = 0.0f;
		BalanceScenarioPeakPelvisAngularSpeed = 0.0f;
		BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
		BalanceScenarioPeakActorDisplacementCm = 0.0f;
		if (OwnerActor)
		{
			BalanceScenarioStartActorLocation = OwnerActor->GetActorLocation();
		}
		BalanceScenarioStartPelvisLocation = PelvisTransform.GetLocation();
		BalanceScenarioStartPelvisRotation = PelvisTransform.GetRotation();
	}
	else
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] All scenarios completed."));
		StopBalancePerturbationMode();
	}
}


bool UPhysAnimComponent::EvaluateBalanceModeQueueGates(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutReason) const
{
	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive)
	{
		OutReason = TEXT("command_context_invalid");
		return false;
	}

	const int32 CoreFinalBringUpGroupIndex = FMath::Min(1, GetBringUpGroupCount() - 1);
	if (HighestUnlockedBringUpGroupIndex < CoreFinalBringUpGroupIndex)
	{
		OutReason = TEXT("queue_bring_up_incomplete");
		return false;
	}

	if (!IsBringUpGroupControlRampActive(CoreFinalBringUpGroupIndex))
	{
		OutReason = TEXT("queue_final_group_ramp_inactive");
		return false;
	}

	const float PolicyAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
	if (PolicyAlpha < EffectiveSettings.BalanceEntryMinPolicyAlpha)
	{
		OutReason = TEXT("queue_policy_influence_below_threshold");
		return false;
	}

	// Unified sim-count gates (Section 10/20)
	TArray<FName> SimulatingBones;
	GetSimulatingBodies(SimulatingBones);
	const int32 TotalSim = SimulatingBones.Num();
	
	if (TotalSim > EffectiveSettings.BalanceEntryMaxSimCount)
	{
		OutReason = TEXT("queue_sim_count_too_high");
		return false;
	}

	int32 DistalSim = 0;
	for (const FName& Bone : SimulatingBones)
	{
		if (ResolveBringUpGroupIndex(Bone) > 0) DistalSim++;
	}

	if (DistalSim > EffectiveSettings.BalanceEntryMaxDistalSimCount)
	{
		OutReason = TEXT("queue_distal_sim_too_high");
		return false;
	}

	return true;
}


bool UPhysAnimComponent::EvaluateBalanceBridgeActivePreEntryPrerequisitesFromTelemetry(
	EPhysAnimRuntimeState RuntimeState,
	bool bHasPendingBodyModifierCachedResets,
	bool bIdlePoseActive,
	EBridgeLocomotionAuthorityState LocomotionAuthorityState,
	float RootLinearSpeedCmPerSecond,
	float RootAngularSpeedDegPerSecond,
	float MaxBodyLinearSpeedCmPerSecond,
	float MaxBodyAngularSpeedDegPerSecond,
	float RootTiltDeg,
	float QuietTiltThresholdDeg,
	int32 NumLowerLimbTargetsConsidered,
	float MaxLowerLimbLimitProxyDegrees,
	float MaxLowerLimbLimitOccupancy,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	FString& OutReason)
{
	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive)
	{
		OutReason = TEXT("preentry_command_context_invalid");
		return false;
	}

	if (bHasPendingBodyModifierCachedResets)
	{
		OutReason = TEXT("preentry_pending_resets");
		return false;
	}

	if (!bIdlePoseActive)
	{
		OutReason = TEXT("preentry_idle_pose_inactive");
		return false;
	}

	if (LocomotionAuthorityState != EBridgeLocomotionAuthorityState::Idle)
	{
		OutReason = TEXT("preentry_locomotion_active");
		return false;
	}

	if (RootLinearSpeedCmPerSecond > EffectiveSettings.BalancePhase1MaxRootLinearBaseline)
	{
		OutReason = TEXT("preentry_root_linear_above_phase1_baseline");
		return false;
	}

	if (RootAngularSpeedDegPerSecond > EffectiveSettings.BalancePhase1MaxRootAngularBaseline)
	{
		OutReason = TEXT("preentry_root_angular_above_phase1_baseline");
		return false;
	}

	if (MaxBodyLinearSpeedCmPerSecond > EffectiveSettings.BalancePhase1LateValidateAdmissionMaxSimulatedBoneLinearSpeed)
	{
		OutReason = TEXT("preentry_body_linear_above_phase1_admission");
		return false;
	}

	if (MaxBodyAngularSpeedDegPerSecond > EffectiveSettings.BalancePhase1LateValidateAdmissionMaxSimulatedBoneAngularSpeed)
	{
		OutReason = TEXT("preentry_body_angular_above_phase1_admission");
		return false;
	}

	if (RootTiltDeg > QuietTiltThresholdDeg)
	{
		OutReason = TEXT("preentry_tilt_high");
		return false;
	}

	if (NumLowerLimbTargetsConsidered > 0 &&
		MaxLowerLimbLimitProxyDegrees > UE_SMALL_NUMBER &&
		MaxLowerLimbLimitOccupancy > AutoBalancePreEntryMaxLowerLimbLimitOccupancy)
	{
		OutReason = TEXT("preentry_lower_limb_limit_occupancy_high");
		return false;
	}

	OutReason = TEXT("ready");
	return true;
}


bool UPhysAnimComponent::EvaluateBalanceBridgeActivePreEntryPrerequisites(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutReason) const
{
	FString RootTiltSource;
	const float RootTiltDeg = ResolvePhase1Uprightness(GetMeshComponent(), GetOwner(), PhysAnimBridge::GetRootBoneName(), RootTiltSource);
	const FPhysAnimControlTargetDiagnostics& ControlTargetDiagnostics = GetLastControlTargetDiagnostics();
	return EvaluateBalanceBridgeActivePreEntryPrerequisitesFromTelemetry(
		RuntimeState,
		!PendingBodyModifierCachedResetNames.IsEmpty(),
		IsIdlePoseActive(),
		BridgeLocomotionAuthorityState,
		LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond,
		LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond,
		LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond,
		LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond,
		RootTiltDeg,
		BalanceQuietTiltThresholdDeg,
		ControlTargetDiagnostics.NumLowerLimbTargetsConsidered,
		ControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees,
		ControlTargetDiagnostics.MaxLowerLimbLimitOccupancy,
		EffectiveSettings,
		OutReason);
}


void UPhysAnimComponent::QueueBalanceModeStartRequest(const FString& Reason)
{
	bPendingBalanceModeStartRequest = true;
	bPendingBalanceModeStartAttemptIssued = false;
	PendingBalanceModeStartReason = Reason;
	PendingBalanceModeRequestTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
	
	static FString LastQueuedReason;
	if (Reason != LastQueuedReason)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Request status: balance_start_queued. reason=%s"), *Reason);
		LastQueuedReason = Reason;
	}

}

bool UPhysAnimComponent::ShouldAttemptAutoTriggeredBalanceStart(
	const EPhysAnimRuntimeState RuntimeState,
	const bool bPendingBalanceModeStartRequest,
	const bool bTransitionStarted,
	const bool bPhase1AutoCalibOwnsStartRequests,
	const bool bPhase1AutoCalibSubsystemActive)
{
	return RuntimeState == EPhysAnimRuntimeState::BridgeActive &&
		!bPendingBalanceModeStartRequest &&
		!bTransitionStarted &&
		!bPhase1AutoCalibOwnsStartRequests &&
		!bPhase1AutoCalibSubsystemActive;
}

bool UPhysAnimComponent::ShouldDeferAutoTriggeredBalanceStartForRecoveryTelemetry(const int32 RemainingSkipFrames)
{
	return RemainingSkipFrames > 0;
}

void UPhysAnimComponent::TryStartPendingBalanceModeRequest(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	auto ClearPendingBalanceModeStartRequestState = [this]()
	{
		bPendingBalanceModeStartRequest = false;
		bPendingBalanceModeStartAttemptIssued = false;
		PendingBalanceModeStartReason.Reset();
		PendingBalanceModeRequestTimeSeconds = -1.0;
		RecoveryPreEntryTelemetrySkipFrames = 0;
	};

	if (!bPendingBalanceModeStartRequest)
	{
		bPendingBalanceModeStartAttemptIssued = false;
		return;
	}

	if (RuntimeState == EPhysAnimRuntimeState::FailStopped || 
		(RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny && PendingBalanceModeStartReason != TEXT("manual_trigger")))
	{
		if (BalanceReadyTransition.HasActuallyStarted())
		{
			BalanceReadyTransition.Cancel(this);
		}
		ClearPendingBalanceModeStartRequestState();
		return;
	}

	if (BalanceReadyTransition.HasActuallyStarted())
	{
		ClearPendingBalanceModeStartRequestState();
		return;
	}

	(void)EffectiveSettings;
	if (bPendingBalanceModeStartAttemptIssued)
	{
		return;
	}

	// The pending request owns the start attempt; keep BridgeActive public state until Start accepts.
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] Pending balance request entering transition start attempt."));
	ClearPublishedBalanceTransitionFailureReason();
	BalanceReadyTransition.Start(PendingBalanceModeStartReason, this);
	ClearPendingBalanceModeStartRequestState();
}

// Obsolete pre-entry settle functions removed in favor of BalanceReadyTransition controller.


void UPhysAnimComponent::StartBalancePerturbationMode()
{
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	if (IsBalanceActiveState(RuntimeState))
	{
		return;
	}

	FString GateReason;
	if (!EvaluateBalanceModeQueueGates(EffectiveSettings, GateReason))
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] StartBalancePerturbationMode blocked by queue gates: %s"), *GateReason);
		return;
	}

	// Queue gates passed. Mark request pending so TryStartPendingBalanceModeRequest tick takes ownership.
	QueueBalanceModeStartRequest(TEXT("manual_trigger"));
}

void UPhysAnimComponent::CompleteBalanceModeEntry()
{
	USkeletalMeshComponent* const Mesh = MeshComponent.Get();
	FBodyInstance* const PelvisBody = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
	UWorld* const World = GetWorld();
	if (!Mesh || !PelvisBody || !World)
	{
		return;
	}

	bPendingBalanceModeStartRequest = false;
	PendingBalanceModeStartReason.Reset();
	PendingBalanceModeRequestTimeSeconds = -1.0;
	ClearPublishedBalanceTransitionFailureReason();
	BalanceReadyTransition.Cancel(this);
	
	if (!CanEnterBalanceActiveStanding())
	{
		TransitionRuntimeState(EPhysAnimRuntimeState::BalanceSafeDeny);
		return;
	}

	const double CurrentWorldTimeSeconds = World->GetTimeSeconds();
	const FPhysAnimStabilizationSettings RecoverySettings = ResolveEffectiveStabilizationSettings();
	const double SettledRampStartTimeSeconds =
		CurrentWorldTimeSeconds - static_cast<double>(FMath::Max(RecoverySettings.StartupRampSeconds, 0.0f)) - 0.01;
	HighestUnlockedBringUpGroupIndex = GetBringUpGroupCount() - 1;
	BringUpGroupStableAccumulatedSeconds = 0.0f;
	for (int32 GroupIndex = 0; GroupIndex < GetBringUpGroupCount(); ++GroupIndex)
	{
		if (BringUpGroupActivationTimeSeconds.IsValidIndex(GroupIndex))
		{
			BringUpGroupActivationTimeSeconds[GroupIndex] = SettledRampStartTimeSeconds;
		}
		if (BringUpGroupControlRampStartTimeSeconds.IsValidIndex(GroupIndex))
		{
			BringUpGroupControlRampStartTimeSeconds[GroupIndex] = SettledRampStartTimeSeconds;
		}
		if (BringUpGroupAlphaActiveLogged.IsValidIndex(GroupIndex))
		{
			BringUpGroupAlphaActiveLogged[GroupIndex] = 1;
		}
	}

	TransitionRuntimeState(EPhysAnimRuntimeState::BalanceActive_Standing);
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
	int32 RecoveryTotalSimCount = 0;
	for (const FName& BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		if (const FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
		{
			if (BodyInstance->IsInstanceSimulatingPhysics())
			{
				++RecoveryTotalSimCount;
			}
		}
	}
	EPhysicsMovementType RecoveryPelvisModifierMovementType = EPhysicsMovementType::Static;
	if (UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get())
	{
		const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
		if (const FPhysicsBodyModifierRecord* const Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, PelvisModifierName))
		{
			RecoveryPelvisModifierMovementType = Record->BodyModifier.ModifierData.MovementType;
		}
	}
	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimBalance] BALANCE_ACTIVE_ENTRY_STATE pelvisRawSim=%d pelvisModifier=%s simCount=%d"),
		PelvisBody->IsInstanceSimulatingPhysics() ? 1 : 0,
		GetPhysicsMovementTypeName(RecoveryPelvisModifierMovementType),
		RecoveryTotalSimCount);

	ApplyStartupMovementLock();
	ResetBridgeLocomotionAuthorityState();
	BridgePoseSearchLatchedWalkResult = FPoseSearchBlueprintResult();
	BridgePoseSearchLatchedQueryDirection = FVector::ZeroVector;
	BridgePoseSearchLatchedQuerySpeedCmPerSecond = 0.0f;
	BridgePoseSearchWalkLatchExpireTimeSeconds = -1.0;
	bHasBridgePoseSearchLatchedWalkResult = false;

	BalanceIdlePoseSearchResult = FPoseSearchBlueprintResult();
	bHasBalanceIdlePoseSearchResult =
		LastValidPoseSearchResult.SelectedAnim != nullptr &&
		IsBridgePoseSearchIdleResult(LastValidPoseSearchResult);
	if (bHasBalanceIdlePoseSearchResult)
	{
		BalanceIdlePoseSearchResult = LastValidPoseSearchResult;
	}

	BalanceScenarios.Empty();
	BalanceScenarios.Add({ TEXT("IdleHold_NoPush"), EPhysAnimPerturbationDirection::Forward, EPhysAnimPerturbationMagnitude::Small, 0.5f, BalanceRecoveryTimeoutSeconds, 0.0f });

	const TArray<EPhysAnimPerturbationDirection> Directions = {
		EPhysAnimPerturbationDirection::Forward,
		EPhysAnimPerturbationDirection::Backward,
		EPhysAnimPerturbationDirection::Left,
		EPhysAnimPerturbationDirection::Right };
	const TArray<EPhysAnimPerturbationMagnitude> Magnitudes = {
		EPhysAnimPerturbationMagnitude::Small,
		EPhysAnimPerturbationMagnitude::Medium,
		EPhysAnimPerturbationMagnitude::Large };
	const TArray<FString> DirectionNames = { TEXT("Forward"), TEXT("Backward"), TEXT("Left"), TEXT("Right") };
	const TArray<FString> MagnitudeNames = { TEXT("Small"), TEXT("Medium"), TEXT("Large") };

	for (int32 DirectionIndex = 0; DirectionIndex < Directions.Num(); ++DirectionIndex)
	{
		for (int32 MagnitudeIndex = 0; MagnitudeIndex < Magnitudes.Num(); ++MagnitudeIndex)
		{
			FPhysAnimBalanceScenario Scenario;
			Scenario.Name = FString::Printf(
				TEXT("IdleHold_PelvisImpulse_%s_%s"),
				*DirectionNames[DirectionIndex],
				*MagnitudeNames[MagnitudeIndex]);
			Scenario.Direction = Directions[DirectionIndex];
			Scenario.Magnitude = Magnitudes[MagnitudeIndex];
			Scenario.TriggerDelaySeconds = 0.5f;
			Scenario.RecoveryTimeoutSeconds = BalanceRecoveryTimeoutSeconds;
			Scenario.CooldownSeconds = 0.0f;
			BalanceScenarios.Add(Scenario);
		}
	}

	RuntimeInstabilityState = {};
	LastRuntimeInstabilityDiagnostics = {};
	RecoveryPreEntryTelemetrySkipFrames = 0;
	ActiveBalanceScenarioIndex = 0;
	BalanceScenarioStartTimeSeconds = World->GetTimeSeconds();
	BalanceScenarioStableWindowStartTimeSeconds = BalanceScenarioStartTimeSeconds;
	BalanceScenarioQuietWindowAccumulatedSeconds = 0.0;
	BalanceScenarioRecoveryStableAccumulatedSeconds = 0.0;
	LastBalanceStabilizationLogTimeSeconds = -1.0;
	LastBalanceScenarioImpactTimeSeconds = -1.0;
	BalanceScenarioPeakPelvisVel = 0.0f;
	BalanceScenarioPeakPelvisTilt = 0.0f;
	BalanceScenarioPeakPelvisAngularSpeed = 0.0f;
	BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
	BalanceScenarioPeakActorDisplacementCm = 0.0f;
	bBalanceScenarioAwaitingStableWindow = true;
	if (AActor* const OwnerActor = GetOwner())
	{
		BalanceScenarioStartActorLocation = OwnerActor->GetActorLocation();
	}
	
	const FTransform PelvisTransform = PelvisBody->GetUnrealWorldTransform();
	BalanceScenarioStartPelvisLocation = PelvisTransform.GetLocation();
	BalanceScenarioStartPelvisRotation = PelvisTransform.GetRotation();

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	UE_LOG(
		LogPhysAnimBridge,
		Warning,
		TEXT("[PhysAnimBalance] Balance mode started. scenarios=%d idlePoseCached=%s"),
		BalanceScenarios.Num(),
		bHasBalanceIdlePoseSearchResult ? TEXT("true") : TEXT("false"));
}


void UPhysAnimComponent::StopBalancePerturbationMode()
{
	const bool bIsBalanceTransitionalState =
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Prepare ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_LateValidate ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_RootOn ||
		RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle ||
		RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny;
	if (IsBalanceActiveState(RuntimeState) || bIsBalanceTransitionalState)
	{
		TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
		ReleaseStartupMovementLock(true);
	}

	if (BalanceReadyTransition.HasAnyInternalPhase())
	{
		BalanceReadyTransition.Cancel(this);
	}

	bPendingBalanceModeStartRequest = false;
	bPendingBalanceModeStartAttemptIssued = false;
	PendingBalanceModeStartReason.Reset();
	PendingBalanceModeRequestTimeSeconds = -1.0;
	ClearPublishedBalanceTransitionFailureReason();

	BalanceScenarios.Empty();
	ActiveBalanceScenarioIndex = INDEX_NONE;
	BalanceScenarioStartTimeSeconds = -1.0;
	BalanceScenarioStableWindowStartTimeSeconds = -1.0;
	BalanceScenarioQuietWindowAccumulatedSeconds = 0.0;
	BalanceScenarioRecoveryStableAccumulatedSeconds = 0.0;
	LastBalanceStabilizationLogTimeSeconds = -1.0;
	LastBalanceScenarioImpactTimeSeconds = -1.0;
	BalanceScenarioImpactPelvisLinearVelPre = FVector::ZeroVector;
	BalanceScenarioImpactPelvisLinearVelPost = FVector::ZeroVector;
	BalanceScenarioImpactPelvisAngularVelPre = FVector::ZeroVector;
	BalanceScenarioImpactPelvisAngularVelPost = FVector::ZeroVector;
	BalanceScenarioPeakPelvisVel = 0.0f;
	BalanceScenarioPeakPelvisTilt = 0.0f;
	BalanceScenarioPeakPelvisAngularSpeed = 0.0f;
	BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
	BalanceScenarioPeakActorDisplacementCm = 0.0f;
	bBalanceScenarioAwaitingStableWindow = false;
	BalanceIdlePoseSearchResult = FPoseSearchBlueprintResult();
	bHasBalanceIdlePoseSearchResult = false;
}


void UPhysAnimComponent::UpdateStabilizationStressTestState(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	const bool bStressTestEnabled = PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() > 0;
	if (!bStressTestEnabled)
	{
		StabilizationStressTestStartTimeSeconds = -1.0;
		bStabilizationStressTestCompletionLogged = false;
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive || !AreAllBringUpGroupsUnlocked())
	{
		return;
	}

	FPhysAnimStabilizationSettings PolicyRampSettings = EffectiveSettings;
	PolicyRampSettings.bForceZeroActions =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimForceZeroActions,
			PolicyRampSettings.bForceZeroActions);
	PolicyRampSettings.StartupRampSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimStartupRampSeconds,
			PolicyRampSettings.StartupRampSeconds);
	if (CalculateCurrentPolicyInfluenceAlpha(PolicyRampSettings) < (1.0f - KINDA_SMALL_NUMBER))
	{
		return;
	}

	if (StabilizationStressTestStartTimeSeconds < 0.0)
	{
		StabilizationStressTestStartTimeSeconds = World->GetTimeSeconds();
		bStabilizationStressTestCompletionLogged = false;
		StabilizationStressTestFirstAngularSpikeTimeSeconds = -1.0;
		StabilizationStressTestFirstLinearSpikeTimeSeconds = -1.0;
		StabilizationStressTestFirstInstabilitySignTimeSeconds = -1.0;
		StabilizationStressTestFirstAngularSpikeMultiplier = 1.0f;
		StabilizationStressTestFirstLinearSpikeMultiplier = 1.0f;
		StabilizationStressTestFirstInstabilityMultiplier = 1.0f;
		StabilizationStressTestFirstAngularSpikeBoneName = NAME_None;
		StabilizationStressTestFirstLinearSpikeBoneName = NAME_None;
		if (const AActor* const OwnerActor = GetOwner())
		{
			StabilizationStressTestBaselineActorLocation = OwnerActor->GetActorLocation();
		}
		if (const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get())
		{
			StabilizationStressTestBaselineSpineLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("spine_01")) - StabilizationStressTestBaselineActorLocation;
			StabilizationStressTestBaselineHeadLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("head")) - StabilizationStressTestBaselineActorLocation;
			StabilizationStressTestBaselineLeftFootLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("foot_l")) - StabilizationStressTestBaselineActorLocation;
			StabilizationStressTestBaselineRightFootLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("foot_r")) - StabilizationStressTestBaselineActorLocation;
		}
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test started: profile=%d sweep=%d rampSeconds=%.1f targetMultiplier=%.2f holdSeconds=%.1f recoveryRampSeconds=%.1f baseStrength=%.2f baseDampingRatio=%.2f baseExtraDamping=%.2f"),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestProfile.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestSweepMode.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestRampSeconds.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestTargetMultiplier.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestHoldSeconds.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestRecoveryRampSeconds.GetValueOnGameThread(),
			PolicyRampSettings.AngularStrengthMultiplier,
			PolicyRampSettings.AngularDampingRatioMultiplier,
			PolicyRampSettings.AngularExtraDampingMultiplier);
	}

	const float StressMultiplier = ResolveStabilizationStressTestMultiplier();
	if (!bStabilizationStressTestCompletionLogged && StressMultiplier <= KINDA_SMALL_NUMBER)
	{
		bStabilizationStressTestCompletionLogged = true;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test reached zero gain multiplier after %.2fs."),
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds);
	}
}


float UPhysAnimComponent::ResolveStabilizationStressTestMultiplier() const
{
	if (PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() <= 0 ||
		StabilizationStressTestStartTimeSeconds < 0.0)
	{
		return 1.0f;
	}

	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : StabilizationStressTestStartTimeSeconds;
	const float ElapsedSeconds = static_cast<float>(FMath::Max(CurrentTimeSeconds - StabilizationStressTestStartTimeSeconds, 0.0));
	return CalculateStabilizationStressTestMultiplier(
		PhysAnimComponentInternal::CVarPaStabilizationStressTestProfile.GetValueOnGameThread(),
		ElapsedSeconds,
		PhysAnimComponentInternal::CVarPaStabilizationStressTestRampSeconds.GetValueOnGameThread(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestTargetMultiplier.GetValueOnGameThread(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestHoldSeconds.GetValueOnGameThread(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestRecoveryRampSeconds.GetValueOnGameThread());
}


void UPhysAnimComponent::TrackStabilizationStressTestObservations()
{
	if (PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() <= 0 ||
		StabilizationStressTestStartTimeSeconds < 0.0)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	const float StressMultiplier = ResolveStabilizationStressTestMultiplier();
	const float AngularSpikeThresholdDegPerSec =
		PhysAnimComponentInternal::CVarPaStabilizationStressTestAngularSpikeThreshold.GetValueOnGameThread();
	const float LinearSpikeThresholdCmPerSec =
		PhysAnimComponentInternal::CVarPaStabilizationStressTestLinearSpikeThreshold.GetValueOnGameThread();

	if (StabilizationStressTestFirstAngularSpikeTimeSeconds < 0.0 &&
		LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond >= AngularSpikeThresholdDegPerSec)
	{
		StabilizationStressTestFirstAngularSpikeTimeSeconds = World->GetTimeSeconds();
		StabilizationStressTestFirstAngularSpikeMultiplier = StressMultiplier;
		StabilizationStressTestFirstAngularSpikeBoneName = LastRuntimeInstabilityDiagnostics.MaxAngularSpeedBoneName;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test first angular spike: bone=%s multiplier=%.2f elapsed=%.2fs angularDegPerSec=%.1f"),
			*StabilizationStressTestFirstAngularSpikeBoneName.ToString(),
			StressMultiplier,
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds,
			LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond);
	}

	if (StabilizationStressTestFirstLinearSpikeTimeSeconds < 0.0 &&
		LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond >= LinearSpikeThresholdCmPerSec)
	{
		StabilizationStressTestFirstLinearSpikeTimeSeconds = World->GetTimeSeconds();
		StabilizationStressTestFirstLinearSpikeMultiplier = StressMultiplier;
		StabilizationStressTestFirstLinearSpikeBoneName = LastRuntimeInstabilityDiagnostics.MaxLinearSpeedBoneName;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test first linear spike: bone=%s multiplier=%.2f elapsed=%.2fs linearCmPerSec=%.1f"),
			*StabilizationStressTestFirstLinearSpikeBoneName.ToString(),
			StressMultiplier,
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds,
			LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond);
	}

	if (StabilizationStressTestFirstInstabilitySignTimeSeconds < 0.0 &&
		RuntimeInstabilityState.UnstableAccumulatedSeconds > 0.0f)
	{
		StabilizationStressTestFirstInstabilitySignTimeSeconds = World->GetTimeSeconds();
		StabilizationStressTestFirstInstabilityMultiplier = StressMultiplier;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test first instability sign: multiplier=%.2f elapsed=%.2fs rootHeightDeltaCm=%.1f rootLinearCmPerSec=%.1f rootAngularDegPerSec=%.1f"),
			StressMultiplier,
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds,
			LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm,
			LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond,
			LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond);
	}
}


FPhysAnimCapsuleContractSnapshot UPhysAnimComponent::BuildCapsuleContractSnapshot() const
{
	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr;
	const USkeletalMeshComponent* Mesh = GetMeshComponent();
	const UCharacterMovementComponent* CMC = Character ? Character->GetCharacterMovement() : nullptr;

	FPhysAnimCapsuleContractSnapshotCaptureInput CaptureInput;
	CaptureInput.CapsuleComponent = const_cast<UCapsuleComponent*>(Capsule);
	CaptureInput.SkeletalMeshComponent = const_cast<USkeletalMeshComponent*>(Mesh);
	CaptureInput.CharacterMovementComponent = const_cast<UCharacterMovementComponent*>(CMC);
	CaptureInput.RebaseOriginCm = Character
		? Character->GetActorLocation()
		: (Capsule ? Capsule->GetComponentLocation() : FVector::ZeroVector);

	FPhysAnimCapsuleContractSnapshot Snapshot = PhysAnimRuntimeAdapter::CaptureCapsuleContractSnapshot(CaptureInput);
	Snapshot.bIsBridgeActive = (RuntimeState == EPhysAnimRuntimeState::BridgeActive || IsBalanceActiveState(RuntimeState));

	if (bEnableLiveRuntimeEvidenceProof)
	{
		UE_LOG(LogPhysAnimBridge, Verbose, TEXT("[PhysAnim] BuildCapsuleContractSnapshot: state=%d bIsBridgeActive=%d"), (int32)RuntimeState, (int32)Snapshot.bIsBridgeActive);
	}

	return Snapshot;
}

FPhysAnimContinuitySnapshot UPhysAnimComponent::BuildContinuitySnapshot(float DeltaTimeSeconds) const
{
	FPhysAnimContinuitySnapshot Snapshot;
	USkeletalMeshComponent* const Mesh = GetMeshComponent();
	const FName PelvisName = PhysAnimBridge::GetRootBoneName();
	const FBodyInstance* const PelvisBody = Mesh ? Mesh->GetBodyInstance(PelvisName) : nullptr;
	TArray<FString> CriticalBodyNames;
	TArray<FString> MissingCriticalBodyNames;
	int32 MissingCriticalBodies = 0;
	int32 NonSimulatingCriticalBodies = 0;

	for (const FName& BodyName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		CriticalBodyNames.Add(BodyName.ToString());
		const FBodyInstance* const BodyInstance = Mesh ? Mesh->GetBodyInstance(BodyName) : nullptr;
		if (!BodyInstance)
		{
			MissingCriticalBodyNames.Add(BodyName.ToString());
			++MissingCriticalBodies;
			continue;
		}

		if (!BodyInstance->IsInstanceSimulatingPhysics())
		{
			++NonSimulatingCriticalBodies;
		}
	}

	Snapshot.bAllCriticalBodiesValid = MissingCriticalBodies == 0;
	Snapshot.bAllCriticalBodiesSimulating = Snapshot.bAllCriticalBodiesValid && NonSimulatingCriticalBodies == 0;
	if (PelvisBody && PelvisBody->IsValidBodyInstance() && PelvisBody->IsInstanceSimulatingPhysics())
	{
		Snapshot.PelvisSleepDurationMs = PelvisBody->IsInstanceAwake()
			? 0.0
			: LiveRuntimeEvidenceTerminationState.LatestArtifact.PelvisSleepDurationMs +
				static_cast<double>(FMath::Max(0.0f, DeltaTimeSeconds) * 1000.0f);
	}
	else
	{
		Snapshot.PelvisSleepDurationMs = 0.0;
	}
	Snapshot.TopologyChangeCount = MissingCriticalBodies;
	Snapshot.bContinuityBookkeepingMismatch = !PendingBodyModifierCachedResetNames.IsEmpty();
	Snapshot.bIsBridgeActive = (RuntimeState == EPhysAnimRuntimeState::BridgeActive || IsBalanceActiveState(RuntimeState));
	Snapshot.bKineticGateActive = bKineticGateActiveLastFrame;

	if (bEnableLiveRuntimeEvidenceProof)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Verbose,
			TEXT("[PhysAnim] BuildContinuitySnapshot root=%s criticalBodies=%s missingBodies=%s pelvisSleepMs=%.1f topologyChanges=%d bookkeepingMismatch=%d valid=%d simulating=%d bridgeActive=%d pendingResets=%d"),
			*PelvisName.ToString(),
			*FString::Join(CriticalBodyNames, TEXT(",")),
			*FString::Join(MissingCriticalBodyNames, TEXT(",")),
			Snapshot.PelvisSleepDurationMs,
			Snapshot.TopologyChangeCount,
			Snapshot.bContinuityBookkeepingMismatch ? 1 : 0,
			Snapshot.bAllCriticalBodiesValid ? 1 : 0,
			Snapshot.bAllCriticalBodiesSimulating ? 1 : 0,
			Snapshot.bIsBridgeActive ? 1 : 0,
			PendingBodyModifierCachedResetNames.Num());
	}

	return Snapshot;
}

