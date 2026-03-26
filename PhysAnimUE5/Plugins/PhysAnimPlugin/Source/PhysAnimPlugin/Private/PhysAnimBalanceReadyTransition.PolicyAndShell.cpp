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
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase2_RootOn || !BalanceTransitionSets::IsProximal(BoneName))
	{
		return 1.0f;
	}

	return FMath::Clamp(
		PhaseTimeSeconds / BalanceTransitionSets::Phase2AuthorityRampSeconds,
		0.0f,
		1.0f);
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
		// Force spine chain to be kinematic during the first tick of RootOn to avoid spikes
		if ((BoneName == TEXT("spine_01") || BoneName == TEXT("spine_02") || BoneName == TEXT("spine_03")) && 
			InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn && Phase2GuardTickCount == 1)
		{
			return true;
		}

		return BalanceTransitionSets::IsDistalLowerLimb(BoneName);
	}
	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed)
	{
		return BalanceTransitionSets::IsTransitionCritical(BoneName) || BalanceTransitionSets::IsUpperBody(BoneName);
	}

	return false;
}


float FPhysAnimBalanceReadyTransition::GetTransitionExtraDampingMultiplier(const FPhysAnimStabilizationSettings& Settings) const
{
	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		return 1.0f;
	}

	const bool bInBootstrap = InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn;
	return bInBootstrap ? Settings.BalanceBootstrapExtraDampingMultiplier : Settings.BalanceActiveExtraDampingMultiplier;
}



void FPhysAnimBalanceReadyTransition::ReconcileKinematicHoldSet(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings)
{
	if (!Owner || InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase2_RootOn || Phase2GuardTickCount != 1)
	{
		return;
	}


	const FName TargetBones[] = { TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03") };
	for (const FName& BoneName : TargetBones)
	{
		if (ShouldKeepBoneKinematic(BoneName, Settings))
		{
			USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
			if (FBodyInstance* const BI = Mesh ? Mesh->GetBodyInstance(BoneName) : nullptr)
			{
				const bool bRawSimBefore = BI->IsInstanceSimulatingPhysics();
				if (bRawSimBefore)
				{
					BI->SetInstanceSimulatePhysics(false, true);
					const bool bRawSimAfter = BI->IsInstanceSimulatingPhysics();

					UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] PHASE2_SPINE_KINEMATIC_RECONCILE bone=%s frame=%llu rootOnTick=%d rawSimBefore=%d rawSimAfter=%d modifier=Kinematic"),
						*BoneName.ToString(),
						GFrameCounter,
						static_cast<int32>(Phase2GuardTickCount),
						bRawSimBefore ? 1 : 0,
						bRawSimAfter ? 1 : 0);
				}
			}
		}
	}
}
