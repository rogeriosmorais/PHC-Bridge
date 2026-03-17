#include "PhysAnimBalanceQuietHandoff.h"

#include "PhysAnimComponent.h"
#include "PhysAnimBridge.h"
#include "Components/SkeletalMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"

void FPhysAnimBalanceQuietHandoff::Start(const FString& InReason, UPhysAnimComponent* /*Owner*/)
{
	Phase = EPhase::RequestSim;
	bActive = true;
	bSucceeded = false;
	bFailed = false;
	StartReason = InReason;
	FailureReason.Reset();
	ElapsedSeconds = 0.0;
	StableSeconds = 0.0;
	UE_LOG(LogTemp, Warning, TEXT("[PhysAnimBalance] BalanceReadyTransition started. reason=%s"), *StartReason);
}

void FPhysAnimBalanceQuietHandoff::Cancel()
{
	Phase = EPhase::Inactive;
	bActive = false;
	bSucceeded = false;
	bFailed = false;
	StartReason.Reset();
	FailureReason.Reset();
	ElapsedSeconds = 0.0;
	StableSeconds = 0.0;
}

void FPhysAnimBalanceQuietHandoff::Fail(const FString& Reason)
{
	Phase = EPhase::Failed;
	bFailed = true;
	bSucceeded = false;
	FailureReason = Reason;
	UE_LOG(LogTemp, Warning, TEXT("[PhysAnimBalance] Transition FAILED: %s"), *FailureReason);
}

void FPhysAnimBalanceQuietHandoff::Tick(float DeltaTime, UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& /*EffectiveSettings*/)
{
	if (!bActive || bSucceeded || bFailed || !Owner)
	{
		return;
	}

	ElapsedSeconds += DeltaTime;

	USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
	if (!Mesh)
	{
		Fail(TEXT("missingMesh"));
		return;
	}

	FBodyInstance* const RootBody = Mesh->GetBodyInstance(PhysAnimBridge::GetRootBoneName());
	if (!RootBody)
	{
		Fail(TEXT("missingRootBody"));
		return;
	}

	if (Phase == EPhase::RequestSim)
	{
		Phase = EPhase::WaitForSettle;
		UE_LOG(LogTemp, Warning, TEXT("[PhysAnimBalance] Transition Phase Change: 0 -> 1"));
	}

	const bool bRootSim = RootBody->IsInstanceSimulatingPhysics();
	const float RootLinear = RootBody->GetUnrealWorldVelocity().Size();
	const float RootAngular = FMath::RadiansToDegrees(RootBody->GetUnrealWorldAngularVelocityInRadians()).Size();

	if (!bRootSim)
	{
		StableSeconds = 0.0;
		if (ElapsedSeconds >= 2.0)
		{
			Fail(TEXT("handoff_timeout"));
		}
		return;
	}

	constexpr float QuietLinearThreshold = 60.0f;
	constexpr float QuietAngularThreshold = 120.0f;
	constexpr float QuietSettleSeconds = 0.20f;

	if (RootLinear <= QuietLinearThreshold && RootAngular <= QuietAngularThreshold)
	{
		StableSeconds += DeltaTime;
		if (StableSeconds >= QuietSettleSeconds)
		{
			Phase = EPhase::Succeeded;
			bSucceeded = true;
			bFailed = false;
			UE_LOG(LogTemp, Warning, TEXT("[PhysAnimBalance] Transition SUCCEEDED: rootLin=%.1f rootAng=%.1f"), RootLinear, RootAngular);
		}
	}
	else
	{
		StableSeconds = 0.0;
		if (ElapsedSeconds >= 2.0)
		{
			Fail(TEXT("handoff_timeout"));
		}
	}
}
