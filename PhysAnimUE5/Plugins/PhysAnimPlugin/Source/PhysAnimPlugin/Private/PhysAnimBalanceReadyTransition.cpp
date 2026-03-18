#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

void FPhysAnimBalanceReadyTransition::Start(const FString& InRequestReason, UPhysAnimComponent* Owner)
{
	if (IsActive() || !Owner)
	{
		return;
	}

	// TRANSITION_ENTRY_CLASSIFICATION and Hard Entry Gate
	const EBalanceReadyEntryClassification Classification = ClassifyEntryState(Owner, Owner->ResolveEffectiveStabilizationSettings());
	if (Classification == EBalanceReadyEntryClassification::InvalidEntryState)
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] TRANSITION_REJECTED_NO_RETRY reason=invalid_entry_state"));
		return; // Direct return to Idle, no recovery phase
	}

	RequestReason = InRequestReason;
	StableHoldAccumulatedSeconds = 0.0f;
	PhaseTimeSeconds = 0.0f;
	TotalTransitionTimeSeconds = 0.0f;
	LastLogTimeSeconds = -1.0;
	Diagnostics = {};
	bLatchedPelvisResetApplied = false;
	QuietHandoffCount = 0;

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
	{
		if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName))
		{
			bLastRootSimulating = PelvisBody->IsInstanceSimulatingPhysics();
		}
	}
	bLastPendingResetsEmpty = Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();

	SetPhase(EBalanceReadyTransitionPhase::BRT_Handoff, Owner);
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_START reason=%s"), *RequestReason);
}

EBalanceReadyEntryClassification FPhysAnimBalanceReadyTransition::ClassifyEntryState(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings) const
{
	if (!Owner) return EBalanceReadyEntryClassification::InvalidEntryState;

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

	EBalanceReadyEntryClassification Classification = EBalanceReadyEntryClassification::InvalidEntryState;

	// Hard Gate Thresholds:
	// simCount <= 5
	// distalSim == 0
	// policyAlpha <= 0.05
	// rootAng <= 25.0
	// rootLin <= 15.0
	const bool bStateClean = (TotalSim <= 5) && (DistalSim == 0) && (PolicyAlpha <= 0.05f) && (RootLin <= 15.0f) && (RootAng <= 25.0f);

	if (bPelvisSimulating)
	{
		// If pelvis is simulating, we allow start if state is within bounds (though it's usually already started)
		// but since we are classifying for a potential start call:
		Classification = bStateClean ? EBalanceReadyEntryClassification::ReadyToStart : EBalanceReadyEntryClassification::InvalidEntryState;
	}
	else
	{
		// Kinematic root. 
		// If state is clean, it's a retryable wait.
		Classification = bStateClean ? EBalanceReadyEntryClassification::RetryableWait : EBalanceReadyEntryClassification::InvalidEntryState;
	}

	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_ENTRY_CLASSIFICATION state=%s simCount=%d distalSim=%d policyAlpha=%.2f classification=%s"), 
		bPelvisSimulating ? TEXT("simulating") : TEXT("kinematic"),
		TotalSim, DistalSim, PolicyAlpha,
		(Classification == EBalanceReadyEntryClassification::InvalidEntryState) ? TEXT("invalid_entry_state") : 
		(Classification == EBalanceReadyEntryClassification::ReadyToStart ? TEXT("ready_to_start") : TEXT("retryable_wait")));

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
	if (!IsActive())
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

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Handoff)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		FBodyInstance* PelvisBody = Owner->GetMeshComponent() ? Owner->GetMeshComponent()->GetBodyInstance(RootBoneName) : nullptr;
		const bool bIsSimulating = PelvisBody && PelvisBody->IsInstanceSimulatingPhysics();
		
		if (Owner->WasPelvisResetAppliedThisTick())
		{
			bLatchedPelvisResetApplied = true;
			Diagnostics.bResetApplied = true;
		}

		const bool bSimJustStarted = bIsSimulating && !bLastRootSimulating;
		
		if (bSimJustStarted)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_PHASE1_ROOT_ON rootLinVel=%.1f rootAngVel=%.1f"), 
				PelvisBody->GetUnrealWorldVelocity().Size(), 
				FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians()).Size());

			// Spike check immediately on flip
			const float RootSpeed = PelvisBody->GetUnrealWorldVelocity().Size();
			const float RootAngSpeed = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians()).Size();
			const float SpikeLinThreshold = 150.0f; // Threshold for initial flip spike
			const float SpikeAngThreshold = 180.0f;

			if (RootSpeed > SpikeLinThreshold || RootAngSpeed > SpikeAngThreshold)
			{
				UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] TRANSITION_ABORT_SPIKE lin=%.1f ang=%.1f"), RootSpeed, RootAngSpeed);
				Diagnostics.FailureReason = TEXT("initial_sim_spike");
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
				return;
			}

			Diagnostics.bSimFlipped = true;
			CaptureFlipDiagnostics(Owner);
			SetPhase(EBalanceReadyTransitionPhase::BRT_PostHandoffSettle);
			return;
		}

		if (PhaseTimeSeconds > 2.0f) // Timeout
		{
			Diagnostics.FailureReason = TEXT("handoff_timeout");
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
		}
		else if (PelvisBody && (!bIsSimulating || !bLatchedPelvisResetApplied))
		{
			// Capture pre-flip velocities while waiting
			Diagnostics.PelvisLinearVelPre = PelvisBody->GetUnrealWorldVelocity();
			Diagnostics.PelvisAngularVelPre = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_PostHandoffSettle)
	{
		if (bReadyThisFrame)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_PHASE2_PROXIMAL_STABLE rootLinVel=%.1f"), Diagnostics.RootSpeed);
			SetPhase(EBalanceReadyTransitionPhase::BRT_RestoreControls);
		}
		else if (PhaseTimeSeconds > 3.0f)
		{
			Diagnostics.FailureReason = TEXT("post_handoff_settle_timeout");
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_RestoreControls)
	{
		// Phase 3: Enable distal bodies
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_PHASE3_DISTAL_ENABLE"));
		SetPhase(EBalanceReadyTransitionPhase::BRT_FinalSettle);
	}
		if (InternalPhase == EBalanceReadyTransitionPhase::BRT_FinalSettle)
	{
		if (bReadyThisFrame)
		{
			StableHoldAccumulatedSeconds += DeltaTime;
			if (StableHoldAccumulatedSeconds >= Owner->BalanceQuietWindowRequiredSeconds)
			{
				SetPhase(EBalanceReadyTransitionPhase::BRT_Succeeded);
			}
		}
		else
		{
			StableHoldAccumulatedSeconds = 0.0f;
		}

		if (PhaseTimeSeconds > 5.0f)
		{
			Diagnostics.FailureReason = TEXT("final_settle_timeout");
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed);
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

		if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION SUCCESS."));
		}
		else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION FAILED: %s"), *Diagnostics.FailureReason);
			
			// TRANSITION_RECOVERY
			int32 TotalSim = 0;
			bool bDistalForcedKinematic = false;
			if (Owner)
			{
				USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
				if (Mesh)
				{
					for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
					{
						if (FBodyInstance* BI = Mesh->GetBodyInstance(BoneName))
						{
							if (BI->IsInstanceSimulatingPhysics())
							{
								TotalSim++;
							}
						}
					}
					bDistalForcedKinematic = true; 
				}
			}

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_RECOVERY simCount=%d policyDisabled=1 distalForcedKinematic=%d"), 
				TotalSim, bDistalForcedKinematic ? 1 : 0);
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

	if (Diagnostics.RootSpeed > Owner->BalanceQuietLinearSpeedThresholdCmPerSec)
	{
		OutReason = TEXT("root_linear_high");
		return false;
	}

	if (Diagnostics.RootAngularSpeed > Owner->BalanceQuietTiltThresholdDeg * 2.0f)
	{
		OutReason = TEXT("root_angular_high");
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
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  Root Pre: lin=%.1f ang=%.1f"), Diagnostics.PelvisLinearVelPre.Size(), Diagnostics.PelvisLinearVelPost.Size());
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  Root Post: lin=%.1f ang=%.1f"), Diagnostics.PelvisLinearVelPost.Size(), Diagnostics.PelvisAngularVelPost.Size());
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  Systems: shell=%d policy=%d resetScheduled=%d"), Diagnostics.bShellContributed, Diagnostics.bPolicyWroteTargets, Diagnostics.bResetScheduled);
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("  MaxLinVel: Pelvis=%.1f Thighs=%.1f Spine=%.1f Feet=%.1f"), Diagnostics.MaxLinVelPelvis, Diagnostics.MaxLinVelThighs, Diagnostics.MaxLinVelSpine, Diagnostics.MaxLinVelFeet);
}

bool FPhysAnimBalanceReadyTransition::ShouldSuppressPolicy() const 
{ 
	// Freeze policy during entire bootstrap until stable settle confirmed (moving to phase 3)
	// Also keep suppressed if we failed to avoid spikes during abort recovery
	return (IsActive() && (InternalPhase == EBalanceReadyTransitionPhase::BRT_Handoff || InternalPhase == EBalanceReadyTransitionPhase::BRT_PostHandoffSettle)) || InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed; 
}
bool FPhysAnimBalanceReadyTransition::ShouldSuppressShell() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Handoff || InternalPhase == EBalanceReadyTransitionPhase::BRT_PostHandoffSettle; }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressPerturbations() const { return IsActive(); }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressResets() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Handoff; } // Only suppress reset on flip frame
bool FPhysAnimBalanceReadyTransition::ShouldSuppressMoveSmoke() const { return InternalPhase == EBalanceReadyTransitionPhase::BRT_Handoff || InternalPhase == EBalanceReadyTransitionPhase::BRT_PostHandoffSettle; }

float FPhysAnimBalanceReadyTransition::GetRootBodyModifierSoftSimAlpha() const { return 1.0f; }
float FPhysAnimBalanceReadyTransition::GetProximalControlSoftAlpha(FName BoneName) const { return 1.0f; }

bool FPhysAnimBalanceReadyTransition::ShouldKeepBoneKinematic(FName BoneName) const
{
	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_Failed) return false;
	
	// On failure, we keep distal bodies kinematic to restore safety
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		const FString BoneStr = BoneName.ToString().ToLower();

		if (BoneName == RootBoneName || 
			BoneStr.Contains(TEXT("spine")) || 
			BoneStr.Contains(TEXT("thigh")))
		{
			return false; // Allowed to simulate (or controlled safe mode)
		}
		return true; // Force kinematic
	}

	// Phase 1 (Handoff) and Phase 2 (PostHandoffSettle): Pelvis, Spine, Thighs simulate. Rest are kinematic.
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Handoff || 
		InternalPhase == EBalanceReadyTransitionPhase::BRT_PostHandoffSettle)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		const FString BoneStr = BoneName.ToString().ToLower();

		if (BoneName == RootBoneName || 
			BoneStr.Contains(TEXT("spine")) || 
			BoneStr.Contains(TEXT("thigh")))
		{
			return false; // Allowed to simulate
		}
		return true; // Keep kinematic
	}

	return false; // Phase 3 (DistalEnable) onwards: All allowed
}

float FPhysAnimBalanceReadyTransition::GetTransitionExtraDampingMultiplier() const
{
	if (!IsActive()) return 1.0f;
	// Increase damping during instability window
	return (InternalPhase == EBalanceReadyTransitionPhase::BRT_Handoff || InternalPhase == EBalanceReadyTransitionPhase::BRT_PostHandoffSettle) ? 2.0f : 1.0f;
}
