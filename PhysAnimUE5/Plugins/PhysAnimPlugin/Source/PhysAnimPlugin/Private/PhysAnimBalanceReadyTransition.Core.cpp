#include "PhysAnimBalanceReadyTransitionPrivate.h"

namespace
{
	struct FRootOnWarmStartMotionCache
	{
		FTransform PreviousPelvisTransform = FTransform::Identity;
		double PreviousWorldTimeSeconds = 0.0;
		FVector DerivedLinearVelocity = FVector::ZeroVector;
		FVector DerivedAngularVelocityRadians = FVector::ZeroVector;
		bool bHasPreviousSample = false;
		bool bCurrentSampleFromKinematicPelvis = false;
	};

	static TMap<TObjectPtr<UPhysAnimComponent>, FRootOnWarmStartMotionCache> GRootOnWarmStartMotionCache;

	static void ResetRootOnWarmStartMotionCache(UPhysAnimComponent* Owner)
	{
		if (!Owner)
		{
			return;
		}

		GRootOnWarmStartMotionCache.Remove(Owner);
	}

	static FVector DeriveAngularVelocityRadians(const FQuat& PreviousRotation, const FQuat& CurrentRotation, const double DeltaSeconds)
	{
		if (DeltaSeconds <= KINDA_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}

		FQuat DeltaRotation = CurrentRotation * PreviousRotation.Inverse();
		if (DeltaRotation.W < 0.0f)
		{
			DeltaRotation *= -1.0f;
		}
		DeltaRotation.Normalize();

		FVector Axis = FVector::UpVector;
		double AngleRadians = 0.0;
		DeltaRotation.ToAxisAndAngle(Axis, AngleRadians);
		if (!Axis.Normalize())
		{
			return FVector::ZeroVector;
		}

		return Axis * static_cast<float>(AngleRadians / DeltaSeconds);
	}

	static void UpdateRootOnWarmStartMotionCache(UPhysAnimComponent* Owner)
	{
		if (!Owner)
		{
			return;
		}

		USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
		const UWorld* const World = Owner->GetWorld();
		if (!Mesh || !World)
		{
			ResetRootOnWarmStartMotionCache(Owner);
			return;
		}

		const FName PelvisBoneName = PhysAnimBridge::GetRootBoneName();
		const FBodyInstance* const PelvisBody = Mesh->GetBodyInstance(PelvisBoneName);
		const bool bHasValidPelvisBody = PelvisBody && PelvisBody->IsValidBodyInstance();
		const FTransform CurrentPelvisTransform = bHasValidPelvisBody
			? PelvisBody->GetUnrealWorldTransform()
			: Mesh->GetBoneTransform(Mesh->GetBoneIndex(PelvisBoneName));
		const bool bPelvisKinematic = !bHasValidPelvisBody || !PelvisBody->IsInstanceSimulatingPhysics();
		const double CurrentWorldTimeSeconds = World->GetTimeSeconds();

		FRootOnWarmStartMotionCache& Cache = GRootOnWarmStartMotionCache.FindOrAdd(Owner);
		if (Cache.bHasPreviousSample)
		{
			const double DeltaSeconds = CurrentWorldTimeSeconds - Cache.PreviousWorldTimeSeconds;
			if (DeltaSeconds > KINDA_SMALL_NUMBER)
			{
				Cache.DerivedLinearVelocity =
					(CurrentPelvisTransform.GetLocation() - Cache.PreviousPelvisTransform.GetLocation()) /
					static_cast<float>(DeltaSeconds);
				Cache.DerivedAngularVelocityRadians = DeriveAngularVelocityRadians(
					Cache.PreviousPelvisTransform.GetRotation(),
					CurrentPelvisTransform.GetRotation(),
					DeltaSeconds);
			}
			else
			{
				Cache.DerivedLinearVelocity = FVector::ZeroVector;
				Cache.DerivedAngularVelocityRadians = FVector::ZeroVector;
			}
		}
		else
		{
			Cache.DerivedLinearVelocity = FVector::ZeroVector;
			Cache.DerivedAngularVelocityRadians = FVector::ZeroVector;
		}

		Cache.PreviousPelvisTransform = CurrentPelvisTransform;
		Cache.PreviousWorldTimeSeconds = CurrentWorldTimeSeconds;
		Cache.bHasPreviousSample = true;
		Cache.bCurrentSampleFromKinematicPelvis = bPelvisKinematic;
	}
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
	ResetTransitionLocalState(Owner);
	bLatchedPelvisResetApplied = false;
	QuietHandoffCount = 0;
	HipQuarantineTimerSeconds = 0.0f;
	Owner->LastPhase1PelvisCouplingRotationForensics = {};
	LateValidationAccumulatedSeconds = 0.0f;
	LastLateValidateBlockReason.Reset();
	bLoggedLateValidateEntry = false;
	bLoggedPhase1UpperBodyAudit = false;
	bLoggedPhase2EntryAudit = false;
	bLoggedPhase2FirstFailureAudit = false;
	bLoggedPhase3EntryAudit = false;
	bLoggedPhase3FirstFailureAudit = false;
	bLoggedPhase2ReadyForPhase3 = false;
	bLoggedDirectPelvisLinkForensics = false;
	EntryHoldRotations.Empty();
	DistalBoneMismatchTicks.Empty();
	DistalBoneConsecutiveMismatchTicks.Empty();
	DistalBonePersistentTicks.Empty();
	DistalMismatchesTransientCount = 0;
	DistalMismatchesPersistentCount = 0;
	DistalMismatchesPendingCount = 0;
	Owner->LastDistalClassification.Empty();
	SafePhase2DenialReason.Reset();
	ResetRootOnWarmStartMotionCache(Owner);

	if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
	{
		for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
		{
			EntryHoldRotations.Add(BoneName, Mesh->GetBoneQuaternion(BoneName, EBoneSpaces::WorldSpace));
		}
	}

	{
		FString SeedError;
		if (!Owner->SeedControlTargetsFromCurrentPose(0.0f, SeedError))
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_ENTRY_CONTROL_RESEED_FAILED reason=%s"), *SeedError);
		}
		else
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_ENTRY_CONTROL_RESEEDED source=current_pose"));
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


void FPhysAnimBalanceReadyTransition::Cancel(UPhysAnimComponent* Owner)
{
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive)
	{
		ResetRootOnWarmStartMotionCache(Owner);
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

	UpdateRootOnWarmStartMotionCache(Owner);

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
							LoggedProximalPromotions.Add(BoneName);
						}
					}
					else if (LoggedProximalPromotions.Contains(BoneName) && !LoggedProximalStates.Contains(BoneName))
					{
						LoggedProximalStates.Add(BoneName);
					}
				}
			}
		}
	}

	PhaseTimeSeconds += DeltaTime;
	TotalTransitionTimeSeconds += DeltaTime;

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		Phase2GuardTickCount++;
		ReconcileKinematicHoldSet(Owner, Settings);
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		Phase3GuardTickCount++;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		const bool bKineticGateActiveNow = Owner->bKineticGateActiveLastFrame;
		if (bKineticGateActiveNow)
		{
			Phase3KineticGateReleaseTickCount = 0;
		}
		else
		{
			Phase3KineticGateReleaseTickCount++;
		}
		bLastKineticGateActive = bKineticGateActiveNow;
	}
	else
	{
		// Ensure counter is out-of-grace when not in Phase 3
		Phase3KineticGateReleaseTickCount = 999;
	}
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
	
					const bool bContributedToFirstFailure = (Diagnostics.FailureReason.Contains(TEXT("distal")) || 
						Diagnostics.FailureReason.Contains(TEXT("topology")));
					if (bContributedToFirstFailure)
					{
						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] DISTAL_EXPERIMENT_STATE bone=%s intended=%s actual=%s changedByLaterSubsystem=%d"),
							*BoneName.ToString(),
							UPhysAnimComponent::GetPhysicsMovementTypeName(bIntendedKinematic ? EPhysicsMovementType::Kinematic : EPhysicsMovementType::Simulated),
							bActualSimulating ? TEXT("Simulating") : TEXT("Kinematic"),
							bStateMismatch);
					}
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
				: TerminalQuietBlockReason;
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
						const bool bFirstLateValidateFrame = !bLoggedPhase1UpperBodyAudit;
						float MaxAuditTargetDelta = 0.0f;
						FName MaxAuditTargetDeltaBone = NAME_None;

						if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate)
						{

							const FName AuditBones[] = { 
								TEXT("neck_01"), TEXT("head"), 
								TEXT("clavicle_l"), TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
								TEXT("clavicle_r"), TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r") 
							};

							// Pre-calculate instability for gating
							for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
							{
								if (!BalanceTransitionSets::IsUpperBody(BoneName)) { continue; }
								const FBodyInstance* BodyInst = Mesh->GetBodyInstance(BoneName);
								if (!BodyInst || !BodyInst->IsValidBodyInstance()) { continue; }

								const bool bRawSimViolation = BodyInst->IsInstanceSimulatingPhysics();
								const float LinVel = BodyInst->GetUnrealWorldVelocity().Size();
								const float AngVel = FMath::RadiansToDegrees(BodyInst->GetUnrealWorldAngularVelocityInRadians().Size());
								const bool bMotionViolation = LinVel > 1.0f || AngVel > 10.0f;
								const bool bPendingResetViolation = Owner->GetPendingBodyModifierCachedResetNames().Contains(PhysAnimBridge::MakeBodyModifierName(BoneName));

								if (IsLateValidationUpperBodyViolation(bRawSimViolation, bMotionViolation, bPendingResetViolation))
								{
									bUpperBodyInstability = true;
									break;
								}
							}

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

								const bool bIsUpperBodyFailureFrame = (bUpperBodyInstability && InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate);
								
								const bool bShouldEmitAudit = bFirstLateValidateFrame || bUpperBodyInstability;

								if (bShouldEmitAudit)
								{
									const FPhysicsBodyModifierRecord* ModifierRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(Owner->PhysicsControlComponent.Get(), PhysAnimBridge::MakeBodyModifierName(AuditBone));
									const EPhysicsMovementType ModifierType = ModifierRecord ? ModifierRecord->BodyModifier.ModifierData.MovementType : EPhysicsMovementType::Simulated;
									const bool bHeldTargetWritten = HoldRot != nullptr;
									const float LinVel = AuditBody->GetUnrealWorldVelocity().Size();
									const float AngVel = FMath::RadiansToDegrees(AuditBody->GetUnrealWorldAngularVelocityInRadians().Size());
									const bool bPendingReset = Owner->GetPendingBodyModifierCachedResetNames().Contains(PhysAnimBridge::MakeBodyModifierName(AuditBone));
									const bool bInAllowlist = BalanceTransitionSets::IsUpperBody(AuditBone);

								}
						}

						const bool bShouldEmitSummary = bFirstLateValidateFrame || bUpperBodyInstability;

						if (bShouldEmitSummary)
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
							const bool bMotionViolation = LinVel > 1.0f || AngVel > 10.0f;
							const bool bPendingResetViolation = bHasPendingReset;
							const bool bTargetDeltaViolation = TargetDeltaDeg > 0.1f;

							if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate &&
								Phase1TopologyRecord.UpperBodyOwnershipMode == EBalanceReadyUpperBodyOwnershipMode::LateValidationKinematicHold &&
								bTargetDeltaViolation && !IsLateValidationUpperBodyViolation(bRawSimViolation, bMotionViolation, bPendingResetViolation) &&
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
							}

							if (IsLateValidationUpperBodyViolation(bRawSimViolation, bMotionViolation, bPendingResetViolation))
							{
								bUpperBodyInstability = true;

								if (true)
								{
									static int32 LastSummaryFrame = -1;
									const bool bShouldEmitFailure = bFirstLateValidateFrame || bUpperBodyInstability;

									if (bShouldEmitFailure)
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

										FString Reason;
										if (bRawSimViolation) Reason += TEXT("raw_sim_violation ");
										if (bMotionViolation) Reason += TEXT("motion_violation ");
										if (bPendingResetViolation) Reason += TEXT("pending_reset_violation ");

										if (bShouldEmitFailure)
										{
											UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_UPPER_BODY_GATE bone=%s reason=%s targetDelta=%.2f linVel=%.2f angVel=%.2f"),
												*BoneName.ToString(), *Reason.TrimEnd(), TargetDeltaDeg, LinVel, AngVel);
										}
									}
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

		const bool bLateValidationDurationSatisfied =
			LateValidationAccumulatedSeconds + KINDA_SMALL_NUMBER >= Settings.BalancePhase1LateValidateRequiredSeconds;
		const bool bNoCouplingRootBoundsSatisfied =
			Diagnostics.RootSpeed <= Settings.BalancePhase2EntryMaxRootLinearSpeed &&
			Diagnostics.RootAngularSpeed <= Settings.BalancePhase2EntryMaxRootAngularSpeed;
		const bool bNoCouplingProofGateEligible =
			bLateValidationThisFrame &&
			bCurrentSnapshotValid &&
			bLiveSnapshotValid &&
			!bUpperBodyInstability &&
			!bSimCoverageRegressed &&
			!bLateValidateTargetDiscontinuity &&
			bLateValidationDurationSatisfied &&
			bNoCouplingRootBoundsSatisfied &&
			CurrentResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady &&
			CurrentResult.bRootOnReadinessShellHoldSatisfied &&
			CurrentResult.bRootOnReadinessFinalBringUpControlSettled &&
			CurrentResult.bRootOnReadinessPolicyInfluenceSettled &&
			CurrentResult.bPreRootOnShellSafetyProofSatisfied &&
			CurrentResult.bRootOnDirectPelvisLinkGeometrySatisfied &&
			CurrentResult.bRootOnDirectPelvisLinkAngularSatisfied &&
			CurrentResult.PelvisThighLAngularErrorDeg <=
				BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg + KINDA_SMALL_NUMBER &&
			CurrentResult.PelvisThighRAngularErrorDeg <=
				BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg + KINDA_SMALL_NUMBER &&
			CurrentResult.PelvisSpine01AngularErrorDeg <=
				BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg + KINDA_SMALL_NUMBER;

		const auto EmitNoCouplingProofLog = [&](const TCHAR* State, const FString& Reason)
		{
			UE_LOG(
				LogPhysAnimBridge,
				Log,
				TEXT("[PhysAnimBalance] PHASE1_ROOT_ON_NO_COUPLING_PROOF state=%s reason=%s duration=%.2f required=%.2f maxBodyLinearSpeed=%.2f maxBodyAngularSpeed=%.2f worstBone=%s pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f"),
				State,
				Reason.IsEmpty() ? TEXT("none") : *Reason,
				RootOnReadinessNoCouplingProofAccumulatedSeconds,
				Settings.BalancePhase2RequiredShellHoldDuration,
				RootOnReadinessNoCouplingPeakBodyLinearSpeed,
				RootOnReadinessNoCouplingPeakBodyAngularSpeed,
				*RootOnReadinessNoCouplingWorstBone.ToString(),
				CurrentResult.PelvisThighLErrorCm,
				CurrentResult.PelvisThighRErrorCm,
				CurrentResult.PelvisSpine01ErrorCm);
		};

		const bool bNoCouplingProofSatisfiedBeforeTick =
			RootOnReadinessNoCouplingProofAccumulatedSeconds + KINDA_SMALL_NUMBER >= Settings.BalancePhase2RequiredShellHoldDuration;
		const bool bStartedNoCouplingProofThisFrame =
			!bNoCouplingProofSatisfiedBeforeTick &&
			!bPhase1RootOnReadinessNoCouplingProofActive &&
			bNoCouplingProofGateEligible;

		if (bStartedNoCouplingProofThisFrame)
		{
			ResetRootOnReadinessNoCouplingProofState();
			bPhase1RootOnReadinessNoCouplingProofActive = true;
			Diagnostics.Phase1RootOnReadinessGateReason = TEXT("phase1_root_on_readiness_requires_pelvis_coupling");
			EmitNoCouplingProofLog(TEXT("start"), TEXT("phase1_root_on_readiness_requires_pelvis_coupling"));
		}
		else if (bPhase1RootOnReadinessNoCouplingProofActive && !bNoCouplingProofGateEligible)
		{
			const FString ProofResetReason = !LateValidateBlockReason.IsEmpty()
				? LateValidateBlockReason
				: (!bCurrentSnapshotValid || !bLiveSnapshotValid
					? TEXT("phase1_late_validate_handoff_invalidated")
					: (!bLateValidationDurationSatisfied
						? TEXT("late_validate_duration_unsatisfied")
						: (!bNoCouplingRootBoundsSatisfied
							? TEXT("root_entry_bounds_regressed")
							: (CurrentResult.RootOnReadinessClassification != EBalanceReadyRootOnReadinessClassification::RootCoupledReady
								? (CurrentResult.RootOnReadinessGateReason.IsEmpty()
									? TEXT("phase1_root_on_readiness_topology_not_ready")
									: CurrentResult.RootOnReadinessGateReason)
								: TEXT("phase1_root_on_readiness_requires_pelvis_coupling")))));

			EmitNoCouplingProofLog(TEXT("reset"), ProofResetReason);
			ResetRootOnReadinessNoCouplingProofState();

			const FString DenialReason = TEXT("phase1_root_on_readiness_requires_pelvis_coupling");
			Diagnostics.FailureReason = DenialReason;
			Diagnostics.Phase1RootOnReadinessGateReason = DenialReason;
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s"), *DenialReason);
			Owner->ReleaseTransitionOwnedShellLock();
			MarkSafePhase2Denied(Owner, DenialReason);
			return;
		}

		if (bPhase1RootOnReadinessNoCouplingProofActive && bNoCouplingProofGateEligible && !bStartedNoCouplingProofThisFrame)
		{
			const float LinearThreshold = FMath::Max(Settings.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed, KINDA_SMALL_NUMBER);
			const float AngularThreshold = FMath::Max(Settings.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed, KINDA_SMALL_NUMBER);
			const float LinearRatio = CachedConvergenceSnapshot.MaxBodyLinearSpeed / LinearThreshold;
			const float AngularRatio = CachedConvergenceSnapshot.MaxBodyAngularSpeed / AngularThreshold;

			RootOnReadinessNoCouplingProofAccumulatedSeconds += DeltaTime;
			if (CachedConvergenceSnapshot.MaxBodyLinearSpeed > RootOnReadinessNoCouplingPeakBodyLinearSpeed)
			{
				RootOnReadinessNoCouplingPeakBodyLinearSpeed = CachedConvergenceSnapshot.MaxBodyLinearSpeed;
				if (LinearRatio >= AngularRatio && CachedConvergenceSnapshot.MaxBodyLinearSpeedBone != NAME_None)
				{
					RootOnReadinessNoCouplingWorstBone = CachedConvergenceSnapshot.MaxBodyLinearSpeedBone;
				}
			}
			if (CachedConvergenceSnapshot.MaxBodyAngularSpeed > RootOnReadinessNoCouplingPeakBodyAngularSpeed)
			{
				RootOnReadinessNoCouplingPeakBodyAngularSpeed = CachedConvergenceSnapshot.MaxBodyAngularSpeed;
				if (AngularRatio > LinearRatio && CachedConvergenceSnapshot.MaxBodyAngularSpeedBone != NAME_None)
				{
					RootOnReadinessNoCouplingWorstBone = CachedConvergenceSnapshot.MaxBodyAngularSpeedBone;
				}
			}
			if (RootOnReadinessNoCouplingWorstBone == NAME_None)
			{
				RootOnReadinessNoCouplingWorstBone =
					AngularRatio > LinearRatio
						? CachedConvergenceSnapshot.MaxBodyAngularSpeedBone
						: CachedConvergenceSnapshot.MaxBodyLinearSpeedBone;
			}

			const bool bNoCouplingProofSatisfiedThisFrame =
				RootOnReadinessNoCouplingProofAccumulatedSeconds + KINDA_SMALL_NUMBER >= Settings.BalancePhase2RequiredShellHoldDuration;
			CurrentResult.bRootOnReadinessNoCouplingProofSatisfied = bNoCouplingProofSatisfiedThisFrame;
			CurrentResult.bRootOnReadinessProven =
				CurrentResult.bRootOnReadinessShellHoldSatisfied &&
				CurrentResult.bRootOnReadinessFinalBringUpControlSettled &&
				CurrentResult.bRootOnReadinessPolicyInfluenceSettled &&
				CurrentResult.bPreRootOnShellSafetyProofSatisfied &&
				CurrentResult.bRootOnReadinessNoCouplingProofSatisfied &&
				CurrentResult.bRootOnDirectPelvisLinkAngularSatisfied &&
				CurrentResult.PelvisThighLAngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg &&
				CurrentResult.PelvisThighRAngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg &&
				CurrentResult.PelvisSpine01AngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg &&
				CurrentResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady;
			const bool bDirectPelvisThighMarginsSatisfied =
				CurrentResult.PelvisThighLAngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg &&
				CurrentResult.PelvisThighRAngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisThighDirectLinkAngularErrorDeg;
			const bool bDirectPelvisSpineMarginSatisfied =
				CurrentResult.PelvisSpine01AngularErrorDeg <= BalanceTransitionSets::Phase2MaxRootOnReadinessPelvisSpineDirectLinkAngularErrorDeg;
			const bool bTiltLimitedViability =
				IsPhase1TiltLimitedRootOnViability(Owner->LastPhase1PelvisCouplingRotationForensics, Settings);
			CurrentResult.RootOnReadinessGateReason = ResolveRootOnReadinessGateReason(
				CurrentResult.RootOnReadinessClassification,
				CurrentResult.bRootOnDirectPelvisLinkGeometrySatisfied,
				CurrentResult.bRootOnDirectPelvisLinkAngularSatisfied,
				bDirectPelvisThighMarginsSatisfied,
				bDirectPelvisSpineMarginSatisfied,
				CurrentResult.bRootOnReadinessShellHoldSatisfied,
				CurrentResult.bRootOnReadinessFinalBringUpControlSettled,
				CurrentResult.bRootOnReadinessPolicyInfluenceSettled,
				CurrentResult.bPreRootOnShellSafetyProofSatisfied,
				CurrentResult.bRootOnReadinessNoCouplingProofSatisfied,
				bTiltLimitedViability,
				CurrentResult.bRootOnReadinessPolicyInfluenceSettled ? 1.0f : 0.0f);
			Diagnostics.Phase1RootOnReadinessGateReason = CurrentResult.RootOnReadinessGateReason;
			EmitNoCouplingProofLog(
				bNoCouplingProofSatisfiedThisFrame ? TEXT("satisfied") : TEXT("progress"),
				CurrentResult.RootOnReadinessGateReason);
		}
		else if (bNoCouplingProofGateEligible && !bNoCouplingProofSatisfiedBeforeTick)
		{
			CurrentResult.bRootOnReadinessNoCouplingProofSatisfied = false;
			CurrentResult.RootOnReadinessGateReason = TEXT("phase1_root_on_readiness_requires_pelvis_coupling");
			Diagnostics.Phase1RootOnReadinessGateReason = CurrentResult.RootOnReadinessGateReason;
		}

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

			Diagnostics.Phase1RootOnReadinessGateReason = CurrentResult.RootOnReadinessGateReason.IsEmpty()
				? (bIsRootCoupledTopology && CurrentResult.bRootOnReadinessProven
					? TEXT("ready")
					: bIsUpperOnlyTopology
						? TEXT("phase1_root_on_readiness_upper_only_safe_deny_pending")
						: TEXT("phase1_root_on_readiness_shell_hold_pending"))
				: CurrentResult.RootOnReadinessGateReason;
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
					const bool bTiltLimitedViability =
						(CertifiedLateValidationResult.RootOnReadinessGateReason == TEXT("phase1_root_on_readiness_pelvis_angular_incoherent") ||
						 CertifiedLateValidationResult.RootOnReadinessGateReason == TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")) &&
						IsPhase1TiltLimitedRootOnViability(Owner->LastPhase1PelvisCouplingRotationForensics, Settings);
					const FString ResolvedRootOnDenialReason = bTiltLimitedViability
						? TEXT("phase1_root_on_readiness_tilt_limited_viability")
						: CertifiedLateValidationResult.RootOnReadinessGateReason;
					if (!CertifiedLateValidationResult.bRootOnReadinessProven &&
						(ResolvedRootOnDenialReason == TEXT("phase1_root_on_readiness_pelvis_angular_incoherent") ||
						 ResolvedRootOnDenialReason == TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient") ||
						 ResolvedRootOnDenialReason == TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient") ||
						 ResolvedRootOnDenialReason == TEXT("phase1_root_on_readiness_tilt_limited_viability")))
					{
						const FString DenialReason = ResolvedRootOnDenialReason;
						Diagnostics.FailureReason = DenialReason;
						Diagnostics.Phase1RootOnReadinessGateReason = DenialReason;
						if (DenialReason == TEXT("phase1_root_on_readiness_tilt_limited_viability"))
						{
							UE_LOG(
								LogPhysAnimBridge,
								Warning,
								TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f pelvisThighLAngular=%.2f pelvisThighRAngular=%.2f pelvisSpine01Angular=%.2f requiredTiltDeg=%.2f unconstrainedTiltDeg=%.2f unconstrainedPelvisThighLAngular=%.2f unconstrainedPelvisThighRAngular=%.2f unconstrainedPelvisSpine01Angular=%.2f"),
								*DenialReason,
								CertifiedLateValidationResult.PelvisThighLErrorCm,
								CertifiedLateValidationResult.PelvisThighRErrorCm,
								CertifiedLateValidationResult.PelvisSpine01ErrorCm,
								CertifiedLateValidationResult.PelvisThighLAngularErrorDeg,
								CertifiedLateValidationResult.PelvisThighRAngularErrorDeg,
								CertifiedLateValidationResult.PelvisSpine01AngularErrorDeg,
								Settings.BalancePhase2EntryMaxRootTiltDeg,
								CertifiedHandoff.RootOnReadinessUnconstrainedTiltDeg,
								CertifiedHandoff.RootOnReadinessUnconstrainedPelvisThighLAngularErrorDeg,
								CertifiedHandoff.RootOnReadinessUnconstrainedPelvisThighRAngularErrorDeg,
								CertifiedHandoff.RootOnReadinessUnconstrainedPelvisSpine01AngularErrorDeg);
						}
						else
						{
							UE_LOG(
								LogPhysAnimBridge,
								Warning,
								TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f pelvisThighLAngular=%.2f pelvisThighRAngular=%.2f pelvisSpine01Angular=%.2f"),
								*DenialReason,
								CertifiedLateValidationResult.PelvisThighLErrorCm,
								CertifiedLateValidationResult.PelvisThighRErrorCm,
								CertifiedLateValidationResult.PelvisSpine01ErrorCm,
								CertifiedLateValidationResult.PelvisThighLAngularErrorDeg,
								CertifiedLateValidationResult.PelvisThighRAngularErrorDeg,
								CertifiedLateValidationResult.PelvisSpine01AngularErrorDeg);
						}
						Owner->ReleaseTransitionOwnedShellLock();
						MarkSafePhase2Denied(Owner, DenialReason);
						return;
					}

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
						CertifiedLateValidationResult.RootOnReadinessGateReason.IsEmpty()
							? (CertifiedLateValidationResult.RootOnReadinessClassification == EBalanceReadyRootOnReadinessClassification::RootCoupledReady
								? TEXT("ready")
								: Diagnostics.Phase1RootOnReadinessGateReason)
							: CertifiedLateValidationResult.RootOnReadinessGateReason;
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

		if (TotalTransitionTimeSeconds > (GStrictPhase1Certification != 0 ? 5.0f : Settings.BalancePhase1TimeoutDuration))
		{
			if (TotalTransitionTimeSeconds > 5.0f + KINDA_SMALL_NUMBER)
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
			if (!bRootOnReadinessProven)
			{
				UnsatisfiedGates.Add(
					CurrentResult.RootOnReadinessGateReason.IsEmpty()
						? TEXT("ready_proof_failed")
						: CurrentResult.RootOnReadinessGateReason);
			}

			const FString PrimaryReason = UnsatisfiedGates.Num() > 0 ? UnsatisfiedGates[0] : TEXT("unknown");
			const FString SecondaryReason = UnsatisfiedGates.Num() > 1 ? UnsatisfiedGates[1] : TEXT("none");

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_CONVERGENCE_REPORT upperBodyHold=%d simCoverage=%d targetContinuity=%d quietProof=%d bodyMotion=%d rootValidity=%d expectedRelease=%d readyProven=%d shellHold=%d bringUp=%d policyInfl=%d shellSafety=%d rootOnGate=%s pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f lateValidateSeconds=%.2f/%.2f shellHoldSeconds=%.2f/%.2f quietProofSeconds=%.2f/%.2f shellProofSeconds=%.2f/%.2f"),
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
				*CurrentResult.RootOnReadinessGateReason,
				CurrentResult.PelvisThighLErrorCm,
				CurrentResult.PelvisThighRErrorCm,
				CurrentResult.PelvisSpine01ErrorCm,
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

			if (!bPreRootOnShellSafetyProofSatisfied && PrimaryReason.Equals(TEXT("shell_safety_proof_unsatisfied")))
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

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_LATE_VALIDATE_NONCONVERGENCE primary=%s secondary=%s"), *PrimaryReason, *SecondaryReason);
			LateValidateBlockReason = PrimaryReason;

			// DISTAL_FORENSIC_REPORT
			bool bAnyDistalContribution = false;
			for (const FName DistalBoneName : PhysAnimBridge::GetControlledBoneNames())
			{
				if (BalanceTransitionSets::IsDistalLowerLimb(DistalBoneName))
				{
					const int32 PersistentTicks = DistalBonePersistentTicks.Contains(DistalBoneName) ? DistalBonePersistentTicks[DistalBoneName] : 0;
					const int32 ConsecutiveTicks = DistalBoneConsecutiveMismatchTicks.Contains(DistalBoneName) ? DistalBoneConsecutiveMismatchTicks[DistalBoneName] : 0;
					if (PersistentTicks > 0 || ConsecutiveTicks > 0)
					{
						bAnyDistalContribution = true;
						break;
					}
				}
			}

			if (bAnyDistalContribution)
			{
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
			}

			const FString TimeoutReason = LateValidateBlockReason.IsEmpty()
				? TEXT("phase1_no_convergence_path")
				: LateValidateBlockReason;
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
		if (Phase2GuardTickCount > 1)
		{
			bPhase2RootAuthorityQuarantined = false;
		}
		CaptureFlipDiagnostics(Owner);
		Diagnostics.bShellMaterialGuardSuppressed = false;
		const bool bPelvisRequestedSim = Owner->WasPelvisSimulatingLastFrame();
		const bool bPelvisActualSim = Owner->IsPelvisSimulatingNow();
		Diagnostics.bPhase2RequestedRootSim = bPelvisRequestedSim;
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
					UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_PRE_GUARD_PELVIS_STATE tick=%d rawSim=%d modMoveType=%d shellLocked=%d quarantined=%d state=%d owner=%d actor=%s component=%s"),
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
				Log,
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
		Diagnostics.PeakMaxNonRootBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxNonRootBodyAngularSpeed, Diagnostics.MaxAngVelThighs);
		Diagnostics.PeakMaxNonRootBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxNonRootBodyAngularSpeed, Diagnostics.MaxAngVelSpine);
		Diagnostics.PeakMaxNonRootBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxNonRootBodyAngularSpeed, Diagnostics.MaxAngVelFeet);
		Diagnostics.PeakMaxThighBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxThighBodyAngularSpeed, Diagnostics.MaxAngVelThighs);
		Diagnostics.PeakMaxSpineBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxSpineBodyAngularSpeed, Diagnostics.MaxAngVelSpine);
		Diagnostics.PeakMaxFeetBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxFeetBodyAngularSpeed, Diagnostics.MaxAngVelFeet);
		Diagnostics.PeakTotalThighBodyAngularSpeed = FMath::Max(Diagnostics.PeakTotalThighBodyAngularSpeed, Diagnostics.TotalAngVelThighs);
		Diagnostics.PeakTotalSpineBodyAngularSpeed = FMath::Max(Diagnostics.PeakTotalSpineBodyAngularSpeed, Diagnostics.TotalAngVelSpine);
		Diagnostics.PeakTotalFeetBodyAngularSpeed = FMath::Max(Diagnostics.PeakTotalFeetBodyAngularSpeed, Diagnostics.TotalAngVelFeet);

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
			IsMaterialShellCorrectionActive(
				IsPhase2ShellCorrectionOwnerActive(
					Owner->HasExplicitTransitionOwnedShellLock(),
					Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle),
				Diagnostics.BaselineShellOffset,
				Diagnostics.BaselineShellVel,
				Settings.BalancePhase2AbortShellOffsetDelta,
				Settings.BalancePhase2AbortShellVelocityDelta))
		{
			const bool bSuppressionConditionsMet = bPelvisActualSim && Diagnostics.SimCountPost >= 6;
			if (bSuppressionConditionsMet)
			{
				static int32 LastSuppressionFrame = -1;
				if (LastSuppressionFrame != GFrameCounter)
				{
					LastSuppressionFrame = GFrameCounter;
						Diagnostics.bShellMaterialGuardSuppressed = true;
						if (!bLoggedPhase2ReadyForPhase3)
						{
							UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_SHELL_MATERIAL_GUARD_SUPPRESSED frame=%d tick=%d shellOffsetDelta=%.2f shellVelocityDelta=%.2f rootActualSim=%d simCountPost=%d shellLocked=%d shellReanchored=%d owner=%d actor=%s component=%s"),
							GFrameCounter, Phase2GuardTickCount, Diagnostics.BaselineShellOffset, Diagnostics.BaselineShellVel,
							bPelvisActualSim ? 1 : 0, Diagnostics.SimCountPost,
							CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
							CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
							static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase2_shell_correction_material"))),
							*Owner->GetOwner()->GetName(), *Owner->GetName());
						}
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
		else if (!ValidatePhase2Continuity(Owner, Settings, AbortReason))
		{
			AbortDetail = FString::Printf(
				TEXT("requestedRootSim=%d actualRootSim=%d Phase2GuardTickCount=%d simCountPost=%d"),
				bPelvisRequestedSim ? 1 : 0,
				bPelvisActualSim ? 1 : 0,
				Phase2GuardTickCount,
				Diagnostics.SimCountPost);
		}
		else if (Owner->IsInstabilityPrecursorActive())
		{
			AbortReason = BalanceReadinessReasons::Phase2RootOnSpike;
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
			(IsMaterialShellCorrectionActive(
				IsPhase2ShellCorrectionOwnerActive(
					Owner->HasExplicitTransitionOwnedShellLock(),
					Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle),
				Diagnostics.BaselineShellOffset,
				Diagnostics.BaselineShellVel,
				Settings.BalancePhase2AbortShellOffsetDelta,
				Settings.BalancePhase2AbortShellVelocityDelta) && !(bPelvisActualSim && Diagnostics.SimCountPost >= 6)) ||
			Diagnostics.PeakMaxBodyLinearSpeed > Settings.BalancePhase2AbortMaxBodyLinearSpeed ||
			Diagnostics.PeakMaxBodyAngularSpeed > Settings.BalancePhase2AbortMaxBodyAngularSpeed)
		{
			FName WorstLinearBone = NAME_None;
			float WorstLinearSpeed = 0.0f;
			FName WorstAngularBone = NAME_None;
			float WorstAngularSpeed = 0.0f;
			FName LateLoopWorstBone = NAME_None;
			float LateLoopWorstBoneLinearSpeed = -1.0f;
			FName LateLoopArmWorstBone = NAME_None;
			float LateLoopArmWorstBoneLinearSpeed = -1.0f;

			if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
			{
				for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
				{
					const float BoneLinearSpeed = Mesh->GetPhysicsLinearVelocity(BoneName).Size();
					if (BoneLinearSpeed > WorstLinearSpeed)
					{
						WorstLinearSpeed = BoneLinearSpeed;
						WorstLinearBone = BoneName;
					}

					const float BoneAngularSpeed = Mesh->GetPhysicsAngularVelocityInDegrees(BoneName).Size();
					if (BoneAngularSpeed > WorstAngularSpeed)
					{
						WorstAngularSpeed = BoneAngularSpeed;
						WorstAngularBone = BoneName;
					}
				}

				static const FName HighForensicBones[] = { 
					TEXT("head"), TEXT("neck_01"), TEXT("clavicle_l"), TEXT("clavicle_r"),
					TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
					TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r")
				};
				for (const FName& BoneName : HighForensicBones)
				{
					const float BoneLinearSpeed = Mesh->GetPhysicsLinearVelocity(BoneName).Size();
					if (BoneLinearSpeed > LateLoopWorstBoneLinearSpeed)
					{
						LateLoopWorstBoneLinearSpeed = BoneLinearSpeed;
						LateLoopWorstBone = BoneName;
					}
				}

				static const FName ArmForensicBones[] = {
					TEXT("upperarm_l"), TEXT("lowerarm_l"), TEXT("hand_l"),
					TEXT("upperarm_r"), TEXT("lowerarm_r"), TEXT("hand_r")
				};
				for (const FName& BoneName : ArmForensicBones)
				{
					const float BoneLinearSpeed = Mesh->GetPhysicsLinearVelocity(BoneName).Size();
					if (BoneLinearSpeed > LateLoopArmWorstBoneLinearSpeed)
					{
						LateLoopArmWorstBoneLinearSpeed = BoneLinearSpeed;
						LateLoopArmWorstBone = BoneName;
					}
				}
			}

			const bool bLinearSpike =
				Diagnostics.RootSpeed > Settings.BalancePhase2AbortRootLinearSpeed ||
				Diagnostics.PeakMaxBodyLinearSpeed > Settings.BalancePhase2AbortMaxBodyLinearSpeed;
			const bool bAngularSpike =
				Diagnostics.RootAngularSpeed > Settings.BalancePhase2AbortRootAngularSpeed ||
				Diagnostics.PeakMaxBodyAngularSpeed > Settings.BalancePhase2AbortMaxBodyAngularSpeed;
			const FName WorstSpikeBone =
				Diagnostics.RootSpeed > Settings.BalancePhase2AbortRootLinearSpeed ||
				Diagnostics.RootAngularSpeed > Settings.BalancePhase2AbortRootAngularSpeed
					? PhysAnimBridge::GetRootBoneName()
					: (bLinearSpike && (!bAngularSpike || WorstLinearSpeed >= WorstAngularSpeed) ? WorstLinearBone : WorstAngularBone);
			
			if (Phase2GuardTickCount == 4)
			{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOTON_TICK4_SPIKE_SOURCE source=pre_guard_tick4 worstBone=%s maxLinearSpeed=%.2f maxAngularSpeed=%.2f"),
						*WorstSpikeBone.ToString(), Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.PeakMaxBodyAngularSpeed);

				if (Diagnostics.FirstContradictionSource.IsEmpty())
				{
					Diagnostics.FirstContradictionSource = TEXT("pre_guard_tick4");
					Diagnostics.LateLoopWorstBone = WorstSpikeBone;
				}
			}


			UE_LOG(
				LogPhysAnimBridge,
				Warning,
				TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_SPIKE_AUDIT frame=%d rootOnTick=%d maxBodyLinearSpeed=%.2f maxBodyAngularSpeed=%.2f worstBone=%s worstLinearSpeed=%.2f worstAngularSpeed=%.2f rootRawSim=%d pelvisModifier=%s totalSimCount=%d firstContradictionSource=%s firstLateLoopSource=%s lateLoopWorstBone=%s lateLoopArmWorstBone=%s"),
				static_cast<int32>(GFrameCounter),
				Phase2GuardTickCount,
				Diagnostics.PeakMaxBodyLinearSpeed,
				Diagnostics.PeakMaxBodyAngularSpeed,
				*WorstSpikeBone.ToString(),
				WorstLinearSpeed,
				WorstAngularSpeed,
				bPelvisActualSim ? 1 : 0,
				UPhysAnimComponent::GetPhysicsMovementTypeName(PelvisModifierMovementType),
				Diagnostics.SimCountPost,
				*Diagnostics.FirstContradictionSource,
				*Diagnostics.FirstLateLoopSource,
				*Diagnostics.LateLoopWorstBone.ToString(),
				*LateLoopArmWorstBone.ToString());

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
			else if (AbortReason == BalanceReadinessReasons::Phase2FailStopPrecursor) FailureType = TEXT("root_motion_spike");

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
				const bool bRetryBudgetAvailable = (Phase2RetryCount < Settings.BalancePhase2MaxAutomaticRetries);
				const bool bSafeDeniedOutcome = !IsAutomaticRetryAllowed(
					AbortReason,
					true,
					false,
					false,
					true,
					bRetryBudgetAvailable);
				if (bSafeDeniedOutcome)
				{
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_FIRST_FAILURE_AUDIT reason=%s type=%s bone=%s measured=%.2f threshold=%.2f state=%s rootRawSim=%d pelvisRawSim=%d pelvisModType=%s simCountPost=%d upperBodySimPost=%d policyAlpha=%.2f controlAlpha=%.2f shellLocked=%d shellReanchored=%d firstContradictionSource=%s owner=%d actor=%s component=%s"),
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
						*Diagnostics.FirstContradictionSource,
						static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
						*Owner->GetOwner()->GetName(), *Owner->GetName());
				}
				else
				{
					UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_FIRST_FAILURE_AUDIT reason=%s type=%s bone=%s measured=%.2f threshold=%.2f state=%s rootRawSim=%d pelvisRawSim=%d pelvisModType=%s simCountPost=%d upperBodySimPost=%d policyAlpha=%.2f controlAlpha=%.2f shellLocked=%d shellReanchored=%d firstContradictionSource=%s owner=%d actor=%s component=%s"),
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
						*Diagnostics.FirstContradictionSource,
						static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
						*Owner->GetOwner()->GetName(), *Owner->GetName());
				}
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
			const bool bSafeDeniedOutcome = (Diagnostics.LastRetryDecision == TEXT("denied"));
			if (bSafeDeniedOutcome)
			{
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
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
			}
			else
			{
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
			}

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
			if (bSafeDeniedOutcome)
			{
				MarkSafePhase2Denied(Owner, Diagnostics.FailureReason);
				return;
			}

			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
			return;
		}

		if (PhaseTimeSeconds > Settings.BalancePhase2GuardWindowDuration)
		{
			UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_READY_FOR_PHASE3"));
			bLoggedPhase2ReadyForPhase3 = true;
			SetPhase(EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3, Owner);

			if (!bLoggedPhase3EntryAudit)
			{
				USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
				FBodyInstance* const RootBI = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
				const bool bRootRawSim = RootBI ? RootBI->IsInstanceSimulatingPhysics() : false;
				
				const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
				const FPhysicsBodyModifierRecord* const PelvisRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(Owner->PhysicsControlComponent.Get(), PelvisModifierName);
				const EPhysicsMovementType PelvisModifierType = PelvisRecord ? PelvisRecord->BodyModifier.ModifierData.MovementType : EPhysicsMovementType::Static;

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_ENTRY_AUDIT frame=%d tick=%d rootRawSim=%d pelvisRawSim=%d pelvisModifierName=%s simCountPre=%d simCountPost=%d shellLocked=%d shellReanchored=%d owner=%d actor=%s component=%s"),
					static_cast<int32>(GFrameCounter),
					static_cast<int32>(Phase2GuardTickCount),
					bRootRawSim ? 1 : 0,
					bRootRawSim ? 1 : 0, // Pelvis is root
					PelvisModifierType == EPhysicsMovementType::Simulated ? TEXT("Simulated") : (PelvisModifierType == EPhysicsMovementType::Kinematic ? TEXT("Kinematic") : TEXT("Static")),
					Diagnostics.SimCountPre,
					Diagnostics.SimCountPost,
					CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
					CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
					static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Diagnostics.FailureReason)),
					*Owner->GetOwner()->GetName(),
					*Owner->GetName());
				bLoggedPhase3EntryAudit = true;
			}
		}
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3)
	{
		// Section 17.3.2 - Phase3 Entry Grace: seed counter to 0 so the first
		// Phase3KineticGateReleaseGraceTicks (5) ticks suppress shell/instability
		// aborts caused by the transient burst when root goes fully simulated.
		Phase3KineticGateReleaseTickCount = 0;
		SetPhase(EBalanceReadyTransitionPhase::BRT_Phase3_Settle, Owner);
		return;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		if (!bLoggedPhase3PreGuardRootState)
		{
			USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
			FBodyInstance* const RootBI = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
			const bool bRootRawSim = RootBI ? RootBI->IsInstanceSimulatingPhysics() : false;

			const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
			const FPhysicsBodyModifierRecord* const PelvisRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(Owner->PhysicsControlComponent.Get(), PelvisModifierName);
			const EPhysicsMovementType PelvisModifierType = PelvisRecord ? PelvisRecord->BodyModifier.ModifierData.MovementType : EPhysicsMovementType::Static;

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_PRE_GUARD_ROOT_STATE frame=%d rootRawSim=%d pelvisRawSim=%d pelvisModifierName=%s simCountPost=%d shellLocked=%d shellReanchored=%d owner=%d actor=%s component=%s"),
				static_cast<int32>(GFrameCounter),
				bRootRawSim ? 1 : 0,
				bRootRawSim ? 1 : 0,
				PelvisModifierType == EPhysicsMovementType::Simulated ? TEXT("Simulated") : (PelvisModifierType == EPhysicsMovementType::Kinematic ? TEXT("Kinematic") : TEXT("Static")),
				Diagnostics.SimCountPost,
				CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
				CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
				static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(TEXT("phase3_pre_guard_root_state"))),
				*Owner->GetOwner()->GetName(),
				*Owner->GetName());
			bLoggedPhase3PreGuardRootState = true;
		}

		FString Phase3Violation;
		if (!ValidatePhase3Continuity(Owner, Settings, Phase3Violation))
		{
			Diagnostics.FailureReason = Phase3Violation;
			if (!bLoggedPhase3FirstFailureAudit)
			{
				USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
				FBodyInstance* const RootBI = Mesh ? Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()) : nullptr;
				const bool bRootRawSim = RootBI ? RootBI->IsInstanceSimulatingPhysics() : false;
				const FVector RootLinearVelocity = RootBI ? RootBI->GetUnrealWorldVelocity() : FVector::ZeroVector;
				const FVector RootAngularVelocityDegPerSecond = RootBI
					? FMath::RadiansToDegrees(RootBI->GetUnrealWorldAngularVelocityInRadians())
					: FVector::ZeroVector;
				const AActor* const OwnerActor = Owner->GetOwner();
				const FVector EffectiveRootLinearVelocity =
					FPhysAnimBalanceReadyTransition::ResolvePhase3EffectiveRootLinearVelocityCmPerSecond(
						RootLinearVelocity,
						OwnerActor ? OwnerActor->GetVelocity() : FVector::ZeroVector,
						Owner->BridgeShellState.AppliedPlanarCorrectionVelocityCmPerSecond,
						Owner->HasExplicitTransitionOwnedShellLock());
				const float RootLinearSpeed = EffectiveRootLinearVelocity.Size();
				const float RootAngularSpeed = RootAngularVelocityDegPerSecond.Size();
				const float RootLinearThreshold = Settings.MaxRootLinearSpeedCmPerSecond * 2.5f;
				const float RootAngularThreshold = Settings.MaxRootAngularSpeedDegPerSecond * 3.0f;
				const float ShellOffsetDeltaCm = Owner->GetCurrentShellPlanarOffsetDeltaCm();
				const float ShellVelocityDeltaCmPerSecond = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
				const bool bShellCorrectionOwnerActive = FPhysAnimBalanceReadyTransition::IsPhase3ShellCorrectionOwnerActive(
					Owner->HasExplicitTransitionOwnedShellLock(),
					Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle);

				const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
				const FPhysicsBodyModifierRecord* const PelvisRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(Owner->PhysicsControlComponent.Get(), PelvisModifierName);
				const EPhysicsMovementType PelvisModifierType = PelvisRecord ? PelvisRecord->BodyModifier.ModifierData.MovementType : EPhysicsMovementType::Static;

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_FIRST_FAILURE_AUDIT frame=%d reason=%s tick=%d rootRawSim=%d pelvisRawSim=%d pelvisModifierName=%s simCountPost=%d shellLocked=%d shellReanchored=%d rootLinear=%.2f/%.2f rootAngular=%.2f/%.2f shellOffsetDelta=%.2f/%.2f shellVelocityDelta=%.2f/%.2f prePhase3PeakNonRootAngular=%.2f observedNonRootAngularEnvelope=%.2f currentMaxNonRootAngular=%.2f currentMaxNonRootAngularBone=%s observedNonRootFamilyAngularEnvelope=%.2f currentNonRootFamilyAngular=%.2f shellCorrectionActive=%d owner=%d actor=%s component=%s"),
					static_cast<int32>(GFrameCounter),
					*Phase3Violation,
					static_cast<int32>(Phase3GuardTickCount),
					bRootRawSim ? 1 : 0,
					bRootRawSim ? 1 : 0,
					PelvisModifierType == EPhysicsMovementType::Simulated ? TEXT("Simulated") : (PelvisModifierType == EPhysicsMovementType::Kinematic ? TEXT("Kinematic") : TEXT("Static")),
					Diagnostics.SimCountPost,
					CertifiedHandoff.bTransitionOwnedShellLocked ? 1 : 0,
					CertifiedHandoff.bTransitionShellReferenceReanchored ? 1 : 0,
					RootLinearSpeed,
					RootLinearThreshold,
					RootAngularSpeed,
					RootAngularThreshold,
					ShellOffsetDeltaCm,
					Settings.BalancePhase2AbortShellOffsetDelta,
					ShellVelocityDeltaCmPerSecond,
					Settings.BalancePhase2AbortShellVelocityDelta,
					Diagnostics.PeakMaxNonRootBodyAngularSpeed,
					Diagnostics.Phase3CurrentObservedNonRootAngularEnvelope,
					Diagnostics.Phase3CurrentMaxNonRootAngularSpeed,
					*Diagnostics.Phase3CurrentMaxNonRootAngularBone.ToString(),
					Diagnostics.Phase3CurrentObservedNonRootFamilyAngularEnvelope,
					Diagnostics.Phase3CurrentNonRootFamilyAngularSpeed,
					bShellCorrectionOwnerActive ? 1 : 0,
					static_cast<int32>(FPhysAnimBalanceReadyTransition::ClassifyConditionOwner(Phase3Violation)),
					*Owner->GetOwner()->GetName(),
					*Owner->GetName());
				bLoggedPhase3FirstFailureAudit = true;
			}
			if (!IsFailureClassRetryable(Phase3Violation))
			{
				MarkSafePhase2Denied(Owner, Phase3Violation);
				return;
			}
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
			if (!IsFailureClassRetryable(Diagnostics.FailureReason))
			{
				MarkSafePhase2Denied(Owner, Diagnostics.FailureReason);
				return;
			}
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

	PreviousPhase = InternalPhase;

	if (Owner &&
		(NewPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || NewPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate) &&
		PreviousPhase != NewPhase)
	{
		Owner->ActivateTransitionOwnedShellLock();
	}

	if (NewPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn && Owner)
	{
		const FPhysAnimStabilizationSettings EffectiveSettings = Owner->ResolveEffectiveStabilizationSettings();
		FString DenyReason;
		if (!ValidatePhase2EntryPreconditions(Owner, EffectiveSettings, DenyReason))
		{
			if ((DenyReason == TEXT("phase1_root_on_readiness_pelvis_angular_incoherent") ||
				 DenyReason == TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient") ||
				 DenyReason == TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient")) &&
				IsPhase1TiltLimitedRootOnViability(Owner->LastPhase1PelvisCouplingRotationForensics, EffectiveSettings))
			{
				DenyReason = TEXT("phase1_root_on_readiness_tilt_limited_viability");
			}
			const bool bTerminalPhase1ReadinessBlocker =
				DenyReason == TEXT("phase1_root_on_readiness_pelvis_angular_incoherent") ||
				DenyReason == TEXT("phase1_root_on_readiness_pelvis_thigh_margin_insufficient") ||
				DenyReason == TEXT("phase1_root_on_readiness_pelvis_spine_margin_insufficient") ||
				DenyReason == TEXT("phase1_root_on_readiness_tilt_limited_viability");
			if (IsPhase1OwnedCondition(DenyReason) && !bTerminalPhase1ReadinessBlocker)
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
		!ShouldRetainExplicitShellLockForPhase(NewPhase) &&
		PreviousPhase != EBalanceReadyTransitionPhase::BRT_Inactive)
	{
		Owner->ReleaseTransitionOwnedShellLock();
	}

	if (Owner &&
		PreviousPhase != EBalanceReadyTransitionPhase::BRT_Inactive &&
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

	InternalPhase = NewPhase;

	if (NewPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && Owner)
	{
		TArray<BalanceTransitionSets::FDirectPelvisLinkForensicRecord> ZeroSolverRecords;
		const USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
		const FName RootBone = PhysAnimBridge::GetRootBoneName();
		static const FName Bones[] = { TEXT("thigh_l"), TEXT("thigh_r"), TEXT("spine_01") };

		for (FName Bone : Bones)
		{
			BalanceTransitionSets::FDirectPelvisLinkForensicRecord Record;
			if (BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, RootBone, Bone, Record))
			{
				ZeroSolverRecords.Add(Record);
			}
		}
		BalanceTransitionSets::LogDirectPelvisLinkForensicRecords(ZeroSolverRecords, TEXT("PHASE1_RUNTIME_START"), true);
	}

	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE_ENTRY phase=%d"), static_cast<int32>(InternalPhase));
	PhaseTimeSeconds = 0.0f;
	StableHoldAccumulatedSeconds = 0.0f;
	TargetDiscontinuityAccumulatedSeconds = 0.0f;
	LastQuietBlockReason.Reset();
	Phase2GuardTickCount = 0;
	Phase3GuardTickCount = 0;
	ConsecutiveBodyMotionInstabilityTicks = 0;
	bPreviousFrameSettleEndRootRawSim = false;
	bPreviousFrameSettleEndPelvisRawSim = false;
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
				static const TPair<FName, FName> PreservedProximalLinks[] =
				{
					TPair<FName, FName>(PhysAnimBridge::GetRootBoneName(), TEXT("thigh_l")),
					TPair<FName, FName>(PhysAnimBridge::GetRootBoneName(), TEXT("thigh_r")),
					TPair<FName, FName>(PhysAnimBridge::GetRootBoneName(), TEXT("spine_01")),
					TPair<FName, FName>(TEXT("spine_01"), TEXT("spine_02")),
					TPair<FName, FName>(TEXT("spine_02"), TEXT("spine_03"))
				};
				for (const TPair<FName, FName>& Link : PreservedProximalLinks)
				{
					BalanceTransitionSets::FDirectPelvisLinkForensicRecord LinkRecord;
					BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, Link.Key, Link.Value, LinkRecord);
					UE_LOG(
						LogPhysAnimBridge,
						Warning,
						TEXT("[PhysAnimBalance] PHASE2_PROXIMAL_CHAIN_LINK_PRE link=%s_%s errorCm=%.2f bodyOriginDistanceCm=%.2f constraintFound=%d"),
						*Link.Key.ToString(),
						*Link.Value.ToString(),
						LinkRecord.bConstraintFound ? LinkRecord.AnchorDistanceCm : LinkRecord.BodyOriginDistanceCm,
						LinkRecord.BodyOriginDistanceCm,
						LinkRecord.bConstraintFound ? 1 : 0);
					if (LinkRecord.bConstraintFound)
					{
						UE_LOG(
							LogPhysAnimBridge,
							Warning,
							TEXT("[PhysAnimBalance] PHASE2_PROXIMAL_CHAIN_ANGULAR_PRE link=%s_%s angularErrorDeg=%.2f"),
							*Link.Key.ToString(),
							*Link.Value.ToString(),
							LinkRecord.ConstraintAngularErrorDeg);
					}
				}
				if (PelvisProximalConstraintErrorCm > BalanceTransitionSets::Phase2MaxPelvisProximalConstraintErrorCm)
				{
					Diagnostics.FailureReason = TEXT("phase2_constraint_error_too_high");
					SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
					return;
				}

				TArray<BalanceTransitionSets::FDirectPelvisLinkForensicRecord> DirectPelvisLinkForensics;
				DirectPelvisLinkForensics.Reserve(3);
				BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisThighLRecord;
				BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisThighRRecord;
				BalanceTransitionSets::FDirectPelvisLinkForensicRecord PelvisSpine01Record;
				BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, PhysAnimBridge::GetRootBoneName(), TEXT("thigh_l"), PelvisThighLRecord);
				BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, PhysAnimBridge::GetRootBoneName(), TEXT("thigh_r"), PelvisThighRRecord);
				BalanceTransitionSets::BuildDirectPelvisLinkForensicRecord(Mesh, PhysAnimBridge::GetRootBoneName(), TEXT("spine_01"), PelvisSpine01Record);
				DirectPelvisLinkForensics.Add(PelvisThighLRecord);
				DirectPelvisLinkForensics.Add(PelvisThighRRecord);
				DirectPelvisLinkForensics.Add(PelvisSpine01Record);
				const float PelvisThighLErrorCm = PelvisThighLRecord.bConstraintFound ? PelvisThighLRecord.AnchorDistanceCm : PelvisThighLRecord.BodyOriginDistanceCm;
				const float PelvisThighRErrorCm = PelvisThighRRecord.bConstraintFound ? PelvisThighRRecord.AnchorDistanceCm : PelvisThighRRecord.BodyOriginDistanceCm;
				const float PelvisSpine01ErrorCm = PelvisSpine01Record.bConstraintFound ? PelvisSpine01Record.AnchorDistanceCm : PelvisSpine01Record.BodyOriginDistanceCm;
				const bool bPelvisThighLAngularSatisfied =
					BalanceTransitionSets::IsPhase2WarmStartDirectLinkAngularSatisfied(PelvisThighLRecord);
				const bool bPelvisThighRAngularSatisfied =
					BalanceTransitionSets::IsPhase2WarmStartDirectLinkAngularSatisfied(PelvisThighRRecord);
				const bool bPelvisSpine01AngularSatisfied =
					BalanceTransitionSets::IsPhase2WarmStartDirectLinkAngularSatisfied(PelvisSpine01Record);
				for (const BalanceTransitionSets::FDirectPelvisLinkForensicRecord& Record : DirectPelvisLinkForensics)
				{
					const float AngularThresholdDeg = BalanceTransitionSets::GetPhase2WarmStartDirectLinkAngularThresholdDeg(Record);
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_LINK_ERROR_PRE link=%s errorCm=%.2f threshold=%.2f angularErrorDeg=%.2f authoredAngularFloorDeg=%.2f baselineCompensatedAngularErrorDeg=%.2f angularThreshold=%.2f"),
						*Record.LinkName,
						Record.bConstraintFound ? Record.AnchorDistanceCm : Record.BodyOriginDistanceCm,
						BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm,
						Record.ConstraintAngularErrorDeg,
						Record.AuthoredConstraintFrameAngularFloorDeg,
						Record.BaselineCompensatedConstraintAngularErrorDeg,
						AngularThresholdDeg);
				}
				if (PelvisThighLErrorCm > BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm ||
					PelvisThighRErrorCm > BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm ||
					PelvisSpine01ErrorCm > BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm)
				{
					TArray<BalanceTransitionSets::FDirectPelvisLinkForensicRecord> FailingLinkForensics;
					for (const BalanceTransitionSets::FDirectPelvisLinkForensicRecord& Record : DirectPelvisLinkForensics)
					{
						if (!Record.bConstraintFound ||
							Record.AnchorDistanceCm > BalanceTransitionSets::Phase2MaxDirectPelvisLinkErrorCm ||
							!BalanceTransitionSets::IsPhase2WarmStartDirectLinkAngularSatisfied(Record))
						{
							FailingLinkForensics.Add(Record);
						}
					}
					BalanceTransitionSets::LogDirectPelvisLinkForensicRecords(
						FailingLinkForensics,
						TEXT("PHASE2_ROOT_ON_LINK_FORENSIC"),
						true);
					const FString SafeDenyReason = TEXT("phase2_root_on_warm_start_incoherent");
					Diagnostics.FailureReason = SafeDenyReason;
					UE_LOG(
						LogPhysAnimBridge,
						Warning,
						TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED %s pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f pelvisThighLAngular=%.2f pelvisThighRAngular=%.2f pelvisSpine01Angular=%.2f pelvisThighLCompensatedAngular=%.2f pelvisThighRCompensatedAngular=%.2f pelvisSpine01CompensatedAngular=%.2f"),
						*SafeDenyReason,
						PelvisThighLErrorCm,
						PelvisThighRErrorCm,
						PelvisSpine01ErrorCm,
						PelvisThighLRecord.ConstraintAngularErrorDeg,
						PelvisThighRRecord.ConstraintAngularErrorDeg,
						PelvisSpine01Record.ConstraintAngularErrorDeg,
						PelvisThighLRecord.BaselineCompensatedConstraintAngularErrorDeg,
						PelvisThighRRecord.BaselineCompensatedConstraintAngularErrorDeg,
						PelvisSpine01Record.BaselineCompensatedConstraintAngularErrorDeg);
					Owner->ReleaseTransitionOwnedShellLock();
					MarkSafePhase2Denied(Owner, SafeDenyReason);
					return;
				}
				if (!bPelvisThighLAngularSatisfied ||
					!bPelvisThighRAngularSatisfied ||
					!bPelvisSpine01AngularSatisfied)
				{
					TArray<BalanceTransitionSets::FDirectPelvisLinkForensicRecord> AngularMismatchForensics;
					for (const BalanceTransitionSets::FDirectPelvisLinkForensicRecord& Record : DirectPelvisLinkForensics)
					{
						if (!BalanceTransitionSets::IsPhase2WarmStartDirectLinkAngularSatisfied(Record))
						{
							AngularMismatchForensics.Add(Record);
						}
					}
					BalanceTransitionSets::LogDirectPelvisLinkForensicRecords(
						AngularMismatchForensics,
						TEXT("PHASE2_ROOT_ON_LINK_ANGULAR_FORENSIC"),
						true);
					UE_LOG(
						LogPhysAnimBridge,
						Warning,
						TEXT("[PhysAnimBalance] PHASE2_SAFE_DENIED phase2_root_on_warm_start_incoherent pelvisThighL=%.2f pelvisThighR=%.2f pelvisSpine01=%.2f pelvisThighLAngular=%.2f pelvisThighRAngular=%.2f pelvisSpine01Angular=%.2f pelvisThighLCompensatedAngular=%.2f pelvisThighRCompensatedAngular=%.2f pelvisSpine01CompensatedAngular=%.2f"),
						PelvisThighLErrorCm,
						PelvisThighRErrorCm,
						PelvisSpine01ErrorCm,
						PelvisThighLRecord.ConstraintAngularErrorDeg,
						PelvisThighRRecord.ConstraintAngularErrorDeg,
						PelvisSpine01Record.ConstraintAngularErrorDeg,
						PelvisThighLRecord.BaselineCompensatedConstraintAngularErrorDeg,
						PelvisThighRRecord.BaselineCompensatedConstraintAngularErrorDeg,
						PelvisSpine01Record.BaselineCompensatedConstraintAngularErrorDeg);
					const FString SafeDenyReason = TEXT("phase2_root_on_warm_start_incoherent");
					Diagnostics.FailureReason = SafeDenyReason;
					Owner->ReleaseTransitionOwnedShellLock();
					MarkSafePhase2Denied(Owner, SafeDenyReason);
					return;
				}
				static const FName RootOnApplicationBones[] =
				{
					TEXT("pelvis"),
					TEXT("thigh_l"),
					TEXT("thigh_r"),
					TEXT("spine_01"),
					TEXT("spine_02"),
					TEXT("spine_03")
				};
				struct FRootOnVelocityReseedRecord
				{
					FName BoneName = NAME_None;
					FVector LinearVelocity = FVector::ZeroVector;
					FVector AngularVelocityRadians = FVector::ZeroVector;
					bool bWasSimulating = false;
				};
				TArray<FRootOnVelocityReseedRecord, TInlineAllocator<UE_ARRAY_COUNT(RootOnApplicationBones)>> RootOnVelocityReseedRecords;
				RootOnVelocityReseedRecords.Reserve(UE_ARRAY_COUNT(RootOnApplicationBones));
				FTransform LivePelvisTransform = PelvisBody->GetUnrealWorldTransform();
				FVector WarmStartLinearVelocity = FVector::ZeroVector;
				FVector WarmStartAngularVelocityRadians = FVector::ZeroVector;
				int32 WarmStartVelocitySamples = 0;
				const int32 WarmStartRotationSamples = 0;
				for (const FName BoneName : RootOnApplicationBones)
				{
					FRootOnVelocityReseedRecord& VelocityRecord = RootOnVelocityReseedRecords.AddDefaulted_GetRef();
					VelocityRecord.BoneName = BoneName;
					if (BoneName == PhysAnimBridge::GetRootBoneName())
					{
						continue;
					}

					if (const FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
					{
						if (BodyInstance->IsValidBodyInstance())
						{
							VelocityRecord.bWasSimulating = BodyInstance->IsInstanceSimulatingPhysics();
							VelocityRecord.LinearVelocity = BodyInstance->GetUnrealWorldVelocity();
							VelocityRecord.AngularVelocityRadians = BodyInstance->GetUnrealWorldAngularVelocityInRadians();
							if (VelocityRecord.bWasSimulating)
							{
								WarmStartLinearVelocity += VelocityRecord.LinearVelocity;
								WarmStartAngularVelocityRadians += VelocityRecord.AngularVelocityRadians;
								++WarmStartVelocitySamples;
							}
						}
					}
				}
				if (WarmStartVelocitySamples > 0)
				{
					const float WarmStartSampleScale = 1.0f / static_cast<float>(WarmStartVelocitySamples);
					WarmStartLinearVelocity *= WarmStartSampleScale;
					WarmStartAngularVelocityRadians *= WarmStartSampleScale;
				}
				if (const FRootOnWarmStartMotionCache* const WarmStartMotionCache = GRootOnWarmStartMotionCache.Find(Owner))
				{
					const float ExistingLinearSpeed = WarmStartLinearVelocity.Size();
					const float ExistingAngularSpeed = WarmStartAngularVelocityRadians.Size();
					const float DerivedLinearSpeed = WarmStartMotionCache->DerivedLinearVelocity.Size();
					const float DerivedAngularSpeed = WarmStartMotionCache->DerivedAngularVelocityRadians.Size();
					const bool bNeedDerivedWarmStartVelocity =
						WarmStartMotionCache->bCurrentSampleFromKinematicPelvis &&
						(ExistingLinearSpeed <= 1.0f && ExistingAngularSpeed <= FMath::DegreesToRadians(10.0f)) &&
						(DerivedLinearSpeed > 1.0f || DerivedAngularSpeed > FMath::DegreesToRadians(10.0f));
					if (bNeedDerivedWarmStartVelocity)
					{
						WarmStartLinearVelocity = WarmStartMotionCache->DerivedLinearVelocity;
						WarmStartAngularVelocityRadians = WarmStartMotionCache->DerivedAngularVelocityRadians;
						WarmStartVelocitySamples = 1;
					}
				}
				for (FRootOnVelocityReseedRecord& VelocityRecord : RootOnVelocityReseedRecords)
				{
					if (VelocityRecord.BoneName == PhysAnimBridge::GetRootBoneName())
					{
						VelocityRecord.LinearVelocity = WarmStartLinearVelocity;
						VelocityRecord.AngularVelocityRadians = WarmStartAngularVelocityRadians;
					}
				}
				FString WarmStartRotationSource = TEXT("live_pelvis_transform");
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_WARM_START source=%s pelvisLoc=(%.2f,%.2f,%.2f) pelvisRot=(%.4f,%.4f,%.4f,%.4f) directLinks=(%.2f,%.2f,%.2f)"),
					*WarmStartRotationSource,
					LivePelvisTransform.GetLocation().X,
					LivePelvisTransform.GetLocation().Y,
					LivePelvisTransform.GetLocation().Z,
					LivePelvisTransform.GetRotation().X,
					LivePelvisTransform.GetRotation().Y,
					LivePelvisTransform.GetRotation().Z,
					LivePelvisTransform.GetRotation().W,
					PelvisThighLErrorCm,
					PelvisThighRErrorCm,
					PelvisSpine01ErrorCm);
				if (UPhysicsControlComponent* const PhysicsControl = Owner->PhysicsControlComponent.Get())
				{
					for (const FName BoneName : RootOnApplicationBones)
					{
						const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
						PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(ModifierName, false, false, false);
						PhysicsControl->SetBodyModifierPhysicsBlendWeight(ModifierName, 1.0f, false, false);
						PhysicsControl->SetBodyModifierCollisionType(ModifierName, ECollisionEnabled::QueryAndPhysics, false, false);
						PhysicsControl->SetBodyModifierMovementType(ModifierName, EPhysicsMovementType::Simulated, false, true);
						ForceBodyModifierRecordState(
							PhysicsControl,
							ModifierName,
							EPhysicsMovementType::Simulated,
							1.0f,
							ECollisionEnabled::QueryAndPhysics,
							false);
					}
				}
				for (const FName BoneName : RootOnApplicationBones)
				{
					if (FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
					{
						if (!BodyInstance->IsInstanceSimulatingPhysics())
						{
							BodyInstance->SetInstanceSimulatePhysics(true, true);
						}
					}
				}
				float RootOnReseedPeakLinearSpeed = 0.0f;
				float RootOnReseedPeakAngularSpeedDeg = 0.0f;
				for (const FRootOnVelocityReseedRecord& VelocityRecord : RootOnVelocityReseedRecords)
				{
					if (FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(VelocityRecord.BoneName))
					{
						BodyInstance->SetLinearVelocity(VelocityRecord.LinearVelocity, false);
						BodyInstance->SetAngularVelocityInRadians(VelocityRecord.AngularVelocityRadians, false);
						RootOnReseedPeakLinearSpeed = FMath::Max(RootOnReseedPeakLinearSpeed, VelocityRecord.LinearVelocity.Size());
						RootOnReseedPeakAngularSpeedDeg = FMath::Max(
							RootOnReseedPeakAngularSpeedDeg,
							FMath::RadiansToDegrees(VelocityRecord.AngularVelocityRadians).Size());
					}
				}
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_VELOCITY_RESEED bodies=%d peakLinear=%.2f peakAngular=%.2f"),
					RootOnVelocityReseedRecords.Num(),
					RootOnReseedPeakLinearSpeed,
					RootOnReseedPeakAngularSpeedDeg);
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] PHASE2_ROOT_ON_APPLIED pelvisRawSim=%d pelvisModifierName=%s simCountPre=%d warmStartLin=%.2f warmStartAng=%.2f warmStartSamples=%d warmStartRotSamples=%d"),
					PelvisBody->IsInstanceSimulatingPhysics() ? 1 : 0,
					([](UPhysAnimComponent* LocalOwner) -> const TCHAR*
					{
						if (UPhysicsControlComponent* const PhysicsControl = LocalOwner->PhysicsControlComponent.Get())
						{
							const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(PhysAnimBridge::GetRootBoneName());
							if (const FPhysicsBodyModifierRecord* const PelvisRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, PelvisModifierName))
							{
								return UPhysAnimComponent::GetPhysicsMovementTypeName(PelvisRecord->BodyModifier.ModifierData.MovementType);
							}
						}
						return TEXT("Unknown");
					})(Owner),
					Diagnostics.SimCountPre,
					WarmStartLinearVelocity.Size(),
					FMath::RadiansToDegrees(WarmStartAngularVelocityRadians).Size(),
					WarmStartVelocitySamples,
					WarmStartRotationSamples);

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
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Transition succeeded."));
		Phase2RetryCount = 0;
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_SAFE_DENIED final_outcome. reason=%s"), *Diagnostics.FailureReason);
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
	{
		const FString FailureReason = Diagnostics.FailureReason;
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ABORT reason=%s"), *FailureReason);
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
				Owner->RecoverBridgeActiveStateAfterBalanceTransitionFailure(FailureReason);
				ResetTransitionLocalState(Owner);
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


void FPhysAnimBalanceReadyTransition::ResetTransitionLocalState(class UPhysAnimComponent* Owner)
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
	ResetRootOnReadinessNoCouplingProofState();
	bHasLoggedDistalExperimentState = false;
	LastLateValidateBlockReason.Reset();
	bHasLateValidationProof = false;
	Diagnostics.Phase1LateValidateBodyMotionViolationAccumulatedSeconds = 0.0f;
	Diagnostics.Phase1LateValidateWorstLinearSpeedBone = NAME_None;
	Diagnostics.Phase1LateValidateWorstAngularSpeedBone = NAME_None;
	Diagnostics.Phase1LateValidateWorstLinearSpeed = 0.0f;
	Diagnostics.Phase1LateValidateWorstAngularSpeed = 0.0f;
	Diagnostics.PeakMaxNonRootBodyAngularSpeed = 0.0f;
	Diagnostics.PeakMaxThighBodyAngularSpeed = 0.0f;
	Diagnostics.PeakMaxSpineBodyAngularSpeed = 0.0f;
	Diagnostics.PeakMaxFeetBodyAngularSpeed = 0.0f;
	Diagnostics.PeakTotalThighBodyAngularSpeed = 0.0f;
	Diagnostics.PeakTotalSpineBodyAngularSpeed = 0.0f;
	Diagnostics.PeakTotalFeetBodyAngularSpeed = 0.0f;
	Diagnostics.Phase3CurrentMaxNonRootAngularSpeed = 0.0f;
	Diagnostics.Phase3CurrentObservedNonRootAngularEnvelope = 0.0f;
	Diagnostics.Phase3CurrentNonRootFamilyAngularSpeed = 0.0f;
	Diagnostics.Phase3CurrentObservedNonRootFamilyAngularEnvelope = 0.0f;
	Diagnostics.Phase3CurrentMaxNonRootAngularBone = NAME_None;
	bLateValidationProofPassed = false;
	ResetCertifiedHandoffState();
	Phase1TopologyRecord = {};
	bHasPhase1TopologyRecord = false;
	bLoggedPhase2EntryAudit = false;
	bLoggedPhase2FirstFailureAudit = false;
	bLoggedPhase3EntryAudit = false;
	bLoggedPhase3FirstFailureAudit = false;
	bLoggedPhase2ReadyForPhase3 = false;
	bLoggedDirectPelvisLinkForensics = false;
	LoggedSuppressedDistalBones.Empty();
	LoggedProximalPromotions.Empty();
	LoggedProximalStates.Empty();
	Phase2GuardTickCount = 0;
	bLastKineticGateActive = Owner ? Owner->bKineticGateActiveLastFrame : false;
	Phase3KineticGateReleaseTickCount = bLastKineticGateActive ? 0 : 999;
	Phase3GuardTickCount = 0;
	bPreviousFrameSettleEndRootRawSim = false;
	bPreviousFrameSettleEndPelvisRawSim = false;
}


void FPhysAnimBalanceReadyTransition::ResetRootOnReadinessNoCouplingProofState()
{
	bPhase1RootOnReadinessNoCouplingProofActive = false;
	RootOnReadinessNoCouplingProofAccumulatedSeconds = 0.0f;
	RootOnReadinessNoCouplingPeakBodyLinearSpeed = 0.0f;
	RootOnReadinessNoCouplingPeakBodyAngularSpeed = 0.0f;
	RootOnReadinessNoCouplingWorstBone = NAME_None;
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
	Diagnostics.PeakMaxNonRootBodyAngularSpeed = 0.0f;
	Diagnostics.PeakMaxThighBodyAngularSpeed = 0.0f;
	Diagnostics.PeakMaxSpineBodyAngularSpeed = 0.0f;
	Diagnostics.PeakMaxFeetBodyAngularSpeed = 0.0f;
	Diagnostics.PeakTotalThighBodyAngularSpeed = 0.0f;
	Diagnostics.PeakTotalSpineBodyAngularSpeed = 0.0f;
	Diagnostics.PeakTotalFeetBodyAngularSpeed = 0.0f;
	Diagnostics.Phase3CurrentMaxNonRootAngularSpeed = 0.0f;
	Diagnostics.Phase3CurrentObservedNonRootAngularEnvelope = 0.0f;
	Diagnostics.Phase3CurrentNonRootFamilyAngularSpeed = 0.0f;
	Diagnostics.Phase3CurrentObservedNonRootFamilyAngularEnvelope = 0.0f;
	Diagnostics.Phase3CurrentMaxNonRootAngularBone = NAME_None;
	ResetRootOnReadinessNoCouplingProofState();
}


