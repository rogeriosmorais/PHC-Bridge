#include "PhysAnimBalanceReadyTransitionPrivate.h"

bool FPhysAnimBalanceReadyTransition::IsProximal(FName BoneName)
{
	return BalanceTransitionSets::IsProximal(BoneName);
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
	if (!BalanceTransitionSets::IsProximal(BoneName))
	{
		return 1.0f;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		const bool bIsTick1 = PhaseTimeSeconds < 0.05f; // Estimated Tick 1 window
		if (bIsTick1 && (BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03"))
		{
			return 0.0f;
		}

		return FMath::Clamp(
			PhaseTimeSeconds / BalanceTransitionSets::Phase2AuthorityRampSeconds,
			0.0f,
			1.0f);
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		return FMath::Clamp(
			PhaseTimeSeconds / 0.25f,
			0.0f,
			1.0f);
	}

	return 1.0f;
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


float FPhysAnimBalanceReadyTransition::GetTransitionExtraDampingMultiplier(FName BoneName, const FPhysAnimStabilizationSettings& Settings) const
{
	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		return 1.0f;
	}

	const bool bInBootstrap = InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
	if (!bInBootstrap)
	{
		return Settings.BalanceActiveExtraDampingMultiplier;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		const bool bIsTick1 = PhaseTimeSeconds < 0.05f; // Estimated Tick 1 window
		if (bIsTick1 && (BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03"))
		{
			return Settings.BalanceBootstrapExtraDampingMultiplier * 2.0f;
		}
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		return Settings.BalanceBootstrapExtraDampingMultiplier * 2.0f;
	}

	return Settings.BalanceBootstrapExtraDampingMultiplier;
}



void FPhysAnimBalanceReadyTransition::ReconcileKinematicHoldSet(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings)
{
	if (!Owner || InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase2_RootOn || Phase2GuardTickCount > 2)
	{
		return;
	}

	const FName PreservedStabilizeBones[] = { PhysAnimBridge::GetRootBoneName(), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
	USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
	
	for (const FName BoneName : PreservedStabilizeBones)
	{
		const float OverriddenAlpha = GetProximalControlSoftAlpha(BoneName);
		const float OverriddenDamping = GetTransitionExtraDampingMultiplier(BoneName, Settings);

		// Zero out velocities to prevent the solver jump
		if (Mesh)
		{
			if (FBodyInstance* const BI = Mesh->GetBodyInstance(BoneName))
			{
				BI->SetLinearVelocity(FVector::ZeroVector, false);
				BI->SetAngularVelocityInRadians(FVector::ZeroVector, false);
			}
		}

		if (GVerbosePhase1Forensics != 0)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_PRESERVED_BONE_ZERO_VELOCITY bone=%s tick=%d softAlpha=%.4f extraDampingMultiplier=%.2f"),
				*BoneName.ToString(), Phase2GuardTickCount, OverriddenAlpha, OverriddenDamping);
		}
	}
}
