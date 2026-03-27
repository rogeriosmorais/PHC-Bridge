#include "PhysAnimBalanceReadyTransitionPrivate.h"

EBalanceReadyEntryClassification FPhysAnimBalanceReadyTransition::ClassifyEntryState(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings) const
{
	if (!Owner || !Owner->GetMeshComponent() || !Owner->GetOwner())
	{
		return EBalanceReadyEntryClassification::Preflight_HardFailure;
	}

	if (Owner->CalculateCurrentPolicyInfluenceAlpha(Settings) < Settings.BalanceEntryMinPolicyAlpha)
	{
		return EBalanceReadyEntryClassification::Preflight_QueueBlock;
	}

	return EBalanceReadyEntryClassification::Preflight_Accept;
}


bool FPhysAnimBalanceReadyTransition::IsRootStable(const FPhase1AcceptedConvergenceSnapshot& Snapshot, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	if (Snapshot.RootLinearSpeed > Settings.MaxRootLinearSpeedCmPerSecond || Snapshot.RootAngularSpeed > Settings.MaxRootAngularSpeedDegPerSecond)
	{
		OutReason = TEXT("fail_stop_precursor");
		return false;
	}

	if (!FMath::IsNearlyZero(Settings.BalanceEntryMaxGroundDistanceCm) && Snapshot.RootGroundDistance > Settings.BalanceEntryMaxGroundDistanceCm)
	{
		OutReason = TEXT("root_too_far_from_ground");
		return false;
	}

	return true;
}


bool FPhysAnimBalanceReadyTransition::EvaluateReadiness(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	AActor* OwnerActor = Owner->GetOwner();
	if (!Mesh || !OwnerActor)
	{
		OutReason = TEXT("owner_missing");
		return false;
	}

	if (!CachedConvergenceSnapshot.IsValid())
	{
		OutReason = TEXT("no_authoritative_snapshot");
		return false;
	}

	Diagnostics.RootSpeed = CachedConvergenceSnapshot.RootLinearSpeed;
	Diagnostics.RootAngularSpeed = CachedConvergenceSnapshot.RootAngularSpeed;
	Diagnostics.RootTilt = CachedConvergenceSnapshot.RootTilt;
	Diagnostics.ShellMetric = CachedConvergenceSnapshot.ShellPlanarVelocity;

	// Note: InternalPhase check against BRT_Phase1_Prepare is preserved because 
	// Phase 1 Prepare specifically allows pending resets while waiting for the quiet window. 
	// LateValidate and beyond require convergence.

	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && CachedConvergenceSnapshot.bHasPendingResets)
	{
		OutReason = TEXT("pending_resets");
		return false;
	}

	if (!IsRootStable(CachedConvergenceSnapshot, Settings, OutReason))
	{
		return false;
	}

	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && Diagnostics.RootSpeed > Settings.BalanceSettleMaxRootLinearSpeed)
	{
		OutReason = TEXT("root_linear_above_settle");
		return false;
	}
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && Diagnostics.RootAngularSpeed > Settings.BalanceSettleMaxRootAngularSpeed)
	{
		OutReason = TEXT("root_angular_above_settle");
		return false;
	}
	const bool bUsePhase2EntryTiltGate =
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3;
	const bool bUsePreEntryQuietTiltGate = !bUsePhase2EntryTiltGate && InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
	const float MaxAllowedTiltDeg = bUsePhase2EntryTiltGate
		? Settings.BalancePhase2EntryMaxRootTiltDeg
		: Owner->BalanceQuietTiltThresholdDeg;
	if ((bUsePhase2EntryTiltGate || bUsePreEntryQuietTiltGate) && Diagnostics.RootTilt > MaxAllowedTiltDeg)
	{
		OutReason = TEXT("tilt_high");
		return false;
	}
	if (Diagnostics.ShellMetric > Owner->BalanceQuietLinearSpeedThresholdCmPerSec)
	{
		OutReason = TEXT("shell_metric_high");
		return false;
	}
	if (!Owner->IsIdlePoseActive())
	{
		OutReason = TEXT("idle_pose_inactive");
		return false;
	}
	if (Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle)
	{
		OutReason = TEXT("locomotion_active");
		return false;
	}

	OutReason = TEXT("ready");
	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidatePhase2EntryPreconditions(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	if (!Owner)
	{
		OutReason = TEXT("phase2_owner_missing");
		return false;
	}

	if (!ShouldSuppressPolicy() || !ShouldSuppressResets() || !ShouldSuppressShell() || !ShouldSuppressMoveSmoke())
	{
		OutReason = TEXT("phase2_same_frame_conflicting_authority");
		return false;
	}
	if (CachedConvergenceSnapshot.bHasPendingResets)
	{
		OutReason = TEXT("phase2_reset_pending");
		return false;
	}
	if (CachedConvergenceSnapshot.bIsInstabilityPrecursorActive)
	{
		OutReason = TEXT("phase2_fail_stop_precursor");
		return false;
	}
	if (Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle)
	{
		OutReason = TEXT("phase2_locomotion_active");
		return false;
	}
	if (!Owner->IsTransitionOwnedShellLocked())
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	if (!bHasLateValidationProof || !bHasCertifiedHandoff || !CertifiedLateValidationResult.bLateValidationCompleted)
	{
		OutReason = TEXT("phase2_late_validate_not_completed");
		return false;
	}

	if (CertifiedLateValidationResult.Outcome == EBalanceLateValidationOutcome::Outcome_SafeDenyUpperOnly)
	{
		OutReason = TEXT("phase1_upper_only_handoff_safe_denied");
		return false;
	}

	const float ControlAuthorityAlpha = Owner->CalculateCurrentControlAuthorityAlpha(Settings);
	if (ControlAuthorityAlpha < 1.0f - KINDA_SMALL_NUMBER)
	{
		OutReason = TEXT("phase2_control_authority_not_settled");
		return false;
	}

	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	// Phase 2 entry must consume the accepted live RootOn-ready handoff, not persist the LateValidate hold mode.
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult, false))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (CurrentSnapshot.UpperBodyOwnershipMode != CertifiedHandoff.UpperBodyOwnershipMode ||
		CurrentSnapshot.UpperBodySimCount != CertifiedHandoff.UpperBodySimCount)
	{
		OutReason = TEXT("phase2_upper_body_instability");
		return false;
	}

	FString HandoffReadinessReason;
	if (!ValidateLateValidationHandoffSnapshot(CertifiedHandoff, CertifiedLateValidationResult, Settings, HandoffReadinessReason) ||
		!ValidateLateValidationHandoffSnapshot(CurrentSnapshot, CurrentResult, Settings, HandoffReadinessReason))
	{
		OutReason = HandoffReadinessReason;
		return false;
	}

	if (!ValidateRootOnReadinessSnapshot(CurrentSnapshot, CurrentResult, Settings, HandoffReadinessReason))
	{
		OutReason = HandoffReadinessReason;
		return false;
	}

	if (!CurrentResult.bRootOnReadinessProven)
	{
		OutReason = CurrentResult.RootOnReadinessGateReason.IsEmpty()
			? TEXT("phase2_root_on_readiness_not_proven")
			: CurrentResult.RootOnReadinessGateReason;
		return false;
	}

	if (!ValidatePreRootOnShellSafetyProofSnapshot(CurrentSnapshot, CurrentResult, Settings, HandoffReadinessReason))
	{
		OutReason = HandoffReadinessReason;
		return false;
	}

	const float ShellOffset = CachedConvergenceSnapshot.ShellPlanarOffset;
	const float ShellVel = CachedConvergenceSnapshot.ShellPlanarVelocity;
	if (Diagnostics.RootSpeed > Settings.BalancePhase2EntryMaxRootLinearSpeed)
	{
		OutReason = TEXT("phase2_entry_root_linear_too_high");
		return false;
	}
	if (Diagnostics.RootAngularSpeed > Settings.BalancePhase2EntryMaxRootAngularSpeed)
	{
		OutReason = TEXT("phase2_entry_root_angular_too_high");
		return false;
	}
	if (Diagnostics.RootTilt > Settings.BalancePhase2EntryMaxRootTiltDeg)
	{
		OutReason = TEXT("phase2_entry_root_tilt_too_high");
		return false;
	}

	if (!CurrentSnapshot.bRootOnDirectPelvisLinkGeometrySatisfied)
	{
		OutReason = TEXT("phase2_pre_root_on_link_error_too_high");
		return false;
	}

	if (ShellOffset > Settings.BalancePhase2EntryMaxShellOffsetDelta)
	{
		OutReason = TEXT("phase2_entry_shell_offset_too_high");
		return false;
	}
	if (ShellVel > Settings.BalancePhase2EntryMaxShellVelocityDelta)
	{
		OutReason = TEXT("phase2_entry_shell_velocity_too_high");
		return false;
	}

	const FPhysAnimControlTargetDiagnostics& ControlTargetDiagnostics = Owner->GetLastControlTargetDiagnostics();
	if (ControlTargetDiagnostics.MaxTargetDeltaDegrees > Settings.BalancePhase2EntryMaxTargetDeltaDeg ||
		ControlTargetDiagnostics.MeanTargetDeltaDegrees > Settings.BalancePhase2EntryMaxTargetDeltaDeg)
	{
		OutReason = TEXT("phase2_target_discontinuity_too_high");
		return false;
	}

	if (CurrentResult.LateValidationSustainDurationSeconds + KINDA_SMALL_NUMBER < CertifiedLateValidationResult.LateValidationSustainDurationSeconds)
	{
		OutReason = TEXT("phase2_late_validate_not_completed");
		return false;
	}

	OutReason = TEXT("ready");
	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidatePhase2Continuity(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	if (!Owner || !Owner->GetMeshComponent())
	{
		OutReason = TEXT("phase2_context_invalid");
		return false;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName);

	const bool bPelvisActualSim = PelvisBody ? PelvisBody->IsInstanceSimulatingPhysics() : false;

	if (!bPelvisActualSim)
	{
		OutReason = TEXT("phase2_root_simulation_dropped");
		return false;
	}

	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidatePhase3Continuity(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	if (!Owner || !Owner->GetMeshComponent())
	{
		OutReason = TEXT("phase3_context_invalid");
		return false;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName);

	if (!PelvisBody || !PelvisBody->IsInstanceSimulatingPhysics())
	{
		OutReason = TEXT("phase3_root_simulation_dropped");
		return false;
	}

	if (UPhysicsControlComponent* const PhysicsControl = Owner->PhysicsControlComponent.Get())
	{
		const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
		if (const FPhysicsBodyModifierRecord* const PelvisRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, PelvisModifierName))
		{
			const bool bModifierMismatch = PelvisRecord->BodyModifier.ModifierData.MovementType != EPhysicsMovementType::Simulated;
			if (bModifierMismatch && GVerbosePhase2Forensics != 0)
			{
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] PHASE3_ROOT_MODIFIER_DIAGNOSTIC frame=%d rootRawSim=1 pelvisModifierName=%s simCountPost=%d"),
					static_cast<int32>(GFrameCounter),
					UPhysAnimComponent::GetPhysicsMovementTypeName(PelvisRecord->BodyModifier.ModifierData.MovementType),
					Diagnostics.SimCountPost);
			}
		}
	}

	const FVector PelvisLinearVelocity = PelvisBody->GetUnrealWorldVelocity();
	const FVector PelvisAngularVelocityDegPerSec = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());
	const float PelvisLinearSpeed = PelvisLinearVelocity.Size();
	const float PelvisAngularSpeed = PelvisAngularVelocityDegPerSec.Size();
	const float Phase3LinearInstabilityThreshold = Settings.MaxRootLinearSpeedCmPerSecond * 2.5f;
	const float Phase3AngularInstabilityThreshold = Settings.MaxRootAngularSpeedDegPerSecond * 3.0f;

	// Section 17.4 - Root simulation spike (instability)
	if (PelvisLinearSpeed > Phase3LinearInstabilityThreshold ||
		PelvisAngularSpeed > Phase3AngularInstabilityThreshold)
	{
		OutReason = TEXT("phase3_post_root_on_instability");
		return false;
	}

	// Section 17.3 - post-root-on topology preserved
	TArray<FName> SimulatingBones;
	Owner->GetSimulatingBodies(SimulatingBones);
	TSet<FName> SimulatingBoneSet(SimulatingBones);
	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		if (!SimulatingBoneSet.Contains(BoneName))
		{
			continue;
		}

		if (BalanceTransitionSets::IsProximal(BoneName))
		{
			ProximalSimCount++;
		}
		else if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
		{
			DistalSimCount++;
		}
	}

	if (!BalanceTransitionSets::IsExpectedPhase2Topology(
			CertifiedHandoff.SimCount,
			SimulatingBones.Num(),
			CertifiedHandoff.DistalSimCount,
			DistalSimCount))
	{
		OutReason = TEXT("phase3_topology_regressed");
		return false;
	}

	// Section 17.3 - shell lock preserved
	if (!Owner->IsTransitionOwnedShellLocked())
	{
		OutReason = TEXT("phase3_shell_lock_lost");
		return false;
	}

	// Section 17.3 - no shell reference reseed
	if (Owner->WasTransitionShellReferenceReseededAfterLock())
	{
		OutReason = TEXT("phase3_shell_reference_reseeded");
		return false;
	}

	// Section 17.3 - no startup/gameplay ownership reclaim (CharacterMovement active)
	if (Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle)
	{
		OutReason = TEXT("phase3_startup_or_gameplay_authority_reclaimed");
		return false;
	}

	// Section 17.3 - no reset pending / no topology flip pending
	if (!Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
	{
		OutReason = TEXT("phase3_reset_pending");
		return false;
	}

	// Section 17.3 - no material shell correction
	if (Owner->GetCurrentShellPlanarOffsetDeltaCm() > Settings.BalancePhase2AbortShellOffsetDelta ||
		Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond() > Settings.BalancePhase2AbortShellVelocityDelta)
	{
		OutReason = TEXT("phase3_material_shell_correction");
		return false;
	}

	return true;
}
