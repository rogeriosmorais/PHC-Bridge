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

enum class EPhysAnimContaminationClass : uint8
{
	None,
	MeshWideAssist,
	NonCriticalBodyAssist,
	ExcludedBodyWorldBrace,
	GlobalBlendOrKinematicAssist
};

struct FPhysAnimAuthoritySnapshot
{
	int32 AuthorityConflictCount = 0;
	EPhysAnimContaminationClass ContaminationClass = EPhysAnimContaminationClass::None;
	FName ContaminationSourceBody = NAME_None;
	FName ContaminationSourceSubsystem = NAME_None;
	bool bMeshWideAssistDetected = false;
};

struct FPhysAnimAuthorityValidationResult
{
	int32 AuthorityConflictCount = 0;
	EPhysAnimContaminationClass ContaminationClass = EPhysAnimContaminationClass::None;
	FName ContaminationSourceBody = NAME_None;
	FName ContaminationSourceSubsystem = NAME_None;
	bool bMeshWideAssistDetected = false;
	bool bAuthorityPassed = true;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

struct FPhysAnimMovementReclaimSnapshot
{
	int32 MovementReclaimCount = 0;
};

struct FPhysAnimMovementReclaimValidationResult
{
	int32 MovementReclaimCount = 0;
	bool bMovementReclaimPassed = true;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

struct FPhysAnimShellHelperSnapshot
{
	int32 ShellHelperUsedCount = 0;
};

struct FPhysAnimShellHelperValidationResult
{
	int32 ShellHelperUsedCount = 0;
	bool bShellHelperPassed = true;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

enum class EPhysAnimTargetDiscontinuityPhase : uint8
{
	None,
	BlendStart
};

enum class EPhysAnimControllerStabilityFailureField : uint8
{
	None,
	TargetDiscontinuityDeg,
	ControllerGainScale,
	ControllerDampingRatio,
	MaxRootTiltDeg,
	PeakAngularSpeed,
	MaxBodyMismatchDeg,
	RmsMismatchDeg,
	StandingValidationTimedOut
};

struct FPhysAnimControllerStabilitySnapshot
{
	double TargetDiscontinuityDeg = 0.0;
	EPhysAnimTargetDiscontinuityPhase TargetDiscontinuityPhase = EPhysAnimTargetDiscontinuityPhase::None;
	bool bControllerGainDampingValid = true;
	double ControllerGainScale = 1.0;
	double ControllerDampingRatio = 1.0;
	double MaxRootTiltDeg = 0.0;
	double PeakAngularSpeedDegPerSec = 0.0;
	double MaxBodyMismatchDeg = 0.0;
	double RmsMismatchDeg = 0.0;
	double MismatchDurationMs = 0.0;
	double HoldDurationSec = 0.0;
	bool bStandingValidationTimedOut = false;
};

struct FPhysAnimControllerStabilityValidationResult
{
	double TargetDiscontinuityDeg = 0.0;
	EPhysAnimTargetDiscontinuityPhase TargetDiscontinuityPhase = EPhysAnimTargetDiscontinuityPhase::None;
	bool bControllerGainDampingValid = true;
	double ControllerGainScale = 1.0;
	double ControllerDampingRatio = 1.0;
	double MaxRootTiltDeg = 0.0;
	double PeakAngularSpeedDegPerSec = 0.0;
	double MaxBodyMismatchDeg = 0.0;
	double RmsMismatchDeg = 0.0;
	double MismatchDurationMs = 0.0;
	double HoldDurationSec = 0.0;
	bool bStandingValidationTimedOut = false;
	EPhysAnimControllerStabilityFailureField FailureField = EPhysAnimControllerStabilityFailureField::None;
	bool bControllerStabilityPassed = true;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

namespace PhysAnimValidators
{
	FPhysAnimContinuityValidationResult ValidateContinuity(const FPhysAnimContinuitySnapshot& Snapshot);
	FPhysAnimCapsuleContractValidationResult ValidateCapsule(const FPhysAnimCapsuleContractSnapshot& Snapshot);
	FPhysAnimPlantContractValidationResult ValidatePlant(const FPhysAnimPlantContractSnapshot& Snapshot);
	FPhysAnimAuthorityValidationResult ValidateAuthority(const FPhysAnimAuthoritySnapshot& Snapshot);
	FPhysAnimMovementReclaimValidationResult ValidateMovementReclaim(const FPhysAnimMovementReclaimSnapshot& Snapshot);
	FPhysAnimShellHelperValidationResult ValidateShellHelper(const FPhysAnimShellHelperSnapshot& Snapshot);
	FPhysAnimControllerStabilityValidationResult ValidateControllerStability(const FPhysAnimControllerStabilitySnapshot& Snapshot);
}
