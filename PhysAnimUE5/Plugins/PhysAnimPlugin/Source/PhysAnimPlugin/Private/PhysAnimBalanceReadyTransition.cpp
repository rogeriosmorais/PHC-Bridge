#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

namespace BalanceTransitionSets
{
	static bool IsRoot(FName BoneName) { return BoneName == "pelvis"; }
	static bool IsProximal(FName BoneName) { return BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03" || BoneName == "thigh_l" || BoneName == "thigh_r"; }
	static bool IsDistalLowerLimb(FName BoneName) { return BoneName == "calf_l" || BoneName == "calf_r" || BoneName == "foot_l" || BoneName == "foot_r" || BoneName == "ball_l" || BoneName == "ball_r"; }
	static bool IsTransitionCritical(FName BoneName) { return IsRoot(BoneName) || IsProximal(BoneName) || IsDistalLowerLimb(BoneName); }
}

void FPhysAnimBalanceReadyTransition::Start(const FString& InRequestReason, UPhysAnimComponent* Owner)
{
	if (IsActive() || !Owner)
	{
		return;
	}

	// Gather initial state for logging (Section 18)
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	bool bPelvisSimulating = false;
	if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
	{
		if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName))
		{
			bPelvisSimulating = PelvisBody->IsInstanceSimulatingPhysics();
		}
	}

	// Authoritative Invocation Log (Section 17)
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_INVOCATION sourcePhase=%d reason=%s pelvisSimOn=%d ownsRootOn=%d"), 
		static_cast<int32>(InternalPhase), *InRequestReason, bPelvisSimulating ? 1 : 0, bPelvisSimulating ? 0 : 1);

	// TRANSITION_ENTRY_CLASSIFICATION and Preflight Gate (Section 10)
	const EBalanceReadyEntryClassification Classification = ClassifyEntryState(Owner, Owner->ResolveEffectiveStabilizationSettings());
	if (Classification == EBalanceReadyEntryClassification::Preflight_HardFailure)
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] TRANSITION_REJECTED reason=preflight_hard_failure"));
		return; 
	}
	else if (Classification == EBalanceReadyEntryClassification::Preflight_QueueBlock)
	{
		// Should have been handled by component queue gates, but providing safety here
		return;
	}

	RequestReason = InRequestReason;
	StableHoldAccumulatedSeconds = 0.0f;
	QuietWindowAccumulatedSeconds = 0.0f;
	PhaseTimeSeconds = 0.0f;
	TotalTransitionTimeSeconds = 0.0f;
	LastLogTimeSeconds = -1.0;
	Diagnostics = {};
	bLatchedPelvisResetApplied = false;
	QuietHandoffCount = 0;
	HipQuarantineTimerSeconds = 0.0f;
	EntryHoldRotations.Empty();

	// Section 8.6: Capture entry hold-reference from current skeletal pose
	if (Owner)
	{
		if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
		{
			for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
			{
				EntryHoldRotations.Add(BoneName, Mesh->GetBoneQuaternion(BoneName, EBoneSpaces::WorldSpace));
			}
			UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_ENTRY hold_reference_captured request=%s"), *InRequestReason);
		}
	}

	// In Phase 1, bLastRootSimulating will capture whether it WAS simulating 
	bLastRootSimulating = bPelvisSimulating;
	bLastPendingResetsEmpty = Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();

	SetPhase(EBalanceReadyTransitionPhase::BRT_Phase1_Prepare, Owner);
}

EBalanceReadyEntryClassification FPhysAnimBalanceReadyTransition::ClassifyEntryState(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings) const
{
	if (!Owner) return EBalanceReadyEntryClassification::Preflight_HardFailure;

	int32 TotalSim = 0;
	int32 DistalSim = 0;
	float RootLin = 0.0f;
	float RootAng = 0.0f;
	float PolicyAlpha = Owner->CalculateCurrentPolicyInfluenceAlpha(Settings);

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
	{
		for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
		{
			if (FBodyInstance* BI = Mesh->GetBodyInstance(BoneName))
			{
				if (BI->IsInstanceSimulatingPhysics())
				{
					TotalSim++;
					const FString BoneStr = BoneName.ToString().ToLower();
					if (BoneName != RootBoneName && 
						!BoneStr.Contains(TEXT("spine")) && 
						!BoneStr.Contains(TEXT("thigh")))
					{
						DistalSim++;
					}
				}
			}
		}
		if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName))
		{
			RootLin = PelvisBody->GetUnrealWorldVelocity().Size();
			RootAng = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians()).Size();
		}
	}

	bool bPelvisSimulating = false;
	if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
	{
		if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName))
		{
			bPelvisSimulating = PelvisBody->IsInstanceSimulatingPhysics();
		}
	}

	EBalanceReadyEntryClassification Classification = EBalanceReadyEntryClassification::Preflight_HardFailure;

	// Unified Entry Gates (Section 10/11)
	const int32 simCountThreshold = Settings.BalanceEntryMaxSimCount;
	const int32 distalSimThreshold = Settings.BalanceEntryMaxDistalSimCount;
	const float minPolicyThreshold = Settings.BalanceEntryMinPolicyAlpha;

	// Section 11: Ownership Rules for Preconditions
	// pelvisBodyNotSimulating and distalBodySimulating are TRANSITION-OWNED.
	// They must not be permanent rejections.

	// Structural/Command context checks remain Hard Failures
	if (TotalSim == 0) // Sanity check: if NO bodies simulate, maybe bridge isn't even running
	{
		// But wait, if we are in BridgeActive, at least something should be happening.
		// If we are actually 100% kinematic, preflight should still accept and Phase 2 will flip it.
	}

	// Policy Influence check (Section 20 Appendix vs Section 11)
	// If policy is too high, we might want to wait for a ramp down (QueueBlock), 
	// but per Section 11, we should probably just accept and let Phase 1 suppress it.
	
	// Policy Influence check (Unified with queue gate)
	// If policy is too low, we wait/block.
	if (PolicyAlpha < minPolicyThreshold)
	{
		Classification = EBalanceReadyEntryClassification::Preflight_QueueBlock;
	}
	else
	{
		// Per Section 11: "simCount=21 distalSim=16 -> not automatically invalid"
		Classification = EBalanceReadyEntryClassification::Preflight_Accept;
	}

	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_ENTRY_CLASSIFICATION state=%s simCount=%d distalSim=%d policyAlpha=%.2f classification=%d"), 
		bPelvisSimulating ? TEXT("simulating") : TEXT("kinematic"),
		TotalSim, DistalSim, PolicyAlpha, static_cast<int32>(Classification));

	return Classification;
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
	if (RetryCooldownTimerSeconds > 0.0f)
	{
		RetryCooldownTimerSeconds = FMath::Max(0.0f, RetryCooldownTimerSeconds - DeltaTime);
	}

	if (!IsActive() || !Owner)
	{
		return;
	}

	PhaseTimeSeconds += DeltaTime;
	TotalTransitionTimeSeconds += DeltaTime;
	
	const double CurrentTime = Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : 0.0;
	const bool bShouldLog = LastLogTimeSeconds < 0.0 || (CurrentTime - LastLogTimeSeconds) >= 0.5;

	FString BlockReason;
	const bool bReadyThisFrame = EvaluateReadiness(Owner, Settings, BlockReason);
	Diagnostics.BlockReason = BlockReason;

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		// Phase 1: Prepares topology and verifies stability (Section 10/11)
		// 1. Update quarantine logic (Section 12)
		const float dt = DeltaTime;
		if (HipQuarantineTimerSeconds > 0.0f)
		{
			HipQuarantineTimerSeconds = FMath::Max(0.0f, HipQuarantineTimerSeconds - dt);
		}

		// 2. Aggregate quiet window metrics (Section 10.3)
		bool bQuietThisFrame = true;
		FString QuietBlockReason;

		if (HipQuarantineTimerSeconds > 0.0f || RetryCooldownTimerSeconds > 0.0f)
		{
			bQuietThisFrame = false;
			QuietBlockReason = RetryCooldownTimerSeconds > 0.0f ? TEXT("retry_cooldown") : TEXT("quarantine_active");
		}
		else
		{
			// Section 10.3/13: check precursor and pending resets
			const bool bPendingResets = !Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();
			const bool bPrecursorActive = Owner->IsInstabilityPrecursorActive();

			if (bPrecursorActive)
			{
				bQuietThisFrame = false;
				QuietBlockReason = TEXT("instability_precursor");
			}
			else if (bPendingResets)
			{
				bQuietThisFrame = false;
				QuietBlockReason = TEXT("pending_resets");
			}
			// Check motion thresholds
			else if (Diagnostics.RootSpeed > Settings.BalancePhase1QuietRootLinearSpeed || 
				Diagnostics.RootAngularSpeed > Settings.BalancePhase1QuietRootAngularSpeed)
			{
				bQuietThisFrame = false;
				QuietBlockReason = TEXT("motion_above_limit");
			}
			// Check shell contamination
			else if (Diagnostics.BaselineShellOffset > Settings.BalancePhase1QuietShellOffsetDelta ||
					 Diagnostics.BaselineShellVel > Settings.BalancePhase1QuietShellVelocityDelta)
			{
				bQuietThisFrame = false;
				QuietBlockReason = TEXT("shell_contamination");
			}
			// Check topology correctness (all transition critical bodies must be kinematic)
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
		}

		if (bQuietThisFrame)
		{
			const float PreviousQuiet = QuietWindowAccumulatedSeconds;
			QuietWindowAccumulatedSeconds += dt;
			if (PreviousQuiet <= 0.0f && QuietWindowAccumulatedSeconds > 0.0f)
			{
				UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_QUIET_WINDOW_STARTED"));
			}

			// Success Condition: Quiet hold met (Section 10.4)
			if (QuietWindowAccumulatedSeconds >= Settings.PolicySettleRequiredSeconds) 
			{
				UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE1_READY_FOR_ROOT_ON"));
				SetPhase(EBalanceReadyTransitionPhase::BRT_Phase2_RootOn, Owner);
				return;
			}
		}
		else
		{
			if (QuietWindowAccumulatedSeconds > 0.0f)
			{
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE1_QUIET_WINDOW_RESET reason=%s"), *QuietBlockReason);
			}
			QuietWindowAccumulatedSeconds = 0.0f;
		}

		// Timeout check (Section 16: Retry logic)
		if (PhaseTimeSeconds > Settings.BalancePhase1PrepareDuration && QuietWindowAccumulatedSeconds <= 0.0f)
		{
			Diagnostics.FailureReason = TEXT("phase1_quiet_timeout_") + QuietBlockReason;
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
			return;
		}

		// Section 9.3: Bounded target continuity check on first entry frame
		if (PhaseTimeSeconds < dt * 1.5f) // approx first frame
		{
			if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
			{
				for (auto& Pair : EntryHoldRotations)
				{
					if (BalanceTransitionSets::IsTransitionCritical(Pair.Key))
					{
						const FQuat CurrentPose = Mesh->GetBoneQuaternion(Pair.Key, EBoneSpaces::WorldSpace);
						const float Delta = FMath::RadiansToDegrees(Pair.Value.AngularDistance(CurrentPose));
						if (Delta > Settings.BalancePhase1MaxEntryTargetDeltaDeg)
						{
							UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE1_REJECTED repeated_target_discontinuity bone=%s delta=%.1f"), *Pair.Key.ToString(), Delta);
							Diagnostics.FailureReason = TEXT("phase1_repeated_target_discontinuity");
							SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
							return;
						}
					}
				}
			}
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		// Phase 2: Post root-on settle guard window (Section 8.5/10)
		
		// Section 11: Spike detection and tracking
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelPelvis);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelThighs);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelSpine);
		Diagnostics.PeakMaxBodyLinearSpeed = FMath::Max(Diagnostics.PeakMaxBodyLinearSpeed, Diagnostics.MaxLinVelFeet);

		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelPelvis);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelThighs);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelSpine);
		Diagnostics.PeakMaxBodyAngularSpeed = FMath::Max(Diagnostics.PeakMaxBodyAngularSpeed, Diagnostics.MaxAngVelFeet);

		// Section 11: Abort Condition Checks
		FString AbortReason;
		if (Diagnostics.RootSpeed > Settings.BalancePhase2AbortRootLinearSpeed) AbortReason = TEXT("root_linear_spike");
		else if (Diagnostics.RootAngularSpeed > Settings.BalancePhase2AbortRootAngularSpeed) AbortReason = TEXT("root_angular_spike");
		else if (Diagnostics.PeakMaxBodyLinearSpeed > Settings.BalancePhase2AbortMaxBodyLinearSpeed) AbortReason = TEXT("body_linear_spike");
		else if (Diagnostics.PeakMaxBodyAngularSpeed > Settings.BalancePhase2AbortMaxBodyAngularSpeed) AbortReason = TEXT("body_angular_spike");
		else if (Diagnostics.BaselineShellOffset > Settings.BalancePhase2AbortShellOffsetDelta) AbortReason = TEXT("shell_offset_spike");
		else if (Diagnostics.BaselineShellVel > Settings.BalancePhase2AbortShellVelocityDelta) AbortReason = TEXT("shell_velocity_spike");
		else if (!Owner->WasPelvisSimulatingLastFrame()) AbortReason = TEXT("root_sim_dropped");
		else if (!Owner->GetPendingBodyModifierCachedResetNames().IsEmpty()) AbortReason = TEXT("reset_violation");
		else if (Owner->IsInstabilityPrecursorActive()) AbortReason = TEXT("fail_stop_precursor");

		if (!AbortReason.IsEmpty())
		{
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_ABORT_SPIKE reason=%s lin=%.1f ang=%.1f peakBodyLin=%.1f"), 
				*AbortReason, Diagnostics.RootSpeed, Diagnostics.RootAngularSpeed, Diagnostics.PeakMaxBodyLinearSpeed);
			Diagnostics.FailureReason = TEXT("phase2_") + AbortReason;
			
			// Section 16/495: Retry logic for Phase 2 spiky/dropped failures
			const bool bIsRetryable = AbortReason.Contains(TEXT("spike")) || AbortReason.Contains(TEXT("dropped"));
			if (bIsRetryable && Phase2RetryCount < Settings.BalancePhase2MaxAutomaticRetries)
			{
				Phase2RetryCount++;
				RetryCooldownTimerSeconds = Settings.BalancePhase2RetryCooldownSeconds;
				UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_RETRY_SCHEDULED attempt=%d/%d cooldown=%.1f"), 
					Phase2RetryCount, Settings.BalancePhase2MaxAutomaticRetries, RetryCooldownTimerSeconds);
				SetPhase(EBalanceReadyTransitionPhase::BRT_Phase1_Prepare, Owner);
				return;
			}

			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
			return;
		}

		// Success Condition: Guard window duration reached (Section 10.1)
		if (PhaseTimeSeconds > Settings.BalancePhase2GuardWindowDuration)
		{
			UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] PHASE2_READY_FOR_PHASE3"));
			SetPhase(EBalanceReadyTransitionPhase::BRT_Phase3_Settle, Owner);
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		// Phase 3: Bounded settle duration (Section 14).
		if (bReadyThisFrame)
		{
			StableHoldAccumulatedSeconds += DeltaTime;
			if (StableHoldAccumulatedSeconds >= Settings.BalancePhase3RequiredStableHoldDuration) // Required settle success duration
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

	if (bShouldLog && IsActive() && PhaseTimeSeconds < (DeltaTime * 2.0f))
	{
		// Only log progress once per phase to reduce noise
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] Phase Progress: phase=%d ready=%s reason=%s"), 
			static_cast<int32>(InternalPhase), bReadyThisFrame ? TEXT("true") : TEXT("false"), *BlockReason);
		LastLogTimeSeconds = CurrentTime;
	}

	// Update latches for next frame
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Owner->GetMeshComponent() ? Owner->GetMeshComponent()->GetBodyInstance(RootBoneName) : nullptr;
	bLastRootSimulating = PelvisBody && PelvisBody->IsInstanceSimulatingPhysics();
	bLastPendingResetsEmpty = Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();
}

void FPhysAnimBalanceReadyTransition::SetPhase(EBalanceReadyTransitionPhase NewPhase, UPhysAnimComponent* Owner)
{
	if (InternalPhase != NewPhase)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] Transition Phase Change: %d -> %d"), static_cast<int32>(InternalPhase), static_cast<int32>(NewPhase));
		InternalPhase = NewPhase;
		PhaseTimeSeconds = 0.0f;
		StableHoldAccumulatedSeconds = 0.0f;

		if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
		{
			// Section 8.3: Execute Root-On
			if (Owner)
			{
				if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
				{
					if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()))
					{
						// Section 8.1/17: Snapshot and Log
						Diagnostics.BaselineRootLinVel = PelvisBody->GetUnrealWorldVelocity().Size();
						Diagnostics.BaselineRootAngVel = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians()).Size();
						Diagnostics.PeakMaxBodyLinearSpeed = 0.0f;
						Diagnostics.PeakMaxBodyAngularSpeed = 0.0f;

						UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON preLin=%.1f preAng=%.1f"), 
							Diagnostics.BaselineRootLinVel, Diagnostics.BaselineRootAngVel);

						PelvisBody->SetInstanceSimulatePhysics(true);
						Diagnostics.bSimFlipped = true;
						
						CaptureFlipDiagnostics(Owner);
					}
				}
			}
		}

		if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_SUCCESS."));
			Phase2RetryCount = 0;
		}
		else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
		{
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_ABORT reason=%s"), *Diagnostics.FailureReason);
			
			// Section 15: Recovery Contract (Restore coherent BridgeActive state)
			if (Owner)
			{
				USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
				if (Mesh)
				{
					// If we flipped pelvis sim on, flip it back (Section 15/470)
					if (Diagnostics.bSimFlipped)
					{
						if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()))
						{
							PelvisBody->SetInstanceSimulatePhysics(false);
							UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_RECOVERY_BEGIN disabled_pelvis_simulation"));
						}
					}

					// Recovery Cleanup

					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_RECOVERY_COMPLETE"));
				}
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

	if (!PelvisBody->IsInstanceSimulatingPhysics())
	{
		OutReason = TEXT("pelvis_not_simulating");
		return false;
	}

	if (!Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
	{
		OutReason = TEXT("pending_resets");
		return false;
	}

	const FVector PelvisLinearVelocity = Mesh->GetPhysicsLinearVelocity(RootBoneName);
	const FVector PelvisAngularVelocityDegPerSec = Mesh->GetPhysicsAngularVelocityInDegrees(RootBoneName);
	const FTransform PelvisTransform = Mesh->GetBoneTransform(Mesh->GetBoneIndex(RootBoneName));
	
	Diagnostics.RootSpeed = PelvisLinearVelocity.Size();
	Diagnostics.RootAngularSpeed = PelvisAngularVelocityDegPerSec.Size();
	Diagnostics.RootTilt = FMath::RadiansToDegrees(OwnerActor->GetActorQuat().AngularDistance(PelvisTransform.GetRotation()));
	Diagnostics.ShellMetric = Owner->GetAcceptedShellPlanarVelocity().Size2D();

	if (Diagnostics.RootSpeed > Settings.MaxRootLinearSpeedCmPerSecond ||
		Diagnostics.RootAngularSpeed > Settings.MaxRootAngularSpeedDegPerSecond)
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

void FPhysAnimBalanceReadyTransition::CaptureFlipDiagnostics(UPhysAnimComponent* Owner)
{
	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (!Mesh) return;

	Diagnostics.PelvisLinearVelPost = Mesh->GetPhysicsLinearVelocity(RootBoneName);
	Diagnostics.PelvisAngularVelPost = Mesh->GetPhysicsAngularVelocityInDegrees(RootBoneName);
	
	Diagnostics.bShellContributed = Owner->GetAcceptedShellPlanarVelocity().Size2D() > 0.1f;
	Diagnostics.bPolicyWroteTargets = Owner->WasPolicyTargetAppliedLastFrame();
	Diagnostics.bResetScheduled = !Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();

	// Bone velocities
	auto GetMaxVel = [&](const TArray<FName>& Bones, float& MaxLin, float& MaxAng) {
		MaxLin = 0.0f;
		MaxAng = 0.0f;
		for (FName Bone : Bones) {
			MaxLin = FMath::Max(MaxLin, Mesh->GetPhysicsLinearVelocity(Bone).Size());
			MaxAng = FMath::Max(MaxAng, Mesh->GetPhysicsAngularVelocityInDegrees(Bone).Size());
		}
	};

	GetMaxVel({RootBoneName}, Diagnostics.MaxLinVelPelvis, Diagnostics.MaxAngVelPelvis);
	GetMaxVel({TEXT("thigh_l"), TEXT("thigh_r")}, Diagnostics.MaxLinVelThighs, Diagnostics.MaxAngVelThighs);
	GetMaxVel({TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03")}, Diagnostics.MaxLinVelSpine, Diagnostics.MaxAngVelSpine);
	GetMaxVel({TEXT("foot_l"), TEXT("foot_r")}, Diagnostics.MaxLinVelFeet, Diagnostics.MaxAngVelFeet);

	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] SIM FLIP DIAGNOSTICS:"));
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  Root Pre: lin=%.1f ang=%.1f"), Diagnostics.PelvisLinearVelPre.Size(), Diagnostics.PelvisAngularVelPre.Size());
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  Root Post: lin=%.1f ang=%.1f"), Diagnostics.PelvisLinearVelPost.Size(), Diagnostics.PelvisAngularVelPost.Size());
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  Systems: shell=%d policy=%d resetScheduled=%d"), Diagnostics.bShellContributed, Diagnostics.bPolicyWroteTargets, Diagnostics.bResetScheduled);
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  MaxLinVel: Pelvis=%.1f Thighs=%.1f Spine=%.1f Feet=%.1f"), Diagnostics.MaxLinVelPelvis, Diagnostics.MaxLinVelThighs, Diagnostics.MaxLinVelSpine, Diagnostics.MaxLinVelFeet);
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  MaxAngVel: Pelvis=%.1f Thighs=%.1f Spine=%.1f Feet=%.1f"), Diagnostics.MaxAngVelPelvis, Diagnostics.MaxAngVelThighs, Diagnostics.MaxAngVelSpine, Diagnostics.MaxAngVelFeet);
}

bool FPhysAnimBalanceReadyTransition::ShouldSuppressPolicy() const 
{ 
	// Global suppression if entire transition-critical set is suppressed
	return IsActive() && (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn); 
}
bool FPhysAnimBalanceReadyTransition::ShouldSuppressPolicyWrites(FName BoneName) const
{
	if (!IsActive()) return false;
	
	// During Phase 1 and Phase 2 (Guard Window), we suppress policy writes to hold the entry pose
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		return true; // Everything holds entry pose during guard window
	}
	
	return false;
}
bool FPhysAnimBalanceReadyTransition::ShouldSuppressShell() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn; }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressPerturbations() const { return IsActive(); }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressResets() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn; } 
bool FPhysAnimBalanceReadyTransition::ShouldSuppressMoveSmoke() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn; }

float FPhysAnimBalanceReadyTransition::GetRootBodyModifierSoftSimAlpha() const { return 1.0f; }
float FPhysAnimBalanceReadyTransition::GetProximalControlSoftAlpha(FName BoneName) const { return 1.0f; }

bool FPhysAnimBalanceReadyTransition::ShouldKeepBoneKinematic(FName BoneName) const
{
	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_Failed) return false;
	
	// Phase 1: All transition critical bodies are forced kinematic (Section 6)
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		return BalanceTransitionSets::IsTransitionCritical(BoneName);
	}

	// Phase 2: Root (pelvis) flips to sim in SetPhase, but proximal and distal remain kinematic for isolation
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		// Proximal and Distal must stay kinematic until root-on dwell is complete
		return BalanceTransitionSets::IsProximal(BoneName) || BalanceTransitionSets::IsDistalLowerLimb(BoneName);
	}

	// On failure, we keep distal bodies kinematic to restore safety
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
	{
		return BalanceTransitionSets::IsDistalLowerLimb(BoneName);
	}

	return false; // Phase 3 (DistalEnable) onwards: All allowed
}

float FPhysAnimBalanceReadyTransition::GetTransitionExtraDampingMultiplier(const FPhysAnimStabilizationSettings& Settings) const
{
	if (!IsActive()) return 1.0f;
	// Increase damping during instability window
	const bool bInBootstrap = (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn);
	return bInBootstrap ? Settings.BalanceBootstrapExtraDampingMultiplier : Settings.BalanceActiveExtraDampingMultiplier;
}
