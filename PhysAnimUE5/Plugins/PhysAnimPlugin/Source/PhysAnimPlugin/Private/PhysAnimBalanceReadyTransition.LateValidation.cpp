#include "PhysAnimBalanceReadyTransitionPrivate.h"

FString FPhysAnimBalanceReadyTransition::ClassifyLateValidationFailureReason(bool bUpperBodyInstability, bool bSimCoverageRegressed, bool bTargetDiscontinuity)
{
	if (bUpperBodyInstability)
	{
		return TEXT("phase1_late_validate_upper_body_instability");
	}
	if (bSimCoverageRegressed)
	{
		return TEXT("phase1_late_validate_sim_coverage_regressed");
	}
	if (bTargetDiscontinuity)
	{
		return TEXT("phase1_late_validate_target_discontinuity");
	}
	return TEXT("phase1_late_validate_unknown");
}

bool FPhysAnimBalanceReadyTransition::IsLateValidationUpperBodyViolation(
	bool bModifierMovementTypeMismatch,
	bool bRawSimViolation,
	bool bPendingResetViolation,
	bool bRelativePoseViolation,
	bool bControlReadbackViolation)
{
	return bModifierMovementTypeMismatch ||
		bRawSimViolation ||
		bPendingResetViolation ||
		bRelativePoseViolation ||
		bControlReadbackViolation;
}

bool FPhysAnimBalanceReadyTransition::UpdateSustainedViolation(
	bool bViolationThisFrame,
	float DeltaTimeSeconds,
	float GraceDurationSeconds,
	float& InOutViolationDurationSeconds)
{
	if (!bViolationThisFrame)
	{
		InOutViolationDurationSeconds = 0.0f;
		return false;
	}

	InOutViolationDurationSeconds += FMath::Max(0.0f, DeltaTimeSeconds);
	return InOutViolationDurationSeconds + KINDA_SMALL_NUMBER >= FMath::Max(0.0f, GraceDurationSeconds);
}

FString FPhysAnimBalanceReadyTransition::ResolveRootOnReadinessGateReason(
	EBalanceReadyRootOnReadinessClassification Classification,
	bool bDirectPelvisLinkPositionSatisfied,
	bool bRootOnDirectPelvisLinkAngularSatisfied,
	bool bDirectPelvisThighMarginsSatisfied,
	bool bDirectPelvisSpineMarginSatisfied,
	bool bRootOnReadinessShellHoldSatisfied,
	bool bRootOnReadinessFinalBringUpControlSettled,
	bool bRootOnReadinessPolicyInfluenceSettled,
	bool bPreRootOnShellSafetyProofSatisfied,
	bool bRootOnReadinessNoCouplingProofSatisfied,
	bool bTiltLimitedByUprightness,
	float PolicyInfluenceAlphaAtCapture)
{
	if (Classification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady && !bDirectPelvisLinkPositionSatisfied)
	{
		return TEXT("phase2_pre_root_on_link_error_too_high");
	}
	if (Classification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady &&
		!bRootOnDirectPelvisLinkAngularSatisfied &&
		bTiltLimitedByUprightness)
	{
		return TEXT("phase1_root_on_readiness_tilt_limited_viability");
	}
	if (Classification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady && !bRootOnDirectPelvisLinkAngularSatisfied)
	{
		return TEXT("phase1_root_on_readiness_pelvis_angular_incoherent");
	}
	if (Classification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady && !bDirectPelvisThighMarginsSatisfied)
	{
		return TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient");
	}
	if (Classification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady &&
		!bDirectPelvisSpineMarginSatisfied &&
		bTiltLimitedByUprightness)
	{
		return TEXT("phase1_root_on_readiness_tilt_limited_viability");
	}
	if (Classification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady && !bDirectPelvisSpineMarginSatisfied)
	{
		return TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient");
	}
	if (Classification == EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny)
	{
		return TEXT("phase1_root_on_readiness_upper_only_safe_deny_pending");
	}
	if (Classification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady)
	{
		if (!bRootOnReadinessShellHoldSatisfied)
		{
			return TEXT("phase2_root_on_readiness_shell_hold_not_completed");
		}
		if (!bRootOnReadinessFinalBringUpControlSettled)
		{
			return TEXT("phase2_root_on_readiness_final_bring_up_control_not_settled");
		}
		if (!bRootOnReadinessPolicyInfluenceSettled)
		{
			return PolicyInfluenceAlphaAtCapture <= KINDA_SMALL_NUMBER
				? TEXT("phase2_root_on_readiness_policy_influence_not_started")
				: TEXT("phase2_root_on_readiness_policy_influence_below_threshold");
		}
		if (!bPreRootOnShellSafetyProofSatisfied)
		{
			return TEXT("phase2_root_on_readiness_shell_proof_not_satisfied");
		}
		if (!bRootOnReadinessNoCouplingProofSatisfied)
		{
			return TEXT("phase1_root_on_readiness_requires_pelvis_coupling");
		}
		return TEXT("ready");
	}

	return TEXT("phase1_root_on_readiness_topology_not_ready");
}



bool FPhysAnimBalanceReadyTransition::ValidateLateValidationHandoffSnapshot(const FPhysAnimCertifiedHandoffSnapshot& Snapshot, const FPhysAnimLateValidationResult& Result, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const
{
	if (!Result.bLateValidationCompleted)
	{
		OutReason = TEXT("phase2_late_validate_not_completed");
		return false;
	}

	if (!Snapshot.bControlAuthoritySettled)
	{
		OutReason = TEXT("phase2_control_authority_not_settled");
		return false;
	}

	if (Result.Outcome == EBalanceLateValidationOutcome::Outcome_Pending)
	{
		OutReason = TEXT("phase2_late_validate_outcome_pending");
		return false;
	}

	if (Result.MaxTargetDeltaDegrees > Settings.BalancePhase1MaxEntryTargetDeltaDeg)
	{
		OutReason = TEXT("phase2_target_discontinuity_too_high");
		return false;
	}

	if (Result.LateValidationSustainDurationSeconds + KINDA_SMALL_NUMBER < Settings.BalancePhase1LateValidateRequiredSeconds)
	{
		OutReason = TEXT("phase2_late_validate_not_sustained");
		return false;
	}

	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidateRootOnReadinessSnapshot(const FPhysAnimCertifiedHandoffSnapshot& Snapshot, const FPhysAnimLateValidationResult& Result, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const
{
	if (!Result.bRootOnReadinessShellHoldSatisfied)
	{
		OutReason = Result.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady
				? TEXT("phase2_root_on_readiness_shell_hold_not_completed")
			: Result.bLateValidationCompleted &&
				Result.LateValidationSustainDurationSeconds + KINDA_SMALL_NUMBER >= Settings.BalancePhase1LateValidateRequiredSeconds
				? TEXT("phase2_root_on_readiness_shell_hold_capped_by_late_validate_window")
				: TEXT("phase2_root_on_readiness_shell_hold_not_completed");
		return false;
	}

	if (!Result.bRootOnReadinessFinalBringUpControlSettled)
	{
		OutReason = TEXT("phase2_root_on_readiness_final_bring_up_control_not_settled");
		return false;
	}

	if (!Result.bRootOnReadinessPolicyInfluenceSettled)
	{
		OutReason = Snapshot.PolicyInfluenceAlphaAtCapture <= KINDA_SMALL_NUMBER
			? TEXT("phase2_root_on_readiness_policy_influence_not_started")
			: TEXT("phase2_root_on_readiness_policy_influence_below_threshold");
		return false;
	}

	if (!Result.bPreRootOnShellSafetyProofSatisfied)
	{
		OutReason = TEXT("phase2_root_on_readiness_shell_proof_not_satisfied");
		return false;
	}

	if (Result.RootOnReadinessClassification != EBalanceReadyRootOnReadinessClassification::RootCoupledReady)
	{
		OutReason = Result.RootOnReadinessGateReason.IsEmpty()
			? TEXT("phase2_root_on_readiness_topology_not_certified")
			: Result.RootOnReadinessGateReason;
		return false;
	}

	if (!Result.bRootOnReadinessProven)
	{
		OutReason = Result.RootOnReadinessGateReason.IsEmpty()
			? TEXT("phase2_root_on_readiness_not_proven")
			: Result.RootOnReadinessGateReason;
		return false;
	}

	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidatePreRootOnShellSafetyProofSnapshot(const FPhysAnimCertifiedHandoffSnapshot& Snapshot, const FPhysAnimLateValidationResult& Result, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const
{
	if (Result.RootOnReadinessClassification != EBalanceReadyRootOnReadinessClassification::RootCoupledReady)
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	const float SignificantShellOffsetThresholdCm = 0.1f;
	const float SignificantShellVelocityThresholdCmPerSecond = 1.0f;
	const bool bShellCorrectionActivelyAffecting = Snapshot.bShellCorrectionOwnerActive && 
		(Snapshot.ShellOffsetDeltaAtCaptureCm > SignificantShellOffsetThresholdCm || 
		 Snapshot.ShellVelocityDeltaAtCaptureCmPerSecond > SignificantShellVelocityThresholdCmPerSecond);

	const bool bBypassReanchor = !Snapshot.bShellCorrectionOwnerActive && 
		!bShellCorrectionActivelyAffecting &&
		Snapshot.ShellOffsetDeltaAtCaptureCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm &&
		Snapshot.ShellVelocityDeltaAtCaptureCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond &&
		Snapshot.ShellOffsetGrowthCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm &&
		Snapshot.ShellVelocityGrowthCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond;

	const bool bReanchorSatisfied = Snapshot.bTransitionShellReferenceReanchored || bBypassReanchor;

	if (!Snapshot.bTransitionOwnedShellLocked ||
		!bReanchorSatisfied ||
		Snapshot.bTransitionShellReferenceReseededAfterLock)
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	if (Snapshot.ShellOffsetDeltaAtCaptureCm > Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm ||
		Snapshot.ShellVelocityDeltaAtCaptureCmPerSecond > Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond ||
		Snapshot.ShellOffsetGrowthCm > Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm ||
		Snapshot.ShellVelocityGrowthCmPerSecond > Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond)
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	if (Snapshot.bShellCorrectionOwnerActive && !bBypassReanchor)
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	if (Snapshot.RootOnReadinessShellProofDurationSeconds + KINDA_SMALL_NUMBER < Settings.BalancePhase2PreRootOnShellProofRequiredSeconds && !bBypassReanchor)
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	return true;
}


