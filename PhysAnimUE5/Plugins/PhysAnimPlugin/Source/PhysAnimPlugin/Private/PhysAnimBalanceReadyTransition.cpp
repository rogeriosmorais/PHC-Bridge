#include "PhysAnimBalanceReadyTransition.h"
#include "PhysAnimComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

void FPhysAnimBalanceReadyTransition::Start(const FString& InRequestReason, UPhysAnimComponent* Owner)
{
	if (IsActive())
	{
		return;
	}

	RequestReason = InRequestReason;
	StableHoldAccumulatedSeconds = 0.0f;
	PhaseTimeSeconds = 0.0f;
	TotalTransitionTimeSeconds = 0.0f;
	LastLogTimeSeconds = -1.0;
	Diagnostics = {};
	bLatchedPelvisResetApplied = false;
	
	if (Owner)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		if (USkeletalMeshComponent* Mesh = Owner->GetMeshComponent())
		{
			if (FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName))
			{
				bLastRootSimulating = PelvisBody->IsInstanceSimulatingPhysics();
			}
		}
		bLastPendingResetsEmpty = Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();
	}

	SetPhase(EBalanceReadyTransitionPhase::Handoff);
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] BalanceReadyTransition started. reason=%s"), *RequestReason);
}

void FPhysAnimBalanceReadyTransition::Cancel()
{
	if (Phase != EBalanceReadyTransitionPhase::Inactive)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] BalanceReadyTransition cancelled. phase=%d"), static_cast<int32>(Phase));
		SetPhase(EBalanceReadyTransitionPhase::Inactive);
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

	if (Phase == EBalanceReadyTransitionPhase::Handoff)
	{
		const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
		FBodyInstance* PelvisBody = Owner->GetMeshComponent() ? Owner->GetMeshComponent()->GetBodyInstance(RootBoneName) : nullptr;
		const bool bIsSimulating = PelvisBody && PelvisBody->IsInstanceSimulatingPhysics();
		
		if (Owner->bPelvisResetAppliedThisTick)
		{
			bLatchedPelvisResetApplied = true;
			Diagnostics.bResetApplied = true;
		}

		if (bIsSimulating && bLatchedPelvisResetApplied)
		{
			// Basic sanity threshold check (must be below these to even begin settling)
			const float MaxSanitySpeed = 100.0f; // 100 cm/s
			const float MaxSanityAngSpeed = 180.0f; // 180 deg/s
			
			if (Diagnostics.RootSpeed < MaxSanitySpeed && Diagnostics.RootAngularSpeed < MaxSanityAngSpeed)
			{
				Diagnostics.bSimFlipped = true;
				CaptureFlipDiagnostics(Owner);
				SetPhase(EBalanceReadyTransitionPhase::PostHandoffSettle);
			}
		}

		if (PhaseTimeSeconds > 2.0f) // Timeout
		{
			Diagnostics.FailureReason = TEXT("handoff_timeout");
			SetPhase(EBalanceReadyTransitionPhase::Failed);
		}
		else if (!bIsSimulating || !bLatchedPelvisResetApplied)
		{
			// Capture pre-flip velocities while waiting
			if (PelvisBody)
			{
				Diagnostics.PelvisLinearVelPre = PelvisBody->GetUnrealWorldVelocity();
				Diagnostics.PelvisAngularVelPre = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());
			}
		}
	}
	else if (Phase == EBalanceReadyTransitionPhase::PostHandoffSettle)
	{
		if (bReadyThisFrame)
		{
			SetPhase(EBalanceReadyTransitionPhase::RestoreControls);
		}
		else if (PhaseTimeSeconds > 2.0f)
		{
			Diagnostics.FailureReason = TEXT("post_handoff_settle_timeout");
			SetPhase(EBalanceReadyTransitionPhase::Failed);
		}
	}
	else if (Phase == EBalanceReadyTransitionPhase::RestoreControls)
	{
		// In a real implementation this would progressively re-enable systems.
		// For now, we transition once stable.
		if (bReadyThisFrame)
		{
			SetPhase(EBalanceReadyTransitionPhase::FinalSettle);
		}
		else if (PhaseTimeSeconds > 2.0f)
		{
			Diagnostics.FailureReason = TEXT("restore_controls_timeout");
			SetPhase(EBalanceReadyTransitionPhase::Failed);
		}
	}
	else if (Phase == EBalanceReadyTransitionPhase::FinalSettle)
	{
		if (bReadyThisFrame)
		{
			StableHoldAccumulatedSeconds += DeltaTime;
			if (StableHoldAccumulatedSeconds >= Owner->BalanceBridgeActivePreEntrySettleSeconds)
			{
				SetPhase(EBalanceReadyTransitionPhase::Succeeded);
			}
		}
		else
		{
			StableHoldAccumulatedSeconds = 0.0f;
		}

		if (PhaseTimeSeconds > 5.0f)
		{
			Diagnostics.FailureReason = TEXT("final_settle_timeout");
			SetPhase(EBalanceReadyTransitionPhase::Failed);
		}
	}

	if (bShouldLog && IsActive())
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Transition Progress: phase=%d time=%.2f ready=%s reason=%s"), 
			static_cast<int32>(Phase), PhaseTimeSeconds, bReadyThisFrame ? TEXT("true") : TEXT("false"), *BlockReason);
		LastLogTimeSeconds = CurrentTime;
	}

	// Update latches for next frame
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Owner->GetMeshComponent() ? Owner->GetMeshComponent()->GetBodyInstance(RootBoneName) : nullptr;
	bLastRootSimulating = PelvisBody && PelvisBody->IsInstanceSimulatingPhysics();
	bLastPendingResetsEmpty = Owner->GetPendingBodyModifierCachedResetNames().IsEmpty();
}

void FPhysAnimBalanceReadyTransition::SetPhase(EBalanceReadyTransitionPhase NewPhase)
{
	if (Phase != NewPhase)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Transition Phase Change: %d -> %d"), static_cast<int32>(Phase), static_cast<int32>(NewPhase));
		Phase = NewPhase;
		PhaseTimeSeconds = 0.0f;
		StableHoldAccumulatedSeconds = 0.0f;

		if (Phase == EBalanceReadyTransitionPhase::Succeeded)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Transition SUCCESS latched."));
		}
		else if (Phase == EBalanceReadyTransitionPhase::Failed)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] Transition FAILED: %s"), *Diagnostics.FailureReason);
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
	
	Diagnostics.bShellContributed = Owner->BridgeShellState.AcceptedPlanarVelocityCmPerSecond.Size2D() > 0.1f;
	Diagnostics.bPolicyWroteTargets = Owner->bPolicyTargetsAppliedLastFrame;
	Diagnostics.bResetScheduled = !Owner->PendingBodyModifierCachedResetNames.IsEmpty();

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

bool FPhysAnimBalanceReadyTransition::ShouldSuppressPolicy() const { return Phase == EBalanceReadyTransitionPhase::Handoff; }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressShell() const { return Phase == EBalanceReadyTransitionPhase::Handoff; }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressPerturbations() const { return IsActive(); }
bool FPhysAnimBalanceReadyTransition::ShouldSuppressResets() const { return Phase == EBalanceReadyTransitionPhase::Handoff; }
