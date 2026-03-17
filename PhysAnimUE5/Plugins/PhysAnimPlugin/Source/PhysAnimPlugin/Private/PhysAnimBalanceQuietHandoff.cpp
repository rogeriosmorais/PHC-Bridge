#include "PhysAnimBalanceQuietHandoff.h"
#include "PhysAnimComponent.h"
#include "PhysAnimBridge.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBalanceQuietHandoff, Log, All);

namespace PhysAnimBalanceQuietHandoffInternal
{
	static constexpr float QuietRootLinearThresholdCmPerSec = 80.0f;
	static constexpr float QuietRootAngularThresholdDegPerSec = 120.0f;
	static constexpr float QuietShellVelocityThresholdCmPerSec = 50.0f;
	static constexpr float QuietShellOffsetThresholdCm = 8.0f;
	static constexpr float QuietRequiredSeconds = 0.20f;
	static constexpr float MaxQuietWaitSeconds = 1.25f;
	static constexpr float RestorePolicyRampSeconds = 0.20f;
}

void FPhysAnimBalanceQuietHandoff::Start(const FString& InReason, UPhysAnimComponent* InOwner)
{
	Cancel();
	StartReason = InReason;
	SetPhase(EPhase::SuspendExternalSystems, InOwner, TEXT("start"));
}

void FPhysAnimBalanceQuietHandoff::Cancel()
{
	Phase = EPhase::Inactive;
	StartReason.Reset();
	PhaseStartTimeSeconds = -1.0;
	LastProgressLogTimeSeconds = -1.0;
	QuietAccumulatedSeconds = 0.0;
	bSystemsSuspended = false;
	bRootFlipRequested = false;
	bPolicyRestoreStarted = false;
}

bool FPhysAnimBalanceQuietHandoff::IsActive() const
{
	return Phase != EPhase::Inactive && Phase != EPhase::Succeeded && Phase != EPhase::Failed;
}

bool FPhysAnimBalanceQuietHandoff::HasSucceeded() const
{
	return Phase == EPhase::Succeeded;
}

bool FPhysAnimBalanceQuietHandoff::HasFailed() const
{
	return Phase == EPhase::Failed;
}

FPhysAnimBalanceQuietHandoff::EPhase FPhysAnimBalanceQuietHandoff::GetPhase() const
{
	return Phase;
}

const FString& FPhysAnimBalanceQuietHandoff::GetStartReason() const
{
	return StartReason;
}

void FPhysAnimBalanceQuietHandoff::SetPhase(EPhase NewPhase, UPhysAnimComponent* Owner, const TCHAR* Context)
{
	if (Phase == NewPhase)
	{
		return;
	}

	const int32 OldValue = static_cast<int32>(Phase);
	const int32 NewValue = static_cast<int32>(NewPhase);
	Phase = NewPhase;
	PhaseStartTimeSeconds = Owner && Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : -1.0;
	LastProgressLogTimeSeconds = -1.0;
	QuietAccumulatedSeconds = 0.0;

	UE_LOG(LogPhysAnimBridge, Warning,
		TEXT("[PhysAnimBalance] QuietHandoff Phase Change: %d -> %d (%s)"),
		OldValue,
		NewValue,
		Context);
}

void FPhysAnimBalanceQuietHandoff::Tick(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, UPhysAnimComponent* Owner)
{
	if (!Owner || !IsActive())
	{
		return;
	}

	switch (Phase)
	{
	case EPhase::SuspendExternalSystems:
		if (TickSuspendExternalSystems(Owner))
		{
			SetPhase(EPhase::FlipRootSimulation, Owner, TEXT("systems_suspended"));
		}
		break;

	case EPhase::FlipRootSimulation:
		if (TickFlipRootSimulation(Owner))
		{
			SetPhase(EPhase::WaitForQuiet, Owner, TEXT("root_flip_done"));
		}
		break;

	case EPhase::WaitForQuiet:
		if (TickWaitForQuiet(DeltaTime, EffectiveSettings, Owner))
		{
			SetPhase(EPhase::RestorePolicy, Owner, TEXT("quiet_window_passed"));
		}
		break;

	case EPhase::RestorePolicy:
		if (TickRestorePolicy(DeltaTime, Owner))
		{
			SetPhase(EPhase::Succeeded, Owner, TEXT("restore_complete"));
		}
		break;

	default:
		break;
	}
}

bool FPhysAnimBalanceQuietHandoff::TickSuspendExternalSystems(UPhysAnimComponent* Owner)
{
	if (bSystemsSuspended)
	{
		return true;
	}

	// These helpers are expected to be added on UPhysAnimComponent.
	Owner->SetBalanceTransitionPolicySuppressed(true);
	Owner->SetBalanceTransitionShellSuppressed(true);
	Owner->SetBalanceTransitionMovementSuppressed(true);
	Owner->SetBalanceTransitionTargetWritesSuppressed(true);
	Owner->SetBalanceTransitionCachedResetsSuppressed(true);

	bSystemsSuspended = true;
	UE_LOG(LogPhysAnimBridge, Warning,
		TEXT("[PhysAnimBalance] QuietHandoff: external systems suspended before root sim flip."));
	return true;
}

bool FPhysAnimBalanceQuietHandoff::TickFlipRootSimulation(UPhysAnimComponent* Owner)
{
	if (bRootFlipRequested)
	{
		return true;
	}

	Owner->SetBalanceTransitionRootSimulationRequested(true);
	Owner->ForceBalanceTransitionRootBodyDynamic();
	Owner->ZeroBalanceTransitionRootVelocities();
	bRootFlipRequested = true;

	UE_LOG(LogPhysAnimBridge, Warning,
		TEXT("[PhysAnimBalance] QuietHandoff: requested root sim flip with policy/shell suspended."));
	return true;
}

bool FPhysAnimBalanceQuietHandoff::TickWaitForQuiet(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, UPhysAnimComponent* Owner)
{
	const double WorldTime = Owner->GetWorld() ? Owner->GetWorld()->GetTimeSeconds() : -1.0;
	const double Elapsed = (PhaseStartTimeSeconds >= 0.0 && WorldTime >= 0.0) ? (WorldTime - PhaseStartTimeSeconds) : 0.0;

	const float RootLinear = Owner->GetBalanceTransitionRootLinearSpeedCmPerSec();
	const float RootAngular = Owner->GetBalanceTransitionRootAngularSpeedDegPerSec();
	const float ShellOffset = Owner->GetBalanceTransitionShellOffsetDeltaCm();
	const float ShellVelocity = Owner->GetBalanceTransitionShellVelocityDeltaCmPerSec();

	const bool bQuietNow =
		RootLinear <= PhysAnimBalanceQuietHandoffInternal::QuietRootLinearThresholdCmPerSec &&
		RootAngular <= PhysAnimBalanceQuietHandoffInternal::QuietRootAngularThresholdDegPerSec &&
		ShellOffset <= PhysAnimBalanceQuietHandoffInternal::QuietShellOffsetThresholdCm &&
		ShellVelocity <= PhysAnimBalanceQuietHandoffInternal::QuietShellVelocityThresholdCmPerSec;

	if (bQuietNow)
	{
		QuietAccumulatedSeconds += DeltaTime;
	}
	else
	{
		QuietAccumulatedSeconds = 0.0;
	}

	if (WorldTime >= 0.0 && (LastProgressLogTimeSeconds < 0.0 || WorldTime - LastProgressLogTimeSeconds >= 0.25))
	{
		LastProgressLogTimeSeconds = WorldTime;
		UE_LOG(LogPhysAnimBridge, Warning,
			TEXT("[PhysAnimBalance] QuietHandoff: quiet=%d settle=%.2f/%.2f rootLin=%.1f rootAng=%.1f shellOff=%.1f shellVel=%.1f"),
			bQuietNow ? 1 : 0,
			QuietAccumulatedSeconds,
			PhysAnimBalanceQuietHandoffInternal::QuietRequiredSeconds,
			RootLinear,
			RootAngular,
			ShellOffset,
			ShellVelocity);
	}

	if (QuietAccumulatedSeconds >= PhysAnimBalanceQuietHandoffInternal::QuietRequiredSeconds)
	{
		return true;
	}

	if (Elapsed >= PhysAnimBalanceQuietHandoffInternal::MaxQuietWaitSeconds)
	{
		UE_LOG(LogPhysAnimBridge, Warning,
			TEXT("[PhysAnimBalance] QuietHandoff FAILED: timeout rootLin=%.1f rootAng=%.1f shellOff=%.1f shellVel=%.1f"),
			RootLinear,
			RootAngular,
			ShellOffset,
			ShellVelocity);
		SetPhase(EPhase::Failed, Owner, TEXT("quiet_timeout"));
	}

	return false;
}

bool FPhysAnimBalanceQuietHandoff::TickRestorePolicy(float DeltaTime, UPhysAnimComponent* Owner)
{
	if (!bPolicyRestoreStarted)
	{
		bPolicyRestoreStarted = true;
		Owner->SetBalanceTransitionTargetWritesSuppressed(false);
		Owner->BeginBalanceTransitionPolicyRestore(PhysAnimBalanceQuietHandoffInternal::RestorePolicyRampSeconds);
		UE_LOG(LogPhysAnimBridge, Warning,
			TEXT("[PhysAnimBalance] QuietHandoff: restoring policy after quiet window."));
	}

	if (!Owner->IsBalanceTransitionPolicyRestoreComplete())
	{
		return false;
	}

	Owner->SetBalanceTransitionPolicySuppressed(false);
	Owner->SetBalanceTransitionShellSuppressed(false);
	Owner->SetBalanceTransitionMovementSuppressed(false);
	Owner->SetBalanceTransitionCachedResetsSuppressed(false);
	Owner->SetBalanceTransitionRootSimulationRequested(false);
	return true;
}
