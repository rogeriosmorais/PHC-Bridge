#pragma once

#include "PhysAnimTruthTypes.h"

struct FPhysAnimContinuitySnapshot
{
	int32 TopologyChangeCount = 0;
	bool bAllCriticalBodiesValid = true;
	bool bAllCriticalBodiesSimulating = true;
	double PelvisSleepDurationMs = 0.0;
	bool bContinuityBookkeepingMismatch = false;
};

struct FPhysAnimContinuityValidationResult
{
	int32 TopologyChangeCount = 0;
	bool bContinuityBookkeepingMismatch = false;
	double PelvisSleepDurationMs = 0.0;
	bool bPhysicalContinuityValidatorPassed = true;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

enum class EPhysAnimCapsuleCollisionState : uint8
{
	NoCollision,
	CollisionEnabled
};

struct FPhysAnimCapsuleContractSnapshot
{
	double CapsuleLockDeltaCm = 0.0;
	EPhysAnimCapsuleCollisionState CapsuleCollisionEnabled = EPhysAnimCapsuleCollisionState::NoCollision;
	bool bCapsuleGenerateOverlapEvents = false;
	bool bMeshUsesAbsoluteLocation = true;
	bool bMeshUsesAbsoluteRotation = true;
	bool bMeshUsesAbsoluteScale = true;
	bool bCmcIsActive = false;
	bool bCmcTickEnabled = false;
	bool bCmcUpdatedComponentIsNull = true;
};

struct FPhysAnimCapsuleContractValidationResult
{
	double CapsuleLockDeltaCm = 0.0;
	EPhysAnimCapsuleCollisionState CapsuleCollisionEnabled = EPhysAnimCapsuleCollisionState::NoCollision;
	bool bCapsuleGenerateOverlapEvents = false;
	bool bMeshUsesAbsoluteLocation = true;
	bool bMeshUsesAbsoluteRotation = true;
	bool bMeshUsesAbsoluteScale = true;
	bool bCmcIsActive = false;
	bool bCmcTickEnabled = false;
	bool bCmcUpdatedComponentIsNull = true;
	bool bCapsuleContractPassed = true;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

enum class EPhysAnimPlantFailureClass : uint8
{
	None,
	StaticStructural,
	Mutation,
	Dynamic
};

enum class EPhysAnimPlantFailureField : uint8
{
	None,
	Skeleton,
	SegmentLength,
	AxisAlignment,
	Mass,
	PhysicsAssetIdentity
};

struct FPhysAnimPlantContractSnapshot
{
	bool bPhysicsAssetContractValid = true;
	bool bSkeletonAuditPassed = true;
	EPhysAnimPlantFailureClass PlantFailureClass = EPhysAnimPlantFailureClass::None;
	EPhysAnimPlantFailureField PlantFailureField = EPhysAnimPlantFailureField::None;
	double MassDriftTotalPct = 0.0;
};

struct FPhysAnimPlantContractValidationResult
{
	bool bPhysicsAssetContractValid = true;
	bool bSkeletonAuditPassed = true;
	EPhysAnimPlantFailureClass PlantFailureClass = EPhysAnimPlantFailureClass::None;
	EPhysAnimPlantFailureField PlantFailureField = EPhysAnimPlantFailureField::None;
	double MassDriftTotalPct = 0.0;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

namespace PhysAnimValidators
{
	FPhysAnimContinuityValidationResult ValidateContinuity(const FPhysAnimContinuitySnapshot& Snapshot);
	FPhysAnimCapsuleContractValidationResult ValidateCapsule(const FPhysAnimCapsuleContractSnapshot& Snapshot);
	FPhysAnimPlantContractValidationResult ValidatePlant(const FPhysAnimPlantContractSnapshot& Snapshot);
}
