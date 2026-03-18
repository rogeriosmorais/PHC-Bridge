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
	PhaseTimeSeconds = 0.0f;
	TotalTransitionTimeSeconds = 0.0f;
	LastLogTimeSeconds = -1.0;
	Diagnostics = {};
	bLatchedPelvisResetApplied = false;
	QuietHandoffCount = 0;

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
	// If policy is too high, we might want to wait for a ramp down, but BridgeActive doesn't do that.
	// We'll treat it as a QueueBlock if it's extremely unstable, but per Section 11,
	// we should probably just accept and let Phase 1 suppress it.
	
	const bool bPolicyCompatible = (PolicyAlpha < 0.1f) || (PolicyAlpha > 0.9f); // Allow either startup or fully-brought-up baseline
	if (!bPolicyCompatible)
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
		// Phase 1: Prepares topology.
		// Section 12 requirements: distal kinematic, capture baselines.
		if (PhaseTimeSeconds > 0.10f) 
		{
			if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
			{
				const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
				Diagnostics.BaselineRootLinVel = Mesh->GetPhysicsLinearVelocity(RootBoneName).Size();
				Diagnostics.BaselineRootAngVel = Mesh->GetPhysicsAngularVelocityInDegrees(RootBoneName).Size();
				
				// Section 12 Exit Criteria check (Invariants)
				const float MaxBaselineLin = 25.0f;
				const float MaxBaselineAng = 45.0f;
				if (Diagnostics.BaselineRootLinVel > MaxBaselineLin || Diagnostics.BaselineRootAngVel > MaxBaselineAng)
				{
					UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE1_REJECTED baseline_movement_too_high lin=%.1f ang=%.1f"), 
						Diagnostics.BaselineRootLinVel, Diagnostics.BaselineRootAngVel);
					Diagnostics.FailureReason = TEXT("phase1_baseline_movement_too_high");
					SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
					return;
				}
			}

			SetPhase(EBalanceReadyTransitionPhase::BRT_Phase2_RootOn, Owner);
			return;
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		// Phase 2: Post root-on settle check.
		// Section 13: Transition flipped sim in SetPhase. Now check for spikes.
		const float SpikeLinThreshold = 150.0f; 
		const float SpikeAngThreshold = 180.0f;

		if (Diagnostics.RootSpeed > SpikeLinThreshold || Diagnostics.RootAngularSpeed > SpikeAngThreshold)
		{
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] PHASE2_ABORT_SPIKE lin=%.1f ang=%.1f"), Diagnostics.RootSpeed, Diagnostics.RootAngularSpeed);
			Diagnostics.FailureReason = TEXT("phase2_root_on_spike");
			SetPhase(EBalanceReadyTransitionPhase::BRT_Failed, Owner);
			return;
		}

		// Mode remains in Phase 2 for a fixed short window before checking settle convergence in Phase 3
		if (PhaseTimeSeconds > 0.1f)
		{
			SetPhase(EBalanceReadyTransitionPhase::BRT_Phase3_Settle, Owner);
		}
	}
	else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		// Phase 3: Bounded settle duration (Section 14).
		if (bReadyThisFrame)
		{
			StableHoldAccumulatedSeconds += DeltaTime;
			if (StableHoldAccumulatedSeconds >= 0.25f) // Required settle success duration
			{
				SetPhase(EBalanceReadyTransitionPhase::BRT_Succeeded, Owner);
			}
		}
		else
		{
			StableHoldAccumulatedSeconds = 0.0f;
		}

		if (PhaseTimeSeconds > 2.0f)
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
			// Phase 2: Turns pelvis/root sim ON (Section 13)
			if (Owner)
			{
				if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
				{
					if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName()))
					{
						if (!PelvisBody->IsInstanceSimulatingPhysics())
						{
							UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_ROOT_ON"));
							
							// Capture pre-flip diagnostics
							Diagnostics.PelvisLinearVelPre = PelvisBody->GetUnrealWorldVelocity();
							Diagnostics.PelvisAngularVelPre = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());

							PelvisBody->SetInstanceSimulatePhysics(true);
							Diagnostics.bSimFlipped = true;
							
							CaptureFlipDiagnostics(Owner);
						}
					}
				}
			}
		}

		if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Succeeded)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_SUCCESS."));
		}
		else if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
		{
			UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnimBalance] TRANSITION_FAILED reason=%s"), *Diagnostics.FailureReason);
			
			// Section 16: Recovery Contract (Restore coherent BridgeActive state)
			if (Owner)
			{
				USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
				if (Mesh)
				{
					int32 SimCount = 0;
					for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
					{
						if (FBodyInstance* BI = Mesh->GetBodyInstance(BoneName))
						{
							if (BI->IsInstanceSimulatingPhysics()) SimCount++;
						}
					}
					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] TRANSITION_RECOVERY simCount=%d policySuppressed=1"), SimCount);
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

	if (Diagnostics.RootSpeed > 5.0f)
	{
		OutReason = TEXT("root_linear_above_settle");
		return false;
	}

	if (Diagnostics.RootAngularSpeed > 1.0f)
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
	// Freeze policy during entire bootstrap until stable settle confirmed (Section 12, 13)
	return (IsActive() && (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)) || InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed; 
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

	// Phase 1 (Prepare) and Phase 2 (RootOn): Pelvis, Spine, Thighs simulate. Rest are kinematic.
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || 
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
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
	return (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare || InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn) ? 2.0f : 1.0f;
}
