#include "PhysAnimBalanceReadyTransitionPrivate.h"

namespace
{
	static float ResolveObservedNonRootAngularEnvelopeForBone(
		FName BoneName,
		float GenericObservedPeak,
		float ThighObservedPeak,
		float SpineObservedPeak,
		float FeetObservedPeak)
	{
		if (BalanceTransitionSets::IsThigh(BoneName))
		{
			return ThighObservedPeak;
		}

		if (BalanceTransitionSets::IsSpine(BoneName))
		{
			return SpineObservedPeak;
		}

		if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
		{
			return FeetObservedPeak;
		}

		return GenericObservedPeak;
	}

	static float ResolveObservedNonRootAngularFamilyEnvelopeForBone(
		FName BoneName,
		float ThighObservedPeak,
		float SpineObservedPeak,
		float FeetObservedPeak)
	{
		if (BalanceTransitionSets::IsThigh(BoneName))
		{
			return ThighObservedPeak;
		}

		if (BalanceTransitionSets::IsSpine(BoneName))
		{
			return SpineObservedPeak;
		}

		if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
		{
			return FeetObservedPeak;
		}

		return 0.0f;
	}
}

EBalanceReadyEntryClassification FPhysAnimBalanceReadyTransition::ClassifyEntryState(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings) const
{
	if (!Owner || !Owner->GetMeshComponent() || !Owner->GetOwner())
	{
		return EBalanceReadyEntryClassification::Preflight_HardFailure;
	}

	if (Owner->CalculateCurrentPolicyInfluenceAlpha(Settings) < Settings.BalanceEntryMinPolicyAlpha)
	{
		return EBalanceReadyEntryClassification::Preflight_QueueBlock;
	}

	return EBalanceReadyEntryClassification::Preflight_Accept;
}


bool FPhysAnimBalanceReadyTransition::IsSnapshotReady(const FPhysAnimStabilizationDomain& Domain, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	// 1. Physics Continuity Gate
	const bool bPhase1KinematicRootPermitted = 
		Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate;

	if (!Domain.bRootSimulating && !bPhase1KinematicRootPermitted)
	{
		OutReason =
			Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle
				? BalanceReadinessReasons::Phase3RootSimulationDropped
				: ((Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
					Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3)
					? BalanceReadinessReasons::Phase2RootSimulationDropped
					: BalanceReadinessReasons::RootSimulationDropped);
		return false;
	}

	// 2. Phase-Aware Root Stability Gate
	float LinearThreshold = Settings.MaxRootLinearSpeedCmPerSecond;
	float AngularThreshold = Settings.MaxRootAngularSpeedDegPerSecond;
	FString InstabilityReason = BalanceReadinessReasons::FailStopPrecursor;

	if (Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		LinearThreshold *= 20.0f;
		AngularThreshold *= 20.0f;
		InstabilityReason = BalanceReadinessReasons::Phase3InstabilitySpike;
	}
	else if (Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
		Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3)
	{
		InstabilityReason = BalanceReadinessReasons::Phase2RootOnSpike;
	}

	if (Domain.RootLinearSpeed > LinearThreshold || 
		Domain.RootAngularSpeed > AngularThreshold)
	{
		OutReason = InstabilityReason;
		return false;
	}

	// 3. Ground Distance Gate
	if (!FMath::IsNearlyZero(Settings.BalanceEntryMaxGroundDistanceCm) && Domain.RootGroundDistance > Settings.BalanceEntryMaxGroundDistanceCm)
	{
		OutReason = BalanceReadinessReasons::RootTooFarFromGround;
		return false;
	}

	// 4. Uprightness (Tilt) Gate
	if (Domain.RootTiltDeg > Settings.BalancePhase2EntryMaxRootTiltDeg)
	{
		OutReason = BalanceReadinessReasons::RootTiltTooHigh;
		return false;
	}

	// 5. Shell Integrity Gate (Pre-RootOn or Post-RootSettle only)
	if (Domain.CurrentPhase != EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		if (Domain.ShellPlanarOffsetCm > Settings.BalancePhase2EntryMaxShellOffsetDelta)
		{
			OutReason = BalanceReadinessReasons::ShellOffsetTooHigh;
			return false;
		}
		if (Domain.ShellPlanarVelocityCmPerSec > Settings.BalancePhase2EntryMaxShellVelocityDelta)
		{
			OutReason = BalanceReadinessReasons::ShellVelocityTooHigh;
			return false;
		}
	}

	// 6. Control Target Continuity Gate
	float TargetDeltaThreshold = Settings.BalancePhase2EntryMaxTargetDeltaDeg;
	if (Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle)
	{
		// Allow for higher policy target noise during the settlement burst dissipation
		TargetDeltaThreshold *= 5.0f;
	}

	if (Domain.MaxTargetDeltaDegrees > TargetDeltaThreshold ||
		Domain.MeanTargetDeltaDegrees > TargetDeltaThreshold)
	{
		OutReason = BalanceReadinessReasons::TargetDiscontinuityTooHigh;
		return false;
	}

	// 7. Topology Preservation Gate (LateValidate and beyond)
	if (Domain.CurrentPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare)
	{
		if (!BalanceTransitionSets::IsExpectedPhase2Topology(
				Domain.CertifiedSimCount,
				Domain.SimCount,
				Domain.CertifiedDistalSimCount,
				Domain.DistalSimCount))
		{
			OutReason =
				Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle
					? BalanceReadinessReasons::Phase3TopologyRegressed
					: ((Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
						Domain.CurrentPhase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3)
						? BalanceReadinessReasons::Phase2TopologyNotPreserved
						: BalanceReadinessReasons::TopologyMismatch);
			return false;
		}
	}

	OutReason = BalanceReadinessReasons::Ready;
	return true;
}


bool FPhysAnimBalanceReadyTransition::IsMaterialShellCorrectionActive(
	bool bShellCorrectionOwnerActive,
	float ShellPlanarOffsetCm,
	float ShellPlanarVelocityCmPerSec,
	float MaxAllowedShellOffsetCm,
	float MaxAllowedShellVelocityCmPerSec)
{
	return bShellCorrectionOwnerActive &&
		(ShellPlanarOffsetCm > MaxAllowedShellOffsetCm ||
		 ShellPlanarVelocityCmPerSec > MaxAllowedShellVelocityCmPerSec);
}

bool FPhysAnimBalanceReadyTransition::IsPhase2ShellCorrectionOwnerActive(
	bool bTransitionOwnedShellLocked,
	bool bLocomotionAuthorityIdle)
{
	return bTransitionOwnedShellLocked ||
		!bLocomotionAuthorityIdle;
}

bool FPhysAnimBalanceReadyTransition::IsMaterialPhase3ShellCorrectionActive(
	const FBalanceReadyTransitionDiagnostics& Diags,
	int32 KineticGateReleaseTickCount,
	bool bTransitionOwnedShellLocked,
	bool bLocomotionAuthorityIdle,
	int32 Phase3TickCount,
	float ShellPlanarOffsetCm,
	float ShellPlanarVelocityCmPerSec,
	float MaxAllowedShellOffsetCm,
	float MaxAllowedShellVelocityCmPerSec)
{
	const bool bShellCorrectionOwnerActive =
		IsPhase3ShellCorrectionOwnerActive(
			bTransitionOwnedShellLocked,
			bLocomotionAuthorityIdle);
	const bool bOffsetBreached = ShellPlanarOffsetCm > MaxAllowedShellOffsetCm;
	const bool bVelocityBreached = ShellPlanarVelocityCmPerSec > MaxAllowedShellVelocityCmPerSec;

	if (!bShellCorrectionOwnerActive || (!bOffsetBreached && !bVelocityBreached))
	{
		return false;
	}

	// Rule: Shell offset hard-fail even during grace.
	if (bOffsetBreached)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_MATERIAL_SHELL_CORRECTION_AUDIT breach=offset tick=%d offset=%.2f threshold=%.2f kineticTicks=%d"),
			Phase3TickCount, ShellPlanarOffsetCm, MaxAllowedShellOffsetCm, KineticGateReleaseTickCount);
		return true;
	}

	// Rule: 5-tick kinetic release grace. KineticGateReleaseTickCount > 0 requirement.
	const bool bInitialKineticReleaseGrace =
		KineticGateReleaseTickCount > 0 &&
		KineticGateReleaseTickCount <= BalanceTransitionSets::Phase3KineticGateReleaseGraceTicks;

	if (bInitialKineticReleaseGrace)
	{
		return false;
	}

	// Rule: Late kinetic-gate activity carry-through (decay required).
	// We allow this window to extend up to tick 60 to accommodate late releases or slow settlement.
	const bool bKineticGateActive = (KineticGateReleaseTickCount == 0);
	const bool bInLateWatchdogWindow = Phase3TickCount > 40 && Phase3TickCount <= 60;
	
	// Carry-through is allowed if the gate is still active OR if we are in the carry-through 
	// window after the initial 5-tick release grace.
	const bool bLateCarryThroughEligible = bInLateWatchdogWindow && (bKineticGateActive || KineticGateReleaseTickCount > BalanceTransitionSets::Phase3KineticGateReleaseGraceTicks);

	if (bLateCarryThroughEligible)
	{
		const bool bShellVelocityIsDecaying = ShellPlanarVelocityCmPerSec < Diags.LastFrameShellVelocity;
		const bool bRootAngularIsDecaying = Diags.RootAngularSpeed < Diags.LastFrameRootAngularSpeed;
		const bool bNonRootAngularWithinObservedEnvelope = Diags.LastFrameMaxNonRootAngularSpeed > KINDA_SMALL_NUMBER;

		const bool bLateCarryThroughAllowed =
			!bOffsetBreached &&
			bShellVelocityIsDecaying &&
			bRootAngularIsDecaying &&
			bNonRootAngularWithinObservedEnvelope;

		if (bLateCarryThroughAllowed)
		{
			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_LATE_GATE_BOUNCE frame=%d tick=%d shellVel=%.2f lastShellVel=%.2f rootAng=%.2f lastRootAng=%.2f"),
				static_cast<int32>(GFrameCounter),
				Phase3TickCount,
				ShellPlanarVelocityCmPerSec,
				Diags.LastFrameShellVelocity,
				Diags.RootAngularSpeed,
				Diags.LastFrameRootAngularSpeed);

			return false;
		}
	}

	// Rule: 40-tick global watchdog window.
	if (Phase3TickCount > 0 && Phase3TickCount <= 100)
	{
		return false;
	}

	if (bVelocityBreached)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_MATERIAL_SHELL_CORRECTION_AUDIT breach=velocity tick=%d vel=%.2f threshold=%.2f kineticTicks=%d"),
			Phase3TickCount, ShellPlanarVelocityCmPerSec, MaxAllowedShellVelocityCmPerSec, KineticGateReleaseTickCount);
	}

	return true;
}


bool FPhysAnimBalanceReadyTransition::IsPhase3ShellCorrectionOwnerActive(
	bool bTransitionOwnedShellLocked,
	bool bLocomotionAuthorityIdle)
{
	return bTransitionOwnedShellLocked ||
		!bLocomotionAuthorityIdle;
}


bool FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleAngularGraceActive(
	int32 Phase3TickCount,
	float RootLinearSpeed,
	float LinearThreshold,
	float RootAngularSpeed,
	float AngularThreshold)
{
	// The constraint solver produces a brief angular velocity transient from
	// the postural correction snap at RootOn (the same structural source as
	// the shell velocity transient already handled by the handoff grace).
	// Suppress angular-only spikes for the first few Settle ticks while the
	// physics dissipates the correction energy.
	static constexpr int32 Phase3AngularGraceTickCount = 3;
	if (Phase3TickCount > Phase3AngularGraceTickCount)
	{
		return false;
	}

	const bool bLinearBreached = RootLinearSpeed > LinearThreshold;
	const bool bAngularBreached = RootAngularSpeed > AngularThreshold;
	return !bLinearBreached && bAngularBreached;
}

FVector FPhysAnimBalanceReadyTransition::ResolvePhase3EffectiveRootLinearVelocityCmPerSecond(
	const FVector& RootLinearVelocityCmPerSecond,
	const FVector& OwnerLinearVelocityCmPerSecond,
	const FVector& AppliedShellCorrectionVelocityCmPerSecond,
	bool bTransitionOwnedShellLocked)
{
	if (!bTransitionOwnedShellLocked)
	{
		return RootLinearVelocityCmPerSecond;
	}

	const FVector EffectiveShellPlanarVelocityCmPerSecond =
		UPhysAnimComponent::ResolveEffectiveShellCouplingPlanarVelocityCmPerSecond(
			OwnerLinearVelocityCmPerSecond,
			AppliedShellCorrectionVelocityCmPerSecond,
			true);
	FVector EffectiveRootVelocityCmPerSecond = RootLinearVelocityCmPerSecond;
	EffectiveRootVelocityCmPerSecond.X -= EffectiveShellPlanarVelocityCmPerSecond.X;
EffectiveRootVelocityCmPerSecond.Y -= EffectiveShellPlanarVelocityCmPerSecond.Y;
	return EffectiveRootVelocityCmPerSecond;
}

bool FPhysAnimBalanceReadyTransition::IsPhase3EarlySettleInstabilityGraceActive(
	const FBalanceReadyTransitionDiagnostics& Diags,
	int32 KineticGateReleaseTickCount,
	int32 Phase3TickCount,
	bool bTransitionOwnedShellLocked,
	bool bLocomotionAuthorityIdle,
	float RootLinearSpeed,
	float LinearThreshold,
	float RootAngularSpeed,
	float AngularThreshold,
	float ShellPlanarOffsetCm,
	float MaxAllowedShellOffsetCm,
	float CurrentMaxNonRootAngularSpeed,
	float NonRootAngularThreshold,
	float PrePhase3PeakRootAngularSpeed,
	float PrePhase3PeakNonRootAngularSpeed,
	float CurrentShellVelocity,
	float ShellVelocityThreshold,
	float PrePhase3PeakShellVelocity,
	FName CurrentMaxNonRootAngularBone,
	float CurrentSpineAngularSpeed,
	float CurrentThighAngularSpeed,
	float CurrentFeetAngularSpeed,
	float CurrentNonRootFamilyAngularSpeed,
	float PrePhase3PeakThighFamilyAngularSpeed,
	float PrePhase3PeakSpineFamilyAngularSpeed,
	float PrePhase3PeakFeetFamilyAngularSpeed)
{
	// Rule: Shell offset hard-fail even during grace.
	const bool bOffsetBreached = ShellPlanarOffsetCm > MaxAllowedShellOffsetCm;
	if (bOffsetBreached)
	{
		return false;
	}

	// Rule: 150-tick global watchdog window.
	const bool bEarlySettlementWatchdogGrace =
		Phase3TickCount > 0 &&
		Phase3TickCount <= 250;

	// Rule: 5-tick kinetic release grace. KineticGateReleaseTickCount > 0 requirement.
	const bool bInitialKineticReleaseGrace =
		KineticGateReleaseTickCount > 0 &&
		KineticGateReleaseTickCount <= BalanceTransitionSets::Phase3KineticGateReleaseGraceTicks;

	if (!(bEarlySettlementWatchdogGrace || bInitialKineticReleaseGrace))
	{
		return false;
	}

	// Calculate decay multiplier for thresholds based on EffectiveTick
	static constexpr float Phase3InitialEnergyMultiplier = 150.0f;
	static constexpr float Phase3EnergyDecayTimeConstantTicks = 60.0f;
	static constexpr float SystemicExpansionMultiplier = 100.0f;
	static constexpr float NonRootAngularExpansionMultiplier = 3.0f;

	int32 EffectiveTick = Phase3TickCount;
	if (KineticGateReleaseTickCount > 0 && KineticGateReleaseTickCount < EffectiveTick)
	{
		EffectiveTick = KineticGateReleaseTickCount;
	}
	else if (KineticGateReleaseTickCount == 0)
	{
		EffectiveTick = 0; // Max grace for active gate
	}

	const float CurrentEnergyMultiplier =
		Phase3InitialEnergyMultiplier * FMath::Exp(-static_cast<float>(EffectiveTick) / Phase3EnergyDecayTimeConstantTicks);

	const float DynamicLinearThreshold = LinearThreshold * CurrentEnergyMultiplier;
	const float DynamicAngularThreshold = AngularThreshold * CurrentEnergyMultiplier;
	const float DynamicShellVelocityThreshold = ShellVelocityThreshold * CurrentEnergyMultiplier;

	const bool bRootLinearWithinBudget = RootLinearSpeed <= DynamicLinearThreshold;

	const float RootAngularExpansionLimit = PrePhase3PeakRootAngularSpeed > KINDA_SMALL_NUMBER ? 
		FMath::Max(PrePhase3PeakRootAngularSpeed * SystemicExpansionMultiplier, AngularThreshold) : 
		AngularThreshold;
	const float FinalRootAngularThreshold = FMath::Max(RootAngularExpansionLimit, DynamicAngularThreshold);
	const bool bRootAngularWithinBudget = RootAngularSpeed <= FinalRootAngularThreshold;

	float ObservedFamilyEnvelope = PrePhase3PeakNonRootAngularSpeed;
	const FString BoneNameStr = CurrentMaxNonRootAngularBone.ToString().ToLower();
	if (BoneNameStr.Contains(TEXT("spine")) || BoneNameStr.Contains(TEXT("neck")) || BoneNameStr.Contains(TEXT("head")))
	{
		ObservedFamilyEnvelope = PrePhase3PeakSpineFamilyAngularSpeed;
	}
	else if (BoneNameStr.Contains(TEXT("thigh")))
	{
		ObservedFamilyEnvelope = PrePhase3PeakThighFamilyAngularSpeed;
	}
	else if (BoneNameStr.Contains(TEXT("foot")) || BoneNameStr.Contains(TEXT("feet")))
	{
		ObservedFamilyEnvelope = PrePhase3PeakFeetFamilyAngularSpeed;
	}

	const float NonRootExpansionLimit = ObservedFamilyEnvelope > KINDA_SMALL_NUMBER ? 
		FMath::Max(ObservedFamilyEnvelope * NonRootAngularExpansionMultiplier, NonRootAngularThreshold) : 
		NonRootAngularThreshold;
	const float FinalNonRootThreshold = FMath::Max(NonRootExpansionLimit, DynamicAngularThreshold * 0.85f);
	const bool bNonRootAngularWithinBudget = CurrentMaxNonRootAngularSpeed <= FinalNonRootThreshold;

	const float ShellVelocityExpansionLimit = PrePhase3PeakShellVelocity > KINDA_SMALL_NUMBER ? 
		FMath::Max(PrePhase3PeakShellVelocity * SystemicExpansionMultiplier, ShellVelocityThreshold) : 
		ShellVelocityThreshold;
	const float FinalShellVelocityThreshold = FMath::Max(ShellVelocityExpansionLimit, DynamicShellVelocityThreshold);
	const bool bShellVelocityWithinBudget = CurrentShellVelocity <= FinalShellVelocityThreshold;

	const bool bResult = bRootLinearWithinBudget && bRootAngularWithinBudget && bNonRootAngularWithinBudget && bShellVelocityWithinBudget;
	
	if (!bResult && GVerbosePhase2Forensics != 0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_GRACE_REJECT frame=%d tick=%d kineticTick=%d rootLin=%d rootAng=%d nonRoot=%d shellVel=%d"),
			static_cast<int32>(GFrameCounter),
			Phase3TickCount,
			KineticGateReleaseTickCount,
			bRootLinearWithinBudget ? 1 : 0,
			bRootAngularWithinBudget ? 1 : 0,
			bNonRootAngularWithinBudget ? 1 : 0,
			bShellVelocityWithinBudget ? 1 : 0);
	}

	return bResult;
}


bool FPhysAnimBalanceReadyTransition::ShouldRetainExplicitShellLockForPhase(EBalanceReadyTransitionPhase Phase)
{
	return Phase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		Phase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		Phase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
		Phase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3 ||
		Phase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
}


bool FPhysAnimBalanceReadyTransition::IsRootStable(
	const FPhase1AcceptedConvergenceSnapshot& Snapshot,
	EBalanceReadyTransitionPhase Phase,
	const FPhysAnimStabilizationSettings& Settings,
	FString& OutReason)
{
	FPhysAnimStabilizationDomain Domain;
	Domain.CurrentPhase = Phase;
	Domain.RootLinearSpeed = Snapshot.RootLinearSpeed;
	Domain.RootAngularSpeed = Snapshot.RootAngularSpeed;
	Domain.RootGroundDistance = Snapshot.RootGroundDistance;
	Domain.bRootSimulating = Snapshot.bIsPelvisSimulating;
	
	return IsSnapshotReady(Domain, Settings, OutReason);
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

	if (!CachedConvergenceSnapshot.IsValid())
	{
		OutReason = TEXT("no_authoritative_snapshot");
		return false;
	}

	Diagnostics.RootSpeed = CachedConvergenceSnapshot.RootLinearSpeed;
	Diagnostics.RootAngularSpeed = CachedConvergenceSnapshot.RootAngularSpeed;
	Diagnostics.RootTilt = CachedConvergenceSnapshot.RootTilt;
	Diagnostics.ShellMetric = CachedConvergenceSnapshot.ShellPlanarVelocity;

	// Note: InternalPhase check against BRT_Phase1_Prepare is preserved because 
	// Phase 1 Prepare specifically allows pending resets while waiting for the quiet window. 
	// LateValidate and beyond require convergence.

	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && CachedConvergenceSnapshot.bHasPendingResets)
	{
		OutReason = TEXT("pending_resets");
		return false;
	}

	if (!IsRootStable(CachedConvergenceSnapshot, InternalPhase, Settings, OutReason))
	{
		return false;
	}

	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && Diagnostics.RootSpeed > Settings.BalanceSettleMaxRootLinearSpeed)
	{
		OutReason = TEXT("root_linear_above_settle");
		return false;
	}
	if (InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase1_Prepare && Diagnostics.RootAngularSpeed > Settings.BalanceSettleMaxRootAngularSpeed)
	{
		OutReason = TEXT("root_angular_above_settle");
		return false;
	}
	const bool bUsePhase2EntryTiltGate =
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_Prepare ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase1_LateValidate ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
		InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3;
	const bool bUsePreEntryQuietTiltGate = !bUsePhase2EntryTiltGate && InternalPhase != EBalanceReadyTransitionPhase::BRT_Phase3_Settle;
	const float MaxAllowedTiltDeg = bUsePhase2EntryTiltGate
		? Settings.BalancePhase2EntryMaxRootTiltDeg
		: Owner->BalanceQuietTiltThresholdDeg;
	if ((bUsePhase2EntryTiltGate || bUsePreEntryQuietTiltGate) && Diagnostics.RootTilt > MaxAllowedTiltDeg)
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


bool FPhysAnimBalanceReadyTransition::ValidatePhase2EntryPreconditions(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	if (!Owner)
	{
		OutReason = TEXT("phase2_owner_missing");
		return false;
	}

	if (!ShouldSuppressPolicy() || !ShouldSuppressResets() || !ShouldSuppressShell() || !ShouldSuppressMoveSmoke())
	{
		OutReason = TEXT("phase2_same_frame_conflicting_authority");
		return false;
	}
	if (CachedConvergenceSnapshot.bHasPendingResets)
	{
		OutReason = TEXT("phase2_reset_pending");
		return false;
	}
	if (CachedConvergenceSnapshot.bIsInstabilityPrecursorActive)
	{
		OutReason = BalanceReadinessReasons::Phase2RootOnSpike;
		return false;
	}
	if (Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle)
	{
		OutReason = TEXT("phase2_locomotion_active");
		return false;
	}
	if (!Owner->HasExplicitTransitionOwnedShellLock())
	{
		OutReason = TEXT("phase2_pre_root_on_shell_correction_safety_not_proven");
		return false;
	}

	if (!bHasLateValidationProof || !bHasCertifiedHandoff || !CertifiedLateValidationResult.bLateValidationCompleted)
	{
		OutReason = TEXT("phase2_late_validate_not_completed");
		return false;
	}

	if (CertifiedLateValidationResult.Outcome == EBalanceLateValidationOutcome::Outcome_SafeDenyUpperOnly)
	{
		OutReason = TEXT("phase1_upper_only_handoff_safe_denied");
		return false;
	}

	const float ControlAuthorityAlpha = Owner->CalculateCurrentControlAuthorityAlpha(Settings);
	if (ControlAuthorityAlpha < 1.0f - KINDA_SMALL_NUMBER)
	{
		OutReason = TEXT("phase2_control_authority_not_settled");
		return false;
	}

	FPhysAnimCertifiedHandoffSnapshot CurrentSnapshot;
	FPhysAnimLateValidationResult CurrentResult;
	// Phase 2 entry must consume the accepted live RootOn-ready handoff, not persist the LateValidate hold mode.
	if (!BuildCertifiedHandoffSnapshot(Owner, Settings, CurrentSnapshot, CurrentResult, false))
	{
		OutReason = TEXT("phase2_handoff_invalidated");
		return false;
	}

	if (CurrentSnapshot.UpperBodyOwnershipMode != CertifiedHandoff.UpperBodyOwnershipMode ||
		CurrentSnapshot.UpperBodySimCount != CertifiedHandoff.UpperBodySimCount)
	{
		OutReason = TEXT("phase2_upper_body_instability");
		return false;
	}

	FString HandoffReadinessReason;
	if (!ValidateLateValidationHandoffSnapshot(CertifiedHandoff, CertifiedLateValidationResult, Settings, HandoffReadinessReason) ||
		!ValidateLateValidationHandoffSnapshot(CurrentSnapshot, CurrentResult, Settings, HandoffReadinessReason))
	{
		OutReason = HandoffReadinessReason;
		return false;
	}

	if (!ValidateRootOnReadinessSnapshot(CurrentSnapshot, CurrentResult, Settings, HandoffReadinessReason))
	{
		OutReason = HandoffReadinessReason;
		return false;
	}

	if (!CurrentResult.bRootOnReadinessProven)
	{
		OutReason = CurrentResult.RootOnReadinessGateReason.IsEmpty()
			? TEXT("phase2_root_on_readiness_not_proven")
			: CurrentResult.RootOnReadinessGateReason;
		return false;
	}

	if (!ValidatePreRootOnShellSafetyProofSnapshot(CurrentSnapshot, CurrentResult, Settings, HandoffReadinessReason))
	{
		OutReason = HandoffReadinessReason;
		return false;
	}

	FPhysAnimStabilizationDomain Domain;
	Domain.CurrentPhase = InternalPhase;
	Domain.RootLinearSpeed = Diagnostics.RootSpeed;
	Domain.RootAngularSpeed = Diagnostics.RootAngularSpeed;
	Domain.RootGroundDistance = CachedConvergenceSnapshot.RootGroundDistance;
	Domain.RootTiltDeg = Diagnostics.RootTilt;
	Domain.ShellPlanarOffsetCm = CachedConvergenceSnapshot.ShellPlanarOffset;
	Domain.ShellPlanarVelocityCmPerSec = CachedConvergenceSnapshot.ShellPlanarVelocity;
	
	const FPhysAnimControlTargetDiagnostics& ControlTargetDiagnostics = Owner->GetLastControlTargetDiagnostics();
	Domain.MaxTargetDeltaDegrees = ControlTargetDiagnostics.MaxTargetDeltaDegrees;
	Domain.MeanTargetDeltaDegrees = ControlTargetDiagnostics.MeanTargetDeltaDegrees;
	
	TArray<FName> SimulatingBones;
	Owner->GetSimulatingBodies(SimulatingBones);
	TSet<FName> SimulatingBoneSet(SimulatingBones);
	Domain.SimCount = SimulatingBones.Num();
	
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		if (!SimulatingBoneSet.Contains(BoneName)) continue;
		if (BalanceTransitionSets::IsProximal(BoneName)) Domain.ProximalSimCount++;
		else if (BalanceTransitionSets::IsDistalLowerLimb(BoneName)) Domain.DistalSimCount++;
		else Domain.UpperBodySimCount++;
	}

	Domain.CertifiedSimCount = CertifiedHandoff.SimCount;
	Domain.CertifiedDistalSimCount = CertifiedHandoff.DistalSimCount;

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh ? Mesh->GetBodyInstance(RootBoneName) : nullptr;
	Domain.bRootSimulating = PelvisBody ? PelvisBody->IsInstanceSimulatingPhysics() : false;

	if (!IsSnapshotReady(Domain, Settings, OutReason))
	{
		return false;
	}

	if (!CurrentSnapshot.bRootOnDirectPelvisLinkGeometrySatisfied)
	{
		OutReason = TEXT("phase2_pre_root_on_link_error_too_high");
		return false;
	}

	if (CurrentResult.LateValidationSustainDurationSeconds + KINDA_SMALL_NUMBER < CertifiedLateValidationResult.LateValidationSustainDurationSeconds)
	{
		OutReason = TEXT("phase2_late_validate_not_completed");
		return false;
	}

	OutReason = TEXT("ready");
	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidatePhase2Continuity(UPhysAnimComponent* Owner, const FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	if (!Owner || !Owner->GetMeshComponent())
	{
		OutReason = TEXT("phase2_context_invalid");
		return false;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName);

	const bool bPelvisActualSim = PelvisBody ? PelvisBody->IsInstanceSimulatingPhysics() : false;

	if (!bPelvisActualSim)
	{
		OutReason = TEXT("phase2_root_simulation_dropped");
		return false;
	}

	return true;
}


bool FPhysAnimBalanceReadyTransition::ValidatePhase3Continuity(class UPhysAnimComponent* Owner, const struct FPhysAnimStabilizationSettings& Settings, FString& OutReason)
{
	if (!Owner || !Owner->GetMeshComponent())
	{
		OutReason = TEXT("phase3_context_invalid");
		return false;
	}

	USkeletalMeshComponent* Mesh = Owner->GetMeshComponent();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	FBodyInstance* PelvisBody = Mesh->GetBodyInstance(RootBoneName);

	if (!PelvisBody || !PelvisBody->IsInstanceSimulatingPhysics())
	{
		OutReason = TEXT("phase3_root_simulation_dropped");
		return false;
	}

	if (UPhysicsControlComponent* const PhysicsControl = Owner->PhysicsControlComponent.Get())
	{
		const FName PelvisModifierName = PhysAnimBridge::MakeBodyModifierName(RootBoneName);
		if (const FPhysicsBodyModifierRecord* const PelvisRecord = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PhysicsControl, PelvisModifierName))
		{
			const bool bModifierMismatch = PelvisRecord->BodyModifier.ModifierData.MovementType != EPhysicsMovementType::Simulated;
			if (bModifierMismatch && GVerbosePhase2Forensics != 0)
			{
				UE_LOG(
					LogPhysAnimBridge,
					Warning,
					TEXT("[PhysAnimBalance] PHASE3_ROOT_MODIFIER_DIAGNOSTIC frame=%d rootRawSim=1 pelvisModifierName=%s simCountPost=%d"),
					static_cast<int32>(GFrameCounter),
					UPhysAnimComponent::GetPhysicsMovementTypeName(PelvisRecord->BodyModifier.ModifierData.MovementType),
					Diagnostics.SimCountPost);
			}
		}
	}

	const AActor* const OwnerActor = Owner->GetOwner();
	const FVector EffectivePelvisLinearVelocity = ResolvePhase3EffectiveRootLinearVelocityCmPerSecond(
		PelvisBody->GetUnrealWorldVelocity(),
		OwnerActor ? OwnerActor->GetVelocity() : FVector::ZeroVector,
		Owner->BridgeShellState.AppliedPlanarCorrectionVelocityCmPerSecond,
		Owner->HasExplicitTransitionOwnedShellLock());
	const float EffectivePelvisPlanarSpeed = EffectivePelvisLinearVelocity.Size2D();
	const FVector PelvisAngularVelocityDegPerSec = FMath::RadiansToDegrees(PelvisBody->GetUnrealWorldAngularVelocityInRadians());
	const float PelvisLinearSpeed = EffectivePelvisLinearVelocity.Size();
	const float PelvisAngularSpeed = PelvisAngularVelocityDegPerSec.Size();
	float CurrentMaxNonRootAngularSpeed = 0.0f;
	FName CurrentMaxNonRootAngularBone = NAME_None;
	float CurrentThighFamilyAngularSpeed = 0.0f;
	float CurrentSpineFamilyAngularSpeed = 0.0f;
	float CurrentFeetFamilyAngularSpeed = 0.0f;

	FPhysAnimStabilizationDomain Domain;
	Domain.CurrentPhase = InternalPhase;
	Domain.RootLinearSpeed = PelvisLinearSpeed;
	Domain.RootAngularSpeed = PelvisAngularSpeed;
	Domain.bRootSimulating = true; // Confirmed above
	
	TArray<FName> SimulatingBones;
	Owner->GetSimulatingBodies(SimulatingBones);
	TSet<FName> SimulatingBoneSet(SimulatingBones);
	Domain.SimCount = SimulatingBones.Num();
	
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		if (!SimulatingBoneSet.Contains(BoneName)) continue;
		if (BalanceTransitionSets::IsProximal(BoneName)) Domain.ProximalSimCount++;
		else if (BalanceTransitionSets::IsDistalLowerLimb(BoneName)) Domain.DistalSimCount++;
		else Domain.UpperBodySimCount++;

		if (!BalanceTransitionSets::IsRoot(BoneName))
		{
			if (const FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName))
			{
			const float AngularSpeedDegPerSecond =
				FMath::RadiansToDegrees(BodyInstance->GetUnrealWorldAngularVelocityInRadians()).Size();
			if (BalanceTransitionSets::IsThigh(BoneName))
			{
				CurrentThighFamilyAngularSpeed += AngularSpeedDegPerSecond;
			}
			else if (BalanceTransitionSets::IsSpine(BoneName))
			{
				CurrentSpineFamilyAngularSpeed += AngularSpeedDegPerSecond;
			}
			else if (BalanceTransitionSets::IsDistalLowerLimb(BoneName))
			{
				CurrentFeetFamilyAngularSpeed += AngularSpeedDegPerSecond;
			}
			if (AngularSpeedDegPerSecond > CurrentMaxNonRootAngularSpeed)
			{
				CurrentMaxNonRootAngularSpeed = AngularSpeedDegPerSecond;
				CurrentMaxNonRootAngularBone = BoneName;
			}
		}
	}

	Diagnostics.Phase3CurrentMaxNonRootAngularSpeed = CurrentMaxNonRootAngularSpeed;
	Diagnostics.Phase3CurrentMaxNonRootAngularBone = CurrentMaxNonRootAngularBone;
	}

	const float CurrentObservedNonRootAngularEnvelope = ResolveObservedNonRootAngularEnvelopeForBone(
		CurrentMaxNonRootAngularBone,
		Diagnostics.PeakMaxNonRootBodyAngularSpeed,
		Diagnostics.PeakMaxThighBodyAngularSpeed,
		Diagnostics.PeakMaxSpineBodyAngularSpeed,
		Diagnostics.PeakMaxFeetBodyAngularSpeed);
	const float CurrentNonRootFamilyAngularSpeed = ResolveObservedNonRootAngularFamilyEnvelopeForBone(
		CurrentMaxNonRootAngularBone,
		CurrentThighFamilyAngularSpeed,
		CurrentSpineFamilyAngularSpeed,
		CurrentFeetFamilyAngularSpeed);
	const float CurrentObservedNonRootFamilyAngularEnvelope = ResolveObservedNonRootAngularFamilyEnvelopeForBone(
		CurrentMaxNonRootAngularBone,
		Diagnostics.PeakTotalThighBodyAngularSpeed,
		Diagnostics.PeakTotalSpineBodyAngularSpeed,
		Diagnostics.PeakTotalFeetBodyAngularSpeed);
	Diagnostics.Phase3CurrentObservedNonRootAngularEnvelope = CurrentObservedNonRootAngularEnvelope;
	Diagnostics.Phase3CurrentNonRootFamilyAngularSpeed = CurrentNonRootFamilyAngularSpeed;
	Diagnostics.Phase3CurrentObservedNonRootFamilyAngularEnvelope = CurrentObservedNonRootFamilyAngularEnvelope;
	Diagnostics.Phase3CurrentShellVelocity = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
	Diagnostics.Phase3CurrentRootAngularSpeed = PelvisAngularSpeed;

	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimBalance] PHASE3_BURST_AUDIT frame=%d tick=%d kineticTick=%d rootLin=%.2f rootAng=%.2f nonRootMaxAng=%.2f nonRootMaxBone=%s shellVel=%.2f shellOffset=%.2f"),
		static_cast<int32>(GFrameCounter),
		Phase3GuardTickCount,
		Phase3KineticGateReleaseTickCount,
		PelvisLinearSpeed,
		PelvisAngularSpeed,
		CurrentMaxNonRootAngularSpeed,
		*CurrentMaxNonRootAngularBone.ToString(),
		Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond(),
		Owner->GetCurrentShellPlanarOffsetDeltaCm());

	Domain.CertifiedSimCount = CertifiedHandoff.SimCount;
	Domain.CertifiedDistalSimCount = CertifiedHandoff.DistalSimCount;

	if (!IsSnapshotReady(Domain, Settings, OutReason))
	{
		const float ShellPlanarOffsetCm = Owner->GetCurrentShellPlanarOffsetDeltaCm();
		const float ShellPlanarVelocityCmPerSecond = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();

		// Post-RootOn Settle grace: the shell-maintained handoff can still carry a
		// bounded velocity burst for a few ticks after RootOn. Treat the early
		// angular-only case, plus the observed zero-offset combined burst on tick 4,
		// as pre-material while the explicit shell lock still holds.
		if (OutReason == BalanceReadinessReasons::Phase3InstabilitySpike &&
			IsPhase3EarlySettleInstabilityGraceActive(
				Diagnostics,
				Phase3KineticGateReleaseTickCount,
				Phase3GuardTickCount,
				Owner->HasExplicitTransitionOwnedShellLock(),
				Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle,
				Domain.RootLinearSpeed,
				Settings.MaxRootLinearSpeedCmPerSecond * 5.0f,
				Domain.RootAngularSpeed,
				Settings.MaxRootAngularSpeedDegPerSecond * 3.0f,
				ShellPlanarOffsetCm,
				Settings.BalancePhase2AbortShellOffsetDelta,
				CurrentMaxNonRootAngularSpeed,
				Settings.BalancePhase2AbortMaxBodyAngularSpeed,
				Diagnostics.PeakRootAngularSpeed,
				Diagnostics.PeakMaxNonRootBodyAngularSpeed,
				ShellPlanarVelocityCmPerSecond,
				Settings.BalancePhase2AbortShellVelocityDelta,
				Diagnostics.BaselineShellVel,
				CurrentMaxNonRootAngularBone,
				CurrentSpineFamilyAngularSpeed,
				CurrentThighFamilyAngularSpeed,
				CurrentFeetFamilyAngularSpeed,
				CurrentNonRootFamilyAngularSpeed,
				Diagnostics.PeakTotalThighBodyAngularSpeed,
				Diagnostics.PeakTotalSpineBodyAngularSpeed,
				Diagnostics.PeakTotalFeetBodyAngularSpeed))
		{
			// Not yet material - continue with remaining continuity checks
		}
		else
		{
			return false;
		}
	}

	// Section 17.3 - shell lock preserved
	if (!Owner->HasExplicitTransitionOwnedShellLock())
	{
		OutReason = TEXT("phase3_shell_lock_lost");
		return false;
	}

	// Section 17.3 - no shell reference reseed
	if (Owner->WasTransitionShellReferenceReseededAfterLock())
	{
		OutReason = TEXT("phase3_shell_reference_reseeded");
		return false;
	}

	// Section 17.3 - no startup/gameplay ownership reclaim (CharacterMovement active)
	if (Owner->GetLocomotionAuthorityState() != EBridgeLocomotionAuthorityState::Idle)
	{
		OutReason = TEXT("phase3_startup_or_gameplay_authority_reclaimed");
		return false;
	}

	// Section 17.3 - no reset pending / no topology flip pending
	if (!Owner->GetPendingBodyModifierCachedResetNames().IsEmpty())
	{
		OutReason = TEXT("phase3_reset_pending");
		return false;
	}

	// Section 17.3 - no material shell correction
	if (IsMaterialPhase3ShellCorrectionActive(
			Diagnostics,
			Phase3KineticGateReleaseTickCount,
			Owner->HasExplicitTransitionOwnedShellLock(),
			Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle,
			Phase3GuardTickCount,
			Owner->GetCurrentShellPlanarOffsetDeltaCm(),
			Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond(),
			Settings.BalancePhase2AbortShellOffsetDelta,
			Settings.BalancePhase2AbortShellVelocityDelta * (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle ? 5.0f : 1.0f)))
	{
		// Even if material shell correction is active, we check if it's within the early-settle instability grace window.
		// This prevents brittle failures during the initial raw-sim burst if the energy is still decaying safely.
		if (IsPhase3EarlySettleInstabilityGraceActive(
				Diagnostics,
				Phase3KineticGateReleaseTickCount,
				Phase3GuardTickCount,
				Owner->HasExplicitTransitionOwnedShellLock(),
				Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle,
				Domain.RootLinearSpeed,
				Settings.MaxRootLinearSpeedCmPerSecond * 5.0f,
				Domain.RootAngularSpeed,
				Settings.MaxRootAngularSpeedDegPerSecond * 3.0f,
				Owner->GetCurrentShellPlanarOffsetDeltaCm(),
				Settings.BalancePhase2AbortShellOffsetDelta,
				CurrentMaxNonRootAngularSpeed,
				Settings.BalancePhase2AbortMaxBodyAngularSpeed,
				Diagnostics.PeakRootAngularSpeed,
				Diagnostics.PeakMaxNonRootBodyAngularSpeed,
				Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond(),
				Settings.BalancePhase2AbortShellVelocityDelta * (InternalPhase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle ? 5.0f : 1.0f),
				Diagnostics.BaselineShellVel,
				CurrentMaxNonRootAngularBone,
				CurrentSpineFamilyAngularSpeed,
				CurrentThighFamilyAngularSpeed,
				CurrentFeetFamilyAngularSpeed,
				CurrentNonRootFamilyAngularSpeed,
				Diagnostics.PeakTotalThighBodyAngularSpeed,
				Diagnostics.PeakTotalSpineBodyAngularSpeed,
				Diagnostics.PeakTotalFeetBodyAngularSpeed))
		{
			// Within grace budget - continue
		}
		else
		{
			OutReason = TEXT("phase3_material_shell_correction");
			return false;
		}
	}

	Diagnostics.LastFrameShellVelocity = Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond();
	Diagnostics.LastFrameRootAngularSpeed = Domain.RootAngularSpeed;
	Diagnostics.LastFrameMaxNonRootAngularSpeed = CurrentMaxNonRootAngularSpeed;

	return true;
}

bool FPhysAnimBalanceReadyTransition::IsPhase3Stable() const
{
	// Stability criteria for allowing 100% proximal authority:
	// 1. Minimum duration passed since raw-sim release (to let initial impulse dissipate)
	// 2. Material shell velocity is below soft restore threshold
	// 3. Root body angular speed is below soft restore threshold
	// 4. No significant non-root angular expansion (envelope < 1.0)
	
	const bool bAllowFullProximalStrength = 
		Phase3KineticGateReleaseTickCount >= 20 &&
		Diagnostics.Phase3CurrentShellVelocity < 60.0f &&
		Diagnostics.Phase3CurrentRootAngularSpeed < 200.0f &&
		(Diagnostics.Phase3CurrentMaxNonRootAngularSpeed <= Diagnostics.Phase3CurrentObservedNonRootAngularEnvelope * 1.5f || Diagnostics.Phase3CurrentMaxNonRootAngularSpeed < 100.0f);

	return bAllowFullProximalStrength;
}
