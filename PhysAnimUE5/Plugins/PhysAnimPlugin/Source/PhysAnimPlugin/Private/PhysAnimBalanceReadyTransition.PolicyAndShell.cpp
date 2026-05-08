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
	const bool bIsRootOrProximal =
		BoneName == PhysAnimBridge::GetRootBoneName() ||
		BalanceTransitionSets::IsProximal(BoneName);
	if (!bIsRootOrProximal)
	{
		return 1.0f;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn)
	{
		// RootOn should warm-start from the accepted live state, not apply a new
		// proximal control impulse on the same frame that root simulation is added.
		if (PhaseTimeSeconds < 0.05f)
		{
			return 0.0f;
		}

		return FMath::Clamp((PhaseTimeSeconds - 0.05f) / 0.10f, 0.0f, 1.0f);
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		// Feedback-Gated Authority Ramp:
		// We only allow authority to increase if the system is stable.
		// If stability is lost (IsPhase3Stable returns false), Phase3StableTickCount resets to 0, 
		// and authority is instantly revoked to allow the system to re-settle.
		
		float Alpha = 0.0f;
		if (Phase3KineticGateReleaseTickCount > BalanceTransitionSets::Phase3KineticGateReleaseGraceTicks)
		{
			// Reversible Authority Ramp (Replaces the fixed 20-tick ramp):
			// Alpha is managed in UpdateInternalState based on real-time stability metrics.
			Alpha = Phase3StableAlpha;
		}

		return Alpha;
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
		return BalanceTransitionSets::IsDistalLowerLimb(BoneName) ||
			BalanceTransitionSets::IsUpperBody(BoneName);
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
		if (bIsTick1 &&
			(BoneName == PhysAnimBridge::GetRootBoneName() ||
			 BoneName == "spine_01" || BoneName == "spine_02" || BoneName == "spine_03" ||
			 BoneName == "thigh_l" || BoneName == "thigh_r"))
		{
			// Flip-frame RootOn should not inject damping torques into the preserved
			// proximal set before the new root-sim state has even taken one step.
			return 0.0f;
		}
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		// Boost damping significantly during the kinetic gate release window.
		// Hold 5.0x damping until the system has maintained stability for at least 40 ticks, 
		// then ramp down to 2.0x over the next 40 ticks of stability.
		float KineticGraceMultiplier = 5.0f;
		if (Phase3StableTickCount > 40)
		{
			const float DampingRampAlpha = FMath::Clamp((float)(Phase3StableTickCount - 40) / 40.0f, 0.0f, 1.0f);
			KineticGraceMultiplier = FMath::Lerp(5.0f, 2.0f, DampingRampAlpha);
		}
		return Settings.BalanceBootstrapExtraDampingMultiplier * KineticGraceMultiplier;
	}

	return Settings.BalanceBootstrapExtraDampingMultiplier;
}


float FPhysAnimBalanceReadyTransition::GetTransitionDampingRatioMultiplier(FName BoneName, const FPhysAnimStabilizationSettings& Settings) const
{
	if (!IsActive() && InternalPhase != EBalanceReadyTransitionPhase::BRT_SafeDenied)
	{
		return 1.0f;
	}

	if (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		// Boost target orientation dampening during the settlement phase to mitigate the
		// energy burst caused by the simultaneous activation of 10 raw-sim bodies.
		return 2.0f;
	}

	return 1.0f;
}



void FPhysAnimBalanceReadyTransition::ReconcileKinematicHoldSet(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings)
{
	if (!Owner || InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase2_RootOn || Phase2GuardTickCount > 2)
	{
		return;
	}

	const FName PreservedStabilizeBones[] = { PhysAnimBridge::GetRootBoneName(), TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"), TEXT("thigh_l"), TEXT("thigh_r") };
	USkeletalMeshComponent* const Mesh = Owner->GetMeshComponent();
	
	for (const FName BoneName : PreservedStabilizeBones)
	{
		const float OverriddenAlpha = GetProximalControlSoftAlpha(BoneName);
		const float OverriddenDamping = GetTransitionExtraDampingMultiplier(BoneName, Settings);
		const int32 RawSimulating = (Mesh && Mesh->GetBodyInstance(BoneName) && Mesh->GetBodyInstance(BoneName)->IsInstanceSimulatingPhysics()) ? 1 : 0;

		if (GVerbosePhase1Forensics != 0)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_PRESERVED_BONE_ROUTING bone=%s tick=%d softAlpha=%.4f extraDampingMultiplier=%.2f rawSim=%d"),
				*BoneName.ToString(), Phase2GuardTickCount, OverriddenAlpha, OverriddenDamping, RawSimulating);
		}

		if (Phase2GuardTickCount == 1 && (BoneName == "thigh_l" || BoneName == "thigh_r"))
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE2_PRESERVED_THIGH_ROUTING bone=%s softAlpha=%.4f extraDampingMultiplier=%.2f policySuppressed=%d shellCorrectionActive=%d"),
				*BoneName.ToString(), OverriddenAlpha, OverriddenDamping,
				ShouldSuppressPolicy() ? 1 : 0,
				Owner->IsTransitionOwnedShellLocked() ? 1 : 0);
		}
	}
}
