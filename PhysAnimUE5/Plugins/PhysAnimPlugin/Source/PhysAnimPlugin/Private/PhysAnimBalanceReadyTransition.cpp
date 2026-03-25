#include "PhysAnimBalanceReadyTransition.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlRecord.h"

struct FPhysAnimPhysicsControlAccessor : public UPhysicsControlComponent
{
public:
	static const FPhysicsBodyModifierRecord* GetModifierRecord(const UPhysicsControlComponent* ControlComp, const FName Name)
	{
		return ((const FPhysAnimPhysicsControlAccessor*)ControlComp)->FindBodyModifierRecord(Name);
	}
};

#include "PhysAnimComponent.h"
#include "PhysicsControlComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicsEngine/BodyInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

extern int32 GStrictPhase1Certification;
extern int32 GVerbosePhase1Forensics;
extern int32 GVerbosePhase2Forensics;


namespace BalanceTransitionSets
{
	static constexpr float Phase2TopologySettleGraceSeconds = 1.0f / 30.0f;
	static constexpr float Phase2AuthorityRampSeconds = 0.10f;
	static constexpr float Phase2MaxPelvisProximalConstraintErrorCm = 15.0f;

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
	Owner->SetStartupBringUpFrozenByBalanceEntry(true, TEXT("transition_accept"));

	RequestReason = InRequestReason;
	StableHoldAccumulatedSeconds = 0.0f;
	QuietWindowAccumulatedSeconds = 0.0f;
	ConsecutivePelvisNotSimulatingTicks = 0;
	ConsecutiveBodyMotionInstabilityTicks = 0;
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
	bLoggedLateValidateEntry = false;
	bLoggedPhase1UpperBodyAudit = false;
	bLoggedPhase2EntryAudit = false;
	bLoggedPhase2FirstFailureAudit = false;
	EntryHoldRotations.Empty();
	DistalBoneMismatchTicks.Empty();
	DistalBoneConsecutiveMismatchTicks.Empty();
	DistalBonePersistentTicks.Empty();
	DistalMismatchesTransientCount = 0;
	DistalMismatchesPersistentCount = 0;
	DistalMismatchesPendingCount = 0;
	Owner->LastDistalClassification.Empty();
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

	SetPhase(EBalanceReadyTransitionPhase::BRT_Phase1_Prepare, Owner);
	CapturePhase1TopologyRecord(Owner, Owner->ResolveEffectiveStabilizationSettings());
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

	// Authoritative proximal promotion during Phase 1 to solve one-frame lag/capture issues.
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate)
	{
		USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
		if (Mesh)
		{
			static const FName ProximalBones[] = { TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
			for (const FName BoneName : ProximalBones)
			{
				if (!ShouldKeepBoneKinematic(BoneName, Settings))
				{
					FBodyInstance* const BodyInst = Mesh->GetBodyInstance(BoneName);
					const bool bRawSimulating = BodyInst && BodyInst->IsValidBodyInstance() ? BodyInst->IsInstanceSimulatingPhysics() : false;

					if (!bRawSimulating)
					{
						if (BodyInst)
						{
							BodyInst->SetInstanceSimulatePhysics(true, true);
							if (!LoggedProximalPromotions.Contains(BoneName))
							{
								UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PROXIMAL_RAW_SIM_PROMOTION bone=%s previousRaw=Kin newIntended=Sim"), *BoneName.ToString());
								LoggedProximalPromotions.Add(BoneName);
							}
						}
					}
					else if (LoggedProximalPromotions.Contains(BoneName) && !LoggedProximalStates.Contains(BoneName))
					{
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PROXIMAL_RAW_SIM_STATE bone=%s raw=Sim"), *BoneName.ToString());
						LoggedProximalStates.Add(BoneName);
					}
				}
			}
		}
	}

	PhaseTimeSeconds += DeltaTime;
	TotalTransitionTimeSeconds += DeltaTime;

	FString BlockReason;
	const bool bReadyThisFrame = EvaluateReadiness(Owner, Settings, BlockReason);
	Diagnostics.BlockReason = BlockReason;

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		if (!bHasLoggedDistalExperimentState && Settings.bPhase1DistalKinematicExperiment)
		{
			bHasLoggedDistalExperimentState = true;
			USkeletalMeshComponent* const Mesh = Owner ? Owner->GetMeshComponent() : nullptr;
			if (Mesh)
			{
				const FName DistalBones[] = { TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r") };
				for (const FName BoneName : DistalBones)
				{
					const bool bIntendedKinematic = ShouldKeepBoneKinematic(BoneName, Settings);
					const FBodyInstance* BodyInst = Mesh->GetBodyInstance(BoneName);
					const bool bActualSimulating = BodyInst && BodyInst->IsValidBodyInstance() ? BodyInst->IsInstanceSimulatingPhysics() : false;
					const bool bStateMismatch = bIntendedKinematic == bActualSimulating;
	
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_EXPERIMENT_STATE bone=%s intended=%s actual=%s changedByLaterSubsystem=%d"),
						*BoneName.ToString(),
						UPhysAnimComponent::GetPhysicsMovementTypeName(bIntendedKinematic ? EPhysicsMovementType::Kinematic : EPhysicsMovementType::Simulated),
						bActualSimulating ? TEXT("Simulating") : TEXT("Kinematic"),
						bStateMismatch);
				}
			}
		}

		const bool bIsBodyMotionUnstable = CachedConvergenceSnapshot.MaxBodyLinearSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed ||
			CachedConvergenceSnapshot.MaxBodyAngularSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed;
		const bool bIsPelvisNotSimulating = !CachedConvergenceSnapshot.bIsPelvisSimulating;

		if (bIsBodyMotionUnstable) { ConsecutiveBodyMotionInstabilityTicks++; }
		else { ConsecutiveBodyMotionInstabilityTicks = 0; }

		const bool bEscalateBodyInstability = ConsecutiveBodyMotionInstabilityTicks >= Settings.BalancePhase1PrepareMaxBlockedTicks;

		if (bEscalateBodyInstability)
		{
			const FString TerminalReason = TEXT("persistent_body_motion_instability");

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PREPARE_TERMINAL reason=%s"), *TerminalReason);

			const FString SafeDenyReason = TEXT("phase1_prepare_terminal_") + TerminalReason;
			Owner->ReleaseTransitionOwnedShellLock();
			MarkSafePhase2Denied(Owner, SafeDenyReason);
			return;
		}

		if (bIsBodyMotionUnstable)
		{
			if (ConsecutiveBodyMotionInstabilityTicks == 1)
			{
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PREPARE_BLOCKED reason=body_motion_instability maxSimBodyLinearSpeed=%.2f maxSimBodyAngularSpeed=%.2f worstLinearBone=%s worstAngularBone=%s"),
					CachedConvergenceSnapshot.MaxBodyLinearSpeed,
					CachedConvergenceSnapshot.MaxBodyAngularSpeed,
					*CachedConvergenceSnapshot.MaxBodyLinearSpeedBone.ToString(),
					*CachedConvergenceSnapshot.MaxBodyAngularSpeedBone.ToString());
			}

			QuietWindowAccumulatedSeconds = 0.0f;
			return;
		}

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
		else if (CachedConvergenceSnapshot.MaxBodyLinearSpeed > Settings.BalancePhase1LateValidateAdmissionMaxSimulatedBoneLinearSpeed ||
			CachedConvergenceSnapshot.MaxBodyAngularSpeed > Settings.BalancePhase1LateValidateAdmissionMaxSimulatedBoneAngularSpeed)
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
				// Pre-LateValidate stability-margin gate
				const bool bHasInsufficientStabilityMargin =
					CachedConvergenceSnapshot.MaxBodyLinearSpeed > Settings.BalancePhase1LateValidateAdmissionMaxSimulatedBoneLinearSpeed ||
					CachedConvergenceSnapshot.MaxBodyAngularSpeed > Settings.BalancePhase1LateValidateAdmissionMaxSimulatedBoneAngularSpeed;

				if (bHasInsufficientStabilityMargin)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_BLOCKED reason=insufficient_stability_margin maxSimBodyLinearSpeed=%.2f maxSimBodyAngularSpeed=%.2f worstLinearBone=%s worstAngularBone=%s"),
						CachedConvergenceSnapshot.MaxBodyLinearSpeed,
						CachedConvergenceSnapshot.MaxBodyAngularSpeed,
						*CachedConvergenceSnapshot.MaxBodyLinearSpeedBone.ToString(),
						*CachedConvergenceSnapshot.MaxBodyAngularSpeedBone.ToString());

					QuietWindowAccumulatedSeconds = 0.0f;
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
					Owner->ConsumeUpperBodyPendingResets();
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

		if (TotalTransitionTimeSeconds > (GStrictPhase1Certification != 0 ? 2.0f : Settings.BalancePhase1TimeoutDuration))
		{
			if (TotalTransitionTimeSeconds > 2.0f + KINDA_SMALL_NUMBER)
			{
				Audit.bUsedTimeoutExtension = true;
			}
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
		if (!bLoggedLateValidateEntry)
		{
			if (GVerbosePhase1Forensics != 0)
			{
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] ENTER_LATE_VALIDATE elapsed=%.4f"), PhaseTimeSeconds);
			}
			bLoggedLateValidateEntry = true;
		}
		const bool bIsBodyMotionUnstable = CachedConvergenceSnapshot.MaxBodyLinearSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed ||
			CachedConvergenceSnapshot.MaxBodyAngularSpeed > Settings.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed;

		if (bIsBodyMotionUnstable)
		{
			const FString FailureReason = TEXT("phase1_no_convergence_path_body_motion_instability");
			Diagnostics.FailureReason = FailureReason;
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s maxSimBodyLinearSpeed=%.2f maxSimBodyAngularSpeed=%.2f worstLinearBone=%s worstAngularBone=%s"), 
				*FailureReason,
				CachedConvergenceSnapshot.MaxBodyLinearSpeed,
				CachedConvergenceSnapshot.MaxBodyAngularSpeed,
				*CachedConvergenceSnapshot.MaxBodyLinearSpeedBone.ToString(),
				*CachedConvergenceSnapshot.MaxBodyAngularSpeedBone.ToString());

			// Instrumentation: Distal chain failure forensics
			USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
			if (Mesh)
			{
				const FName TargetBones[] = { TEXT("calf_r"), TEXT("foot_r"), TEXT("ball_r") };
				const FPhysAnimControlTargetDiagnostics& CtrlDiag = Owner->GetLastControlTargetDiagnostics();
				
				for (const FName BoneName : TargetBones)
				{
					const FVector LinVel = Mesh->GetPhysicsLinearVelocity(BoneName);
					const FVector AngVel = Mesh->GetPhysicsAngularVelocityInDegrees(BoneName);
					const FVector Loc = Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);

					if (GVerbosePhase1Forensics != 0)
					{
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_FAILURE_FORENSICS bone=%s lin=%.1f ang=%.1f locZ=%.1f"),
							*BoneName.ToString(),
							LinVel.Size(),
							AngVel.Size(),
							Loc.Z);
					}
				}
				if (GVerbosePhase1Forensics != 0)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_FAILURE_SUMMARY maxTargetDelta=%.1f(%s) maxLimitOccupancy=%.2f(%s)"),
						CtrlDiag.MaxTargetDeltaDegrees,
						*CtrlDiag.MaxTargetDeltaBoneName.ToString(),
						CtrlDiag.MaxLowerLimbLimitOccupancy,
						*CtrlDiag.MaxLowerLimbLimitOccupancyBoneName.ToString());
				}
			}

				Owner->ReleaseTransitionOwnedShellLock();
			MarkSafePhase2Denied(Owner, FailureReason);
			return;
		}

		const bool bCurrentTargetDiscontinuity =
			CachedConvergenceSnapshot.MaxTargetDeltaDegrees > Settings.BalancePhase1MaxEntryTargetDeltaDeg ||
			CachedConvergenceSnapshot.MeanTargetDeltaDegrees > Settings.BalancePhase1MaxEntryTargetDeltaDeg;

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

		bool bUpperBodyInstability = false;
		if (bCurrentSnapshotValid && bLiveSnapshotValid && !bExpectedUpperBodyRelease)
		{
			if (Phase1TopologyRecord.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold)
			{
				USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
				if (Mesh)
				{
						float MaxAuditTargetDelta = 0.0f;
						FName MaxAuditTargetDeltaBone = NAME_None;

						if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate)
						{
							const bool bFirstLateValidateFrame = !bLoggedPhase1UpperBodyAudit;

							const FName AuditBones[] = { 
								TEXT("neck_01"), TEXT("head"), 
								TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
								TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r") 
							};

							for (const FName AuditBone : AuditBones)
							{
								const FBodyInstance* AuditBody = Mesh->GetBodyInstance(AuditBone);
								if (!AuditBody || !AuditBody->IsValidBodyInstance()) { continue; }

								float TargetDeltaDeg = 0.0f;
								const FQuat* HoldRot = EntryHoldRotations.Find(AuditBone);
								if (HoldRot)
								{
									TargetDeltaDeg = FMath::RadiansToDegrees(HoldRot->AngularDistance(Mesh->GetBoneQuaternion(AuditBone, EBoneSpaces::WorldSpace)));
								}

								if (TargetDeltaDeg > MaxAuditTargetDelta)
								{
									MaxAuditTargetDelta = TargetDeltaDeg;
									MaxAuditTargetDeltaBone = AuditBone;
								}

								if (bFirstLateValidateFrame)
								{
									const FPhysicsBodyModifierRecord* ModifierRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(Owner->PhysicsControlComponent.Get(), PhysAnimBridge::MakeBodyModifierName(AuditBone));
									const EPhysicsMovementType ModifierType = ModifierRecord ? ModifierRecord->BodyModifier.ModifierData.MovementType : EPhysicsMovementType::Simulated;
									const bool bHeldTargetWritten = HoldRot != nullptr;
									const float LinVel = AuditBody->GetUnrealWorldVelocity().Size();
									const float AngVel = FMath::RadiansToDegrees(AuditBody->GetUnrealWorldAngularVelocityInRadians().Size());
									const bool bPendingReset = Owner->GetPendingBodyModifierCachedResetNames().Contains(PhysAnimBridge::MakeBodyModifierName(AuditBone));
									const bool bInAllowlist = BalanceTransitionSets::IsUpperBody(AuditBone);

									UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_PRE_UPPER_BODY_GATE_AUDIT bone=%s rawSim=%d modifier=%s heldWritten=%d targetDelta=%.2f linVel=%.2f angVel=%.2f pendingReset=%d holdActive=%d allowlist=%d"),
										*AuditBone.ToString(), AuditBody->IsInstanceSimulatingPhysics() ? 1 : 0, 
										UPhysAnimComponent::GetPhysicsMovementTypeName(ModifierType),
										bHeldTargetWritten ? 1 : 0, TargetDeltaDeg, LinVel, AngVel, bPendingReset ? 1 : 0,
										Phase1TopologyRecord.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold ? 1 : 0,
										bInAllowlist ? 1 : 0);
								}
							}

							if (bFirstLateValidateFrame)
							{
								UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_SUMMARY upperBodyOwnership=%s upperBodySimCount=%d policySuppressed=%d policyAlpha=%.2f quietProofDuration=%.2f requiredSeconds=%.2f pendingResets=%d maxTargetDeltaBone=%s maxTargetDelta=%.2f consumedResets=%d"),
									BalanceTransitionSets::GetUpperBodyOwnershipModeName(Phase1TopologyRecord.UpperBodyOwnershipMode),
									Phase1TopologyRecord.UpperBodySimCount,
									Phase1TopologyRecord.bPolicySuppressed ? 1 : 0,
									Owner->CalculateCurrentPolicyInfluenceAlpha(Settings),
									CertifiedLateValidationResult.QuietProofDurationSeconds,
									Settings.BalancePhase1LateValidateRequiredSeconds,
									Owner->GetPendingBodyModifierCachedResetNames().Num(),
									*MaxAuditTargetDeltaBone.ToString(),
									MaxAuditTargetDelta,
									(LateValidationAccumulatedSeconds <= DeltaTime + KINDA_SMALL_NUMBER) ? 1 : 0);
								bLoggedPhase1UpperBodyAudit = true;
							}
						}

						const TArray<FName>& PendingResets = Owner->GetPendingBodyModifierCachedResetNames();
						for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
						{
							if (!BalanceTransitionSets::IsUpperBody(BoneName)) { continue; }

							const FBodyInstance* BodyInst = Mesh->GetBodyInstance(BoneName);
							if (!BodyInst || !BodyInst->IsValidBodyInstance()) { continue; }

							const bool bRawSim = BodyInst->IsInstanceSimulatingPhysics();
							const float LinVel = BodyInst->GetUnrealWorldVelocity().Size();
							const float AngVel = FMath::RadiansToDegrees(BodyInst->GetUnrealWorldAngularVelocityInRadians().Size());

							float TargetDeltaDeg = 0.0f;
							const FQuat* HoldRot = EntryHoldRotations.Find(BoneName);
							if (HoldRot)
							{
								TargetDeltaDeg = FMath::RadiansToDegrees(HoldRot->AngularDistance(Mesh->GetBoneQuaternion(BoneName, EBoneSpaces::WorldSpace)));
							}

							const bool bHasPendingReset = PendingResets.Contains(PhysAnimBridge::MakeBodyModifierName(BoneName));

							const bool bRawSimViolation = bRawSim;
							const bool bTargetDeltaViolation = TargetDeltaDeg > 0.1f;
							const bool bMotionViolation = LinVel > 1.0f || AngVel > 10.0f;
							const bool bPendingResetViolation = bHasPendingReset;

							bool bSuppressedByProbe = false;
							if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate &&
								Phase1TopologyRecord.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold &&
								bTargetDeltaViolation && !bRawSimViolation && !bMotionViolation && !bPendingResetViolation &&
								LinVel == 0.0f && AngVel == 0.0f)
							{
								static int32 LastSuppressedFrame = -1;
								const int32 CurrentFrame = GFrameCounter;
								if (LastSuppressedFrame != CurrentFrame)
								{
									LastSuppressedFrame = CurrentFrame;
									UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_TARGETDELTA_SUPPRESSED frame=%d bone=%s reason=late_validate_rebase_probe"),
										CurrentFrame, *BoneName.ToString());
								}
								bSuppressedByProbe = true;
							}

							if ((bRawSimViolation || bTargetDeltaViolation || bMotionViolation || bPendingResetViolation) && !bSuppressedByProbe)
							{
								bUpperBodyInstability = true;

								static FName LastReportedBone = NAME_None;
								static int32 LastSummaryFrame = -1;
								if (LastReportedBone != BoneName)
								{
									LastReportedBone = BoneName;

									if (LastSummaryFrame != GFrameCounter)
									{
										LastSummaryFrame = GFrameCounter;
										UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_SUMMARY (FAILURE_PENDING) upperBodyOwnership=%s upperBodySimCount=%d policySuppressed=%d policyAlpha=%.2f quietProofDuration=%.2f requiredSeconds=%.2f pendingResets=%d maxTargetDeltaBone=%s maxTargetDelta=%.2f consumedResets=%d"),
											BalanceTransitionSets::GetUpperBodyOwnershipModeName(Phase1TopologyRecord.UpperBodyOwnershipMode),
											Phase1TopologyRecord.UpperBodySimCount,
											Phase1TopologyRecord.bPolicySuppressed ? 1 : 0,
											Owner->CalculateCurrentPolicyInfluenceAlpha(Settings),
											CertifiedLateValidationResult.QuietProofDurationSeconds,
											Settings.BalancePhase1LateValidateRequiredSeconds,
											Owner->GetPendingBodyModifierCachedResetNames().Num(),
											*MaxAuditTargetDeltaBone.ToString(),
											MaxAuditTargetDelta,
											0);
									}

									FString Reason;
									if (bRawSimViolation) Reason += TEXT("raw_sim_violation ");
									if (bTargetDeltaViolation) Reason += TEXT("target_delta_violation ");
									if (bMotionViolation) Reason += TEXT("motion_violation ");
									if (bPendingResetViolation) Reason += TEXT("pending_reset_violation ");

									UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_GATE bone=%s reason=%s targetDelta=%.2f linVel=%.2f angVel=%.2f"),
										*BoneName.ToString(), *Reason.TrimEnd(), TargetDeltaDeg, LinVel, AngVel);
								}
								break;
							}
						}
				}
			}
			else
			{
				// Fallback removed: detailed observed-violation gate above is now authoritative.
			}
		}

		const bool bSimCoverageRegressed = bCurrentSnapshotValid && bLiveSnapshotValid &&
			(LiveSnapshot.SimCount < Phase1TopologyRecord.TotalSimCount ||
				LiveSnapshot.RootOwnershipMode != Phase1TopologyRecord.RootOwnershipMode ||
				LiveSnapshot.ProximalOwnershipMode != Phase1TopologyRecord.ProximalOwnershipMode ||
				LiveSnapshot.DistalOwnershipMode != Phase1TopologyRecord.DistalOwnershipMode);

		if (bSimCoverageRegressed)
		{
			static int32 SimDumpCount = 0;
			if (SimDumpCount < 10)
			{
				SimDumpCount++;
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("--- PHASE 1 SIM-COVERAGE FORENSIC DUMP (%d) ---"), SimDumpCount);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("Counts: FrozenTotal=%d LiveTotal=%d FrozenProximal=%d LiveProximal=%d"),
					Phase1TopologyRecord.TotalSimCount, LiveSnapshot.SimCount,
					Phase1TopologyRecord.ProximalSimCount, LiveSnapshot.ProximalSimCount);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("Ownership: FrozenProximal=%d LiveProximal=%d | FrozenDistal=%d LiveDistal=%d"),
					static_cast<int32>(Phase1TopologyRecord.ProximalOwnershipMode), static_cast<int32>(LiveSnapshot.ProximalOwnershipMode),
					static_cast<int32>(Phase1TopologyRecord.DistalOwnershipMode), static_cast<int32>(LiveSnapshot.DistalOwnershipMode));

				const USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
				for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
				{
					if (!BalanceTransitionSets::IsProximal(BoneName))
					{
						continue;
					}
					const bool bIntendedSim = !ShouldKeepBoneKinematic(BoneName, Settings);
					const FBodyInstance* Body = Mesh ? Mesh->GetBodyInstance(BoneName) : nullptr;
					const bool bRawSim = Body && Body->IsInstanceSimulatingPhysics();
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("Bone: %s | FrozenExpect=Sim | Intended=%s | RawBody=%s"),
						*BoneName.ToString(),
						bIntendedSim ? TEXT("Sim") : TEXT("Kin"),
						bRawSim ? TEXT("Sim") : TEXT("Kin"));
				}
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("--- END FORENSIC DUMP ---"));
			}
		}

		const bool bFirstPolicyEnabledFrame = Owner->GetLastControlTargetDiagnostics().bFirstPolicyEnabledFrame;
		const bool bPolicyInfluenceRampReanchored = Owner->WasPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame();

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

				if (bSimCoverageRegressed)
				{
					// Forensic logic moved up
				}

				if (bUpperBodyInstability)
				{
					// Forensic logging for upper-body instability
					USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
					if (Mesh)
					{
						const FName UpperBodyBones[] = {
							TEXT("clavicle_l"), TEXT("clavicle_r"),
							TEXT("upperarm_l"), TEXT("upperarm_r"),
							TEXT("lowerarm_l"), TEXT("lowerarm_r"),
							TEXT("hand_l"), TEXT("hand_r"),
							TEXT("neck_01"), TEXT("head")
						};

						FName MaxErrorBone = NAME_None;
						float MaxLinearSpeed = 0.0f;
						float MaxAngularSpeed = 0.0f;
						float MaxTargetDelta = 0.0f;

						const TArray<FName>& PendingResets = Owner->GetPendingBodyModifierCachedResetNames();

						for (const FName BoneName : UpperBodyBones)
						{
							const FBodyInstance* BodyInst = Mesh->GetBodyInstance(BoneName);
							if (!BodyInst || !BodyInst->IsValidBodyInstance()) { continue; }

							const bool bIntendedKinematic = ShouldKeepBoneKinematic(BoneName, Settings);
							const bool bActualSimulating = BodyInst->IsInstanceSimulatingPhysics();
							const FVector LinVel = BodyInst->GetUnrealWorldVelocity();
							const FVector AngVel = FMath::RadiansToDegrees(BodyInst->GetUnrealWorldAngularVelocityInRadians());
							
							float TargetDeltaDeg = 0.0f;
							const FQuat* HoldRot = EntryHoldRotations.Find(BoneName);
							if (HoldRot)
							{
								TargetDeltaDeg = HoldRot->AngularDistance(Mesh->GetBoneQuaternion(BoneName, EBoneSpaces::WorldSpace));
								TargetDeltaDeg = FMath::RadiansToDegrees(TargetDeltaDeg);
							}

							const bool bHasPendingReset = PendingResets.Contains(PhysAnimBridge::MakeBodyModifierName(BoneName));

						if (GVerbosePhase1Forensics != 0)
						{
							UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] UPPER_BODY_FAILURE_FORENSICS bone=%s intended=%s actual=%s targetDelta=%.2f linVel=%.2f angVel=%.2f pendingReset=%d"),
								*BoneName.ToString(),
								bIntendedKinematic ? TEXT("Kin") : TEXT("Sim"),
								bActualSimulating ? TEXT("Sim") : TEXT("Kin"),
								TargetDeltaDeg,
								LinVel.Size(),
								AngVel.Size(),
								bHasPendingReset ? 1 : 0);
						}

						if (LinVel.Size() > MaxLinearSpeed) { MaxLinearSpeed = LinVel.Size(); MaxErrorBone = BoneName; }
						if (AngVel.Size() > MaxAngularSpeed) { MaxAngularSpeed = AngVel.Size(); }
						if (TargetDeltaDeg > MaxTargetDelta) { MaxTargetDelta = TargetDeltaDeg; }
					}

					if (GVerbosePhase1Forensics != 0)
					{
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] UPPER_BODY_FAILURE_SUMMARY worstBone=%s maxLinSpeed=%.2f maxAngSpeed=%.2f maxTargetDelta=%.2f"),
							*MaxErrorBone.ToString(),
							MaxLinearSpeed,
							MaxAngularSpeed,
							MaxTargetDelta);
					}
					}
				}
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
					if (GVerbosePhase1Forensics != 0)
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
				}

				if (!bCanCompleteAsRootCoupledReady &&
					RootOnReadinessShellHoldAccumulatedSeconds + KINDA_SMALL_NUMBER < Settings.BalancePhase2RequiredShellHoldDuration)
				{
					if (GVerbosePhase1Forensics != 0)
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

		if (TotalTransitionTimeSeconds > (GStrictPhase1Certification != 0 ? 2.0f : Settings.BalancePhase1TimeoutDuration))
		{
			if (TotalTransitionTimeSeconds > 2.0f + KINDA_SMALL_NUMBER)
			{
				Audit.bUsedTimeoutExtension = true;
			}
			const bool bQuietProofSatisfied = QuietWindowAccumulatedSeconds >= Settings.BalancePhase1QuietRequiredSeconds;
			const bool bExpectedReleaseSatisfied = bExpectedUpperBodyRelease;
			const bool bUpperBodyHoldSatisfied = !bUpperBodyInstability;
			const bool bSimCoverageSatisfied = !bSimCoverageRegressed;
			const bool bTargetContinuitySatisfied = !bLateValidateTargetDiscontinuity;
			const bool bBodyMotionThresholdsSatisfied = !bIsBodyMotionUnstable;
			const bool bRootValiditySatisfied = !Phase1TopologyRecord.bRootSimulating;

			const bool bRootOnReadinessShellHoldSatisfied = CurrentResult.bRootOnReadinessShellHoldSatisfied;
			const bool bRootOnReadinessFinalBringUpControlSettled = CurrentResult.bRootOnReadinessFinalBringUpControlSettled;
			const bool bRootOnReadinessPolicyInfluenceSettled = CurrentResult.bRootOnReadinessPolicyInfluenceSettled;
			const bool bPreRootOnShellSafetyProofSatisfied = CurrentResult.bPreRootOnShellSafetyProofSatisfied;
			const bool bRootOnReadinessProven = CurrentResult.bRootOnReadinessProven;

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_CONVERGENCE_REPORT upperBodyHold=%d simCoverage=%d targetContinuity=%d quietProof=%d bodyMotion=%d rootValidity=%d expectedRelease=%d readyProven=%d shellHold=%d bringUp=%d policyInfl=%d shellSafety=%d lateValidateSeconds=%.2f/%.2f shellHoldSeconds=%.2f/%.2f quietProofSeconds=%.2f/%.2f shellProofSeconds=%.2f/%.2f"),
				bUpperBodyHoldSatisfied ? 1 : 0,
				bSimCoverageSatisfied ? 1 : 0,
				bTargetContinuitySatisfied ? 1 : 0,
				bQuietProofSatisfied ? 1 : 0,
				bBodyMotionThresholdsSatisfied ? 1 : 0,
				bRootValiditySatisfied ? 1 : 0,
				bExpectedReleaseSatisfied ? 1 : 0,
				bRootOnReadinessProven ? 1 : 0,
				bRootOnReadinessShellHoldSatisfied ? 1 : 0,
				bRootOnReadinessFinalBringUpControlSettled ? 1 : 0,
				bRootOnReadinessPolicyInfluenceSettled ? 1 : 0,
				bPreRootOnShellSafetyProofSatisfied ? 1 : 0,
				LateValidationAccumulatedSeconds, Settings.BalancePhase1LateValidateRequiredSeconds,
				RootOnReadinessShellHoldAccumulatedSeconds, Settings.BalancePhase2RequiredShellHoldDuration,
				QuietWindowAccumulatedSeconds, Settings.BalancePhase1QuietRequiredSeconds,
				RootOnReadinessShellProofAccumulatedSeconds, Settings.BalancePhase2PreRootOnShellProofRequiredSeconds);

			if (!bRootOnReadinessFinalBringUpControlSettled)
			{
				const int32 FinalGroupIndex = Owner->GetBringUpGroupCount() - 1;
				const float FinalAlpha = CurrentSnapshot.FinalBringUpGroupControlAuthorityAlpha;
				const float Threshold = 1.0f - KINDA_SMALL_NUMBER;
				const float SettleTime = CurrentSnapshot.BringUpStableAccumulatedSeconds;
				const float RequiredSettleTime = FMath::Max(Settings.StartupRampSeconds, 0.25f);

				FString Cause = TEXT("alpha_never_started");
				if (!CurrentSnapshot.bFinalBringUpGroupUnlocked)
				{
					Cause = TEXT("final_group_never_unlocked");
				}
				else if (!CurrentSnapshot.bFinalBringUpGroupRampActive)
				{
					if (!CurrentSnapshot.bBringUpWithinBodyVelocityBounds || !CurrentSnapshot.bBringUpWithinRootBounds)
					{
						Cause = TEXT("ramp_delayed_by_guard");
					}
					else
					{
						Cause = TEXT("alpha_never_started");
					}
				}
				else if (FinalAlpha < Threshold)
				{
					Cause = TEXT("alpha_started_but_never_reached_threshold");
				}

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_BRINGUP_FAILURE_DETAILS group=%d alpha=%.4f threshold=%.4f settleTime=%.2f/%.2f unlocked=%d active=%d bodyGuard=%d rootGuard=%d"),
					FinalGroupIndex, FinalAlpha, Threshold, SettleTime, RequiredSettleTime, 
					CurrentSnapshot.bFinalBringUpGroupUnlocked ? 1 : 0,
					CurrentSnapshot.bFinalBringUpGroupRampActive ? 1 : 0,
					CurrentSnapshot.bBringUpWithinBodyVelocityBounds ? 1 : 0,
					CurrentSnapshot.bBringUpWithinRootBounds ? 1 : 0);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_BRINGUP_NOT_SETTLED cause=%s"), *Cause);
			}

			if (!bPreRootOnShellSafetyProofSatisfied)
			{
				const float ProofSeconds = RootOnReadinessShellProofAccumulatedSeconds;
				const float RequiredProofSeconds = Settings.BalancePhase2PreRootOnShellProofRequiredSeconds;
				const float OffsetCm = CurrentSnapshot.ShellOffsetDeltaAtCaptureCm;
				const float MaxOffsetCm = Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm;
				const float VelocityCmPerSec = CurrentSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond;
				const float MaxVelocityCmPerSec = Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond;
				const float OffsetGrowthCm = CurrentSnapshot.ShellOffsetGrowthCm;
				const float MaxOffsetGrowthCm = Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm;
				const float VelocityGrowthCmPerSec = CurrentSnapshot.ShellVelocityGrowthCmPerSecond;
				const float MaxVelocityGrowthCmPerSec = Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond;

				const float SignificantShellOffsetThresholdCm = 0.1f;
				const float SignificantShellVelocityThresholdCmPerSecond = 1.0f;
				const bool bShellCorrectionActivelyAffecting = CurrentSnapshot.bShellCorrectionOwnerActive && 
					(OffsetCm > SignificantShellOffsetThresholdCm || VelocityCmPerSec > SignificantShellVelocityThresholdCmPerSecond);

				FString Cause = TEXT("unknown");
				if (ProofSeconds + KINDA_SMALL_NUMBER < RequiredProofSeconds) Cause = TEXT("proof_duration_unsatisfied");
				else if (OffsetCm > MaxOffsetCm || VelocityCmPerSec > MaxVelocityCmPerSec || OffsetGrowthCm > MaxOffsetGrowthCm || VelocityGrowthCmPerSec > MaxVelocityGrowthCmPerSec) Cause = TEXT("metrics_out_of_range");
				else if (bShellCorrectionActivelyAffecting) Cause = TEXT("shell_correction_actively_affecting");
				else if (!CurrentSnapshot.bTransitionOwnedShellLocked) Cause = TEXT("shell_not_locked");
				else if (!CurrentSnapshot.bTransitionShellReferenceReanchored) Cause = TEXT("shell_not_reanchored");
				else if (CurrentSnapshot.bTransitionShellReferenceReseededAfterLock) Cause = TEXT("shell_reseeded_after_lock");

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_SHELL_SAFETY_FAILURE_DETAILS proofDuration=%.2f/%.2f offset=%.2f/%.2f velocity=%.2f/%.2f offsetGrowth=%.2f/%.2f velocityGrowth=%.2f/%.2f shellCorrectionActive=%d activelyAffecting=%d locked=%d reanchored=%d reseeded=%d"),
					ProofSeconds, RequiredProofSeconds, OffsetCm, MaxOffsetCm, VelocityCmPerSec, MaxVelocityCmPerSec, OffsetGrowthCm, MaxOffsetGrowthCm, VelocityGrowthCmPerSec, MaxVelocityGrowthCmPerSec,
					CurrentSnapshot.bShellCorrectionOwnerActive ? 1 : 0, bShellCorrectionActivelyAffecting ? 1 : 0, CurrentSnapshot.bTransitionOwnedShellLocked ? 1 : 0, CurrentSnapshot.bTransitionShellReferenceReanchored ? 1 : 0, CurrentSnapshot.bTransitionShellReferenceReseededAfterLock ? 1 : 0);
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_SHELL_SAFETY_UNSATISFIED cause=%s"), *Cause);
			}

			TArray<FString> UnsatisfiedGates;
			if (!bRootValiditySatisfied) UnsatisfiedGates.Add(TEXT("root_topology_mismatch"));
			if (!bSimCoverageSatisfied) UnsatisfiedGates.Add(TEXT("sim_coverage_regressed"));
			if (!bUpperBodyHoldSatisfied) UnsatisfiedGates.Add(TEXT("upper_body_instability"));
			if (!bBodyMotionThresholdsSatisfied) UnsatisfiedGates.Add(TEXT("body_motion_thresholds_exceeded"));
			if (!bTargetContinuitySatisfied) UnsatisfiedGates.Add(TEXT("target_discontinuity"));
			if (!bQuietProofSatisfied) UnsatisfiedGates.Add(TEXT("quiet_proof_unsatisfied"));
			if (!bExpectedReleaseSatisfied) UnsatisfiedGates.Add(TEXT("expected_release_never_reached"));
			if (!bRootOnReadinessShellHoldSatisfied) UnsatisfiedGates.Add(TEXT("shell_hold_unsatisfied"));
			if (!bRootOnReadinessFinalBringUpControlSettled) UnsatisfiedGates.Add(TEXT("bringup_not_settled"));
			if (!bRootOnReadinessPolicyInfluenceSettled) UnsatisfiedGates.Add(TEXT("policy_influence_not_settled"));
			if (!bPreRootOnShellSafetyProofSatisfied) UnsatisfiedGates.Add(TEXT("shell_safety_proof_unsatisfied"));
			if (!bRootOnReadinessProven) UnsatisfiedGates.Add(TEXT("ready_proof_failed"));

			const FString PrimaryReason = UnsatisfiedGates.Num() > 0 ? UnsatisfiedGates[0] : TEXT("unknown");
			const FString SecondaryReason = UnsatisfiedGates.Num() > 1 ? UnsatisfiedGates[1] : TEXT("none");

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_NONCONVERGENCE primary=%s secondary=%s"), *PrimaryReason, *SecondaryReason);

			// DISTAL_FORENSIC_REPORT
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_CONVERGENCE_FORENSIC_REPORT_START"));
			for (const FName DistalBoneName : PhysAnimBridge::GetControlledBoneNames())
			{
				if (!BalanceTransitionSets::IsDistalLowerLimb(DistalBoneName))
				{
					continue;
				}

				const bool bShouldBeKinematic = ShouldKeepBoneKinematic(DistalBoneName, Settings);
				const EPhysicsMovementType ExpectedOwnership = bShouldBeKinematic ? EPhysicsMovementType::Kinematic : EPhysicsMovementType::Simulated;
				
				EPhysicsMovementType ModifierOwnership = EPhysicsMovementType::Simulated;
				if (UPhysicsControlComponent* PhysicsControl = Owner->PhysicsControlComponent.Get())
				{
					const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(DistalBoneName);
					if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
					{
						ModifierOwnership = Record->BodyModifier.ModifierData.MovementType;
					}
				}

				bool bRawSimulating = false;
				FVector LinVel = FVector::ZeroVector;
				FVector AngVel = FVector::ZeroVector;
				if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
				{
					if (FBodyInstance* BI = Mesh->GetBodyInstance(DistalBoneName))
					{
						bRawSimulating = BI->IsInstanceSimulatingPhysics();
						LinVel = BI->GetUnrealWorldVelocity();
						AngVel = FMath::RadiansToDegrees(BI->GetUnrealWorldAngularVelocityInRadians());
					}
				}

				const bool bHasPendingReset = Owner->GetPendingBodyModifierCachedResetNames().Contains(DistalBoneName);
				const int32 PersistentTicks = DistalBonePersistentTicks.Contains(DistalBoneName) ? DistalBonePersistentTicks[DistalBoneName] : 0;
				const int32 ConsecutiveTicks = DistalBoneConsecutiveMismatchTicks.Contains(DistalBoneName) ? DistalBoneConsecutiveMismatchTicks[DistalBoneName] : 0;
				const bool bContributedToFailure = PersistentTicks > 0 || ConsecutiveTicks > 0;

				float BoneMaxTargetDelta = 0.0f;
				if (Owner->GetLastControlTargetDiagnostics().MaxTargetDeltaBoneName == DistalBoneName)
				{
					BoneMaxTargetDelta = Owner->GetLastControlTargetDiagnostics().MaxTargetDeltaDegrees;
				}

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_BONE_FORENSIC: bone=%s expected=%d modifier=%d rawSim=%d linVel=%.1f angVel=%.1f targetDelta=%.2f pendingReset=%d persistentTicks=%d consecutiveTicks=%d contributed=%d"),
					*DistalBoneName.ToString(),
					static_cast<int32>(ExpectedOwnership),
					static_cast<int32>(ModifierOwnership),
					bRawSimulating ? 1 : 0,
					LinVel.Size(),
					AngVel.Size(),
					BoneMaxTargetDelta,
					bHasPendingReset ? 1 : 0,
					PersistentTicks,
					ConsecutiveTicks,
					bContributedToFailure ? 1 : 0);
			}
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_CONVERGENCE_FORENSIC_REPORT_END"));

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

		EPhysicsMovementType PelvisModifierMovementType = EPhysicsMovementType::Static;
		if (UPhysicsControlComponent* PhysicsControl = Owner->PhysicsControlComponent.Get())
		{
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
			{
				PelvisModifierMovementType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (Phase2GuardTickCount <= 2)
		{
			if (Phase2GuardTickCount == 1)
			{

extern int32 GVerbosePhase2Forensics;

				if (GVerbosePhase2Forensics != 0)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_PRE_GUARD_PELVIS_STATE tick=%d rawSim=%d modMoveType=%d shellLocked=%d quarantined=%d state=%d owner=%d actor=%s component=%s"),
						Phase2GuardTickCount,
						bPelvisActualSim ? 1 : 0,
						static_cast<int32>(PelvisModifierMovementType),
						Owner->IsTransitionOwnedShellLocked() ? 1 : 0,
						bPhase2RootAuthorityQuarantined ? 1 : 0,
						static_cast<int32>(Owner->GetRuntimeState()),
						static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
						*Owner->GetOwner()->GetName(), *Owner->GetName());
				}
			}

			if (GVerbosePhase2Forensics != 0)
			{
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] PHASE2_GUARD_TICK tick=%d requestedRootSim=%d actualRootSim=%d resetScheduled=%d simCountPost=%d distalSimPost=%d shellOffsetDelta=%.1f shellVelocityDelta=%.1f owner=%d actor=%s component=%s"),
					Phase2GuardTickCount,
					bPelvisRequestedSim ? 1 : 0,
					bPelvisActualSim ? 1 : 0,
					Diagnostics.bResetScheduled ? 1 : 0,
					Diagnostics.SimCountPost,
					Diagnostics.DistalSimCountPost,
					Diagnostics.BaselineShellOffset,
					Diagnostics.BaselineShellVel,
					static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
					*Owner->GetOwner()->GetName(), *Owner->GetName());
			}
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
			const bool bSuppressionConditionsMet = bPelvisActualSim && Diagnostics.SimCountPost >= 6;
			if (bSuppressionConditionsMet)
			{
				static int32 LastSuppressionFrame = -1;
				if (LastSuppressionFrame != GFrameCounter)
				{
					LastSuppressionFrame = GFrameCounter;
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SHELL_MATERIAL_GUARD_SUPPRESSED frame=%d tick=%d shellOffsetDelta=%.2f shellVelocityDelta=%.2f rootActualSim=%d simCountPost=%d shellLocked=%d shellReanchored=%d owner=%d actor=%s component=%s"),
						GFrameCounter, Phase2GuardTickCount, Diagnostics.BaselineShellOffset, Diagnostics.BaselineShellVel,
						bPelvisActualSim ? 1 : 0, Diagnostics.SimCountPost,
						CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
						CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
						static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase2_shell_correction_material"))),
						*Owner->GetOwner()->GetName(), *Owner->GetName());
				}
			}
			else
			{
				AbortReason = TEXT("phase2_shell_correction_material");
				AbortDetail = FString::Printf(
					TEXT("shellOffsetDelta=%.1f/%.1f shellVelocityDelta=%.1f/%.1f"),
					Diagnostics.BaselineShellOffset,
					Settings.BalancePhase2AbortShellOffsetDelta,
					Diagnostics.BaselineShellVel,
					Settings.BalancePhase2AbortShellVelocityDelta);
			}
		}
		else if (!bPelvisActualSim && Phase2GuardTickCount > 1)
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
			((Diagnostics.BaselineShellOffset > Settings.BalancePhase2AbortShellOffsetDelta ||
			  Diagnostics.BaselineShellVel > Settings.BalancePhase2AbortShellVelocityDelta) && !(bPelvisActualSim && Diagnostics.SimCountPost >= 6)) ||
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

			FString FailureType = TEXT("unknown");
			if (AbortReason == TEXT("phase2_root_on_spike")) FailureType = TEXT("root_motion_spike");
			else if (AbortReason == TEXT("phase2_root_simulation_dropped")) FailureType = TEXT("support/contact_failure");
			else if (AbortReason == TEXT("phase2_policy_write_leak")) FailureType = TEXT("policy_offset_violation");
			else if (AbortReason == TEXT("phase2_reset_violation")) FailureType = TEXT("stale_cached_target");
			else if (AbortReason == TEXT("phase2_shell_correction_material")) FailureType = TEXT("shell_correction_influence");
			else if (AbortReason == TEXT("phase2_topology_not_preserved")) FailureType = TEXT("topology_mismatch");
			else if (AbortReason == TEXT("phase2_fail_stop_precursor")) FailureType = TEXT("root_motion_spike");

			float MeasuredValue = 0.0f;
			float ThresholdValue = 0.0f;
			FName OffendingBoneValue = NAME_None;

			if (AbortReason == TEXT("phase2_root_on_spike"))
			{
				if (Diagnostics.RootSpeed > Settings.BalancePhase2AbortRootLinearSpeed) { MeasuredValue = Diagnostics.RootSpeed; ThresholdValue = Settings.BalancePhase2AbortRootLinearSpeed; }
				else if (Diagnostics.RootAngularSpeed > Settings.BalancePhase2AbortRootAngularSpeed) { MeasuredValue = Diagnostics.RootAngularSpeed; ThresholdValue = Settings.BalancePhase2AbortRootAngularSpeed; }
				else if (Diagnostics.PeakMaxBodyLinearSpeed > Settings.BalancePhase2AbortMaxBodyLinearSpeed) { MeasuredValue = Diagnostics.PeakMaxBodyLinearSpeed; ThresholdValue = Settings.BalancePhase2AbortMaxBodyLinearSpeed; }
				else { MeasuredValue = Diagnostics.PeakMaxBodyAngularSpeed; ThresholdValue = Settings.BalancePhase2AbortMaxBodyAngularSpeed; }
			}
			else if (AbortReason == TEXT("phase2_policy_write_leak"))
			{
				MeasuredValue = ControlTargetDiagnostics.MaxTargetDeltaDegrees;
				ThresholdValue = Settings.BalancePhase2EntryMaxTargetDeltaDeg;
				OffendingBoneValue = ControlTargetDiagnostics.MaxTargetDeltaBoneName;
			}
			else if (AbortReason == TEXT("phase2_shell_correction_material"))
			{
				MeasuredValue = Diagnostics.BaselineShellOffset;
				ThresholdValue = Settings.BalancePhase2AbortShellOffsetDelta;
			}

			if (!bLoggedPhase2FirstFailureAudit)
			{
				UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_FIRST_FAILURE_AUDIT reason=%s type=%s bone=%s measured=%.2f threshold=%.2f state=%s rootRawSim=%d pelvisRawSim=%d pelvisModType=%s simCountPost=%d upperBodySimPost=%d policyAlpha=%.2f controlAlpha=%.2f shellLocked=%d shellReanchored=%d owner=%d actor=%s component=%s"),
					*AbortReason, *FailureType, *OffendingBoneValue.ToString(), MeasuredValue, ThresholdValue, 
					UPhysAnimComponent::GetRuntimeStateName(Owner->GetRuntimeState()),
					bPelvisActualSim ? 1 : 0,
					bPelvisActualSim ? 1 : 0,
					UPhysAnimComponent::GetPhysicsMovementTypeName(PelvisModifierMovementType),
					Diagnostics.SimCountPost,
					Diagnostics.UpperBodySimCountPost,
					Owner->CalculateCurrentPolicyInfluenceAlpha(Settings),
					Owner->CalculateCurrentControlAuthorityAlpha(Settings),
					CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
					CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
					static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
					*Owner->GetOwner()->GetName(), *Owner->GetName());
				bLoggedPhase2FirstFailureAudit = true;
			}
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
				TEXT("[PhysAnimBalance] PHASE2_GUARD_WINDOW_ABORTED reason=%s owner=%d detail=%s rootLinear=%.1f/%.1f rootAngular=%.1f/%.1f shellOffsetDelta=%.1f/%.1f shellVelocityDelta=%.1f/%.1f maxBodyLinear=%.1f/%.1f maxBodyAngular=%.1f/%.1f pelvisLin=%.1f pelvisAng=%.1f thighsLin=%.1f thighsAng=%.1f spineLin=%.1f spineAng=%.1f feetLin=%.1f feetAng=%.1f simCountPre=%d simCountPost=%d upperBodySimPre=%d upperBodySimPost=%d shellLocked=%d shellReanchored=%d shellReseeded=%d policyActive=%d firstPolicyFrame=%d policyWrites=%d maxTargetDeltaBone=%s maxTargetDelta=%.1f maxRawOffsetBone=%s maxRawOffset=%.1f lowerLimbLimitBone=%s lowerLimbLimit=%.2f lowerLimbLimitProxy=%.1f actor=%s component=%s"),
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
				ControlTargetDiagnostics.MaxLowerLimbLimitProxyDegrees,
				*Owner->GetOwner()->GetName(), *Owner->GetName());

			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnimBalance] PHASE2_EARLY_ABORT_SUMMARY reason=%s strict=%d shellLocked=%d shellReanchored=%d rootRequestedSim=%d rootActualSim=%d preSim=%d postSim=%d shellOffset=%.1f shellVel=%.1f detail=%s owner=%d actor=%s component=%s"),
				*Diagnostics.FailureReason,
				GStrictPhase1Certification != 0 ? 1 : 0,
				CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
				CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
				bPelvisRequestedSim ? 1 : 0,
				bPelvisActualSim ? 1 : 0,
				Diagnostics.SimCountPre,
				Diagnostics.SimCountPost,
				Diagnostics.BaselineShellOffset,
				Diagnostics.BaselineShellVel,
				*AbortDetail,
				static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
				*Owner->GetOwner()->GetName(), *Owner->GetName());

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

bool FPhysAnimBalanceReadyTransition::IsProximal(FName BoneName)
{
	return BalanceTransitionSets::IsProximal(BoneName);
}

void FPhysAnimBalanceReadyTransition::SetPhase(EBalanceReadyTransitionPhase NewPhase, UPhysAnimComponent* Owner)
{
	if (InternalPhase == NewPhase)
	{
		return;
	}

	if (Owner &&
		(NewPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || NewPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate) &&
		InternalPhase != NewPhase)
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

		const bool bPelvisRequestedSim = Owner->WasPelvisSimulatingLastFrame();
		const bool bPelvisActualSim = Owner->IsPelvisSimulatingNow();
		TArray<FName> SimulatingBones;
		Owner->GetSimulatingBodies(SimulatingBones);
		Diagnostics.SimCountPre = SimulatingBones.Num();

		Diagnostics.BaselineShellOffset = CachedConvergenceSnapshot.ShellPlanarOffset;
		Diagnostics.BaselineShellVel = CachedConvergenceSnapshot.ShellPlanarVelocity;

		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		EPhysicsMovementType PelvisModifierMovementType = EPhysicsMovementType::Static;
		if (UPhysicsControlComponent* PhysicsControl = Owner->PhysicsControlComponent.Get())
		{
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
			if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, ModifierName))
			{
				PelvisModifierMovementType = Record->BodyModifier.ModifierData.MovementType;
			}
		}

		if (!bLoggedPhase2EntryAudit)
		{
			USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
			const FVector RootLinVel = Mesh ? Mesh->GetPhysicsLinearVelocity(RootBoneName) : FVector::ZeroVector;
			const FVector RootAngVel = Mesh ? Mesh->GetPhysicsAngularVelocityInDegrees(RootBoneName) : FVector::ZeroVector;

			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnimBalance] PHASE2_ENTRY_AUDIT topology=%s owner=%d actor=%s component=%s state=%s rootRawSim=%d rootModType=%s pelvisRawSim=%d pelvisModType=%s simCount=%d upperBodySimCount=%d policyAlpha=%.2f controlAlpha=%.2f groupControlAlpha=%.2f policySuppressed=%d shellLocked=%d shellReanchored=%d pendingResets=%d safetyLatch=%d rootLinVel=%.2f rootAngVel=%.2f pelvisLinVel=%.2f pelvisAngVel=%.2f"),
				*CertifiedHandoff.TopologyClass,
				static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
				*Owner->GetOwner()->GetName(),
				*Owner->GetName(),
				UPhysAnimComponent::GetRuntimeStateName(Owner->GetRuntimeState()),
				bPelvisActualSim ? 1 : 0,
				UPhysAnimComponent::GetPhysicsMovementTypeName(PelvisModifierMovementType),
				bPelvisActualSim ? 1 : 0,
				UPhysAnimComponent::GetPhysicsMovementTypeName(PelvisModifierMovementType),
				CertifiedHandoff.SimCount,
				CertifiedHandoff.UpperBodySimCount,
				Owner->CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings),
				Owner->CalculateCurrentControlAuthorityAlpha(EffectiveSettings),
				Owner->CalculateBringUpGroupControlAuthorityAlpha(0, EffectiveSettings),
				ShouldSuppressPolicy() ? 1 : 0,
				CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
				CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
				Owner->GetPendingBodyModifierCachedResetNames().Num(),
				bPhase2RootAuthorityQuarantined ? 1 : 0,
				RootLinVel.Size(),
				RootAngVel.Size(),
				RootLinVel.Size(),
				RootAngVel.Size());
			bLoggedPhase2EntryAudit = true;
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
		FString TerminalReason = TEXT("transition_terminal_exit");
		if (NewPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
		{
			TerminalReason = TEXT("transition_success");
		}
		else if (NewPhase == EBalanceReadyTransitionPhase::BRT_Failed)
		{
			TerminalReason = TEXT("transition_failed") + (Diagnostics.FailureReason.IsEmpty() ? TEXT("") : TEXT("_") + Diagnostics.FailureReason);
		}
		else if (NewPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied)
		{
			TerminalReason = TEXT("transition_safe_denied") + (Diagnostics.FailureReason.IsEmpty() ? TEXT("") : TEXT("_") + Diagnostics.FailureReason);
		}
		else if (NewPhase == EBalanceReadyTransitionPhase::BRT_Inactive)
		{
			TerminalReason = TEXT("transition_inactive");
		}
		
		Owner->SetStartupBringUpFrozenByBalanceEntry(false, TerminalReason);
		
		FName LongestMismatchBone = NAME_None;
		int32 MaxMismatchTicks = 0;
		for (auto& Pair : DistalBonePersistentTicks)
		{
			if (Pair.Value > MaxMismatchTicks)
			{
				MaxMismatchTicks = Pair.Value;
				LongestMismatchBone = Pair.Key;
			}
		}

		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnimBalance] DISTAL_PHASE1_SUMMARY: transient=%d persistent=%d pending=%d worstBone=%s worstTicks=%d totalPhase1Time=%.2f"),
			DistalMismatchesTransientCount,
			DistalMismatchesPersistentCount,
			DistalMismatchesPendingCount,
			*LongestMismatchBone.ToString(),
			MaxMismatchTicks,
			PhaseTimeSeconds);
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

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ENTRY topology=%s upperBodyOwnership=%s rootPreLin=%.1f rootPreAng=%.1f shellOffsetDelta=%.1f shellVelocityDelta=%.1f simCountPre=%d proximalSimPre=%d distalSimPre=%d upperBodySimPre=%d policySuppressed=%d controlAuthoritySettled=%d finalBringUpControlAlpha=%.2f policyInfluenceAlpha=%.2f policyInfluenceRequired=%.2f policyInfluenceDuration=%.2f policyInfluenceRequiredSeconds=%.2f policyInfluenceRampReanchored=%d shellHoldReady=%d bringUpReady=%d policyInfluenceReady=%d rootOnReady=%d shellHoldDuration=%.2f shellHoldRequired=%.2f maxTargetDelta=%.1f meanTargetDelta=%.1f quietProofDuration=%.2f resetScheduled=%d owner=%d actor=%s component=%s"),
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
					Diagnostics.bResetScheduled ? 1 : 0,
					static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
					*Owner->GetOwner()->GetName(), *Owner->GetName());

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

				bPhase2RootAuthorityQuarantined = true;
				Diagnostics.bSimFlipped = true;
				// Phase 2 must preserve the pre-root-on shell proof reference through the
				// root-on frame and guard window; reseeding here would invalidate that proof.
				CaptureFlipDiagnostics(Owner);
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
		Audit.Reset();
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
	// Phase 1 Prepare specifically allows pending resets while waiting for the quiet window. 
	// LateValidate and beyond require convergence.

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
	if (GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] GET_SIMULATING_BODIES count=%d"), SimulatingBones.Num());
	}
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
	OutSnapshot.bFinalBringUpGroupUnlocked = Owner->IsBringUpGroupUnlocked(FinalBringUpGroupIndex);
	OutSnapshot.bFinalBringUpGroupRampActive = Owner->IsBringUpGroupControlRampActive(FinalBringUpGroupIndex);
	OutSnapshot.BringUpStableAccumulatedSeconds = Owner->BringUpGroupStableAccumulatedSeconds;
	OutSnapshot.bBringUpWithinBodyVelocityBounds = 
		Owner->LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond <= Settings.MaxRootLinearSpeedCmPerSecond &&
		Owner->LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond <= Settings.MaxRootAngularSpeedDegPerSecond;
	OutSnapshot.bBringUpWithinRootBounds = 
		Owner->LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm <= Settings.MaxRootHeightDeltaCm &&
		Owner->LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond <= Settings.MaxRootLinearSpeedCmPerSecond &&
		Owner->LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond <= Settings.MaxRootAngularSpeedDegPerSecond;

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

	OutSnapshot.bShellCorrectionOwnerActive = Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle;
	OutSnapshot.bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame =
		Owner->WasPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame();
	OutSnapshot.bTransitionOwnedShellLocked = Owner->IsTransitionOwnedShellLocked();
	OutSnapshot.bTransitionOwnedShellLocked = Owner->IsTransitionOwnedShellLocked();
	
	const bool bExplicitReanchored = Owner->WasTransitionShellReferenceReanchored();
	
	if (GStrictPhase1Certification != 0)
	{
		OutSnapshot.bTransitionShellReferenceReanchored = bExplicitReanchored;
	}
	else
	{
		const bool bShellLockedAndIdle = OutSnapshot.bTransitionOwnedShellLocked && Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle;
		if (bShellLockedAndIdle && !bExplicitReanchored)
		{
			Audit.bUsedReanchorShortcut = true;
		}
		OutSnapshot.bTransitionShellReferenceReanchored = bExplicitReanchored || bShellLockedAndIdle;
	}

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
	const float SignificantShellOffsetThresholdCm = 0.1f;
	const float SignificantShellVelocityThresholdCmPerSecond = 1.0f;
	
	bool bShellCorrectionActivelyAffecting = false;
	if (GStrictPhase1Certification != 0)
	{
		bShellCorrectionActivelyAffecting = OutSnapshot.bShellCorrectionOwnerActive && 
			(OutSnapshot.ShellOffsetDeltaAtCaptureCm > UE_SMALL_NUMBER || 
			 OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond > UE_SMALL_NUMBER);
	}
	else
	{
		bShellCorrectionActivelyAffecting = OutSnapshot.bShellCorrectionOwnerActive && 
			(OutSnapshot.ShellOffsetDeltaAtCaptureCm > SignificantShellOffsetThresholdCm || 
			 OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond > SignificantShellVelocityThresholdCmPerSecond);
		
		if (OutSnapshot.bShellCorrectionOwnerActive && !bShellCorrectionActivelyAffecting)
		{
			if (OutSnapshot.ShellOffsetDeltaAtCaptureCm > UE_SMALL_NUMBER || 
				OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond > UE_SMALL_NUMBER)
			{
				Audit.bUsedShellOwnershipNarrowing = true;
			}
		}
	}

	if (Owner->bShellCorrectionStateLogged == false || GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_SHELL_CORRECTION_STATE active=%d influencingMetrics=%d staleLatch=%d"),
			OutSnapshot.bShellCorrectionOwnerActive ? 1 : 0,
			bShellCorrectionActivelyAffecting ? 1 : 0,
			(OutSnapshot.bShellCorrectionOwnerActive && !bShellCorrectionActivelyAffecting) ? 1 : 0);
		Owner->bShellCorrectionStateLogged = true;
	}

	const bool bShellSafetySatisfied = 
		OutSnapshot.RootOnReadinessShellProofDurationSeconds + KINDA_SMALL_NUMBER >=
			Settings.BalancePhase2PreRootOnShellProofRequiredSeconds &&
		OutSnapshot.ShellOffsetDeltaAtCaptureCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm &&
		OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond &&
		OutSnapshot.ShellOffsetGrowthCm <= Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm &&
		OutSnapshot.ShellVelocityGrowthCmPerSecond <= Settings.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond &&
		!bShellCorrectionActivelyAffecting &&
		OutSnapshot.bTransitionOwnedShellLocked &&
		OutSnapshot.bTransitionShellReferenceReanchored &&
		!OutSnapshot.bTransitionShellReferenceReseededAfterLock;

	if (Owner->bShellCorrectionStateLogged == false || GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_SHELL_SAFETY_DEBUG proof=%d complete=%d locked=%d reanchored=%d duration=%.2f/%.2f offset=%.2f/%.2f growth=%.2f/%.2f affecting=%d"),
			bShellSafetySatisfied ? 1 : 0, OutResult.bLateValidationCompleted ? 1 : 0, 
			OutSnapshot.bTransitionOwnedShellLocked ? 1 : 0, OutSnapshot.bTransitionShellReferenceReanchored ? 1 : 0, 
			OutSnapshot.RootOnReadinessShellProofDurationSeconds, Settings.BalancePhase2PreRootOnShellProofRequiredSeconds,
			OutSnapshot.ShellOffsetDeltaAtCaptureCm, Settings.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm,
			OutSnapshot.ShellOffsetGrowthCm, Settings.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm,
			bShellCorrectionActivelyAffecting ? 1 : 0);
		Owner->bShellCorrectionStateLogged = true;
	}

	OutResult.bPreRootOnShellSafetyProofSatisfied = bShellSafetySatisfied;

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

	if (OutResult.Outcome == EBalanceLateValidationOutcome::Outcome_AcceptRootOn)
	{
		Audit.bUsedRelaxedCertification = Audit.bUsedTimeoutExtension || Audit.bUsedDwellShortcut || Audit.bUsedReanchorShortcut || Audit.bUsedShellOwnershipNarrowing;
		Audit.Topology = OutSnapshot.TopologyClass;
		Audit.ProximalSimCount = OutSnapshot.ProximalSimCount;
		Audit.DistalSimCount = OutSnapshot.DistalSimCount;
		Audit.UpperBodySimCount = OutSnapshot.UpperBodySimCount;
		Audit.bShellReanchored = OutSnapshot.bTransitionShellReferenceReanchored;
		Audit.bShellLocked = OutSnapshot.bTransitionOwnedShellLocked;
		Audit.ShellOffset = OutSnapshot.ShellOffsetDeltaAtCaptureCm;
		Audit.ShellVelocity = OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond;
	}
	else
	{
		Audit.bUsedRelaxedCertification = Audit.bUsedTimeoutExtension || Audit.bUsedDwellShortcut || Audit.bUsedReanchorShortcut || Audit.bUsedShellOwnershipNarrowing;
		Audit.Topology = OutSnapshot.TopologyClass;
		Audit.ProximalSimCount = OutSnapshot.ProximalSimCount;
		Audit.DistalSimCount = OutSnapshot.DistalSimCount;
		Audit.UpperBodySimCount = OutSnapshot.UpperBodySimCount;
		Audit.bShellReanchored = OutSnapshot.bTransitionShellReferenceReanchored;
		Audit.bShellLocked = OutSnapshot.bTransitionOwnedShellLocked;
		Audit.ShellOffset = OutSnapshot.ShellOffsetDeltaAtCaptureCm;
		Audit.ShellVelocity = OutSnapshot.ShellVelocityDeltaAtCaptureCmPerSecond;
	}

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

	if (GVerbosePhase1Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_CERTIFICATION_AUDIT STRICT_AUDIT_V1 usedRelaxedCertification=%d usedTimeoutExtension=%d usedDwellShortcut=%d usedReanchorShortcut=%d usedShellOwnershipNarrowing=%d topology=%s simCount=%d proximalSimCount=%d distalSimCount=%d upperBodySimCount=%d shellReanchored=%d shellLocked=%d shellOffset=%.2f shellVelocity=%.2f"),
			Audit.bUsedRelaxedCertification ? 1 : 0,
			Audit.bUsedTimeoutExtension ? 1 : 0,
			Audit.bUsedDwellShortcut ? 1 : 0,
			Audit.bUsedReanchorShortcut ? 1 : 0,
			Audit.bUsedShellOwnershipNarrowing ? 1 : 0,
			*Audit.Topology,
			CertifiedHandoff.SimCount,
			Audit.ProximalSimCount,
			Audit.DistalSimCount,
			Audit.UpperBodySimCount,
			Audit.bShellReanchored ? 1 : 0,
			Audit.bShellLocked ? 1 : 0,
			Audit.ShellOffset,
			Audit.ShellVelocity);
	}

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
	ConsecutivePelvisNotSimulatingTicks = 0;
	ConsecutiveBodyMotionInstabilityTicks = 0;
	LateValidationAccumulatedSeconds = 0.0f;
	RootOnReadinessShellHoldAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofAccumulatedSeconds = 0.0f;
	RootOnReadinessShellProofStartOffsetCm = 0.0f;
	RootOnReadinessShellProofStartVelocityCmPerSecond = 0.0f;
	bHasRootOnReadinessShellProofBaseline = false;
	bHasLoggedDistalExperimentState = false;
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
	LoggedSuppressedDistalBones.Empty();
	LoggedProximalPromotions.Empty();
	LoggedProximalStates.Empty();
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
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_OWNERSHIP_FROZEN mode=LateValidationKinematicHold source=Phase1Contract"));
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

bool FPhysAnimBalanceReadyTransition::ShouldKeepBoneKinematic(FName BoneName, const FPhysAnimStabilizationSettings& Settings) const
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
		const bool bDistalKin = Settings.bPhase1DistalKinematicExperiment && BalanceTransitionSets::IsDistalLowerLimb(BoneName);
		return BalanceTransitionSets::IsPrepareCriticalKinematic(BoneName) ||
			BalanceTransitionSets::IsUpperBody(BoneName) ||
			bDistalKin;
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate)
	{
		const bool bDistalKin = Settings.bPhase1DistalKinematicExperiment && BalanceTransitionSets::IsDistalLowerLimb(BoneName);
		if (BalanceTransitionSets::IsRoot(BoneName) || bDistalKin)
		{
			return true;
		}

		if (Phase1TopologyRecord.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold)
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

