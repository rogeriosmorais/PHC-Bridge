#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

namespace BalanceTransitionSets
{
	static constexpr float Phase2TopologySettleGraceSeconds = 1.0f / 30.0f;
	static constexpr float Phase2AuthorityRampSeconds = 0.10f;
	static constexpr float Phase2MaxPelvisProximalConstraintErrorCm = 15.0f;
	static bool IsRoot(FName BoneName) { return BoneName == "pelvis"; }
	static bool IsProximal(FName BoneName) { return BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03" || BoneName == "thigh_l" || BoneName == "thigh_r"; }
	static bool IsDistalLowerLimb(FName BoneName) { return BoneName == "calf_l" || BoneName == "calf_r" || BoneName == "foot_l" || BoneName == "foot_r" || BoneName == "ball_l" || BoneName == "ball_r"; }
	static bool IsUpperLimbDistal(FName BoneName)
	{
		return BoneName == "lowerarm_l" || BoneName == "hand_l" ||
			BoneName == "lowerarm_r" || BoneName == "hand_r";
	}
	static bool IsUpperBodyApex(FName BoneName)
	{
		return BoneName == "neck_01" || BoneName == "head";
	}
	static bool IsUpperLimbChain(FName BoneName)
	{
		return BoneName == "clavicle_l" || BoneName == "upperarm_l" || BoneName == "lowerarm_l" || BoneName == "hand_l" ||
			BoneName == "clavicle_r" || BoneName == "upperarm_r" || BoneName == "lowerarm_r" || BoneName == "hand_r";
	}
	static bool IsUpperBody(FName BoneName)
	{
		return IsUpperLimbChain(BoneName) || BoneName == "neck_01" || BoneName == "head";
	}
	static bool IsLateValidationUpperBodyOwnershipBone(FName BoneName)
	{
		return BoneName == "neck_01" ||
			BoneName == "head" ||
			BoneName == "clavicle_l" ||
			BoneName == "clavicle_r" ||
			BoneName == "upperarm_l" ||
			BoneName == "upperarm_r";
	}
	static bool IsTransitionCritical(FName BoneName) { return IsRoot(BoneName) || IsProximal(BoneName) || IsDistalLowerLimb(BoneName); }
	static bool IsPrepareCriticalKinematic(FName BoneName) { return IsRoot(BoneName); }
	static bool IsExpectedPhase2Topology(int32 SimCountPre, int32 SimCountPost, int32 DistalSimCountPre, int32 DistalSimCountPost)
	{
		return DistalSimCountPre >= 0 &&
			DistalSimCountPost == 0 &&
			(SimCountPost == SimCountPre || SimCountPost == SimCountPre + 1);
	}

	static float ComputePelvisProximalConstraintErrorCm(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones,
		FVector& OutLiveChainCenterCm)
	{
		OutLiveChainCenterCm = FVector::ZeroVector;
		if (!Mesh)
		{
			return 0.0f;
		}

		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		const FTransform PelvisTransform = Mesh->GetBoneTransform(Mesh->GetBoneIndex(RootBoneName));

		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		for (const FName BoneName : SimulatingBones)
		{
			if (!IsProximal(BoneName) && !IsUpperBody(BoneName))
			{
				continue;
			}

			Sum += Mesh->GetBoneTransform(Mesh->GetBoneIndex(BoneName)).GetLocation();
			++Count;
		}

		if (Count > 0)
		{
			OutLiveChainCenterCm = Sum / static_cast<float>(Count);
		}
		else
		{
			OutLiveChainCenterCm = PelvisTransform.GetLocation();
		}

		return FVector::Dist(PelvisTransform.GetLocation(), OutLiveChainCenterCm);
	}

	static FTransform BuildWarmStartPelvisTransform(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& SimulatingBones)
	{
		if (!Mesh)
		{
			return FTransform::Identity;
		}

		FVector LiveChainCenterCm = FVector::ZeroVector;
		const float ErrorCm = ComputePelvisProximalConstraintErrorCm(Mesh, SimulatingBones, LiveChainCenterCm);
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		FTransform PelvisTransform = Mesh->GetBoneTransform(Mesh->GetBoneIndex(RootBoneName));
		if (ErrorCm > KINDA_SMALL_NUMBER)
		{
			PelvisTransform.SetLocation(LiveChainCenterCm);
		}
		return PelvisTransform;
	}

	static bool IsUpperOnlySafeDenyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating)
	{
		return !bRootSimulating && ProximalSimCount == 0 && DistalSimCount == 0 && UpperSimCount > 0;
	}

	static bool IsRootCoupledReadyHandoff(int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount, bool bRootSimulating)
	{
		return !bRootSimulating && ProximalSimCount == 5 && DistalSimCount >= 0 && UpperSimCount >= 0;
	}

	static const TCHAR* GetUpperBodyOwnershipModeName(EBalanceReadyUpperBodyOwnershipMode Mode)
	{
		switch (Mode)
		{
		case EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold:
			return TEXT("late_validation_kinematic_hold");
		case EBalanceReadyUpperBodyOwnershipMode::None:
		default:
			return TEXT("none");
		}
	}

	static const TCHAR* GetRootOnReadinessClassificationName(EBalanceReadyRootOnReadinessClassification Classification)
	{
		switch (Classification)
		{
		case EBalanceReadyRootOnReadinessClassification::RootCoupledReady:
			return TEXT("root_coupled_ready");
		case EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny:
			return TEXT("upper_only_safe_deny");
		case EBalanceReadyRootOnReadinessClassification::NotReady:
		default:
			return TEXT("not_ready");
		}
	}

	static const TCHAR* GetLateValidationOutcomeName(EBalanceLateValidationOutcome Outcome)
	{
		switch (Outcome)
		{
		case EBalanceLateValidationOutcome::Outcome_AcceptRootOn:
			return TEXT("accept_root_on");
		case EBalanceLateValidationOutcome::Outcome_SafeDenyUpperOnly:
			return TEXT("safe_deny_upper_only");
		case EBalanceLateValidationOutcome::Outcome_Pending:
		default:
			return TEXT("pending");
		}
	}

	static const TCHAR* GetShellAuthorityModeName(EBalanceTransitionShellAuthorityMode Mode)
	{
		switch (Mode)
		{
		case EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked:
			return TEXT("transition_owned_shell_locked");
		case EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly:
		default:
			return TEXT("gameplay_shell_observed_only");
		}
	}

	static FString BuildCertifiedHandoffTopologyClass(bool bRootSimulating, int32 ProximalSimCount, int32 DistalSimCount, int32 UpperSimCount)
	{
		return FString::Printf(
			TEXT("root=%s proximal=%s distal=%s upper=%s"),
			bRootSimulating ? TEXT("sim") : TEXT("kin"),
			ProximalSimCount > 0 ? TEXT("sim") : TEXT("kin"),
			DistalSimCount > 0 ? TEXT("sim") : TEXT("kin"),
			UpperSimCount > 0 ? TEXT("sim") : TEXT("kin"));
	}
}

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


bool FPhysAnimBalanceReadyTransition::ValidateLateValidationBaselineSnapshot(
	const FPhysAnimCertifiedHandoffSnapshot& Snapshot,
	const FPhysAnimLateValidationResult& Result,
	const FPhysAnimStabilizationSettings& Settings,
	FString& OutReason)
{
	// A valid baseline for late validation must already be 'certified' as either 
	// Outcome_AcceptRootOn (proximal+upper) or Outcome_SafeDenyUpperOnly (upper body only).
	// Outcome_Pending indicates a transient/partially-shaping topology which is too loose for a stable baseline.

	if (Result.Outcome == EBalanceLateValidationOutcome::Outcome_Pending)
	{
		if (Snapshot.ProximalSimCount >= 5 && Snapshot.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold)
		{
			// Valid baseline: upper body is being held kinematic during late validate, and will release after proof passes.
		}
		else
		{
			OutReason = Snapshot.ProximalSimCount > 0
				? TEXT("phase1_late_validate_baseline_topology_mismatch_proximal_incomplete")
				: TEXT("phase1_late_validate_baseline_topology_mismatch_upper_incomplete");
			return false;
		}
	}

	if (!Snapshot.bControlAuthoritySettled)
	{
		OutReason = TEXT("phase1_late_validate_baseline_control_authority_not_settled");
		return false;
	}

	return true;
}


void FPhysAnimBalanceReadyTransition::Start(const FString& InRequestReason, UPhysAnimComponent* Owner)
{
	if (IsActive() || !Owner)
	{
		return;
	}

	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] Preflight begin."));
	const EBalanceReadyEntryClassification Classification = ClassifyEntryState(Owner, Owner->ResolveEffectiveStabilizationSettings());
	if (Classification != EBalanceReadyEntryClassification::Preflight_Accept)
	{
		if (Classification == EBalanceReadyEntryClassification::Preflight_HardFailure)
		{
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] TRANSITION_REJECTED reason=preflight_hard_failure"));
		}
		else
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_REJECTED reason=preflight_queue_block"));
		}
		return;
	}

	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] TRANSITION_ACCEPT reason=preflight_accept"));
	Owner->SetStartupBringUpFrozenByBalanceEntry(true);
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] BALANCE_ENTRY_FREEZE state=on reason=transition_accept"));

	RequestReason = InRequestReason;
	StableHoldAccumulatedSeconds = 0.0f;
	QuietWindowAccumulatedSeconds = 0.0f;
	TargetDiscontinuityAccumulatedSeconds = 0.0f;
	RootOnReadinessShellHoldAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofStartOffsetCm = 0.0f;
	RootOnReadinessShellProofStartVelocityCmPerSecond = 0.0f;
	bHasRootOnReadinessShellProofBaseline = false;
	PhaseTimeSeconds = 0.0f;
	TotalTransitionTimeSeconds = 0.0f;
	LastLogTimeSeconds = -1.0;
	LastQuietBlockReason.Reset();
	ResetTransitionLocalState();
	bLatchedPelvisResetApplied = false;
	QuietHandoffCount = 0;
	HipQuarantineTimerSeconds = 0.0f;
	LateValidationAccumulatedSeconds = 0.0f;
	LastLateValidateBlockReason.Reset();
	EntryHoldRotations.Empty();
	SafePhase2DenialReason.Reset();

	if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
	{
		for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
		{
			EntryHoldRotations.Add(BoneName, Mesh->GetBoneQuaternion(BoneName, EBoneSpaces::WorldSpace));
		}
	}

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
	{
		if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName))
		{
			bLastRootSimulating = PelvisBody->IsInstanceSimulatingPhysics();
		}
	}
	bLastPendingResetsEmpty = Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();

	CapturePhase1TopologyRecord(Owner, Owner->ResolveEffectiveStabilizationSettings());

	SetPhase(EBalanceReadyTransitionPhase::BRT_Phase1_Prepare, Owner);
}

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

void FPhysAnimBalanceReadyTransition::Cancel(UPhysAnimComponent* Owner)
{
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] BalanceReadyTransition cancelled. phase=%d"), static_cast<int32>(InternalPhase));
		SetPhase(EBalanceReadyTransitionPhase::BRT_Inactive, Owner);
	}
}

void FPhysAnimBalanceReadyTransition::Tick(float DeltaTime, UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings)
{
	if (!IsActive() || !Owner)
	{
		return;
	}

	PhaseTimeSeconds += DeltaTime;
	TotalTransitionTimeSeconds += DeltaTime;

	FString BlockReason;
	const bool bReadyThisFrame = EvaluateReadiness(Owner, Settings, BlockReason);
	Diagnostics.BlockReason = BlockReason;

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		bool bQuietThisFrame = true;
		FString QuietBlockReason;

		if (!bReadyThisFrame)
		{
			bQuietThisFrame = false;
			QuietBlockReason = BlockReason;
		}
		else if (CachedConvergenceSnapshot.bIsInstabilityPrecursorActive)
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("instability_precursor");
		}
		else if (CachedConvergenceSnapshot.bHasPendingResets)
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("pending_resets");
		}
		else if (Diagnostics.RootSpeed > Settings.BalancePhase1QuietRootLinearSpeed || Diagnostics.RootAngularSpeed > Settings.BalancePhase1QuietRootAngularSpeed)
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("motion_above_limit");
		}
		else if (CachedConvergenceSnapshot.ShellPlanarOffset > Settings.BalancePhase1QuietShellOffsetDelta ||
			CachedConvergenceSnapshot.ShellPlanarVelocity > Settings.BalancePhase1QuietShellVelocityDelta)
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("shell_contamination");
		}
		else if (CachedConvergenceSnapshot.MaxBodyLinearSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed ||
			CachedConvergenceSnapshot.MaxBodyAngularSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed)
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("body_motion_instability");

			Diagnostics.Phase1LateValidateWorstLinearSpeed = CachedConvergenceSnapshot.MaxBodyLinearSpeed;
			Diagnostics.Phase1LateValidateWorstAngularSpeed = CachedConvergenceSnapshot.MaxBodyAngularSpeed;
			// Note: Snapshot doesn't track which bone was worst, but we record the speeds for diagnostics.
		}

		if (bQuietThisFrame && (CachedConvergenceSnapshot.MaxTargetDeltaDegrees > Settings.BalancePhase1MaxEntryTargetDeltaDeg ||
			CachedConvergenceSnapshot.MeanTargetDeltaDegrees > Settings.BalancePhase1MaxEntryTargetDeltaDeg))
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("target_discontinuity");
			Diagnostics.Phase1TargetDiscontinuityGateInput.MaxTargetDeltaDegrees = CachedConvergenceSnapshot.MaxTargetDeltaDegrees;
			Diagnostics.Phase1TargetDiscontinuityGateInput.MeanTargetDeltaDegrees = CachedConvergenceSnapshot.MeanTargetDeltaDegrees;
			Diagnostics.Phase1TargetDiscontinuityGateSource = TEXT("snapshot_quiet_window_gate");
			Diagnostics.Phase1TargetDiscontinuityGateReason = QuietBlockReason;
			Diagnostics.Phase1TargetDiscontinuityAccumulatedSeconds = TargetDiscontinuityAccumulatedSeconds;
		}
		else if (bQuietThisFrame)
		{
			if (BalanceTransitionSets::IsRootCoupledReadyHandoff(Phase1TopologyRecord.ProximalSimCount, Phase1TopologyRecord.DistalSimCount, Phase1TopologyRecord.UpperBodySimCount, Phase1TopologyRecord.bRootSimulating))
			{
				// Valid topology for balance entry.
			}
			else if (BalanceTransitionSets::IsUpperOnlySafeDenyHandoff(Phase1TopologyRecord.ProximalSimCount, Phase1TopologyRecord.DistalSimCount, Phase1TopologyRecord.UpperBodySimCount, Phase1TopologyRecord.bRootSimulating))
			{
				// Valid topology for safe-deny.
			}
			else
			{
				bQuietThisFrame = false;
				QuietBlockReason = TEXT("topology_mismatch_simulating_critical");
			}
		}

		if (bQuietThisFrame)
		{
			TargetDiscontinuityAccumulatedSeconds = 0.0f;
			QuietWindowAccumulatedSeconds += DeltaTime;
			if (QuietWindowAccumulatedSeconds >= Settings.BalancePhase1QuietRequiredSeconds)
			{
				if (!CachedConvergenceSnapshot.bIsPelvisSimulating)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_BLOCKED reason=pelvis_not_simulating"));
					QuietWindowAccumulatedSeconds = 0.0f; // fresh qualifying quiet window required
					return;
				}

				FString CaptureReason;
				if (CaptureLateValidationBaseline(Owner, Settings, CaptureReason))
				{
					LateValidationAccumulatedSeconds = 0.0f;
					Diagnostics.Phase1LateValidateAccumulatedSeconds = 0.0f;
					Diagnostics.Phase1LateValidateGateSource = TEXT("phase1_late_validate_start");
					Diagnostics.Phase1LateValidateGateReason.Reset();
					UE_LOG(
						LogPhysAnimBridge,
						Log,
						TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_STARTED topology=%s upperBodyOwnership=%s simCount=%d upperBodySimCount=%d policySuppressed=%d controlAuthoritySettled=%d quietProofDuration=%.2f requiredSeconds=%.2f"),
						*CertifiedHandoff.TopologyClass,
						BalanceTransitionSets::GetUpperBodyOwnershipModeName(CertifiedHandoff.UpperBodyOwnershipMode),
						CertifiedHandoff.SimCount,
						CertifiedHandoff.UpperBodySimCount,
						CertifiedHandoff.bPolicySuppressed ? 1 : 0,
						CertifiedHandoff.bControlAuthoritySettled ? 1 : 0,
						CertifiedLateValidationResult.QuietProofDurationSeconds,
						Settings.BalancePhase1LateValidateRequiredSeconds);
					SetPhase(EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate, Owner);
					return;
				}

				const FString Phase2BlockReason = CaptureReason.IsEmpty()
					? TEXT("phase1_late_validate_baseline_capture_failed")
					: CaptureReason;
				Diagnostics.FailureReason = Phase2BlockReason;
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s"), *Phase2BlockReason);
				Owner->ReleaseTransitionOwnedShellLock();
				MarkSafePhase2Denied(Owner, Phase2BlockReason);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_QUIET_WINDOW_RESET reason=%s"), *Phase2BlockReason);
				return;
			}
		}
		else
		{
			if (!QuietBlockReason.IsEmpty())
			{
				LastQuietBlockReason = QuietBlockReason;
			}
			else
			{
				// Do not carry a stale blocker forward once the live frame is quiet again.
				LastQuietBlockReason.Reset();
			}
			if (QuietBlockReason == TEXT("target_discontinuity"))
			{
				TargetDiscontinuityAccumulatedSeconds += DeltaTime;
				Diagnostics.Phase1TargetDiscontinuityGateInput.MaxTargetDeltaDegrees = CachedConvergenceSnapshot.MaxTargetDeltaDegrees;
				Diagnostics.Phase1TargetDiscontinuityGateInput.MeanTargetDeltaDegrees = CachedConvergenceSnapshot.MeanTargetDeltaDegrees;
				Diagnostics.Phase1TargetDiscontinuityGateSource = TEXT("snapshot_quiet_window_gate");
				Diagnostics.Phase1TargetDiscontinuityGateReason = QuietBlockReason;
				Diagnostics.Phase1TargetDiscontinuityAccumulatedSeconds = TargetDiscontinuityAccumulatedSeconds;
			}
			else
			{
				TargetDiscontinuityAccumulatedSeconds = 0.0f;
				Diagnostics.Phase1TargetDiscontinuityGateInput = {};
				Diagnostics.Phase1TargetDiscontinuityGateSource.Reset();
				Diagnostics.Phase1TargetDiscontinuityGateReason.Reset();
				Diagnostics.Phase1TargetDiscontinuityAccumulatedSeconds = 0.0f;
			}

			QuietWindowAccumulatedSeconds = 0.0f;
		}

		if (TotalTransitionTimeSeconds > Settings.BalancePhase1TimeoutDuration)
		{
			const FString& TerminalQuietBlockReason = !LastLateValidateBlockReason.IsEmpty()
				? LastLateValidateBlockReason
				: (!QuietBlockReason.IsEmpty() ? QuietBlockReason : LastQuietBlockReason);
			const FString TimeoutReason = TerminalQuietBlockReason.IsEmpty()
				? TEXT("phase1_no_convergence_path")
				: TEXT("phase1_no_convergence_path_") + TerminalQuietBlockReason;
			Diagnostics.FailureReason = TimeoutReason;
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s"), *TimeoutReason);
			Owner->ReleaseTransitionOwnedShellLock();
			MarkSafePhase2Denied(Owner, TimeoutReason);
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_QUIET_WINDOW_RESET reason=%s"), *TimeoutReason);
			return;
		}
		return;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate)
	{
		bool bLateValidationThisFrame = true;
		FString LateValidateBlockReason;
		const float PolicyInfluenceAlpha = Owner->CalculateCurrentPolicyInfluenceAlpha(Settings);

		if (!bReadyThisFrame)
		{
			bLateValidationThisFrame = false;
			LateValidateBlockReason = BlockReason;
		}
		else if (CachedConvergenceSnapshot.bIsInstabilityPrecursorActive)
		{
			bLateValidationThisFrame = false;
			LateValidateBlockReason = TEXT("instability_precursor");
		}
		else if (!Phase1TopologyRecord.bResetsSuppressed && CachedConvergenceSnapshot.bHasPendingResets)
		{
			bLateValidationThisFrame = false;
			LateValidateBlockReason = TEXT("pending_resets");
		}
		else if (!Phase1TopologyRecord.bPolicySuppressed && PolicyInfluenceAlpha <= KINDA_SMALL_NUMBER)
		{
			bLateValidationThisFrame = false;
			LateValidateBlockReason = TEXT("policy_influence_inactive");
		}
		else if (Diagnostics.RootSpeed > Settings.BalancePhase1QuietRootLinearSpeed || Diagnostics.RootAngularSpeed > Settings.BalancePhase1QuietRootAngularSpeed)
		{
			bLateValidationThisFrame = false;
			LateValidateBlockReason = TEXT("motion_above_limit");
		}
		else if (CachedConvergenceSnapshot.ShellPlanarOffset > Settings.BalancePhase1QuietShellOffsetDelta ||
			CachedConvergenceSnapshot.ShellPlanarVelocity > Settings.BalancePhase1QuietShellVelocityDelta)
		{
			bLateValidationThisFrame = false;
			LateValidateBlockReason = TEXT("shell_contamination");
		}
		else
		{
			if (Phase1TopologyRecord.bRootSimulating)
			{
				bLateValidationThisFrame = false;
				LateValidateBlockReason = TEXT("topology_mismatch_simulating_critical");
			}
		}

		FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
		FPhysAnimCertifiedHandoffSnapshot LiveSnapshot;
		FPhysAnimLateValidationResult CurrentResult;
		FPhysAnimLateValidationResult LiveResult;

		const bool bCurrentSnapshotValid = BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult, true);
		const bool bLiveSnapshotValid = BuildCertifiedHandoffSnapshot(Owner, Settings, LiveSnapshot, LiveResult, false);

		const bool bExpectedUpperBodyRelease = Phase1TopologyRecord.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold &&
			LateValidationAccumulatedSeconds >= Settings.BalancePhase1LateValidateRequiredSeconds &&
			RootOnReadinessShellHoldAccumulatedSeconds >= Settings.BalancePhase2RequiredShellHoldDuration;

		bLateValidationProofPassed = bExpectedUpperBodyRelease;

		const bool bUpperBodyInstability = bCurrentSnapshotValid && bLiveSnapshotValid &&
			(LiveSnapshot.UpperBodyOwnershipMode != Phase1TopologyRecord.UpperBodyOwnershipMode ||
				(!bExpectedUpperBodyRelease && LiveSnapshot.UpperBodySimCount != Phase1TopologyRecord.UpperBodySimCount));
		const bool bSimCoverageRegressed = bCurrentSnapshotValid && bLiveSnapshotValid &&
			(LiveSnapshot.SimCount < Phase1TopologyRecord.TotalSimCount ||
				LiveSnapshot.RootOwnershipMode != Phase1TopologyRecord.RootOwnershipMode ||
				LiveSnapshot.ProximalOwnershipMode != Phase1TopologyRecord.ProximalOwnershipMode ||
				LiveSnapshot.DistalOwnershipMode != Phase1TopologyRecord.DistalOwnershipMode);
		const bool bFirstPolicyEnabledFrame = Owner->GetLastControlTargetDiagnostics().bFirstPolicyEnabledFrame;
		const bool bPolicyInfluenceRampReanchored = Owner->WasPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame();
		const bool bCurrentTargetDiscontinuity =
			CachedConvergenceSnapshot.MaxTargetDeltaDegrees > Settings.BalancePhase1MaxEntryTargetDeltaDeg ||
			CachedConvergenceSnapshot.MeanTargetDeltaDegrees > Settings.BalancePhase1MaxEntryTargetDeltaDeg;
		// The first policy-enabled frame re-bases the target history; do not treat that expected
		// transition spike as a late-validation failure.
		const bool bLateValidateTargetDiscontinuity =
			bCurrentTargetDiscontinuity &&
			!bFirstPolicyEnabledFrame &&
			!bPolicyInfluenceRampReanchored;

		if (bLateValidationThisFrame && bCurrentSnapshotValid)
		{
			if (bUpperBodyInstability || bSimCoverageRegressed || bLateValidateTargetDiscontinuity)
			{
				bLateValidationThisFrame = false;
				LateValidateBlockReason = ClassifyLateValidationFailureReason(bUpperBodyInstability, bSimCoverageRegressed, bLateValidateTargetDiscontinuity);
			}
		}
		else if (bLateValidationThisFrame && !bCurrentSnapshotValid)
		{
			bLateValidationThisFrame = false;
			LateValidateBlockReason = TEXT("phase1_late_validate_handoff_invalidated");
		}

		if (bLateValidationThisFrame)
		{
			const float CurrentFrameWorstLinearSpeed = CachedConvergenceSnapshot.MaxBodyLinearSpeed;
			const float CurrentFrameWorstAngularSpeed = CachedConvergenceSnapshot.MaxBodyAngularSpeed;
			
			// Diagnostics for worst speed across time.
			if (CurrentFrameWorstLinearSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed &&
				CurrentFrameWorstLinearSpeed > Diagnostics.Phase1LateValidateWorstLinearSpeed)
			{
				Diagnostics.Phase1LateValidateWorstLinearSpeed = CurrentFrameWorstLinearSpeed;
				Diagnostics.Phase1LateValidateWorstLinearSpeedBone = NAME_None; // Snapshot doesn't track bone
			}
			if (CurrentFrameWorstAngularSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed &&
				CurrentFrameWorstAngularSpeed > Diagnostics.Phase1LateValidateWorstAngularSpeed)
			{
				Diagnostics.Phase1LateValidateWorstAngularSpeed = CurrentFrameWorstAngularSpeed;
				Diagnostics.Phase1LateValidateWorstAngularSpeedBone = NAME_None; // Snapshot doesn't track bone
			}

			int32 ProximalSimCount = CurrentSnapshot.ProximalSimCount;
			int32 DistalSimCount = CurrentSnapshot.DistalSimCount;
			int32 UpperSimCount = CurrentSnapshot.UpperBodySimCount;

			const bool bLateValidationMotionViolatesThreshold =
				CurrentFrameWorstLinearSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed ||
				CurrentFrameWorstAngularSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed;
			if (bLateValidationMotionViolatesThreshold)
			{
				Diagnostics.Phase1LateValidateBodyMotionViolationAccumulatedSeconds += DeltaTime;
			}
			else
			{
				Diagnostics.Phase1LateValidateBodyMotionViolationAccumulatedSeconds = 0.0f;
			}

			const bool bIsRootCoupledTopology = BalanceTransitionSets::IsRootCoupledReadyHandoff(ProximalSimCount, DistalSimCount, UpperSimCount, false);
			const bool bIsUpperOnlyTopology = BalanceTransitionSets::IsUpperOnlySafeDenyHandoff(ProximalSimCount, DistalSimCount, UpperSimCount, false);

			LateValidationAccumulatedSeconds += DeltaTime;
			if (bIsRootCoupledTopology || (!bIsRootCoupledTopology && !bIsUpperOnlyTopology))
			{
				RootOnReadinessShellHoldAccumulatedSeconds += DeltaTime;
				const float CurrentShellOffsetCm = CachedConvergenceSnapshot.ShellPlanarOffset;
				const float CurrentShellVelocityDeltaCmPerSecond = CachedConvergenceSnapshot.ShellPlanarVelocity;
				if (!bHasRootOnReadinessShellProofBaseline)
				{
					RootOnReadinessShellProofStartOffsetCm = CurrentShellOffsetCm;
					RootOnReadinessShellProofStartVelocityCmPerSecond = CurrentShellVelocityDeltaCmPerSecond;
					bHasRootOnReadinessShellProofBaseline = true;
				}
				const float ShellOffsetGrowthCm = FMath::Max(0.0f, CurrentShellOffsetCm - RootOnReadinessShellProofStartOffsetCm);
				const float ShellVelocityGrowthCmPerSecond = FMath::Max(0.0f, CurrentShellVelocityDeltaCmPerSecond - RootOnReadinessShellProofStartVelocityCmPerSecond);
				const bool bShellProofThisFrame =
					CurrentShellOffsetCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm &&
					CurrentShellVelocityDeltaCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond &&
					ShellOffsetGrowthCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm &&
					ShellVelocityGrowthCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond &&
					Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle;
				if (bShellProofThisFrame)
				{
					RootOnReadinessShellProofAccumulatedSeconds += DeltaTime;
				}
				else
				{
					RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
					RootOnReadinessShellProofStartOffsetCm = CurrentShellOffsetCm;
					RootOnReadinessShellProofStartVelocityCmPerSecond = CurrentShellVelocityDeltaCmPerSecond;
				}
			}
			else
			{
				// Upper-only Safe-Deny path does not participate in shell-hold/proof
				RootOnReadinessShellHoldAccumulatedSeconds = 0.0f;
				RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
				bHasRootOnReadinessShellProofBaseline = false;
			}

			// Carry the quiet-proof timer through late validation so the certified handoff
			// reflects a real sustain window under initial policy influence.
			QuietWindowAccumulatedSeconds += DeltaTime;

			Diagnostics.Phase1LateValidateGateSource = TEXT("live_late_validate_gate");
			Diagnostics.Phase1LateValidateGateReason = bIsRootCoupledTopology && CurrentResult.bRootOnReadinessShellHoldSatisfied
				? TEXT("ready")
				: bIsUpperOnlyTopology && LateValidationAccumulatedSeconds >= Settings.BalancePhase1LateValidateRequiredSeconds
					? TEXT("upper_only_safe_deny")
					: TEXT("phase1_late_validate_shell_hold_pending");

			Diagnostics.Phase1RootOnReadinessGateReason = bIsRootCoupledTopology && CurrentResult.bRootOnReadinessProven
				? TEXT("ready")
				: bIsUpperOnlyTopology 
					? TEXT("phase1_root_on_readiness_upper_only_safe_deny_pending")
					: TEXT("phase1_root_on_readiness_shell_hold_pending");
			Diagnostics.Phase1LateValidateAccumulatedSeconds = LateValidationAccumulatedSeconds;

			const bool bCanCompleteAsRootCoupledReady =
				bIsRootCoupledTopology &&
				CurrentResult.bRootOnReadinessShellHoldSatisfied &&
				CurrentResult.bRootOnReadinessProven;
			const bool bCanCompleteAsUpperOnlySafeDeny =
				bIsUpperOnlyTopology &&
				LateValidationAccumulatedSeconds >= Settings.BalancePhase1LateValidateRequiredSeconds;

			if (bCanCompleteAsRootCoupledReady || bCanCompleteAsUpperOnlySafeDeny)
			{
				if (!bCanCompleteAsRootCoupledReady &&
					RootOnReadinessShellHoldAccumulatedSeconds + KINDA_SMALL_NUMBER < Settings.BalancePhase2RequiredShellHoldDuration)
				{
					UE_LOG(
						LogPhysAnimBridge,
						Warning,
						TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_MINIMUM_MET shellHoldDuration=%.2f shellHoldRequired=%.2f lateValidateDuration=%.2f requiredLateValidateSeconds=%.2f"),
						RootOnReadinessShellHoldAccumulatedSeconds,
						Settings.BalancePhase2RequiredShellHoldDuration,
						LateValidationAccumulatedSeconds,
						Settings.BalancePhase1LateValidateRequiredSeconds);
				}

				if (!bCanCompleteAsRootCoupledReady &&
					RootOnReadinessShellHoldAccumulatedSeconds + KINDA_SMALL_NUMBER < Settings.BalancePhase2RequiredShellHoldDuration)
				{
					UE_LOG(
						LogPhysAnimBridge,
						Warning,
						TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_SHELL_HOLD_CAPPED_BY_WINDOW lateValidateDuration=%.2f requiredLateValidateSeconds=%.2f shellHoldDuration=%.2f shellHoldRequired=%.2f"),
						LateValidationAccumulatedSeconds,
						Settings.BalancePhase1LateValidateRequiredSeconds,
						RootOnReadinessShellHoldAccumulatedSeconds,
						Settings.BalancePhase2RequiredShellHoldDuration);
				}

				bHasLateValidationProof = true;
				FString CaptureReason;
				if (CaptureCertifiedHandoff(Owner, Settings, CaptureReason))
				{
					if (CertifiedLateValidationResult.Outcome == EBalanceLateValidationOutcome::Outcome_SafeDenyUpperOnly)
					{
						const FString DenialReason = TEXT("phase2_upper_only_handoff_safe_denied");
						Diagnostics.FailureReason = DenialReason;
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s"), *DenialReason);
						Owner->ReleaseTransitionOwnedShellLock();
						MarkSafePhase2Denied(Owner, DenialReason);
						return;
					}

					const FString RootOnReadinessGateReason =
						CertifiedLateValidationResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady
								? TEXT("ready")
							: Diagnostics.Phase1RootOnReadinessGateReason;
					Diagnostics.Phase1RootOnReadinessGateReason = RootOnReadinessGateReason;

					UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] EMIT_READY_HANDOFF classification=%s outcome=%s"), 
						BalanceTransitionSets::GetRootOnReadinessClassificationName(CertifiedLateValidationResult.RootOnReadinessClassification),
						BalanceTransitionSets::GetLateValidationOutcomeName(CertifiedLateValidationResult.Outcome));
					UE_LOG(
						LogPhysAnimBridge,
						Log,
						TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_SUCCESS topology=%s upperBodyOwnership=%s simCount=%d upperBodySimCount=%d policySuppressed=%d controlAuthoritySettled=%d lateValidateDuration=%.2f quietProofDuration=%.2f"),
						*CertifiedHandoff.TopologyClass,
						BalanceTransitionSets::GetUpperBodyOwnershipModeName(CertifiedHandoff.UpperBodyOwnershipMode),
						CertifiedHandoff.SimCount,
						CertifiedHandoff.UpperBodySimCount,
						CertifiedHandoff.bPolicySuppressed ? 1 : 0,
						CertifiedHandoff.bControlAuthoritySettled ? 1 : 0,
						CertifiedLateValidationResult.LateValidationSustainDurationSeconds,
						CertifiedLateValidationResult.QuietProofDurationSeconds);
					UE_LOG(
						LogPhysAnimBridge,
						Log,
					TEXT("[PhysAnimBalance] PHASE1_READY_FOR_ROOT_ON topology=%s upperBodyOwnership=%s simCount=%d proximalSimCount=%d distalSimCount=%d upperBodySimCount=%d policySuppressed=%d controlAuthoritySettled=%d finalBringUpControlAlpha=%.2f policyInfluenceAlpha=%.2f policyInfluenceRequired=%.2f policyInfluenceDuration=%.2f policyInfluenceRequiredSeconds=%.2f policyInfluenceRampReanchored=%d shellHoldReady=%d bringUpReady=%d policyInfluenceReady=%d rootOnReady=%d rootOnReadinessClassification=%s rootOnReadinessGateReason=%s shellHoldDuration=%.2f shellHoldRequired=%.2f maxTargetDelta=%.1f meanTargetDelta=%.1f quietProofDuration=%.2f lateValidateDuration=%.2f"),
						*CertifiedHandoff.TopologyClass,
						BalanceTransitionSets::GetUpperBodyOwnershipModeName(CertifiedHandoff.UpperBodyOwnershipMode),
					CertifiedHandoff.SimCount,
					CertifiedHandoff.ProximalSimCount,
					CertifiedHandoff.DistalSimCount,
					CertifiedHandoff.UpperBodySimCount,
					CertifiedHandoff.bPolicySuppressed ? 1 : 0,
					CertifiedHandoff.bControlAuthoritySettled ? 1 : 0,
					CertifiedHandoff.FinalBringUpGroupControlAuthorityAlpha,
					CertifiedHandoff.PolicyInfluenceAlphaAtCapture,
					CertifiedHandoff.RootOnReadinessPolicyInfluenceRequiredAlpha,
					CertifiedHandoff.RootOnReadinessPolicyInfluenceDurationSeconds,
					CertifiedHandoff.RootOnReadinessPolicyInfluenceRequiredSeconds,
					CertifiedHandoff.bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessShellHoldSatisfied ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessFinalBringUpControlSettled ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessPolicyInfluenceSettled ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessProven ? 1 : 0,
						BalanceTransitionSets::GetRootOnReadinessClassificationName(CertifiedLateValidationResult.RootOnReadinessClassification),
						*RootOnReadinessGateReason,
						CertifiedHandoff.RootOnReadinessShellHoldDurationSeconds,
						CertifiedHandoff.RootOnReadinessShellHoldRequiredSeconds,
						CertifiedLateValidationResult.MaxTargetDeltaDegrees,
						CertifiedLateValidationResult.MeanTargetDeltaDegrees,
						CertifiedLateValidationResult.QuietProofDurationSeconds,
						CertifiedLateValidationResult.LateValidationSustainDurationSeconds);
					SetPhase(EBalanceReadyTransitionPhase::BRT_Phase2_RootOn, Owner);
					return;
				}

				LateValidateBlockReason = CaptureReason.IsEmpty()
					? TEXT("phase2_handoff_capture_failed")
					: CaptureReason;
				Diagnostics.FailureReason = LateValidateBlockReason;
				if (IsPhase1OwnedCondition(LateValidateBlockReason))
				{
					ReturnToPhase1Prepare(Owner, LateValidateBlockReason, TEXT("PHASE1_LATE_VALIDATE_RETURN_TO_PREPARE"));
					return;
				}
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s"), *LateValidateBlockReason);
				Owner->ReleaseTransitionOwnedShellLock();
				MarkSafePhase2Denied(Owner, LateValidateBlockReason);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_RESET reason=%s"), *LateValidateBlockReason);
				return;
			}
		}
		else
		{
			RootOnReadinessShellHoldAccumulatedSeconds = 0.0f;
			RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
			bHasRootOnReadinessShellProofBaseline = false;
			if (!LateValidateBlockReason.IsEmpty())
			{
				LastLateValidateBlockReason = LateValidateBlockReason;
			}
			else
			{
				LastLateValidateBlockReason.Reset();
			}

			if (LateValidateBlockReason == TEXT("live_proximal_upper_body_velocity_instability"))
			{
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_BODY_MOTION_RESET worstLinearBone=%s worstLinear=%.2f linearThreshold=%.2f worstAngularBone=%s worstAngular=%.2f angularThreshold=%.2f accumulatedViolation=%.2f grace=%.2f"),
					*Diagnostics.Phase1LateValidateWorstLinearSpeedBone.ToString(),
					Diagnostics.Phase1LateValidateWorstLinearSpeed,
					Settings.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed,
					*Diagnostics.Phase1LateValidateWorstAngularSpeedBone.ToString(),
					Diagnostics.Phase1LateValidateWorstAngularSpeed,
					Settings.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed,
					Diagnostics.Phase1LateValidateBodyMotionViolationAccumulatedSeconds,
					Settings.BalancePhase1LateValidateBodyMotionGraceDuration);
			}

			if (LateValidateBlockReason == TEXT("live_proximal_upper_body_velocity_instability") &&
				Diagnostics.Phase1LateValidateBodyMotionViolationAccumulatedSeconds < Settings.BalancePhase1LateValidateBodyMotionGraceDuration)
			{
				Diagnostics.Phase1LateValidateGateReason = LateValidateBlockReason;
				Diagnostics.Phase1LateValidateGateSource = TEXT("live_late_validate_gate");
				Diagnostics.Phase1LateValidateAccumulatedSeconds = LateValidationAccumulatedSeconds;
				return;
			}

			if (LateValidateBlockReason != TEXT("policy_influence_inactive"))
			{
				LateValidationAccumulatedSeconds = 0.0f;
				QuietWindowAccumulatedSeconds = 0.0f;
				Diagnostics.Phase1LateValidateGateReason = LateValidateBlockReason;
				Diagnostics.Phase1LateValidateGateSource = TEXT("live_late_validate_gate");
				Diagnostics.Phase1LateValidateAccumulatedSeconds = 0.0f;
				Diagnostics.Phase1LateValidateBodyMotionViolationAccumulatedSeconds = 0.0f;
				Diagnostics.Phase1LateValidateWorstLinearSpeed = 0.0f;
				Diagnostics.Phase1LateValidateWorstAngularSpeed = 0.0f;
				Diagnostics.Phase1LateValidateWorstLinearSpeedBone = NAME_None;
				Diagnostics.Phase1LateValidateWorstAngularSpeedBone = NAME_None;
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_RESET reason=%s lateValidateSeconds=%.2f quietProofSeconds=%.2f policyAlpha=%.2f"),
					*LateValidateBlockReason,
					LateValidationAccumulatedSeconds,
					QuietWindowAccumulatedSeconds,
					PolicyInfluenceAlpha);
				ResetCertifiedHandoffState();
				SetPhase(EBalanceReadyTransitionPhase::BRT_Phase1_Prepare, Owner);
				return;
			}

			Diagnostics.Phase1LateValidateGateReason = LateValidateBlockReason;
			Diagnostics.Phase1LateValidateGateSource = TEXT("live_late_validate_gate");
			Diagnostics.Phase1LateValidateAccumulatedSeconds = LateValidationAccumulatedSeconds;
		}

		if (TotalTransitionTimeSeconds > Settings.BalancePhase1TimeoutDuration)
		{
			const FString TimeoutReason = LateValidateBlockReason.IsEmpty()
				? TEXT("phase1_no_convergence_path")
				: TEXT("phase1_no_convergence_path_") + LateValidateBlockReason;
			Diagnostics.FailureReason = TimeoutReason;
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s"), *TimeoutReason);
			Owner->ReleaseTransitionOwnedShellLock();
			MarkSafePhase2Denied(Owner, TimeoutReason);
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_RESET reason=%s"), *TimeoutReason);
			return;
		}
		return;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		++Phase2GuardTickCount;
		if (Phase2GuardTickCount > 1)
		{
			bPhase2RootAuthorityQuarantined = false;
		}
		CaptureFlipDiagnostics(Owner);
		const bool bPelvisRequestedSim = Owner->WasPelvisSimulatingLastFrame();
		const bool bPelvisActualSim = Owner->IsPelvisSimulatingNow();
		const FPhysAnimControlTargetDiagnostics& ControlTargetDiagnostics = Owner->GetLastControlTargetDiagnostics();

		if (Phase2GuardTickCount <= 2)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnimBalance] PHASE2_GUARD_TICK tick=%d requestedRootSim=%d actualRootSim=%d resetScheduled=%d simCountPost=%d distalSimPost=%d shellOffsetDelta=%.1f shellVelocityDelta=%.1f"),
				Phase2GuardTickCount,
				bPelvisRequestedSim ? 1 : 0,
				bPelvisActualSim ? 1 : 0,
				Diagnostics.bResetScheduled ? 1 : 0,
				Diagnostics.SimCountPost,
				Diagnostics.DistalSimCountPost,
				Diagnostics.BaselineShellOffset,
				Diagnostics.BaselineShellVel);
		}

		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelPelvis);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelThighs);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelSpine);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelFeet);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelPelvis);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelThighs);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelSpine);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelFeet);

		FString AbortReason;
		FString AbortDetail;
		if (Diagnostics.bPolicyWroteTargets)
		{
			AbortReason = TEXT("phase2_policy_write_leak");
			AbortDetail = FString::Printf(
				TEXT("policyWrites=%d firstPolicyFrame=%d maxTargetDeltaBone=%s maxTargetDelta=%.1f maxRawOffsetBone=%s maxRawOffset=%.1f"),
				ControlTargetDiagnostics.NumNormalPolicyTargetsWritten,
				ControlTargetDiagnostics.bFirstPolicyEnabledFrame ? 1 : 0,
				*ControlTargetDiagnostics.MaxTargetDeltaBoneName.ToString(),
				ControlTargetDiagnostics.MaxTargetDeltaDegrees,
				*ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName.ToString(),
				ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees);
		}
		else if (Diagnostics.bResetScheduled || !Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
		{
			AbortReason = TEXT("phase2_reset_violation");
			AbortDetail = FString::Printf(
				TEXT("resetScheduled=%d pendingResets=%d"),
				Diagnostics.bResetScheduled ? 1 : 0,
				Owner->GetPendingBodyModifierCachedResetNames().Num());
		}
		else if (Diagnostics.bShellContributed &&
			(Diagnostics.BaselineShellOffset > Settings.BalancePhase2AbortShellOffsetDelta ||
			 Diagnostics.BaselineShellVel > Settings.BalancePhase2AbortShellVelocityDelta))
		{
			AbortReason = TEXT("phase2_shell_correction_material");
			AbortDetail = FString::Printf(
				TEXT("shellOffsetDelta=%.1f/%.1f shellVelocityDelta=%.1f/%.1f"),
				Diagnostics.BaselineShellOffset,
				Settings.BalancePhase2AbortShellOffsetDelta,
				Diagnostics.BaselineShellVel,
				Settings.BalancePhase2AbortShellVelocityDelta);
		}
		else if (!bPelvisActualSim)
		{
			AbortReason = TEXT("phase2_root_simulation_dropped");
			AbortDetail = FString::Printf(
				TEXT("requestedRootSim=%d actualRootSim=%d"),
				bPelvisRequestedSim ? 1 : 0,
				bPelvisActualSim ? 1 : 0);
		}
		else if (Owner->IsInstabilityPrecursorActive())
		{
			AbortReason = TEXT("phase2_fail_stop_precursor");
			AbortDetail = TEXT("instabilityPrecursor=1");
		}
		else if (PhaseTimeSeconds > BalanceTransitionSets::Phase2TopologySettleGraceSeconds &&
			!BalanceTransitionSets::IsExpectedPhase2Topology(
			Diagnostics.SimCountPre,
			Diagnostics.SimCountPost,
			Diagnostics.DistalSimCountPre,
			Diagnostics.DistalSimCountPost))
		{
			AbortReason = TEXT("phase2_topology_not_preserved");
			AbortDetail = FString::Printf(
				TEXT("simCountPre=%d simCountPost=%d distalSimPre=%d distalSimPost=%d upperBodySimPre=%d upperBodySimPost=%d"),
				Diagnostics.SimCountPre,
				Diagnostics.SimCountPost,
				Diagnostics.DistalSimCountPre,
				Diagnostics.DistalSimCountPost,
				Diagnostics.UpperBodySimCountPre,
				Diagnostics.UpperBodySimCountPost);
		}
		else if (Diagnostics.RootSpeed > Settings.BalancePhase2AbortRootLinearSpeed ||
			Diagnostics.RootAngularSpeed > Settings.BalancePhase2AbortRootAngularSpeed ||
			Diagnostics.BaselineShellOffset > Settings.BalancePhase2AbortShellOffsetDelta ||
			Diagnostics.BaselineShellVel > Settings.BalancePhase2AbortShellVelocityDelta ||
			Diagnostics.PeakMaxBodyLinearSpeed > Settings.BalancePhase2AbortMaxBodyLinearSpeed ||
			Diagnostics.PeakMaxBodyAngularSpeed > Settings.BalancePhase2AbortMaxBodyAngularSpeed)
		{
			AbortReason = TEXT("phase2_root_on_spike");
			if (Diagnostics.RootSpeed > Settings.BalancePhase2AbortRootLinearSpeed)
			{
				AbortDetail = FString::Printf(TEXT("rootLinearSpeed=%.1f/%.1f"), Diagnostics.RootSpeed, Settings.BalancePhase2AbortRootLinearSpeed);
			}
			else if (Diagnostics.RootAngularSpeed > Settings.BalancePhase2AbortRootAngularSpeed)
			{
				AbortDetail = FString::Printf(TEXT("rootAngularSpeed=%.1f/%.1f"), Diagnostics.RootAngularSpeed, Settings.BalancePhase2AbortRootAngularSpeed);
			}
			else if (Diagnostics.BaselineShellOffset > Settings.BalancePhase2AbortShellOffsetDelta)
			{
				AbortDetail = FString::Printf(TEXT("shellOffsetDelta=%.1f/%.1f"), Diagnostics.BaselineShellOffset, Settings.BalancePhase2AbortShellOffsetDelta);
			}
			else if (Diagnostics.BaselineShellVel > Settings.BalancePhase2AbortShellVelocityDelta)
			{
				AbortDetail = FString::Printf(TEXT("shellVelocityDelta=%.1f/%.1f"), Diagnostics.BaselineShellVel, Settings.BalancePhase2AbortShellVelocityDelta);
			}
			else if (Diagnostics.PeakMaxBodyLinearSpeed > Settings.BalancePhase2AbortMaxBodyLinearSpeed)
			{
				AbortDetail = FString::Printf(TEXT("maxBodyLinearSpeed=%.1f/%.1f"), Diagnostics.PeakMaxBodyLinearSpeed, Settings.BalancePhase2AbortMaxBodyLinearSpeed);
			}
			else
			{
				AbortDetail = FString::Printf(TEXT("maxBodyAngularSpeed=%.1f/%.1f"), Diagnostics.PeakMaxBodyAngularSpeed, Settings.BalancePhase2AbortMaxBodyAngularSpeed);
			}
		}

		if (!AbortReason.IsEmpty())
		{
			Diagnostics.FailureReason = AbortReason;
			const EBalanceReadyConditionOwner FailureOwner = ClassifyConditionOwner(Diagnostics.FailureReason);
			Diagnostics.LastRetryDecision = IsAutomaticRetryAllowed(
				Diagnostics.FailureReason,
				true,
				false,
				false,
				true,
				Phase2RetryCount < Settings.BalancePhase2MaxAutomaticRetries) ? TEXT("allowed") : TEXT("denied");
			UE_LOG(
				LogPhysAnimBridge,
				Error,
				TEXT("[PhysAnimBalance] PHASE2_GUARD_WINDOW_ABORTED reason=%s owner=%d detail=%s rootLinear=%.1f/%.1f rootAngular=%.1f/%.1f shellOffsetDelta=%.1f/%.1f shellVelocityDelta=%.1f/%.1f maxBodyLinear=%.1f/%.1f maxBodyAngular=%.1f/%.1f pelvisLin=%.1f pelvisAng=%.1f thighsLin=%.1f thighsAng=%.1f spineLin=%.1f spineAng=%.1f feetLin=%.1f feetAng=%.1f simCountPre=%d simCountPost=%d upperBodySimPre=%d upperBodySimPost=%d shellLocked=%d shellReanchored=%d shellReseeded=%d policyActive=%d firstPolicyFrame=%d policyWrites=%d maxTargetDeltaBone=%s maxTargetDelta=%.1f maxRawOffsetBone=%s maxRawOffset=%.1f lowerLimbLimitBone=%s lowerLimbLimit=%.2f lowerLimbLimitProxy=%.1f"),
				*Diagnostics.FailureReason,
				static_cast<int32>(FailureOwner),
				*AbortDetail,
				Diagnostics.RootSpeed,
				Settings.BalancePhase2AbortRootLinearSpeed,
				Diagnostics.RootAngularSpeed,
				Settings.BalancePhase2AbortRootAngularSpeed,
				Diagnostics.BaselineShellOffset,
				Settings.BalancePhase2AbortShellOffsetDelta,
				Diagnostics.BaselineShellVel,
				Settings.BalancePhase2AbortShellVelocityDelta,
				Diagnostics.PeakMaxBodyLinearSpeed,
				Settings.BalancePhase2AbortMaxBodyLinearSpeed,
				Diagnostics.PeakMaxBodyAngularSpeed,
				Settings.BalancePhase2AbortMaxBodyAngularSpeed,
				Diagnostics.MaxLinVelPelvis,
				Diagnostics.MaxAngVelPelvis,
				Diagnostics.MaxLinVelThighs,
				Diagnostics.MaxAngVelThighs,
				Diagnostics.MaxLinVelSpine,
				Diagnostics.MaxAngVelSpine,
				Diagnostics.MaxLinVelFeet,
				Diagnostics.MaxAngVelFeet,
				Diagnostics.SimCountPre,
				Diagnostics.SimCountPost,
				Diagnostics.UpperBodySimCountPre,
				Diagnostics.UpperBodySimCountPost,
				CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
				CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
				CertifiedHandoff.bTransitionShellReferenceReseededAfterLock ? 1 : 0,
				ControlTargetDiagnostics.bPolicyInfluenceActive ? 1 : 0,
				ControlTargetDiagnostics.bFirstPolicyEnabledFrame ? 1 : 0,
				ControlTargetDiagnostics.NumNormalPolicyTargetsWritten,
				*ControlTargetDiagnostics.MaxTargetDeltaBoneName.ToString(),
				ControlTargetDiagnostics.MaxTargetDeltaDegrees,
				*ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName.ToString(),
				ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees,
				*ControlTargetDiagnostics.MaxLowerLimbLimitOccupancyBoneName.ToString(),
				ControlTargetDiagnostics.MaxLowerLimbLimitOccupancy,
				ControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees);
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_RETRY_DECISION failure=%s owner=%d decision=%s changedState=0 freshQuietProof=0 remainingRetryBudget=%d"),
				*Diagnostics.FailureReason, static_cast<int32>(FailureOwner), *Diagnostics.LastRetryDecision, FMath::Max(Settings.BalancePhase2MaxAutomaticRetries - Phase2RetryCount, 0));
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
			return;
		}

		if (PhaseTimeSeconds > Settings.BalancePhase2GuardWindowDuration)
		{
			UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_READY_FOR_PHASE3"));
			SetPhase(EBalanceReadyTransitionPhase::BRT_Phase3_Settle, Owner);
		}
		return;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		FString Phase3Violation;
		if (!ValidatePhase3Continuity(Owner, Settings, Phase3Violation))
		{
			Diagnostics.FailureReason = Phase3Violation;
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
			return;
		}

		if (bReadyThisFrame)
		{
			StableHoldAccumulatedSeconds += DeltaTime;
			if (StableHoldAccumulatedSeconds >= Settings.BalancePhase3RequiredStableHoldDuration)
			{
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Phase 3 settle success. Ready for perturbation. duration=%.2f"), StableHoldAccumulatedSeconds);
				SetPhase(EBalanceReadyTransitionPhase::BRT_Succeeded, Owner);
			}
		}
		else
		{
			StableHoldAccumulatedSeconds = 0.0f;
		}

		if (PhaseTimeSeconds > Settings.BalancePhase3TimeoutDuration)
		{
			Diagnostics.FailureReason = TEXT("phase3_no_convergence_path");
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
		}
	}
}

void FPhysAnimBalanceReadyTransition::SetPhase(EBalanceReadyTransitionPhase NewPhase, UPhysAnimComponent* Owner)
{
	if (InternalPhase == NewPhase)
	{
		return;
	}

	if (Owner &&
		NewPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate &&
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		Owner->ActivateTransitionOwnedShellLock();
	}

	if (NewPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn && Owner)
	{
		const FPhysAnimStabilizationSettings EffectiveSettings = Owner->ResolveEffectiveStabilizationSettings();
		FString DenyReason;
		if (!ValidatePhase2EntryPreconditions(Owner, EffectiveSettings, DenyReason))
		{
			if (IsPhase1OwnedCondition(DenyReason))
			{
				ReturnToPhase1Prepare(Owner, DenyReason, TEXT("PHASE2_ENTRY_RETURN_TO_PHASE1"));
				return;
			}

			const EBalanceReadyConditionOwner FailureOwner = ClassifyConditionOwner(DenyReason);
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED reason=%s owner=%d"), *DenyReason, static_cast<int32>(FailureOwner));
			Owner->ReleaseTransitionOwnedShellLock();
			MarkSafePhase2Denied(Owner, DenyReason);
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_QUIET_WINDOW_RESET reason=%s"), *DenyReason);
			return;
		}
	}

	if (Owner &&
		NewPhase != EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate &&
		NewPhase != EBalanceReadyTransitionPhase::BRT_Phase2_RootOn &&
		NewPhase != EBalanceReadyTransitionPhase::BRT_Phase3_Settle &&
		InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive)
	{
		Owner->ReleaseTransitionOwnedShellLock();
	}

	if (Owner &&
		InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive &&
		(NewPhase == EBalanceReadyTransitionPhase::BRT_Succeeded ||
			NewPhase == EBalanceReadyTransitionPhase::BRT_Failed ||
			NewPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied ||
			NewPhase == EBalanceReadyTransitionPhase::BRT_Inactive))
	{
		Owner->SetStartupBringUpFrozenByBalanceEntry(false);
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnimBalance] BALANCE_ENTRY_FREEZE state=off reason=transition_terminal_exit phase=%d"),
			static_cast<int32>(NewPhase));
	}

	if (NewPhase != EBalanceReadyTransitionPhase::BRT_Inactive && InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE_COMMIT commit=%d"), static_cast<int32>(InternalPhase));
	}

	InternalPhase = NewPhase;
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE_ENTRY phase=%d"), static_cast<int32>(InternalPhase));
	PhaseTimeSeconds = 0.0f;
	StableHoldAccumulatedSeconds = 0.0f;
	TargetDiscontinuityAccumulatedSeconds = 0.0f;
	LastQuietBlockReason.Reset();
	Phase2GuardTickCount = 0;
	bPhase2RootAuthorityQuarantined = false;

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn && Owner)
	{
		Owner->CommitTransitionOwnedShellDrop();
		if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
		{
			if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()))
			{
				Diagnostics.BaselineRootLinVel = PelvisBody->GetUnrealWorldVelocity().Size();
				Diagnostics.BaselineRootAngVel = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians()).Size();
				Diagnostics.BaselineShellOffset = Owner->GetCurrentShellPlanarOffsetDeltaCm();
				Diagnostics.BaselineShellVel = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
				TArray<FName> SimulatingBones;
				Owner->GetSimulatingBodies(SimulatingBones);
				Diagnostics.SimCountPre = SimulatingBones.Num();
				Diagnostics.DistalSimCountPre = 0;
				Diagnostics.UpperBodySimCountPre = 0;
				int32 ProximalSimCountPre = 0;
				for (const FName BoneName : SimulatingBones)
				{
					if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
					{
						Diagnostics.DistalSimCountPre++;
					}
					else if (BalanceTransitionSets::IsProximal(BoneName))
					{
						ProximalSimCountPre++;
					}
					else if (BalanceTransitionSets::IsUpperBody(BoneName))
					{
						Diagnostics.UpperBodySimCountPre++;
					}
				}

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ENTRY topology=%s upperBodyOwnership=%s rootPreLin=%.1f rootPreAng=%.1f shellOffsetDelta=%.1f shellVelocityDelta=%.1f simCountPre=%d proximalSimPre=%d distalSimPre=%d upperBodySimPre=%d policySuppressed=%d controlAuthoritySettled=%d finalBringUpControlAlpha=%.2f policyInfluenceAlpha=%.2f policyInfluenceRequired=%.2f policyInfluenceDuration=%.2f policyInfluenceRequiredSeconds=%.2f policyInfluenceRampReanchored=%d shellHoldReady=%d bringUpReady=%d policyInfluenceReady=%d rootOnReady=%d shellHoldDuration=%.2f shellHoldRequired=%.2f maxTargetDelta=%.1f meanTargetDelta=%.1f quietProofDuration=%.2f resetScheduled=%d"),
					CertifiedHandoff.TopologyClass.IsEmpty() ? TEXT("unknown") : *CertifiedHandoff.TopologyClass,
					BalanceTransitionSets::GetUpperBodyOwnershipModeName(CertifiedHandoff.UpperBodyOwnershipMode),
					Diagnostics.BaselineRootLinVel,
					Diagnostics.BaselineRootAngVel,
					Diagnostics.BaselineShellOffset,
					Diagnostics.BaselineShellVel,
					Diagnostics.SimCountPre,
					ProximalSimCountPre,
					Diagnostics.DistalSimCountPre,
					Diagnostics.UpperBodySimCountPre,
					CertifiedHandoff.bPolicySuppressed ? 1 : 0,
					CertifiedHandoff.bControlAuthoritySettled ? 1 : 0,
					CertifiedHandoff.FinalBringUpGroupControlAuthorityAlpha,
					CertifiedHandoff.PolicyInfluenceAlphaAtCapture,
					CertifiedHandoff.RootOnReadinessPolicyInfluenceRequiredAlpha,
					CertifiedHandoff.RootOnReadinessPolicyInfluenceDurationSeconds,
					CertifiedHandoff.RootOnReadinessPolicyInfluenceRequiredSeconds,
					CertifiedHandoff.bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessShellHoldSatisfied ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessFinalBringUpControlSettled ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessPolicyInfluenceSettled ? 1 : 0,
					CertifiedLateValidationResult.bRootOnReadinessProven ? 1 : 0,
					CertifiedHandoff.RootOnReadinessShellHoldDurationSeconds,
					CertifiedHandoff.RootOnReadinessShellHoldRequiredSeconds,
					CertifiedLateValidationResult.MaxTargetDeltaDegrees,
					CertifiedLateValidationResult.MeanTargetDeltaDegrees,
					CertifiedLateValidationResult.QuietProofDurationSeconds,
					Diagnostics.bResetScheduled ? 1 : 0);

				FVector LiveChainCenterCm = FVector::ZeroVector;
				const float PelvisProximalConstraintErrorCm =
					BalanceTransitionSets::ComputePelvisProximalConstraintErrorCm(Mesh, SimulatingBones, LiveChainCenterCm);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_CONSTRAINT_ERROR_PRE pelvisProximalError=%.2f liveChainCenter=(%.1f,%.1f,%.1f) threshold=%.2f"),
					PelvisProximalConstraintErrorCm,
					LiveChainCenterCm.X,
					LiveChainCenterCm.Y,
					LiveChainCenterCm.Z,
					BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm);
				if (PelvisProximalConstraintErrorCm > BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm)
				{
					Diagnostics.FailureReason = TEXT("phase2_constraint_error_too_high");
					SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
					return;
				}

				const FVector PelvisLocation = Mesh->GetBoneTransform(Mesh->GetBoneIndex(PhysAnimBridge::GetRootBoneName())).GetLocation();
				const auto LogLinkError = [&](const TCHAR* Label, FName BoneName)
				{
					const FVector BoneLocation = Mesh->GetBoneTransform(Mesh->GetBoneIndex(BoneName)).GetLocation();
					const float ErrorCm = FVector::Dist(PelvisLocation, BoneLocation);
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_PRE link=%s errorCm=%.2f threshold=%.2f"),
						Label,
						ErrorCm,
						BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm);
					return ErrorCm;
				};
				const float PelvisThighLErrorCm = LogLinkError(TEXT("pelvis_thigh_l"), TEXT("thigh_l"));
				const float PelvisThighRErrorCm = LogLinkError(TEXT("pelvis_thigh_r"), TEXT("thigh_r"));
				const float PelvisSpine01ErrorCm = LogLinkError(TEXT("pelvis_spine_01"), TEXT("spine_01"));
				if (PelvisThighLErrorCm > BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm ||
					PelvisThighRErrorCm > BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm ||
					PelvisSpine01ErrorCm > BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm)
				{
					Diagnostics.FailureReason = TEXT("phase2_constraint_error_too_high");
					SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
					return;
				}
				PelvisBody->SetBodyTransform(
					BalanceTransitionSets::BuildWarmStartPelvisTransform(Mesh, SimulatingBones),
					ETeleportType::TeleportPhysics,
					true);
				PelvisBody->SetLinearVelocity(FVector::ZeroVector, false);
				PelvisBody->SetAngularVelocityInRadians(FVector::ZeroVector, false);
				PelvisBody->SetInstanceSimulatePhysics(true);
				PelvisBody->SetLinearVelocity(FVector::ZeroVector, false);
				PelvisBody->SetAngularVelocityInRadians(FVector::ZeroVector, false);
				bPhase2RootAuthorityQuarantined = true;
				Diagnostics.bSimFlipped = true;
				// Phase 2 must preserve the pre-root-on shell proof reference through the
				// root-on frame and guard window; reseeding here would invalidate that proof.
				CaptureFlipDiagnostics(Owner);
				if (!PelvisBody->IsInstanceSimulatingPhysics())
				{
					Diagnostics.FailureReason = TEXT("phase2_root_not_confirmed");
					SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
					return;
				}

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON rootPostLin=%.1f rootPostAng=%.1f simCountPost=%d distalSimPost=%d"),
					Diagnostics.PelvisLinearVelPost.Size(),
					Diagnostics.PelvisAngularVelPost.Size(),
					Diagnostics.SimCountPost,
					Diagnostics.DistalSimCountPost);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_POST link=pelvis_thigh_l errorCm=%.2f threshold=%.2f"),
					FVector::Dist(PelvisLocation, Mesh->GetBoneTransform(Mesh->GetBoneIndex(TEXT("thigh_l"))).GetLocation()),
					BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_POST link=pelvis_thigh_r errorCm=%.2f threshold=%.2f"),
					FVector::Dist(PelvisLocation, Mesh->GetBoneTransform(Mesh->GetBoneIndex(TEXT("thigh_r"))).GetLocation()),
					BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_POST link=pelvis_spine_01 errorCm=%.2f threshold=%.2f"),
					FVector::Dist(PelvisLocation, Mesh->GetBoneTransform(Mesh->GetBoneIndex(TEXT("spine_01"))).GetLocation()),
					BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm);
				UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_GUARD_WINDOW_STARTED duration=%.2f"), Owner->ResolveEffectiveStabilizationSettings().BalancePhase2GuardWindowDuration);
			}
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_SUCCESS."));
		Phase2RetryCount = 0;
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_SAFE_DENIED final_outcome. reason=%s"), *Diagnostics.FailureReason);
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_ABORT reason=%s"), *Diagnostics.FailureReason);
		if (Owner)
		{
			if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
			{
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_RECOVERY_BEGIN"));
				if (Diagnostics.bSimFlipped)
				{
					if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()))
					{
						PelvisBody->SetInstanceSimulatePhysics(false);
					}
				}
				ResetTransitionLocalState();
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_RECOVERY_COMPLETE"));
			}
		}
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Inactive ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		ResetCertifiedHandoffState();
	}
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
	// Phase 1 Prepare specifically allows non-simulating pelvis and pending resets 
	// while waiting for the quiet window. LateValidate and beyond require convergence.
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && !CachedConvergenceSnapshot.bIsPelvisSimulating)
	{
		OutReason = TEXT("pelvis_not_simulating");
		return false;
	}

	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && CachedConvergenceSnapshot.bHasPendingResets)
	{
		OutReason = TEXT("pending_resets");
		return false;
	}

	if (Diagnostics.RootSpeed > Settings.MaxRootLinearSpeedCmPerSecond || Diagnostics.RootAngularSpeed > Settings.MaxRootAngularSpeedDegPerSecond)
	{
		OutReason = TEXT("fail_stop_precursor");
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
	if (Diagnostics.RootTilt > Owner->BalanceQuietTiltThresholdDeg)
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
	// UsefrozenTopology = true ensures we use the accepted Phase 1 snapshot for classification.
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult, true))
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
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
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

bool FPhysAnimBalanceReadyTransition::BuildCertifiedHandoffSnapshot(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FPhysAnimCertifiedHandoffSnapshot& OutSnapshot, FPhysAnimLateValidationResult& OutResult, bool bUseFrozenTopology) const
{
	if (!Owner)
	{
		return false;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh ? Mesh->GetBodyInstance(RootBoneName) : nullptr;
	if (!PelvisBody)
	{
		return false;
	}

	TArray<FName> SimulatingBones;
	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	int32 UpperSimCount = 0;
	bool bRootSimulating = false;

	Owner->GetSimulatingBodies(SimulatingBones);
	TSet<FName> SimulatingBoneSet(SimulatingBones);
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
		else if (BalanceTransitionSets::IsUpperBody(BoneName))
		{
			UpperSimCount++;
		}
	}

	bRootSimulating = PelvisBody->IsInstanceSimulatingPhysics();

	// If we have a frozen Phase 1 record, we prefer its topology for handoff classification 
	// (modes and counts) even if the live state has changed (e.g. bones starting to simulate).
	if (bHasPhase1TopologyRecord && bUseFrozenTopology)
	{
		ProximalSimCount = Phase1TopologyRecord.ProximalSimCount;
		DistalSimCount = Phase1TopologyRecord.DistalSimCount;
		UpperSimCount = Phase1TopologyRecord.UpperBodySimCount;
		bRootSimulating = Phase1TopologyRecord.bRootSimulating;

		OutSnapshot.RootOwnershipMode = Phase1TopologyRecord.RootOwnershipMode;
		OutSnapshot.ProximalOwnershipMode = Phase1TopologyRecord.ProximalOwnershipMode;
		OutSnapshot.DistalOwnershipMode = Phase1TopologyRecord.DistalOwnershipMode;
		OutSnapshot.UpperBodyOwnershipMode = Phase1TopologyRecord.UpperBodyOwnershipMode;
		OutSnapshot.bPolicySuppressed = Phase1TopologyRecord.bPolicySuppressed;
		OutSnapshot.bResetsSuppressed = Phase1TopologyRecord.bResetsSuppressed;
	}
	else
	{
		OutSnapshot.RootOwnershipMode = bRootSimulating ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
		OutSnapshot.ProximalOwnershipMode = ProximalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
		OutSnapshot.DistalOwnershipMode = DistalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
		// UpperBodyOwnershipMode is handled below as it depends on classification.
	}

	const FPhysAnimControlTargetDiagnostics& ControlTargetDiagnostics = Owner->GetLastControlTargetDiagnostics();
	OutSnapshot.PolicyInfluenceAlphaAtCapture = Owner->CalculateCurrentPolicyInfluenceAlpha(Settings);
	OutSnapshot.ShellAuthorityMode = BalanceTransitionSets::GetShellAuthorityModeName(Owner->GetBalanceTransitionShellAuthorityMode());
	OutSnapshot.TopologyClass = BalanceTransitionSets::BuildCertifiedHandoffTopologyClass(
		OutSnapshot.RootOwnershipMode == EBalanceReadyGroupOwnershipMode::Simulating,
		OutSnapshot.ProximalOwnershipMode == EBalanceReadyGroupOwnershipMode::Simulating ? ProximalSimCount : 0,
		OutSnapshot.DistalOwnershipMode == EBalanceReadyGroupOwnershipMode::Simulating ? DistalSimCount : 0,
		UpperSimCount);
	OutSnapshot.SimCount = (bHasPhase1TopologyRecord && bUseFrozenTopology) ? Phase1TopologyRecord.TotalSimCount : SimulatingBones.Num();
	OutSnapshot.ProximalSimCount = ProximalSimCount;
	OutSnapshot.DistalSimCount = DistalSimCount;
	OutSnapshot.UpperBodySimCount = UpperSimCount;
	const EBalanceReadyRootOnReadinessClassification RootOnReadinessClassification = bHasPhase1TopologyRecord && bUseFrozenTopology
		? (BalanceTransitionSets::IsRootCoupledReadyHandoff(Phase1TopologyRecord.ProximalSimCount, Phase1TopologyRecord.DistalSimCount, Phase1TopologyRecord.UpperBodySimCount, Phase1TopologyRecord.bRootSimulating)
			? EBalanceReadyRootOnReadinessClassification::RootCoupledReady
			: (BalanceTransitionSets::IsUpperOnlySafeDenyHandoff(Phase1TopologyRecord.ProximalSimCount, Phase1TopologyRecord.DistalSimCount, Phase1TopologyRecord.UpperBodySimCount, Phase1TopologyRecord.bRootSimulating)
				? EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny
				: EBalanceReadyRootOnReadinessClassification::NotReady))
		: (BalanceTransitionSets::IsRootCoupledReadyHandoff(
			ProximalSimCount,
			DistalSimCount,
			UpperSimCount,
			bRootSimulating)
			? EBalanceReadyRootOnReadinessClassification::RootCoupledReady
			: (BalanceTransitionSets::IsUpperOnlySafeDenyHandoff(
					ProximalSimCount,
					DistalSimCount,
					UpperSimCount,
					bRootSimulating)
				? EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny
				: EBalanceReadyRootOnReadinessClassification::NotReady));

	if (!(bHasPhase1TopologyRecord && bUseFrozenTopology))
	{
		OutSnapshot.UpperBodyOwnershipMode = (RootOnReadinessClassification != EBalanceReadyRootOnReadinessClassification::NotReady)
			? EBalanceReadyUpperBodyOwnershipMode::None
			: EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold;
		OutSnapshot.bPolicySuppressed = ShouldSuppressPolicy();
		OutSnapshot.bResetsSuppressed = ShouldSuppressResets();
	}
	OutSnapshot.bControlAuthoritySettled = Owner->CalculateCurrentControlAuthorityAlpha(Settings) >= 1.0f - KINDA_SMALL_NUMBER;
	const int32 FinalBringUpGroupIndex = Owner->GetBringUpGroupCount() - 1;
	OutSnapshot.FinalBringUpGroupControlAuthorityAlpha = Owner->CalculateBringUpGroupControlAuthorityAlpha(FinalBringUpGroupIndex, Settings);
	OutSnapshot.RootOnReadinessPolicyInfluenceRequiredAlpha = Owner->BalanceReadyPolicyInfluenceThreshold;
	OutSnapshot.RootOnReadinessPolicyInfluenceDurationSeconds =
		OutSnapshot.PolicyInfluenceAlphaAtCapture * FMath::Max(Settings.StartupRampSeconds, 0.0f);
	OutSnapshot.RootOnReadinessPolicyInfluenceRequiredSeconds =
		FMath::Max(Settings.StartupRampSeconds, UE_SMALL_NUMBER) * Owner->BalanceReadyPolicyInfluenceThreshold;
	OutSnapshot.RootOnReadinessShellHoldDurationSeconds = RootOnReadinessShellHoldAccumulatedSeconds;
	OutSnapshot.RootOnReadinessShellHoldRequiredSeconds = Settings.BalancePhase2RequiredShellHoldDuration;
	OutSnapshot.RootOnReadinessShellProofDurationSeconds = RootOnReadinessShellProofAccumulatedSeconds;
	OutSnapshot.ShellOffsetDeltaAtCaptureCm = CachedConvergenceSnapshot.IsValid() ? CachedConvergenceSnapshot.ShellPlanarOffset : Owner->GetCurrentShellPlanarOffsetDeltaCm();
	OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond = CachedConvergenceSnapshot.IsValid() ? CachedConvergenceSnapshot.ShellPlanarVelocity : Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
	OutSnapshot.ShellOffsetGrowthCm = bHasRootOnReadinessShellProofBaseline
		? FMath::Max(0.0f, OutSnapshot.ShellOffsetDeltaAtCaptureCm - RootOnReadinessShellProofStartOffsetCm)
		: 0.0f;
	OutSnapshot.ShellVelocityGrowthCmPerSecond = bHasRootOnReadinessShellProofBaseline
		? FMath::Max(0.0f, OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond - RootOnReadinessShellProofStartVelocityCmPerSecond)
		: 0.0f;

	bool bShellCorrectionOwnerActive = Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle;
	if (const ACharacter* const CharacterOwner = Cast<ACharacter>(Owner->GetOwner()))
	{
		if (const UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			bShellCorrectionOwnerActive |= CharacterMovement->IsComponentTickEnabled() ||
				CharacterMovement->MovementMode != MOVE_None;
		}
		if (const UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			bShellCorrectionOwnerActive |= CapsuleComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		}
	}
	OutSnapshot.bShellCorrectionOwnerActive = bShellCorrectionOwnerActive;
	OutSnapshot.bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame =
		Owner->WasPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame();
	OutSnapshot.bTransitionOwnedShellLocked = Owner->IsTransitionOwnedShellLocked();
	OutSnapshot.bTransitionShellReferenceReanchored = Owner->WasTransitionShellReferenceReanchored();
	OutSnapshot.bTransitionShellReferenceReseededAfterLock = Owner->WasTransitionShellReferenceReseededAfterLock();

	// Populate Logical Result
	OutResult.bLateValidationCompleted = bHasLateValidationProof;
	OutResult.bRootOnReadinessShellHoldSatisfied =
		OutSnapshot.RootOnReadinessShellHoldDurationSeconds + KINDA_SMALL_NUMBER >= Settings.BalancePhase2RequiredShellHoldDuration;
	OutResult.bRootOnReadinessUpperOnlyShellHoldCappedByWindow =
		OutSnapshot.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold &&
		OutResult.bLateValidationCompleted &&
		!OutResult.bRootOnReadinessShellHoldSatisfied;
	OutResult.bRootOnReadinessFinalBringUpControlSettled =
		OutSnapshot.FinalBringUpGroupControlAuthorityAlpha >= 1.0f - KINDA_SMALL_NUMBER;
	OutResult.bRootOnReadinessPolicyInfluenceSettled =
		OutSnapshot.RootOnReadinessPolicyInfluenceDurationSeconds + KINDA_SMALL_NUMBER >=
		OutSnapshot.RootOnReadinessPolicyInfluenceRequiredSeconds;
	OutResult.bPreRootOnShellSafetyProofSatisfied =
		OutSnapshot.RootOnReadinessShellProofDurationSeconds + KINDA_SMALL_NUMBER >=
			Settings.BalancePhase2PreRootOnShellProofRequiredSeconds &&
		OutSnapshot.ShellOffsetDeltaAtCaptureCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm &&
		OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond &&
		OutSnapshot.ShellOffsetGrowthCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm &&
		OutSnapshot.ShellVelocityGrowthCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond &&
		!OutSnapshot.bShellCorrectionOwnerActive &&
		OutSnapshot.bTransitionOwnedShellLocked &&
		OutSnapshot.bTransitionShellReferenceReanchored &&
		!OutSnapshot.bTransitionShellReferenceReseededAfterLock;

	OutResult.bRootOnReadinessProven =
		OutResult.bRootOnReadinessShellHoldSatisfied &&
		OutResult.bRootOnReadinessFinalBringUpControlSettled &&
		OutResult.bRootOnReadinessPolicyInfluenceSettled &&
		OutResult.bPreRootOnShellSafetyProofSatisfied &&
		(RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady);

	OutResult.RootOnReadinessClassification = RootOnReadinessClassification;

	OutResult.Outcome = 
		(OutResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady)
			? EBalanceLateValidationOutcome::Outcome_AcceptRootOn
			: (OutResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny
				? EBalanceLateValidationOutcome::Outcome_SafeDenyUpperOnly
				: EBalanceLateValidationOutcome::Outcome_Pending);

	OutResult.MaxTargetDeltaDegrees = ControlTargetDiagnostics.MaxTargetDeltaDegrees;
	OutResult.MeanTargetDeltaDegrees = ControlTargetDiagnostics.MeanTargetDeltaDegrees;
	OutResult.QuietProofDurationSeconds = QuietWindowAccumulatedSeconds;
	OutResult.LateValidationSustainDurationSeconds = LateValidationAccumulatedSeconds;

	return true;
}

bool FPhysAnimBalanceReadyTransition::CaptureCertifiedHandoff(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (!ValidateLateValidationHandoffSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	CertifiedHandoff = CurrentSnapshot;
	CertifiedLateValidationResult = CurrentResult;
	bHasCertifiedHandoff = true;
	OutReason.Reset();
	return true;
}

bool FPhysAnimBalanceReadyTransition::CaptureLateValidationBaseline(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (!ValidateLateValidationBaselineSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	CertifiedHandoff = CurrentSnapshot;
	CertifiedLateValidationResult = CurrentResult;
	bHasCertifiedHandoff = true;
	OutReason.Reset();
	return true;
}

bool FPhysAnimBalanceReadyTransition::ValidateCertifiedHandoff(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason) const
{
	if (!bHasCertifiedHandoff)
	{
		OutReason = TEXT("phase2_missing_handoff_payload");
		return false;
	}

	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (CurrentSnapshot.TopologyClass != CertifiedHandoff.TopologyClass)
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (!ValidateLateValidationHandoffSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	if (!ValidateRootOnReadinessSnapshot(CurrentSnapshot, CurrentResult, Settings, OutReason))
	{
		return false;
	}

	return true;
}


void FPhysAnimBalanceReadyTransition::MarkSafePhase2Denied(class UPhysAnimComponent* Owner, const FString& Reason)
{
	SafePhase2DenialReason = Reason;
	Diagnostics.FailureReason = Reason;
	SetPhase(EBalanceReadyTransitionPhase::BRT_SafeDenied, Owner);
}

void FPhysAnimBalanceReadyTransition::ResetTransitionLocalState()
{
	Diagnostics = {};
	LateValidationAccumulatedSeconds = 0.0f;
	RootOnReadinessShellHoldAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofStartOffsetCm = 0.0f;
	RootOnReadinessShellProofStartVelocityCmPerSecond = 0.0f;
	bHasRootOnReadinessShellProofBaseline = false;
	LastLateValidateBlockReason.Reset();
	bHasLateValidationProof = false;
	Diagnostics.Phase1LateValidateBodyMotionViolationAccumulatedSeconds = 0.0f;
	Diagnostics.Phase1LateValidateWorstLinearSpeedBone = NAME_None;
	Diagnostics.Phase1LateValidateWorstAngularSpeedBone = NAME_None;
	Diagnostics.Phase1LateValidateWorstLinearSpeed = 0.0f;
	Diagnostics.Phase1LateValidateWorstAngularSpeed = 0.0f;
	bLateValidationProofPassed = false;
	ResetCertifiedHandoffState();
	Phase1TopologyRecord = {};
	bHasPhase1TopologyRecord = false;
}

void FPhysAnimBalanceReadyTransition::ResetCertifiedHandoffState()
{
	bHasCertifiedHandoff = false;
	CertifiedHandoff = {};
	bHasLateValidationProof = false;
	RootOnReadinessShellHoldAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofStartOffsetCm = 0.0f;
	RootOnReadinessShellProofStartVelocityCmPerSecond = 0.0f;
	bHasRootOnReadinessShellProofBaseline = false;
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

	TArray<FName> SimulatingBones;
	Owner->GetSimulatingBodies(SimulatingBones);
	TSet<FName> SimulatingBoneSet(SimulatingBones);
	int32 ProximalSimCount = 0;
	int32 DistalSimCount = 0;
	int32 UpperSimCount = 0;
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
		else if (BalanceTransitionSets::IsUpperBody(BoneName))
		{
			UpperSimCount++;
		}
	}

	Phase1TopologyRecord.bRootSimulating = PelvisBody->IsInstanceSimulatingPhysics();
	Phase1TopologyRecord.ProximalSimCount = ProximalSimCount;
	Phase1TopologyRecord.DistalSimCount = DistalSimCount;
	Phase1TopologyRecord.UpperBodySimCount = UpperSimCount;
	Phase1TopologyRecord.TotalSimCount = SimulatingBones.Num();

	Phase1TopologyRecord.RootOwnershipMode = Phase1TopologyRecord.bRootSimulating ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
	Phase1TopologyRecord.ProximalOwnershipMode = ProximalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;
	Phase1TopologyRecord.DistalOwnershipMode = DistalSimCount > 0 ? EBalanceReadyGroupOwnershipMode::Simulating : EBalanceReadyGroupOwnershipMode::Kinematic;

	const EBalanceReadyRootOnReadinessClassification RootOnReadinessClassification =
		BalanceTransitionSets::IsRootCoupledReadyHandoff(
			ProximalSimCount,
			DistalSimCount,
			UpperSimCount,
			Phase1TopologyRecord.bRootSimulating)
		? EBalanceReadyRootOnReadinessClassification::RootCoupledReady
		: (BalanceTransitionSets::IsUpperOnlySafeDenyHandoff(
			ProximalSimCount,
			DistalSimCount,
			UpperSimCount,
			Phase1TopologyRecord.bRootSimulating)
			? EBalanceReadyRootOnReadinessClassification::UpperOnlySafeDeny
			: EBalanceReadyRootOnReadinessClassification::NotReady);

	Phase1TopologyRecord.UpperBodyOwnershipMode = (RootOnReadinessClassification != EBalanceReadyRootOnReadinessClassification::NotReady)
		? EBalanceReadyUpperBodyOwnershipMode::None
		: EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold;

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

	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_TOPOLOGY_SNAPSHOT topology=root=%s proximal=%s distal=%s upper=%s upperBodyOwnership=%s simCount=%d proximalSimCount=%d distalSimCount=%d upperBodySimCount=%d policySuppressed=%d resetsSuppressed=%d"),
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

void FPhysAnimBalanceReadyTransition::CaptureFlipDiagnostics(UPhysAnimComponent* Owner)
{
	USkeletalMeshComponent* Mesh = Owner ? Owner->GetMeshComponent() : nullptr;
	if (!Mesh || !Owner)
	{
		return;
	}

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

	GetMaxVel({ RootBoneName }, Diagnostics.MaxLinVelPelvis, Diagnostics.MaxAngVelPelvis);
	GetMaxVel({ TEXT("thigh_l"), TEXT("thigh_r") }, Diagnostics.MaxLinVelThighs, Diagnostics.MaxAngVelThighs);
	GetMaxVel({ TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") }, Diagnostics.MaxLinVelSpine, Diagnostics.MaxAngVelSpine);
	GetMaxVel({ TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r"), TEXT("calf_l"), TEXT("calf_r") }, Diagnostics.MaxLinVelFeet, Diagnostics.MaxAngVelFeet);

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Mesh->GetPhysicsLinearVelocity(BoneName).Size());
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Mesh->GetPhysicsAngularVelocityInDegrees(BoneName).Size());
	}
}

bool FPhysAnimBalanceReadyTransition::ShouldSuppressPolicy() const
{
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		return true;
	}

	if (bHasPhase1TopologyRecord && (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate))
	{
		return Phase1TopologyRecord.bPolicySuppressed;
	}

	return IsActive() &&
		(InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		 InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		 InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn);
}

bool FPhysAnimBalanceReadyTransition::ShouldSuppressPolicyWrites(FName BoneName) const
{
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		return true;
	}

	if (!IsActive())
	{
		return false;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		return true;
	}

	return false;
}

bool FPhysAnimBalanceReadyTransition::ShouldSuppressShell() const
{
	return InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
}
bool FPhysAnimBalanceReadyTransition::ShouldSuppressPerturbations() const { return IsActive() || HasSafeDenied(); }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressResets() const
{
	if (bHasPhase1TopologyRecord && (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate))
	{
		return Phase1TopologyRecord.bResetsSuppressed;
	}

	return InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
}
bool FPhysAnimBalanceReadyTransition::ShouldSuppressMoveSmoke() const
{
	return InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
}

float FPhysAnimBalanceReadyTransition::GetRootBodyModifierSoftSimAlpha() const
{
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		return 1.0f;
	}

	return FMath::Clamp(
		PhaseTimeSeconds / BalanceTransitionSets::Phase2AuthorityRampSeconds,
		0.0f,
		1.0f);
}

float FPhysAnimBalanceReadyTransition::GetProximalControlSoftAlpha(FName BoneName) const
{
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase2_RootOn || !BalanceTransitionSets::IsProximal(BoneName))
	{
		return 1.0f;
	}

	return FMath::Clamp(
		PhaseTimeSeconds / BalanceTransitionSets::Phase2AuthorityRampSeconds,
		0.0f,
		1.0f);
}

bool FPhysAnimBalanceReadyTransition::ShouldKeepBoneKinematic(FName BoneName) const
{
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		return BalanceTransitionSets::IsTransitionCritical(BoneName) ||
			BalanceTransitionSets::IsLateValidationUpperBodyOwnershipBone(BoneName);
	}

	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_Failed)
	{
		return false;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		return BalanceTransitionSets::IsPrepareCriticalKinematic(BoneName) ||
			BalanceTransitionSets::IsUpperBody(BoneName);
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate)
	{
		// Late validation keeps the root and lower body kinematic while preserving the full upper-body
		// ownership mode expected by the certified handoff snapshot.
		if (BalanceTransitionSets::IsRoot(BoneName) ||
			BalanceTransitionSets::IsDistalLowerLimb(BoneName))
		{
			return true;
		}

		if (CertifiedHandoff.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold)
		{
			if (!bLateValidationProofPassed)
			{
				return BalanceTransitionSets::IsUpperBody(BoneName);
			}
		}

		return false;
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn || 
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		return BalanceTransitionSets::IsDistalLowerLimb(BoneName);
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
	{
		return BalanceTransitionSets::IsTransitionCritical(BoneName) || BalanceTransitionSets::IsUpperBody(BoneName);
	}

	return false;
}

float FPhysAnimBalanceReadyTransition::GetTransitionExtraDampingMultiplier(const FPhysAnimStabilizationSettings& Settings) const
{
	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		return 1.0f;
	}

	const bool bInBootstrap = InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
	return bInBootstrap ? Settings.BalanceBootstrapExtraDampingMultiplier : Settings.BalanceActiveExtraDampingMultiplier;
}

bool FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(const FString& FailureReason)
{
	return FailureReason == TEXT("phase2_root_not_confirmed") ||
		FailureReason == TEXT("phase2_topology_not_preserved") ||
		FailureReason == TEXT("phase2_guard_window_interrupted_by_transient_contamination");
}

EBalanceReadyConditionOwner FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(const FString& Reason)
{
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
		Reason == TEXT("phase2_root_simulation_dropped") ||
		Reason == TEXT("phase2_root_on_spike"))
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
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] %s reason=%s owner=%d"), EventName, *Reason, static_cast<int32>(FailureOwner));
	if (Owner)
	{
		Owner->ReleaseTransitionOwnedShellLock();
	}
	ResetTransitionLocalState();
	InternalPhase = EBalanceReadyTransitionPhase::BRT_Phase1_Prepare;
	PhaseTimeSeconds = 0.0f;
	StableHoldAccumulatedSeconds = 0.0f;
	TargetDiscontinuityAccumulatedSeconds = 0.0f;
	QuietWindowAccumulatedSeconds = 0.0f;
	LateValidationAccumulatedSeconds = 0.0f;
	LastQuietBlockReason.Reset();
	Diagnostics.FailureReason.Reset();
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

	const FVector PelvisLinearVelocity = PelvisBody->GetUnrealWorldVelocity();
	const FVector PelvisAngularVelocityDegPerSec = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());

	// Section 17.4 - Root simulation spike (instability)
	if (PelvisLinearVelocity.Size() > Settings.MaxRootLinearSpeedCmPerSecond * 2.5f ||
		PelvisAngularVelocityDegPerSec.Size() > Settings.MaxRootAngularSpeedDegPerSecond * 2.5f)
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
		OutReason = TEXT("phase2_root_on_readiness_topology_not_certified");
		return false;
	}

	if (!Result.bRootOnReadinessProven)
	{
		OutReason = TEXT("phase2_root_on_readiness_not_proven");
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

	if (!Snapshot.bTransitionOwnedShellLocked ||
		!Snapshot.bTransitionShellReferenceReanchored ||
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

	if (Snapshot.bShellCorrectionOwnerActive)
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	if (Snapshot.RootOnReadinessShellProofDurationSeconds + KINDA_SMALL_NUMBER < Settings.BalancePhase2PreRootOnShellProofRequiredSeconds)
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	return true;
}
