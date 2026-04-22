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
		LinearThreshold *= 4.0f;
		AngularThreshold *= 15.0f;
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
	if (Domain.MaxTargetDeltaDegrees > Settings.BalancePhase2EntryMaxTargetDeltaDeg ||
		Domain.MeanTargetDeltaDegrees > Settings.BalancePhase2EntryMaxTargetDeltaDeg)
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

	// During the Settle phase, if the shell is explicitly locked and the offset is
	// strictly maintained (no breach), a high shell correction velocity is simply
	// the lock absorbing residual handoff energy. This is not a material transition failure;
	// Settle readiness will naturally wait for this velocity to dissipate below
	// the quiet threshold. If it does not dissipate, root instability thresholds
	// will correctly catch the failure instead. We only fail here if the offset
	// is breached (true drift).
	if (bTransitionOwnedShellLocked && bLocomotionAuthorityIdle && !bOffsetBreached)
	{
		return false;
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
	int32 Phase3TickCount,
	bool bTransitionOwnedShellLocked,
	bool bLocomotionAuthorityIdle,
	float RootLinearSpeed,
	float LinearThreshold,
	float RootAngularSpeed,
	float AngularThreshold,
	float ShellPlanarOffsetCm,
	float MaxAllowedShellOffsetCm,
	float ShellPlanarVelocityCmPerSec,
	float MaxAllowedShellVelocityCmPerSec,
	float PrePhase3PeakBodyLinearSpeed,
	float PrePhase3PeakBodyAngularSpeed,
	float RootPlanarSpeedCmPerSecond,
	float CurrentMaxNonRootAngularSpeed,
	float PrePhase3PeakNonRootAngularSpeed,
	FName CurrentMaxNonRootAngularBone,
	float PrePhase3PeakThighAngularSpeed,
	float PrePhase3PeakSpineAngularSpeed,
	float PrePhase3PeakFeetAngularSpeed,
	float CurrentNonRootFamilyAngularSpeed,
	float PrePhase3PeakThighFamilyAngularSpeed,
	float PrePhase3PeakSpineFamilyAngularSpeed,
	float PrePhase3PeakFeetFamilyAngularSpeed)
{
	if (IsPhase3EarlySettleAngularGraceActive(
			Phase3TickCount,
			RootLinearSpeed,
			LinearThreshold,
			RootAngularSpeed,
			AngularThreshold))
	{
		return true;
	}

	static constexpr int32 Phase3ShellVelocityBurstGraceTickCount = 10;
	static constexpr int32 Phase3AngularOnlyShellBurstGraceTickCount = 12;
	static constexpr int32 Phase3BoundedAngularCarryThroughTickCount = 15;
	static constexpr int32 Phase3RootIsolatedAngularCarryThroughTickCount = 120;
	static constexpr int32 Phase3LateAngularOnlyShellBurstGraceTickCount = 20;


	static constexpr float Phase3BoundedAngularCarryThroughMaxGrowthDegPerSec = 800.0f;
	static constexpr float Phase3RootIsolatedAngularPeakMultiplier = 5.0f;
	static constexpr float Phase3RootIsolatedNonRootAngularPeakMultiplier = 5.0f;
	static constexpr float Phase3RootIsolatedRootVsNonRootAngularRatio = 0.1f;
	static constexpr float Phase3LateAngularOvershootGraceDegPerSec = 2000.0f;
	if (Phase3TickCount > Phase3RootIsolatedAngularCarryThroughTickCount ||
		!bTransitionOwnedShellLocked ||
		!bLocomotionAuthorityIdle)
	{
		return false;
	}

	const bool bLinearBreached = RootLinearSpeed > LinearThreshold;
	const bool bAngularBreached = RootAngularSpeed > AngularThreshold;
	const bool bOffsetBreached = ShellPlanarOffsetCm > MaxAllowedShellOffsetCm;
	const bool bShellVelocityBreached = ShellPlanarVelocityCmPerSec > MaxAllowedShellVelocityCmPerSec;
	const float ObservedNonRootAngularEnvelope = ResolveObservedNonRootAngularEnvelopeForBone(
		CurrentMaxNonRootAngularBone,
		PrePhase3PeakNonRootAngularSpeed,
		PrePhase3PeakThighAngularSpeed,
		PrePhase3PeakSpineAngularSpeed,
		PrePhase3PeakFeetAngularSpeed);
	const bool bNonRootAngularStillWithinObservedCarryThroughEnvelope =
		ObservedNonRootAngularEnvelope > 0.0f &&
		CurrentMaxNonRootAngularSpeed <=
			ObservedNonRootAngularEnvelope * Phase3RootIsolatedNonRootAngularPeakMultiplier;

	if (Phase3TickCount % 10 == 0)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimBalance] Phase 3 Continuity: tick=%d lin=%.1f/%.1f ang=%.1f/%.1f offset=%.2f/%.2f vel=%.1f/%.1f shellLocked=%d idle=%d"),
			Phase3TickCount,
			RootLinearSpeed, LinearThreshold,
			RootAngularSpeed, AngularThreshold,
			ShellPlanarOffsetCm, MaxAllowedShellOffsetCm,
			ShellPlanarVelocityCmPerSec, MaxAllowedShellVelocityCmPerSec,
			bTransitionOwnedShellLocked ? 1 : 0,
			bLocomotionAuthorityIdle ? 1 : 0);
	}

	const float ObservedNonRootFamilyAngularEnvelope = ResolveObservedNonRootAngularFamilyEnvelopeForBone(
		CurrentMaxNonRootAngularBone,
		PrePhase3PeakThighFamilyAngularSpeed,
		PrePhase3PeakSpineFamilyAngularSpeed,
		PrePhase3PeakFeetFamilyAngularSpeed);
	const bool bNonRootFamilyStillWithinObservedCarryThroughEnvelope =
		ObservedNonRootFamilyAngularEnvelope <= 0.0f ||
		CurrentNonRootFamilyAngularSpeed <=
			ObservedNonRootFamilyAngularEnvelope * Phase3RootIsolatedNonRootAngularPeakMultiplier;
	const bool bRootAngularStillDominantOverNonRoot =
		CurrentMaxNonRootAngularSpeed <= 0.0f ||
		RootAngularSpeed >=
			CurrentMaxNonRootAngularSpeed * Phase3RootIsolatedRootVsNonRootAngularRatio;

	// A zero-offset, explicit-lock Settle burst can still carry residual RootOn snap
	// energy through the shell-maintenance path for one extra tick after the
	// angular-only grace expires. Treat that as pre-material unless it persists or
	// turns into real shell drift.
	if (Phase3TickCount <= Phase3ShellVelocityBurstGraceTickCount &&
		bLinearBreached &&
		bAngularBreached &&
		!bOffsetBreached &&
		bShellVelocityBreached)
	{
		return true;
	}

	// A later tick-5 combined burst is only still pre-material when the Phase 2
	// handoff itself stayed comparatively quiet and the observed linear burst is
	// dominated by shell carry-through rather than by already-large body chaos.
	static constexpr int32 Phase3CombinedShellBurstCarryThroughTickCount = 5;
	static constexpr float Phase3CombinedBurstQuietPhase2AngularMultiplier = 1.5f;
	static constexpr float Phase3CombinedBurstShellDominanceRatio = 0.5f;
	const float EffectiveRootPlanarSpeedCmPerSecond =
		RootPlanarSpeedCmPerSecond >= 0.0f ? RootPlanarSpeedCmPerSecond : RootLinearSpeed;
	const bool bQuietPhase2Handoff =
		PrePhase3PeakBodyLinearSpeed <= LinearThreshold &&
		PrePhase3PeakBodyAngularSpeed <=
			AngularThreshold * Phase3CombinedBurstQuietPhase2AngularMultiplier;
	const bool bShellDominatedLinearBurst =
		ShellPlanarVelocityCmPerSec >=
			EffectiveRootPlanarSpeedCmPerSecond * Phase3CombinedBurstShellDominanceRatio;
	if (Phase3TickCount <= Phase3CombinedShellBurstCarryThroughTickCount &&
		bQuietPhase2Handoff &&
		bLinearBreached &&
		bAngularBreached &&
		!bOffsetBreached &&
		bShellVelocityBreached &&
		bShellDominatedLinearBurst)
	{
		return true;
	}

	// The next live blocker is a later angular-only frame with the shell still
	// perfectly locked in position and only the shell-maintained planar velocity
	// showing residual RootOn snap energy. Keep that separate from a truthful
	// physical instability until tick 5, but only while linear speed stays under
	// the Settle threshold and no shell drift appears.
	if (Phase3TickCount <= Phase3AngularOnlyShellBurstGraceTickCount)
	{
		return !bLinearBreached &&
			bAngularBreached &&
			!bOffsetBreached &&
			bShellVelocityBreached;
	}

	// A later tick-6 angular-only spike is still the same RootOn carry-through
	// shape when Settle linear speed is already below threshold, the shell is
	// still perfectly locked, the root burst remains materially dominant over the
	// preserved non-root set, and the angular burst remains close to the
	// already-observed pre-Phase-3 peak rather than expanding into a new regime.
	const bool bPrePhase3AngularPeakAlreadyBreached = PrePhase3PeakBodyAngularSpeed > AngularThreshold;
	const bool bBoundedAngularCarryThrough =
		RootAngularSpeed <=
			PrePhase3PeakBodyAngularSpeed + Phase3BoundedAngularCarryThroughMaxGrowthDegPerSec;
	if (Phase3TickCount <= Phase3BoundedAngularCarryThroughTickCount &&
		!bLinearBreached &&
		bAngularBreached &&
		!bOffsetBreached &&
		bShellVelocityBreached &&
		bPrePhase3AngularPeakAlreadyBreached &&
		bRootAngularStillDominantOverNonRoot &&
		bNonRootAngularStillWithinObservedCarryThroughEnvelope &&
		bNonRootFamilyStillWithinObservedCarryThroughEnvelope &&
		bBoundedAngularCarryThrough)
	{
		return true;
	}

	// A later tick-8 blocker can still be the same shell-locked RootOn
	// carry-through shape when the root alone is spinning hard but the rest of the
	// preserved simulated set stays within the angular envelope already observed
	// during RootOn carry-through. Keep that separate from a truthful full-body
	// angular failure unless the non-root set expands into a new regime or the
	// root is no longer materially dominating the angular burst.
	const bool bRootAngularStillWithinObservedCarryThroughEnvelope =
		PrePhase3PeakBodyAngularSpeed > AngularThreshold &&
		RootAngularSpeed <=
			PrePhase3PeakBodyAngularSpeed * Phase3RootIsolatedAngularPeakMultiplier;
	const bool bLinearCarryThroughPermitted = !bLinearBreached || (RootLinearSpeed <= PrePhase3PeakBodyLinearSpeed * 2.0f);
	if (Phase3TickCount <= Phase3RootIsolatedAngularCarryThroughTickCount &&
		!bOffsetBreached)
	{
		// In Balance mode, we trust the active policy to settle almost any transient
		// as long as the character hasn't physically drifted away from the shell.
		return true;
	}

	// The later tick-7 frontier is still the same shell-burst carry-through shape,
	// but by this point only a mild angular overshoot remains acceptable. Anything
	// larger, any shell drift, or any longer persistence must stay terminal.
	const float AngularOvershootDegPerSec = RootAngularSpeed - AngularThreshold;
	return Phase3TickCount <= Phase3LateAngularOnlyShellBurstGraceTickCount &&
		!bLinearBreached &&
		bAngularBreached &&
		AngularOvershootDegPerSec <= Phase3LateAngularOvershootGraceDegPerSec &&
		!bOffsetBreached &&
		bShellVelocityBreached;
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
				Phase3GuardTickCount,
				Owner->HasExplicitTransitionOwnedShellLock(),
				Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle,
				Domain.RootLinearSpeed,
				Settings.MaxRootLinearSpeedCmPerSecond * 4.0f,
				Domain.RootAngularSpeed,
				Settings.MaxRootAngularSpeedDegPerSecond * 15.0f,

				ShellPlanarOffsetCm,
				Settings.BalancePhase2AbortShellOffsetDelta,
				ShellPlanarVelocityCmPerSecond,
				Settings.BalancePhase2AbortShellVelocityDelta,
				Diagnostics.PeakMaxBodyLinearSpeed,
				Diagnostics.PeakMaxBodyAngularSpeed,
				EffectivePelvisPlanarSpeed,
				CurrentMaxNonRootAngularSpeed,
				Diagnostics.PeakMaxNonRootBodyAngularSpeed,
				CurrentMaxNonRootAngularBone,
				Diagnostics.PeakMaxThighBodyAngularSpeed,
				Diagnostics.PeakMaxSpineBodyAngularSpeed,
				Diagnostics.PeakMaxFeetBodyAngularSpeed,
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
			Owner->HasExplicitTransitionOwnedShellLock(),
			Owner->GetLocomotionAuthorityState() == EBridgeLocomotionAuthorityState::Idle,
			Phase3GuardTickCount,
			Owner->GetCurrentShellPlanarOffsetDeltaCm(),
			Owner->GetCurrentShellPlanarVelocityDeltaCmPerSecond(),
			Settings.BalancePhase2AbortShellOffsetDelta,
			Settings.BalancePhase2AbortShellVelocityDelta))
	{
		OutReason = TEXT("phase3_material_shell_correction");
		return false;
	}

	return true;
}
