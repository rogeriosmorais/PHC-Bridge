#include "PhysAnimBalanceReadyTransitionPrivate.h"
#include "PhysAnimLogger.h"

void FPhysAnimBalanceReadyTransition::MarkSafePhase2Denied(class UPhysAnimComponent* Owner, const FString& Reason)
{
	SafePhase2DenialReason = Reason;
	Diagnostics.FailureReason = Reason;
	SetPhase(EBalanceReadyTransitionPhase::BRT_SafeDenied, Owner);
}


void FPhysAnimBalanceReadyTransition::CapturePhase1TopologyRecord(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings)
{
	if (!Owner)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh ? Mesh->GetBodyInstance(RootBoneName) : nullptr;
	if (!PelvisBody)
	{
		return;
	}

	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	int32 UpperSimCount = 0;
	int32 TotalSimCount = 0;
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		if (ShouldKeepBoneKinematic(BoneName, Settings))
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
		else if (BalanceTransitionSets::IsUpperBody(BoneName))
		{
			UpperSimCount++;
		}
		TotalSimCount++;
	}

	Phase1TopologyRecord.bRootSimulating = !ShouldKeepBoneKinematic(RootBoneName, Settings);
	Phase1TopologyRecord.ProximalSimCount = ProximalSimCount;
	Phase1TopologyRecord.DistalSimCount = DistalSimCount;
	Phase1TopologyRecord.UpperBodySimCount = UpperSimCount;
	Phase1TopologyRecord.TotalSimCount = TotalSimCount;

	Phase1TopologyRecord.RootOwnershipMode = Phase1TopologyRecord.bRootSimulating ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
	Phase1TopologyRecord.ProximalOwnershipMode = ProximalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
	Phase1TopologyRecord.DistalOwnershipMode = DistalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;

	Phase1TopologyRecord.UpperBodyOwnershipMode = EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold;
	if (GVerbosePhase1Forensics != 0)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_OWNERSHIP_FROZEN mode=LateValidationKinematicHold source=Phase1Contract"));
	}

	// Capture authoritative suppression state for Phase 1.
	// Since Phase 1 always suppresses policy and resets (using held poses), we set these
	// flags to true to reflect the active Phase 1 contract.
	Phase1TopologyRecord.bPolicySuppressed = true;
	Phase1TopologyRecord.bResetsSuppressed = true;
	bHasPhase1TopologyRecord = true;

	auto GetModeName = [](EBalanceReadyGroupOwnershipMode Mode)
	{
		return Mode == EBalanceReadyGroupOwnershipMode::Simulating ? TEXT("sim") : TEXT("kin");
	};

	if (GVerbosePhase1Forensics != 0)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnimBalance] PHASE1_TOPOLOGY_SNAPSHOT topology=root=%s proximal=%s distal=%s upper=%s upperBodyOwnership=%s simCount=%d proximalSimCount=%d distalSimCount=%d upperBodySimCount=%d policySuppressed=%d resetsSuppressed=%d"),
			GetModeName(Phase1TopologyRecord.RootOwnershipMode),
			GetModeName(Phase1TopologyRecord.ProximalOwnershipMode),
			GetModeName(Phase1TopologyRecord.DistalOwnershipMode),
			UpperSimCount > 0 ? TEXT("sim") : TEXT("kin"),
			BalanceTransitionSets::GetUpperBodyOwnershipModeName(Phase1TopologyRecord.UpperBodyOwnershipMode),
			Phase1TopologyRecord.TotalSimCount,
			Phase1TopologyRecord.ProximalSimCount,
			Phase1TopologyRecord.DistalSimCount,
			Phase1TopologyRecord.UpperBodySimCount,
			Phase1TopologyRecord.bPolicySuppressed ? 1 : 0,
			Phase1TopologyRecord.bResetsSuppressed ? 1 : 0);
	}
}


void FPhysAnimBalanceReadyTransition::CaptureFlipDiagnostics(UPhysAnimComponent* Owner)
{
	USkeletalMeshComponent* Mesh = Owner ? Owner->GetMeshComponent() : nullptr;
	if (!Mesh || !Owner)
	{
		return;
	}

	const float PreviousPeakMaxBodyLinearSpeed = Diagnostics.PeakMaxBodyLinearSpeed;
	const float PreviousPeakMaxBodyAngularSpeed = Diagnostics.PeakMaxBodyAngularSpeed;
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	Diagnostics.PelvisLinearVelPost = Mesh->GetPhysicsLinearVelocity(RootBoneName);
	Diagnostics.PelvisAngularVelPost = Mesh->GetPhysicsAngularVelocityInDegrees(RootBoneName);
	Diagnostics.bPolicyWroteTargets = Owner->GetLastControlTargetDiagnostics().NumNormalPolicyTargetsWritten > 0;
	Diagnostics.bResetScheduled = !Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();
	Diagnostics.BaselineShellOffset = Owner->GetCurrentShellPlanarOffsetDeltaCm();
	Diagnostics.BaselineShellVel = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
	Diagnostics.bShellContributed = Diagnostics.BaselineShellOffset > KINDA_SMALL_NUMBER || Diagnostics.BaselineShellVel > KINDA_SMALL_NUMBER;

	TArray<FName> SimulatingBones;
	Owner->GetSimulatingBodies(SimulatingBones);
	Diagnostics.SimCountPost = SimulatingBones.Num();
	Diagnostics.DistalSimCountPost = 0;
	Diagnostics.UpperBodySimCountPost = 0;
	for (const FName BoneName : SimulatingBones)
	{
		if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
		{
			Diagnostics.DistalSimCountPost++;
		}
		else if (BalanceTransitionSets::IsUpperBody(BoneName))
		{
			Diagnostics.UpperBodySimCountPost++;
		}
	}

	auto GetMaxVel = [&](const TArray<FName>& Bones, float& MaxLin, float& MaxAng)
	{
		MaxLin = 0.0f;
		MaxAng = 0.0f;
		for (const FName BoneName : Bones)
		{
			MaxLin = FMath::Max(MaxLin, Mesh->GetPhysicsLinearVelocity(BoneName).Size());
			MaxAng = FMath::Max(MaxAng, Mesh->GetPhysicsAngularVelocityInDegrees(BoneName).Size());
		}
	};
	auto GetTotalAngularVel = [&](const TArray<FName>& Bones)
	{
		float TotalAng = 0.0f;
		for (const FName BoneName : Bones)
		{
			TotalAng += Mesh->GetPhysicsAngularVelocityInDegrees(BoneName).Size();
		}
		return TotalAng;
	};

	GetMaxVel({ RootBoneName }, Diagnostics.MaxLinVelPelvis, Diagnostics.MaxAngVelPelvis);
	GetMaxVel({ TEXT("thigh_l"), TEXT("thigh_r") }, Diagnostics.MaxLinVelThighs, Diagnostics.MaxAngVelThighs);
	GetMaxVel({ TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") }, Diagnostics.MaxLinVelSpine, Diagnostics.MaxAngVelSpine);
	GetMaxVel({ TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r"), TEXT("calf_l"), TEXT("calf_r") }, Diagnostics.MaxLinVelFeet, Diagnostics.MaxAngVelFeet);
	Diagnostics.TotalAngVelThighs = GetTotalAngularVel({ TEXT("thigh_l"), TEXT("thigh_r") });
	Diagnostics.TotalAngVelSpine = GetTotalAngularVel({ TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") });
	Diagnostics.TotalAngVelFeet = GetTotalAngularVel({ TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r"), TEXT("calf_l"), TEXT("calf_r") });

	FName CurrentWorstLinearBone = NAME_None;
	float CurrentWorstLinearSpeed = 0.0f;
	FName CurrentWorstAngularBone = NAME_None;
	float CurrentWorstAngularSpeed = 0.0f;
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const float CurrentLinearSpeed = Mesh->GetPhysicsLinearVelocity(BoneName).Size();
		const float CurrentAngularSpeed = Mesh->GetPhysicsAngularVelocityInDegrees(BoneName).Size();
		if (CurrentLinearSpeed > CurrentWorstLinearSpeed)
		{
			CurrentWorstLinearSpeed = CurrentLinearSpeed;
			CurrentWorstLinearBone = BoneName;
		}
		if (CurrentAngularSpeed > CurrentWorstAngularSpeed)
		{
			CurrentWorstAngularSpeed = CurrentAngularSpeed;
			CurrentWorstAngularBone = BoneName;
		}

		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, CurrentLinearSpeed);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, CurrentAngularSpeed);
		if (BoneName == RootBoneName)
		{
			Diagnostics.PeakRootAngularSpeed = FMath::Max(Diagnostics.PeakRootAngularSpeed, CurrentAngularSpeed);
		}
	}

	const bool bCrossedSpikeThresholdThisCapture =
		PreviousPeakMaxBodyLinearSpeed <= 100.0f &&
		PreviousPeakMaxBodyAngularSpeed <= 500.0f &&
		(Diagnostics.PeakMaxBodyLinearSpeed > 100.0f || Diagnostics.PeakMaxBodyAngularSpeed > 500.0f);
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] PHASE2_CAPTURE_FLIP_DIAGNOSTICS frame=%d tick=%d crossedSpike=%d prevPeakLinear=%.2f prevPeakAngular=%.2f peakLinear=%.2f peakAngular=%.2f worstLinearBone=%s worstLinearSpeed=%.2f worstAngularBone=%s worstAngularSpeed=%.2f pelvisLinear=%.2f pelvisAngular=%.2f thighsLinear=%.2f thighsAngular=%.2f thighsAngularTotal=%.2f spineLinear=%.2f spineAngular=%.2f spineAngularTotal=%.2f feetLinear=%.2f feetAngular=%.2f feetAngularTotal=%.2f"),
		static_cast<int32>(GFrameCounter),
		Phase2GuardTickCount,
		bCrossedSpikeThresholdThisCapture ? 1 : 0,
		PreviousPeakMaxBodyLinearSpeed,
		PreviousPeakMaxBodyAngularSpeed,
		Diagnostics.PeakMaxBodyLinearSpeed,
		Diagnostics.PeakMaxBodyAngularSpeed,
		*CurrentWorstLinearBone.ToString(),
		CurrentWorstLinearSpeed,
		*CurrentWorstAngularBone.ToString(),
		CurrentWorstAngularSpeed,
		Diagnostics.MaxLinVelPelvis,
		Diagnostics.MaxAngVelPelvis,
		Diagnostics.MaxLinVelThighs,
		Diagnostics.MaxAngVelThighs,
		Diagnostics.TotalAngVelThighs,
		Diagnostics.MaxLinVelSpine,
		Diagnostics.MaxAngVelSpine,
		Diagnostics.TotalAngVelSpine,
		Diagnostics.MaxLinVelFeet,
		Diagnostics.MaxAngVelFeet,
		Diagnostics.TotalAngVelFeet);
}


bool FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(const FString& FailureReason)
{
	return FailureReason == TEXT("phase2_root_not_confirmed") ||
		FailureReason == TEXT("phase2_topology_not_preserved") ||
		FailureReason == TEXT("phase2_guard_window_interrupted_by_transient_contamination");
}
EBalanceReadyConditionOwner FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(const FString& Reason)
{
	static const FString Phase1NoConvergencePrefix = TEXT("phase1_no_convergence_path_");
	if (Reason.StartsWith(Phase1NoConvergencePrefix))
	{
		const FString UnderlyingReason = Reason.RightChop(Phase1NoConvergencePrefix.Len());
		if (!UnderlyingReason.IsEmpty())
		{
			const EBalanceReadyConditionOwner UnderlyingOwner = ClassifyConditionOwner(UnderlyingReason);
			if (UnderlyingOwner != EBalanceReadyConditionOwner::None &&
				UnderlyingOwner != EBalanceReadyConditionOwner::TransitionRecovery)
			{
				return UnderlyingOwner;
			}
		}
	}

	if (Reason.StartsWith(TEXT("queue_final_group_ramp")))
	{
		return EBalanceReadyConditionOwner::ExternalBridgeBringUp;
	}
	if (Reason.StartsWith(TEXT("queue_policy_influence")))
	{
		return EBalanceReadyConditionOwner::ExternalPolicyRamp;
	}
	if (Reason.StartsWith(TEXT("phase1_late_validate_sim_coverage")) ||
		Reason.StartsWith(TEXT("phase1_late_validate_topology")) ||
		Reason.StartsWith(TEXT("phase1_topology")) ||
		Reason == TEXT("phase1_root_on_readiness_tilt_limited_viability") ||
		Reason == TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient") ||
		Reason == TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient") ||
		Reason == TEXT("phase1_root_on_readiness_pelvis_angular_incoherent") ||
		Reason == TEXT("phase1_root_on_readiness_requires_pelvis_coupling") ||
		Reason == TEXT("topology_mismatch_simulating_critical") ||
		Reason == TEXT("phase1_late_validate_handoff_invalidated") ||
		Reason.StartsWith(TEXT("phase2_sim_coverage")) ||
		Reason == TEXT("phase2_handoff_invalidated"))
	{
		return EBalanceReadyConditionOwner::Phase1TopologyShaping;
	}
	if (Reason.StartsWith(TEXT("phase1_late_validate_upper_body")) ||
		Reason == TEXT("phase2_upper_body_instability"))
	{
		return EBalanceReadyConditionOwner::Phase1UpperBodyOwnership;
	}
	if (Reason.StartsWith(TEXT("phase1_late_validate_target_discontinuity")) ||
		Reason.StartsWith(TEXT("phase1_no_convergence_path_target_discontinuity")) ||
		Reason == TEXT("phase2_target_discontinuity_too_high") ||
		Reason == TEXT("phase2_policy_suppression_regressed"))
	{
		return EBalanceReadyConditionOwner::Phase1PolicyRouting;
	}
	if (Reason.StartsWith(TEXT("phase1_pending_reset")) ||
		Reason == TEXT("phase2_reset_violation") ||
		Reason == TEXT("phase3_reset_pending"))
	{
		return EBalanceReadyConditionOwner::Phase1ResetSuppression;
	}
	if (Reason.StartsWith(TEXT("phase2_pre_root_on_shell")) ||
		Reason.StartsWith(TEXT("phase2_root_on_readiness")) ||
		Reason == TEXT("phase2_upper_only_handoff_not_root_on_ready"))
	{
		return EBalanceReadyConditionOwner::ShellAuthorityTransfer;
	}
	if (Reason == TEXT("phase2_shell_correction_material") ||
		Reason == TEXT("phase3_material_shell_correction"))
	{
		return EBalanceReadyConditionOwner::ShellAuthorityMaintenance;
	}
	if (Reason == TEXT("queue_final_group_ramp_inactive"))
	{
		return EBalanceReadyConditionOwner::ExternalBridgeBringUp;
	}
	if (Reason == TEXT("queue_policy_influence_below_threshold"))
	{
		return EBalanceReadyConditionOwner::ExternalPolicyRamp;
	}
	if (Reason == TEXT("phase1_topology_not_achieved") ||
		Reason == TEXT("phase1_root_on_readiness_tilt_limited_viability") ||
		Reason == TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient") ||
		Reason == TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient") ||
		Reason == TEXT("phase1_root_on_readiness_pelvis_angular_incoherent") ||
		Reason == TEXT("phase1_late_validate_handoff_invalidated") ||
		Reason == TEXT("topology_mismatch_simulating_critical") ||
		Reason == TEXT("phase2_sim_coverage_regressed") ||
		Reason == TEXT("phase2_handoff_invalidated"))
	{
		return EBalanceReadyConditionOwner::Phase1TopologyShaping;
	}
	if (Reason == TEXT("phase2_policy_suppression_regressed"))
	{
		return EBalanceReadyConditionOwner::Phase1PolicyRouting;
	}
	if (Reason == TEXT("phase2_upper_body_instability"))
	{
		return EBalanceReadyConditionOwner::Phase1UpperBodyOwnership;
	}
	if (Reason == TEXT("phase2_target_discontinuity_too_high") ||
		Reason == TEXT("phase1_late_validate_target_discontinuity"))
	{
		return EBalanceReadyConditionOwner::Phase1PolicyRouting;
	}
	if (Reason == TEXT("phase1_pending_reset_not_discharged") ||
		Reason == TEXT("phase2_reset_violation") ||
		Reason == TEXT("phase3_reset_pending"))
	{
		return EBalanceReadyConditionOwner::Phase1ResetSuppression;
	}
	if (Reason == TEXT("phase2_pre_root_on_shell_correction_safety_not_proven"))
	{
		return EBalanceReadyConditionOwner::ShellAuthorityTransfer;
	}
	if (Reason == TEXT("phase2_shell_correction_material") ||
		Reason == TEXT("phase3_material_shell_correction"))
	{
		return EBalanceReadyConditionOwner::ShellAuthorityMaintenance;
	}
	if (Reason == TEXT("phase2_root_not_confirmed") ||
		Reason == TEXT("phase2_root_on_warm_start_incoherent") ||
		Reason == TEXT("phase2_root_simulation_dropped") ||
		Reason == TEXT("phase2_root_on_spike") ||
		Reason == TEXT("phase3_root_simulation_dropped") ||
		Reason == TEXT("phase3_root_modifier_mismatch") ||
		Reason == TEXT("phase3_post_root_on_instability"))
	{
		return EBalanceReadyConditionOwner::Phase2RootOnExecution;
	}
	if (Reason == TEXT("phase2_topology_not_preserved") ||
		Reason == TEXT("phase3_topology_regressed"))
	{
		return EBalanceReadyConditionOwner::Phase2TopologyEnforcement;
	}
	if (Reason.StartsWith(TEXT("phase1_no_convergence_path")) ||
		Reason == TEXT("phase3_no_convergence_path"))
	{
		return EBalanceReadyConditionOwner::TransitionRecovery;
	}

	return EBalanceReadyConditionOwner::None;
}


bool FPhysAnimBalanceReadyTransition::IsPhase1OwnedCondition(const FString& Reason)
{
	const EBalanceReadyConditionOwner Owner = ClassifyConditionOwner(Reason);
	return Owner == EBalanceReadyConditionOwner::Phase1TopologyShaping ||
		Owner == EBalanceReadyConditionOwner::Phase1PolicyRouting ||
		Owner == EBalanceReadyConditionOwner::Phase1UpperBodyOwnership ||
		Owner == EBalanceReadyConditionOwner::Phase1ResetSuppression ||
		Owner == EBalanceReadyConditionOwner::ShellAuthorityTransfer;
}


bool FPhysAnimBalanceReadyTransition::IsAutomaticRetryAllowed(
	const FString& FailureReason,
	bool bRecoveryCompleted,
	bool bRecoveryChangedMaterialState,
	bool bFreshQuietProofOccurred,
	bool bCooldownElapsed,
	bool bRetryBudgetAvailable)
{
	return IsFailureClassRetryable(FailureReason) &&
		bRecoveryCompleted &&
		bRecoveryChangedMaterialState &&
		bFreshQuietProofOccurred &&
		bCooldownElapsed &&
		bRetryBudgetAvailable;
}


void FPhysAnimBalanceReadyTransition::ReturnToPhase1Prepare(UPhysAnimComponent* Owner, const FString& Reason, const TCHAR* EventName)
{
	const EBalanceReadyConditionOwner FailureOwner = ClassifyConditionOwner(Reason);
	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnimBalance] %s reason=%s owner=%d"), EventName, *Reason, static_cast<int32>(FailureOwner));
	if (Owner)
	{
		Owner->ReleaseTransitionOwnedShellLock();
	}
	ResetTransitionLocalState(Owner);
	InternalPhase = EBalanceReadyTransitionPhase::BRT_Phase1_Prepare;
	PhaseTimeSeconds = 0.0f;
	StableHoldAccumulatedSeconds = 0.0f;
	TargetDiscontinuityAccumulatedSeconds = 0.0f;
	QuietWindowAccumulatedSeconds = 0.0f;
	LateValidationAccumulatedSeconds = 0.0f;
	LastQuietBlockReason.Reset();
	Diagnostics.FailureReason.Reset();
}


