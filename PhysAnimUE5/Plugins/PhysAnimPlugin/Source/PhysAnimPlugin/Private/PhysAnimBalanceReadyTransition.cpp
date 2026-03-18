#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

namespace BalanceTransitionSets
{
	static bool IsRoot(FName BoneName) { return BoneName == "pelvis"; }
	static bool IsProximal(FName BoneName) { return BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03" || BoneName == "thigh_l" || BoneName == "thigh_r"; }
	static bool IsDistalLowerLimb(FName BoneName) { return BoneName == "calf_l" || BoneName == "calf_r" || BoneName == "foot_l" || BoneName == "foot_r" || BoneName == "ball_l" || BoneName == "ball_r"; }
	static bool IsUpperBody(FName BoneName)
	{
		return BoneName == "clavicle_l" || BoneName == "upperarm_l" || BoneName == "lowerarm_l" || BoneName == "hand_l" ||
			BoneName == "clavicle_r" || BoneName == "upperarm_r" || BoneName == "lowerarm_r" || BoneName == "hand_r" ||
			BoneName == "neck_01" || BoneName == "head";
	}
	static bool IsTransitionCritical(FName BoneName) { return IsRoot(BoneName) || IsProximal(BoneName) || IsDistalLowerLimb(BoneName); }
	static bool IsExpectedPhase2Topology(int32 SimCountPre, int32 SimCountPost, int32 DistalSimCountPre, int32 DistalSimCountPost)
	{
		return DistalSimCountPre == 0 &&
			DistalSimCountPost == 0 &&
			((SimCountPre == 0 && SimCountPost == 1) || (SimCountPre == 1 && SimCountPost == 1));
	}
}

void FPhysAnimBalanceReadyTransition::Start(const FString& InRequestReason, UPhysAnimComponent* Owner)
{
	if (IsActive() || !Owner)
	{
		return;
	}

	const EBalanceReadyEntryClassification Classification = ClassifyEntryState(Owner, Owner->ResolveEffectiveStabilizationSettings());
	if (Classification != EBalanceReadyEntryClassification::Preflight_Accept)
	{
		if (Classification == EBalanceReadyEntryClassification::Preflight_HardFailure)
		{
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] TRANSITION_REJECTED reason=preflight_hard_failure"));
		}
		return;
	}

	RequestReason = InRequestReason;
	StableHoldAccumulatedSeconds = 0.0f;
	QuietWindowAccumulatedSeconds = 0.0f;
	PhaseTimeSeconds = 0.0f;
	TotalTransitionTimeSeconds = 0.0f;
	LastLogTimeSeconds = -1.0;
	ResetTransitionLocalState();
	bLatchedPelvisResetApplied = false;
	QuietHandoffCount = 0;
	HipQuarantineTimerSeconds = 0.0f;
	EntryHoldRotations.Empty();

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
}

EBalanceReadyEntryClassification FPhysAnimBalanceReadyTransition::ClassifyEntryState(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings) const
{
	if (!Owner || !Owner->GetMeshComponent() || !Owner->GetOwner())
	{
		return EBalanceReadyEntryClassification::Preflight_HardFailure;
	}

	if (!Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
	{
		return EBalanceReadyEntryClassification::Preflight_QueueBlock;
	}

	if (Owner->CalculateCurrentPolicyInfluenceAlpha(Settings) < Settings.BalanceEntryMinPolicyAlpha)
	{
		return EBalanceReadyEntryClassification::Preflight_QueueBlock;
	}

	return EBalanceReadyEntryClassification::Preflight_Accept;
}

void FPhysAnimBalanceReadyTransition::Cancel()
{
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Inactive)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] BalanceReadyTransition cancelled. phase=%d"), static_cast<int32>(InternalPhase));
		SetPhase(EBalanceReadyTransitionPhase::BRT_Inactive);
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

		if (Owner->IsInstabilityPrecursorActive())
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("instability_precursor");
		}
		else if (!Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("pending_resets");
		}
		else if (Diagnostics.RootSpeed > Settings.BalancePhase1QuietRootLinearSpeed || Diagnostics.RootAngularSpeed > Settings.BalancePhase1QuietRootAngularSpeed)
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("motion_above_limit");
		}
		else if (Owner->GetCurrentShellPlanarOffsetDeltaCm() > Settings.BalancePhase1QuietShellOffsetDelta ||
			Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond() > Settings.BalancePhase1QuietShellVelocityDelta)
		{
			bQuietThisFrame = false;
			QuietBlockReason = TEXT("shell_contamination");
		}
		else
		{
			TArray<FName> SimulatingBones;
			Owner->GetSimulatingBodies(SimulatingBones);
			for (const FName BoneName : SimulatingBones)
			{
				if (BalanceTransitionSets::IsTransitionCritical(BoneName))
				{
					bQuietThisFrame = false;
					QuietBlockReason = TEXT("topology_mismatch_simulating_critical");
					break;
				}
			}
		}

		if (bQuietThisFrame)
		{
			QuietWindowAccumulatedSeconds += DeltaTime;
			if (QuietWindowAccumulatedSeconds >= Settings.PolicySettleRequiredSeconds)
			{
				FString Phase2BlockReason;
				if (ValidatePhase2EntryPreconditions(Owner, Settings, Phase2BlockReason))
				{
					SetPhase(EBalanceReadyTransitionPhase::BRT_Phase2_RootOn, Owner);
					return;
				}

				QuietWindowAccumulatedSeconds = 0.0f;
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_QUIET_WINDOW_RESET reason=%s"), *Phase2BlockReason);
			}
		}
		else
		{
			QuietWindowAccumulatedSeconds = 0.0f;
		}

		if (PhaseTimeSeconds > Settings.BalancePhase1PrepareDuration && QuietWindowAccumulatedSeconds <= 0.0f)
		{
			Diagnostics.FailureReason = TEXT("phase1_quiet_timeout_") + QuietBlockReason;
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
		}
		return;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		CaptureFlipDiagnostics(Owner);

		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelPelvis);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelThighs);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelSpine);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelFeet);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelPelvis);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelThighs);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelSpine);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelFeet);

		FString AbortReason;
		if (Diagnostics.bPolicyWroteTargets)
		{
			AbortReason = TEXT("phase2_policy_write_leak");
		}
		else if (Diagnostics.bResetScheduled || !Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
		{
			AbortReason = TEXT("phase2_reset_violation");
		}
		else if (Diagnostics.bShellContributed &&
			(Diagnostics.BaselineShellOffset > Settings.BalancePhase2AbortShellOffsetDelta ||
			 Diagnostics.BaselineShellVel > Settings.BalancePhase2AbortShellVelocityDelta))
		{
			AbortReason = TEXT("phase2_shell_correction_material");
		}
		else if (!Owner->WasPelvisSimulatingLastFrame())
		{
			AbortReason = TEXT("phase2_root_simulation_dropped");
		}
		else if (Owner->IsInstabilityPrecursorActive())
		{
			AbortReason = TEXT("phase2_fail_stop_precursor");
		}
		else if (!BalanceTransitionSets::IsExpectedPhase2Topology(
			Diagnostics.SimCountPre,
			Diagnostics.SimCountPost,
			Diagnostics.DistalSimCountPre,
			Diagnostics.DistalSimCountPost))
		{
			AbortReason = TEXT("phase2_topology_not_preserved");
		}
		else if (Diagnostics.RootSpeed > Settings.BalancePhase2AbortRootLinearSpeed ||
			Diagnostics.RootAngularSpeed > Settings.BalancePhase2AbortRootAngularSpeed ||
			Diagnostics.BaselineShellOffset > Settings.BalancePhase2AbortShellOffsetDelta ||
			Diagnostics.BaselineShellVel > Settings.BalancePhase2AbortShellVelocityDelta ||
			Diagnostics.PeakMaxBodyLinearSpeed > Settings.BalancePhase2AbortMaxBodyLinearSpeed ||
			Diagnostics.PeakMaxBodyAngularSpeed > Settings.BalancePhase2AbortMaxBodyAngularSpeed)
		{
			AbortReason = TEXT("phase2_root_on_spike");
		}

		if (!AbortReason.IsEmpty())
		{
			Diagnostics.FailureReason = AbortReason;
			Diagnostics.LastRetryDecision = IsAutomaticRetryAllowed(
				Diagnostics.FailureReason,
				true,
				false,
				false,
				true,
				Phase2RetryCount < Settings.BalancePhase2MaxAutomaticRetries) ? TEXT("allowed") : TEXT("denied");
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_GUARD_WINDOW_ABORTED reason=%s"), *Diagnostics.FailureReason);
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_RETRY_DECISION failure=%s decision=%s changedState=0 freshQuietProof=0 remainingRetryBudget=%d"),
				*Diagnostics.FailureReason, *Diagnostics.LastRetryDecision, FMath::Max(Settings.BalancePhase2MaxAutomaticRetries - Phase2RetryCount, 0));
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
		if (bReadyThisFrame)
		{
			StableHoldAccumulatedSeconds += DeltaTime;
			if (StableHoldAccumulatedSeconds >= Settings.BalancePhase3RequiredStableHoldDuration)
			{
				SetPhase(EBalanceReadyTransitionPhase::BRT_Succeeded, Owner);
			}
		}
		else
		{
			StableHoldAccumulatedSeconds = 0.0f;
		}

		if (PhaseTimeSeconds > Settings.BalancePhase3TimeoutDuration)
		{
			Diagnostics.FailureReason = TEXT("phase3_settle_timeout");
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

	InternalPhase = NewPhase;
	PhaseTimeSeconds = 0.0f;
	StableHoldAccumulatedSeconds = 0.0f;

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn && Owner)
	{
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
				for (const FName BoneName : SimulatingBones)
				{
					if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
					{
						Diagnostics.DistalSimCountPre++;
					}
				}

				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ENTRY rootPreLin=%.1f rootPreAng=%.1f shellOffsetDelta=%.1f shellVelocityDelta=%.1f simCountPre=%d distalSimPre=%d policySuppressed=1 resetScheduled=%d"),
					Diagnostics.BaselineRootLinVel,
					Diagnostics.BaselineRootAngVel,
					Diagnostics.BaselineShellOffset,
					Diagnostics.BaselineShellVel,
					Diagnostics.SimCountPre,
					Diagnostics.DistalSimCountPre,
					Diagnostics.bResetScheduled ? 1 : 0);

				PelvisBody->SetInstanceSimulatePhysics(true);
				Diagnostics.bSimFlipped = true;
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
				UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_GUARD_WINDOW_STARTED duration=%.2f"), Owner->ResolveEffectiveStabilizationSettings().BalancePhase2GuardWindowDuration);
			}
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_SUCCESS."));
		Phase2RetryCount = 0;
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

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName);
	if (!PelvisBody)
	{
		OutReason = TEXT("pelvis_missing");
		return false;
	}

	const FVector PelvisLinearVelocity = Mesh->GetPhysicsLinearVelocity(RootBoneName);
	const FVector PelvisAngularVelocityDegPerSec = Mesh->GetPhysicsAngularVelocityInDegrees(RootBoneName);
	const FTransform PelvisTransform = Mesh->GetBoneTransform(Mesh->GetBoneIndex(RootBoneName));

	Diagnostics.RootSpeed = PelvisLinearVelocity.Size();
	Diagnostics.RootAngularSpeed = PelvisAngularVelocityDegPerSec.Size();
	Diagnostics.RootTilt = FMath::RadiansToDegrees(OwnerActor->GetActorQuat().AngularDistance(PelvisTransform.GetRotation()));
	Diagnostics.ShellMetric = Owner->GetAcceptedShellPlanarVelocity().Size2D();

	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && !PelvisBody->IsInstanceSimulatingPhysics())
	{
		OutReason = TEXT("pelvis_not_simulating");
		return false;
	}

	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && !Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
	{
		OutReason = TEXT("pending_resets");
		return false;
	}

	if (Diagnostics.RootSpeed > Settings.MaxRootLinearSpeedCmPerSecond || Diagnostics.RootAngularSpeed > Settings.MaxRootAngularSpeedDegPerSecond)
	{
		OutReason = TEXT("fail_stop_precursor");
		return false;
	}
	if (Diagnostics.RootSpeed > Settings.BalanceSettleMaxRootLinearSpeed)
	{
		OutReason = TEXT("root_linear_above_settle");
		return false;
	}
	if (Diagnostics.RootAngularSpeed > Settings.BalanceSettleMaxRootAngularSpeed)
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
	if (!Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
	{
		OutReason = TEXT("phase2_reset_pending");
		return false;
	}
	if (Owner->IsInstabilityPrecursorActive())
	{
		OutReason = TEXT("phase2_fail_stop_precursor");
		return false;
	}
	if (Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle)
	{
		OutReason = TEXT("phase2_locomotion_active");
		return false;
	}

	const float ShellOffset = Owner->GetCurrentShellPlanarOffsetDeltaCm();
	const float ShellVel = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
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

	OutReason = TEXT("ready");
	return true;
}

void FPhysAnimBalanceReadyTransition::ResetTransitionLocalState()
{
	Diagnostics = {};
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
	Diagnostics.bPolicyWroteTargets = Owner->WasPolicyTargetAppliedLastFrame();
	Diagnostics.bResetScheduled = !Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();
	Diagnostics.BaselineShellOffset = Owner->GetCurrentShellPlanarOffsetDeltaCm();
	Diagnostics.BaselineShellVel = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
	Diagnostics.bShellContributed = Diagnostics.BaselineShellOffset > KINDA_SMALL_NUMBER || Diagnostics.BaselineShellVel > KINDA_SMALL_NUMBER;

	TArray<FName> SimulatingBones;
	Owner->GetSimulatingBodies(SimulatingBones);
	Diagnostics.SimCountPost = SimulatingBones.Num();
	Diagnostics.DistalSimCountPost = 0;
	for (const FName BoneName : SimulatingBones)
	{
		if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
		{
			Diagnostics.DistalSimCountPost++;
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
	return IsActive() && (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn);
}

bool FPhysAnimBalanceReadyTransition::ShouldSuppressPolicyWrites(FName BoneName) const
{
	if (!IsActive())
	{
		return false;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		return true;
	}

	return false;
}

bool FPhysAnimBalanceReadyTransition::ShouldSuppressShell() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn; }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressPerturbations() const { return IsActive(); }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressResets() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn; }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressMoveSmoke() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn; }

float FPhysAnimBalanceReadyTransition::GetRootBodyModifierSoftSimAlpha() const { return 1.0f; }
float FPhysAnimBalanceReadyTransition::GetProximalControlSoftAlpha(FName BoneName) const { return BalanceTransitionSets::IsProximal(BoneName) ? 1.0f : 1.0f; }

bool FPhysAnimBalanceReadyTransition::ShouldKeepBoneKinematic(FName BoneName) const
{
	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_Failed)
	{
		return false;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		return BalanceTransitionSets::IsTransitionCritical(BoneName) || BalanceTransitionSets::IsUpperBody(BoneName);
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		return BalanceTransitionSets::IsProximal(BoneName) || BalanceTransitionSets::IsDistalLowerLimb(BoneName) || BalanceTransitionSets::IsUpperBody(BoneName);
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
	{
		return BalanceTransitionSets::IsTransitionCritical(BoneName) || BalanceTransitionSets::IsUpperBody(BoneName);
	}

	return false;
}

float FPhysAnimBalanceReadyTransition::GetTransitionExtraDampingMultiplier(const FPhysAnimStabilizationSettings& Settings) const
{
	if (!IsActive())
	{
		return 1.0f;
	}

	const bool bInBootstrap = InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
	return bInBootstrap ? Settings.BalanceBootstrapExtraDampingMultiplier : Settings.BalanceActiveExtraDampingMultiplier;
}

bool FPhysAnimBalanceReadyTransition::IsFailureClassRetryable(const FString& FailureReason)
{
	return FailureReason == TEXT("phase2_root_not_confirmed") ||
		FailureReason == TEXT("phase2_topology_not_preserved") ||
		FailureReason == TEXT("phase2_guard_window_interrupted_by_transient_contamination");
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
