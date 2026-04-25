#include "PhysAnimValidators.h"

namespace
{
	void AddFallbackFailureCandidate(
		TArray<FPhysAnimFailureCandidate>& Candidates,
		EPhysAnimTerminalReason Reason,
		int64 TerminalSubstepTimestamp)
	{
		if (Reason == EPhysAnimTerminalReason::None)
		{
			return;
		}

		FPhysAnimFailureCandidate Candidate;
		Candidate.TerminalReason = Reason;
		Candidate.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		Candidates.Add(Candidate);
	}
}

namespace PhysAnimValidators
{
	FPhysAnimContinuityValidationResult ValidateContinuity(const FPhysAnimContinuitySnapshot& Snapshot)
	{
		constexpr double PelvisSleepLimitMs = 100.0;

		FPhysAnimContinuityValidationResult Result;
		Result.TopologyChangeCount = Snapshot.TopologyChangeCount;
		Result.bContinuityBookkeepingMismatch = Snapshot.bContinuityBookkeepingMismatch;
		Result.PelvisSleepDurationMs = Snapshot.PelvisSleepDurationMs;

		if (Snapshot.TopologyChangeCount > 0 || !Snapshot.bAllCriticalBodiesValid)
		{
			Result.bPhysicalContinuityValidatorPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationTopologyChange;
			return Result;
		}

		if (!Snapshot.bAllCriticalBodiesSimulating || Snapshot.PelvisSleepDurationMs > PelvisSleepLimitMs)
		{
			Result.bPhysicalContinuityValidatorPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationContinuousSimulationLost;
			return Result;
		}

		return Result;
	}

	FPhysAnimCapsuleContractValidationResult ValidateCapsule(const FPhysAnimCapsuleContractSnapshot& Snapshot)
	{
		constexpr double CapsuleLockDeltaLimitCm = 0.01;

		FPhysAnimCapsuleContractValidationResult Result;
		Result.CapsuleLockDeltaCm = Snapshot.CapsuleLockDeltaCm;
		Result.CapsuleCollisionEnabled = Snapshot.CapsuleCollisionEnabled;
		Result.bCapsuleGenerateOverlapEvents = Snapshot.bCapsuleGenerateOverlapEvents;
		Result.bMeshUsesAbsoluteLocation = Snapshot.bMeshUsesAbsoluteLocation;
		Result.bMeshUsesAbsoluteRotation = Snapshot.bMeshUsesAbsoluteRotation;
		Result.bMeshUsesAbsoluteScale = Snapshot.bMeshUsesAbsoluteScale;
		Result.bCmcIsActive = Snapshot.bCmcIsActive;
		Result.bCmcTickEnabled = Snapshot.bCmcTickEnabled;
		Result.bCmcUpdatedComponentIsNull = Snapshot.bCmcUpdatedComponentIsNull;

		const bool bCapsuleContractViolated =
			Snapshot.CapsuleLockDeltaCm > CapsuleLockDeltaLimitCm ||
			Snapshot.CapsuleCollisionEnabled != EPhysAnimCapsuleCollisionState::NoCollision ||
			Snapshot.bCapsuleGenerateOverlapEvents ||
			!Snapshot.bMeshUsesAbsoluteLocation ||
			!Snapshot.bMeshUsesAbsoluteRotation ||
			!Snapshot.bMeshUsesAbsoluteScale ||
			Snapshot.bCmcIsActive ||
			Snapshot.bCmcTickEnabled ||
			!Snapshot.bCmcUpdatedComponentIsNull;

		if (bCapsuleContractViolated)
		{
			Result.bCapsuleContractPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationCapsuleContractViolation;
		}

		return Result;
	}

	FPhysAnimPlantContractValidationResult ValidatePlant(const FPhysAnimPlantContractSnapshot& Snapshot)
	{
		FPhysAnimPlantContractValidationResult Result;
		Result.bPhysicsAssetContractValid = Snapshot.bPhysicsAssetContractValid;
		Result.bSkeletonAuditPassed = Snapshot.bSkeletonAuditPassed;
		Result.PlantFailureClass = Snapshot.PlantFailureClass;
		Result.PlantFailureField = Snapshot.PlantFailureField;
		Result.MassDriftTotalPct = Snapshot.MassDriftTotalPct;

		const bool bPlantContractViolated =
			!Snapshot.bPhysicsAssetContractValid ||
			!Snapshot.bSkeletonAuditPassed ||
			Snapshot.PlantFailureClass != EPhysAnimPlantFailureClass::None ||
			Snapshot.PlantFailureField != EPhysAnimPlantFailureField::None;

		if (bPlantContractViolated)
		{
			Result.bPhysicsAssetContractValid = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation;
		}

		return Result;
	}

	FPhysAnimAuthorityValidationResult ValidateAuthority(const FPhysAnimAuthoritySnapshot& Snapshot)
	{
		FPhysAnimAuthorityValidationResult Result;
		Result.AuthorityConflictCount = Snapshot.AuthorityConflictCount;
		Result.ContaminationClass = Snapshot.ContaminationClass;
		Result.ContaminationSourceBody = Snapshot.ContaminationSourceBody;
		Result.ContaminationSourceSubsystem = Snapshot.ContaminationSourceSubsystem;
		Result.bMeshWideAssistDetected = Snapshot.bMeshWideAssistDetected;

		const bool bAuthorityConflict =
			Snapshot.AuthorityConflictCount > 0 ||
			Snapshot.ContaminationClass != EPhysAnimContaminationClass::None ||
			Snapshot.bMeshWideAssistDetected;

		if (bAuthorityConflict)
		{
			Result.bAuthorityPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationAuthorityConflict;
		}

		return Result;
	}

	FPhysAnimMovementReclaimValidationResult ValidateMovementReclaim(const FPhysAnimMovementReclaimSnapshot& Snapshot)
	{
		FPhysAnimMovementReclaimValidationResult Result;
		Result.MovementReclaimCount = Snapshot.MovementReclaimCount;

		if (Snapshot.MovementReclaimCount > 0)
		{
			Result.bMovementReclaimPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationMovementReclaim;
		}

		return Result;
	}

	FPhysAnimShellHelperValidationResult ValidateShellHelper(const FPhysAnimShellHelperSnapshot& Snapshot)
	{
		FPhysAnimShellHelperValidationResult Result;
		Result.ShellHelperUsedCount = Snapshot.ShellHelperUsedCount;

		if (Snapshot.ShellHelperUsedCount > 0)
		{
			Result.bShellHelperPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationShellHelperViolation;
		}

		return Result;
	}

	FPhysAnimControllerStabilityValidationResult ValidateControllerStability(const FPhysAnimControllerStabilitySnapshot& Snapshot)
	{
		constexpr double TargetDiscontinuityLimitDeg = 15.0;
		constexpr double ControllerGainScaleMax = 1.0;
		constexpr double ControllerDampingRatioMin = 1.0;
		constexpr double MaxRootTiltLimitDeg = 20.0;
		constexpr double PeakAngularSpeedLimitDegPerSec = 720.0;
		constexpr double MaxBodyMismatchLimitDeg = 25.0;
		constexpr double RmsMismatchLimitDeg = 15.0;
		constexpr double MismatchGraceMs = 200.0;
		constexpr double HoldDurationMinSec = 3.0;

		FPhysAnimControllerStabilityValidationResult Result;
		Result.TargetDiscontinuityDeg = Snapshot.TargetDiscontinuityDeg;
		Result.TargetDiscontinuityPhase = Snapshot.TargetDiscontinuityPhase;
		Result.bControllerGainDampingValid = Snapshot.bControllerGainDampingValid;
		Result.ControllerGainScale = Snapshot.ControllerGainScale;
		Result.ControllerDampingRatio = Snapshot.ControllerDampingRatio;
		Result.MaxRootTiltDeg = Snapshot.MaxRootTiltDeg;
		Result.PeakAngularSpeedDegPerSec = Snapshot.PeakAngularSpeedDegPerSec;
		Result.MaxBodyMismatchDeg = Snapshot.MaxBodyMismatchDeg;
		Result.RmsMismatchDeg = Snapshot.RmsMismatchDeg;
		Result.MismatchDurationMs = Snapshot.MismatchDurationMs;
		Result.HoldDurationSec = Snapshot.HoldDurationSec;
		Result.bStandingValidationTimedOut = Snapshot.bStandingValidationTimedOut;

		auto Fail = [&Result](EPhysAnimControllerStabilityFailureField Field, EPhysAnimTerminalReason Reason)
		{
			Result.bControllerStabilityPassed = false;
			Result.FailureField = Field;
			Result.TerminalReason = Reason;
		};

		if (Snapshot.TargetDiscontinuityDeg > TargetDiscontinuityLimitDeg &&
			Snapshot.TargetDiscontinuityPhase == EPhysAnimTargetDiscontinuityPhase::BlendStart)
		{
			Fail(EPhysAnimControllerStabilityFailureField::TargetDiscontinuityDeg, EPhysAnimTerminalReason::ActivationTargetDiscontinuity);
		}
		else if (!Snapshot.bControllerGainDampingValid || Snapshot.ControllerGainScale > ControllerGainScaleMax)
		{
			Fail(EPhysAnimControllerStabilityFailureField::ControllerGainScale, EPhysAnimTerminalReason::ActivationUnstableGainOrDamping);
			Result.bControllerGainDampingValid = false;
		}
		else if (Snapshot.ControllerDampingRatio < ControllerDampingRatioMin)
		{
			Fail(EPhysAnimControllerStabilityFailureField::ControllerDampingRatio, EPhysAnimTerminalReason::ActivationUnstableGainOrDamping);
			Result.bControllerGainDampingValid = false;
		}
		else if (Snapshot.MaxRootTiltDeg > MaxRootTiltLimitDeg)
		{
			Fail(EPhysAnimControllerStabilityFailureField::MaxRootTiltDeg, EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach);
		}
		else if (Snapshot.PeakAngularSpeedDegPerSec > PeakAngularSpeedLimitDegPerSec)
		{
			Fail(EPhysAnimControllerStabilityFailureField::PeakAngularSpeed, EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach);
		}
		else if (Snapshot.MaxBodyMismatchDeg > MaxBodyMismatchLimitDeg && Snapshot.MismatchDurationMs > MismatchGraceMs)
		{
			Fail(EPhysAnimControllerStabilityFailureField::MaxBodyMismatchDeg, EPhysAnimTerminalReason::ActivationPoseReferenceMismatch);
		}
		else if (Snapshot.RmsMismatchDeg > RmsMismatchLimitDeg && Snapshot.MismatchDurationMs > MismatchGraceMs)
		{
			Fail(EPhysAnimControllerStabilityFailureField::RmsMismatchDeg, EPhysAnimTerminalReason::ActivationPoseReferenceMismatch);
		}
		else if (Snapshot.bStandingValidationTimedOut && Snapshot.HoldDurationSec < HoldDurationMinSec)
		{
			Fail(EPhysAnimControllerStabilityFailureField::StandingValidationTimedOut, EPhysAnimTerminalReason::ActivationStandingValidationTimeout);
		}

		return Result;
	}

	FPhysAnimSupportContractValidationResult ValidateSupport(const FPhysAnimSupportContractSnapshot& Snapshot)
	{
		FPhysAnimSupportContractValidationResult Result;
		Result.bSupportStateL = Snapshot.bSupportStateL;
		Result.bSupportStateR = Snapshot.bSupportStateR;
		Result.SupportMode = Snapshot.SupportMode;
		Result.SupportGapTimerMs = Snapshot.SupportGapTimerMs;
		Result.ActiveSupportSideCount = Snapshot.ActiveSupportSideCount;
		Result.SupportHullAreaCm2 = Snapshot.SupportHullAreaCm2;
		Result.SupportPatchAreaLCm2 = Snapshot.SupportPatchAreaLCm2;
		Result.SupportPatchAreaRCm2 = Snapshot.SupportPatchAreaRCm2;
		Result.SupportHullPointsCm = Snapshot.SupportHullPointsCm;
		Result.ComProxyPosCm = Snapshot.ComProxyPosCm;
		Result.MaxPenetrationCm = Snapshot.MaxPenetrationCm;
		Result.SupportChurnCount = Snapshot.SupportChurnCount;
		Result.SupportChurnHz = Snapshot.SupportChurnHz;
		Result.ProxyInsideHull = Snapshot.ProxyInsideHull;
		Result.ProxyOutsideHullDurationMs = Snapshot.ProxyOutsideHullDurationMs;

		const bool bAreaBreach =
			Snapshot.ActiveSupportSideCount > 0 &&
			Snapshot.SupportHullAreaCm2 < Snapshot.SupportAreaMinCm2;

		const bool bAirborneGapBreach =
			Snapshot.SupportMode == EPhysAnimSupportMode::Airborne &&
			Snapshot.SupportGapTimerMs > Snapshot.SupportGapMaxMs;

		if (bAreaBreach || bAirborneGapBreach)
		{
			Result.bSupportContractPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationSupportFailure;
			return Result;
		}

		if (Snapshot.ProxyTerminalReason == EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion)
		{
			Result.bSupportContractPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
			return Result;
		}

		return Result;
	}

	FPhysAnimRunArtifactSnapshot BuildRunArtifactSnapshot(const FPhysAnimRunArtifactSnapshotInput& Input)
	{
		FPhysAnimRunArtifactSnapshot Snapshot = Input.Values;

		Snapshot.bPhysicsAssetContractValid = Input.Plant.bPhysicsAssetContractValid;
		Snapshot.bSkeletonAuditPassed = Input.Plant.bSkeletonAuditPassed;
		Snapshot.PlantFailureClass = Input.Plant.PlantFailureClass;
		Snapshot.PlantFailureField = Input.Plant.PlantFailureField;
		Snapshot.MassDriftTotalPct = Input.Plant.MassDriftTotalPct;

		Snapshot.CapsuleCollisionEnabled = Input.Capsule.CapsuleCollisionEnabled;
		Snapshot.bCapsuleGenerateOverlapEvents = Input.Capsule.bCapsuleGenerateOverlapEvents;
		Snapshot.CapsuleLockDeltaCm = Input.Capsule.CapsuleLockDeltaCm;
		Snapshot.bMeshUsesAbsoluteLocation = Input.Capsule.bMeshUsesAbsoluteLocation;
		Snapshot.bMeshUsesAbsoluteRotation = Input.Capsule.bMeshUsesAbsoluteRotation;
		Snapshot.bMeshUsesAbsoluteScale = Input.Capsule.bMeshUsesAbsoluteScale;
		Snapshot.bCmcIsActive = Input.Capsule.bCmcIsActive;
		Snapshot.bCmcTickEnabled = Input.Capsule.bCmcTickEnabled;
		Snapshot.bCmcUpdatedComponentIsNull = Input.Capsule.bCmcUpdatedComponentIsNull;

		Snapshot.TopologyChangeCount = Input.Continuity.TopologyChangeCount;
		Snapshot.bContinuityBookkeepingMismatch = Input.Continuity.bContinuityBookkeepingMismatch;
		Snapshot.PelvisSleepDurationMs = Input.Continuity.PelvisSleepDurationMs;
		Snapshot.bPhysicalContinuityValidatorPassed = Input.Continuity.bPhysicalContinuityValidatorPassed;

		Snapshot.bSupportStateL = Input.Support.bSupportStateL;
		Snapshot.bSupportStateR = Input.Support.bSupportStateR;
		Snapshot.SupportMode = Input.Support.SupportMode;
		Snapshot.SupportGapTimerMs = Input.Support.SupportGapTimerMs;
		Snapshot.ProxyInsideHull = Input.Support.ProxyInsideHull;
		Snapshot.ProxyOutsideHullDurationMs = Input.Support.ProxyOutsideHullDurationMs;
		Snapshot.ActiveSupportSideCount = Input.Support.ActiveSupportSideCount;
		Snapshot.SupportHullAreaCm2 = Input.Support.SupportHullAreaCm2;
		Snapshot.SupportPatchAreaLCm2 = Input.Support.SupportPatchAreaLCm2;
		Snapshot.SupportPatchAreaRCm2 = Input.Support.SupportPatchAreaRCm2;
		Snapshot.SupportHullPointsCm = Input.Support.SupportHullPointsCm;
		Snapshot.ComProxyPosCm = Input.Support.ComProxyPosCm;
		Snapshot.MaxPenetrationCm = Input.Support.MaxPenetrationCm;
		Snapshot.SupportChurnCount = Input.Support.SupportChurnCount;
		Snapshot.SupportChurnHz = Input.Support.SupportChurnHz;

		Snapshot.AuthorityConflictCount = Input.Authority.AuthorityConflictCount;
		Snapshot.ContaminationClass = Input.Authority.ContaminationClass;
		Snapshot.ContaminationSourceBody = Input.Authority.ContaminationSourceBody;
		Snapshot.ContaminationSourceSubsystem = Input.Authority.ContaminationSourceSubsystem;
		Snapshot.bMeshWideAssistDetected = Input.Authority.bMeshWideAssistDetected;
		Snapshot.bNonCriticalBodyAssistDetected = Input.Authority.ContaminationClass == EPhysAnimContaminationClass::NonCriticalBodyAssist;
		Snapshot.ExcludedBodyWorldContactSource = Input.Authority.ContaminationClass == EPhysAnimContaminationClass::ExcludedBodyWorldBrace
			? Input.Authority.ContaminationSourceBody
			: Snapshot.ExcludedBodyWorldContactSource;

		Snapshot.MovementReclaimCount = Input.MovementReclaim.MovementReclaimCount;
		Snapshot.ShellHelperUsedCount = Input.ShellHelper.ShellHelperUsedCount;

		Snapshot.HoldDurationSec = Input.ControllerStability.HoldDurationSec;
		Snapshot.MaxRootTiltDeg = Input.ControllerStability.MaxRootTiltDeg;
		Snapshot.PeakAngularSpeedDegPerSec = Input.ControllerStability.PeakAngularSpeedDegPerSec;
		Snapshot.RmsMismatchDeg = Input.ControllerStability.RmsMismatchDeg;
		Snapshot.MaxBodyMismatchDeg = Input.ControllerStability.MaxBodyMismatchDeg;
		Snapshot.TargetDiscontinuityDeg = Input.ControllerStability.TargetDiscontinuityDeg;
		Snapshot.TargetDiscontinuityPhase = Input.ControllerStability.TargetDiscontinuityPhase;
		Snapshot.MismatchDurationMs = Input.ControllerStability.MismatchDurationMs;
		Snapshot.ControllerGainScale = Input.ControllerStability.ControllerGainScale;
		Snapshot.ControllerDampingRatio = Input.ControllerStability.ControllerDampingRatio;
		Snapshot.bControllerGainDampingValid = Input.ControllerStability.bControllerGainDampingValid;
		Snapshot.ControllerStabilityFailureField = Input.ControllerStability.FailureField;
		Snapshot.bStandingValidationTimedOut = Input.ControllerStability.bStandingValidationTimedOut;

		TArray<FPhysAnimFailureCandidate> Candidates = Input.FailureCandidates;

		if (Candidates.IsEmpty())
		{
			const int64 FallbackTimestamp = Input.Values.TerminalSubstepTimestamp;
			AddFallbackFailureCandidate(Candidates, Input.Plant.TerminalReason, FallbackTimestamp);
			AddFallbackFailureCandidate(Candidates, Input.Capsule.TerminalReason, FallbackTimestamp);
			AddFallbackFailureCandidate(Candidates, Input.Continuity.TerminalReason, FallbackTimestamp);
			AddFallbackFailureCandidate(Candidates, Input.Support.TerminalReason, FallbackTimestamp);
			AddFallbackFailureCandidate(Candidates, Input.ControllerStability.TerminalReason, FallbackTimestamp);
			AddFallbackFailureCandidate(Candidates, Input.MovementReclaim.TerminalReason, FallbackTimestamp);
			AddFallbackFailureCandidate(Candidates, Input.ShellHelper.TerminalReason, FallbackTimestamp);
			AddFallbackFailureCandidate(Candidates, Input.Authority.TerminalReason, FallbackTimestamp);
		}

		const FPhysAnimFailureArbitrationResult Arbitration = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

		Snapshot.TerminalReason = Arbitration.TerminalReason;
		Snapshot.CoTerminalReasons = Arbitration.CoTerminalReasons;
		Snapshot.TerminalSubstepTimestamp = Arbitration.bHasTerminalReason
			? Arbitration.TerminalSubstepTimestamp
			: Input.Values.TerminalSubstepTimestamp;

		return Snapshot;
	}
}
