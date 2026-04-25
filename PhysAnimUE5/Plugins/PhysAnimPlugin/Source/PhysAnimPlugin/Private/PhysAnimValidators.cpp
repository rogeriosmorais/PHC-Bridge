#include "PhysAnimValidators.h"

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
}
