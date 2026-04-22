#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::CheckRuntimeInstability(
	float DeltaTime,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	FString& OutError)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved.");
		return false;
	}

	FPhysAnimRuntimeInstabilitySettings InstabilitySettings;
	InstabilitySettings.bEnableAutomaticFailStop = EffectiveSettings.bEnableInstabilityFailStop;
	InstabilitySettings.MaxRootHeightDeltaCm = EffectiveSettings.MaxRootHeightDeltaCm;
	InstabilitySettings.MaxRootLinearSpeedCmPerSecond = EffectiveSettings.MaxRootLinearSpeedCmPerSecond;
	InstabilitySettings.MaxRootAngularSpeedDegPerSecond = EffectiveSettings.MaxRootAngularSpeedDegPerSecond;
	InstabilitySettings.UnstableGracePeriodSeconds = EffectiveSettings.InstabilityGracePeriodSeconds;

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	const FVector RootLocationCm = SkeletalMesh->GetBoneLocation(RootBoneName, EBoneSpaces::WorldSpace);
	const FVector RootLinearVelocityCmPerSecond = SkeletalMesh->GetPhysicsLinearVelocity(RootBoneName);
	const FVector RootAngularVelocityDegPerSecond = SkeletalMesh->GetPhysicsAngularVelocityInDegrees(RootBoneName);
	const AActor* const OwnerActor = GetOwner();
	const bool bPreserveGameplayShell = ShouldPreserveGameplayShellDuringBridgeActive(
		IsMovementSmokeModeEnabled(),
		PhysAnimComponentInternal::CVarPhysAnimAllowCharacterMovementInBridgeActive.GetValueOnGameThread() != 0);
	const FVector OwnerLocationCm = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
	const FVector OwnerLinearVelocityCmPerSecond = OwnerActor ? OwnerActor->GetVelocity() : FVector::ZeroVector;
	FVector EffectiveRootLocationCm = RootLocationCm;
	FVector EffectiveRootLinearVelocityCmPerSecond = RootLinearVelocityCmPerSecond;
	ResolveRuntimeInstabilityRootFrame(
		bPreserveGameplayShell,
		RootLocationCm,
		RootLinearVelocityCmPerSecond,
		OwnerLocationCm,
		OwnerLinearVelocityCmPerSecond,
		EffectiveRootLocationCm,
		EffectiveRootLinearVelocityCmPerSecond);

	const bool bBalanceScenarioAllowsPostImpactGrace =
		IsBalanceActiveState(RuntimeState) &&
		BalanceScenarios.IsValidIndex(ActiveBalanceScenarioIndex) &&
		BalanceScenarios[ActiveBalanceScenarioIndex].bTriggered &&
		!BalanceScenarios[ActiveBalanceScenarioIndex].Name.Contains(TEXT("NoPush")) &&
		LastBalanceScenarioImpactTimeSeconds >= 0.0 &&
		(GetWorld()->GetTimeSeconds() - LastBalanceScenarioImpactTimeSeconds) < 0.5;

	const bool bIsEarlySettleGrace =
		(IsBalanceActiveState(RuntimeState) || RuntimeState == EPhysAnimRuntimeState::BalanceEntry_Settle) &&
		GetWorld() &&
		(GetWorld()->GetTimeSeconds() - BalanceScenarioStartTimeSeconds) < 2.0;

	if (bBalanceScenarioAllowsPostImpactGrace || bIsEarlySettleGrace)
	{
		InstabilitySettings.MaxRootLinearSpeedCmPerSecond *= 50.0f;
		InstabilitySettings.MaxRootAngularSpeedDegPerSecond *= 50.0f;
		InstabilitySettings.MaxRootHeightDeltaCm *= 5.0f;
		InstabilitySettings.UnstableGracePeriodSeconds = FMath::Max(InstabilitySettings.UnstableGracePeriodSeconds, 2.0f);
	}
	
	if (LastRuntimeInstabilityDiagnostics.UnstableAccumulatedSeconds > 0.01f)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] INSTABILITY_ACCUMULATING state=%s unstable=%.3fs limit=%.3fs linSpeed=%.1f/%.1f angSpeed=%.1f/%.1f"),
			GetRuntimeStateName(RuntimeState),
			LastRuntimeInstabilityDiagnostics.UnstableAccumulatedSeconds,
			InstabilitySettings.UnstableGracePeriodSeconds,
			LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond,
			InstabilitySettings.MaxRootLinearSpeedCmPerSecond,
			LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond,
			InstabilitySettings.MaxRootAngularSpeedDegPerSecond);
	}

	FString InstabilityError;
	const bool bStable = EvaluateRuntimeInstability(
		EffectiveRootLocationCm,
		EffectiveRootLinearVelocityCmPerSecond,
		RootAngularVelocityDegPerSecond,
		DeltaTime,
		InstabilitySettings,
		RuntimeInstabilityState,
		LastRuntimeInstabilityDiagnostics,
		InstabilityError);
	LastRuntimeInstabilityDiagnostics.RawRootLocationCm = RootLocationCm;
	LastRuntimeInstabilityDiagnostics.RawRootLinearVelocityCmPerSecondVector = RootLinearVelocityCmPerSecond;

	TArray<FPhysAnimBodyInstabilitySample> BodySamples;
	if (GatherRuntimeInstabilityBodySamples(BodySamples))
	{
		if (bPreserveGameplayShell && OwnerActor)
		{
			for (FPhysAnimBodyInstabilitySample& Sample : BodySamples)
			{
				Sample.Location -= OwnerLocationCm;
				Sample.LinearVelocity -= OwnerLinearVelocityCmPerSecond;
			}
		}

		const FVector ReferenceRootLocationCm = RuntimeInstabilityState.bHasReferenceRootLocation
			? RuntimeInstabilityState.ReferenceRootLocation
			: EffectiveRootLocationCm;
		PhysAnimBridge::EvaluatePerBodyInstabilitySamples(
			BodySamples,
			ReferenceRootLocationCm,
			LastRuntimeInstabilityDiagnostics);
	}
	if (!bStable)
	{
		OutError = InstabilityError;
	}

	return bStable;
}


bool UPhysAnimComponent::GatherRuntimeInstabilityBodySamples(TArray<FPhysAnimBodyInstabilitySample>& OutSamples) const
{
	OutSamples.Reset();

	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return false;
	}

	OutSamples.Reserve(PhysAnimBridge::GetRequiredBodyModifierBoneNames().Num());
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName);
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			continue;
		}

		FPhysAnimBodyInstabilitySample& Sample = OutSamples.AddDefaulted_GetRef();
		Sample.BoneName = BoneName;
		Sample.Location = BodyInstance->GetUnrealWorldTransform().GetLocation();
		Sample.LinearVelocity = BodyInstance->GetUnrealWorldVelocity();
		Sample.AngularVelocity = FMath::RadiansToDegrees(BodyInstance->GetUnrealWorldAngularVelocityInRadians());

		bool bIsSimulating = BodyInstance->IsInstanceSimulatingPhysics();
		if (BalanceReadyTransition.ShouldKeepBoneKinematic(BoneName, ResolveEffectiveStabilizationSettings()))
		{
			bIsSimulating = false;
		}
		Sample.bIsSimulatingPhysics = bIsSimulating;
	}

	return true;
}


void UPhysAnimComponent::ResolveRuntimeInstabilityRootFrame(
	bool bPreserveGameplayShell,
	const FVector& RootLocationCm,
	const FVector& RootLinearVelocityCmPerSecond,
	const FVector& OwnerLocationCm,
	const FVector& OwnerLinearVelocityCmPerSecond,
	FVector& OutEffectiveRootLocationCm,
	FVector& OutEffectiveRootLinearVelocityCmPerSecond)
{
	OutEffectiveRootLocationCm = RootLocationCm;
	OutEffectiveRootLinearVelocityCmPerSecond = RootLinearVelocityCmPerSecond;

	if (!bPreserveGameplayShell)
	{
		return;
	}

	OutEffectiveRootLocationCm -= OwnerLocationCm;
	OutEffectiveRootLinearVelocityCmPerSecond -= OwnerLinearVelocityCmPerSecond;
}

