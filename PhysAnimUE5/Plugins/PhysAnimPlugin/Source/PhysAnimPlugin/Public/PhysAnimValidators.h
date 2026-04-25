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

struct FPhysAnimRunArtifactSnapshot
{
	FString AttemptUuid;
	double Timestamp = 0.0;
	FString BaselineId;
	FString StandingReferenceId;

	bool bPhysicsAssetContractValid = true;
	bool bSkeletonAuditPassed = true;
	EPhysAnimPlantFailureClass PlantFailureClass = EPhysAnimPlantFailureClass::None;
	EPhysAnimPlantFailureField PlantFailureField = EPhysAnimPlantFailureField::None;
	double MassDriftTotalPct = 0.0;

	EPhysAnimCapsuleCollisionState CapsuleCollisionEnabled = EPhysAnimCapsuleCollisionState::NoCollision;
	bool bCapsuleGenerateOverlapEvents = false;
	FVector CapsuleWorldPosCm = FVector::ZeroVector;
	double CapsuleLockDeltaCm = 0.0;
	bool bMeshUsesAbsoluteLocation = true;
	bool bMeshUsesAbsoluteRotation = true;
	bool bMeshUsesAbsoluteScale = true;
	bool bCmcIsActive = false;
	bool bCmcTickEnabled = false;
	FName CmcMovementMode = NAME_None;
	bool bCmcUpdatedComponentIsNull = true;

	double HoldDurationSec = 0.0;
	double SupportUptimeSec = 0.0;
	double MaxRootTiltDeg = 0.0;
	double PeakAngularSpeedDegPerSec = 0.0;
	double RmsMismatchDeg = 0.0;
	double MaxBodyMismatchDeg = 0.0;
	double TargetDiscontinuityDeg = 0.0;
	EPhysAnimTargetDiscontinuityPhase TargetDiscontinuityPhase = EPhysAnimTargetDiscontinuityPhase::None;
	double MismatchDurationMs = 0.0;
	double ControllerGainScale = 1.0;
	double ControllerDampingRatio = 1.0;
	bool bControllerGainDampingValid = true;
	EPhysAnimControllerStabilityFailureField ControllerStabilityFailureField = EPhysAnimControllerStabilityFailureField::None;
	double StandingValidationTimeoutSec = 0.0;
	bool bStandingValidationTimedOut = false;

	bool bSupportStateL = false;
	bool bSupportStateR = false;
	EPhysAnimSupportMode SupportMode = EPhysAnimSupportMode::Airborne;
	double SupportGapTimerMs = 0.0;
	TOptional<bool> ProxyInsideHull;
	TOptional<double> ProxyOutsideHullDurationMs;
	int32 ActiveSupportSideCount = 0;
	double SupportHullAreaCm2 = 0.0;
	double SupportPatchAreaLCm2 = 0.0;
	double SupportPatchAreaRCm2 = 0.0;
	TArray<FVector2D> SupportHullPointsCm;
	FVector2D ComProxyPosCm = FVector2D::ZeroVector;
	double MaxPenetrationCm = 0.0;
	int32 SupportChurnCount = 0;
	double SupportChurnHz = 0.0;
	bool bCalfWorldContactL = false;
	bool bCalfWorldContactR = false;
	bool bCalfContactTerminal = false;

	double ControlAlpha = 0.0;
	FString ShellBookkeepingState;
	double ShellInfluenceMateriality = 0.0;
	int32 TopologyChangeCount = 0;
	int32 AuthorityConflictCount = 0;
	int32 ShellHelperUsedCount = 0;
	int32 MovementReclaimCount = 0;
	bool bContinuityBookkeepingMismatch = false;
	double PelvisSleepDurationMs = 0.0;
	bool bPhysicalContinuityValidatorPassed = true;

	EPhysAnimContaminationClass ContaminationClass = EPhysAnimContaminationClass::None;
	FName ContaminationSourceBody = NAME_None;
	FName ContaminationSourceSubsystem = NAME_None;
	bool bMeshWideAssistDetected = false;
	bool bNonCriticalBodyAssistDetected = false;
	FName ExcludedBodyWorldContactSource = NAME_None;
	double GlobalBlendWeight = 0.0;
	bool bMeshUpdateWhenKinematicEnabled = false;

	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	TArray<EPhysAnimTerminalReason> CoTerminalReasons;
	int64 TerminalSubstepTimestamp = 0;
	bool bTerminalFrameArtifactCaptured = false;
};

struct FPhysAnimRunArtifactSnapshotInput
{
	FPhysAnimPlantContractValidationResult Plant;
	FPhysAnimCapsuleContractValidationResult Capsule;
	FPhysAnimContinuityValidationResult Continuity;
	FPhysAnimAuthorityValidationResult Authority;
	FPhysAnimMovementReclaimValidationResult MovementReclaim;
	FPhysAnimShellHelperValidationResult ShellHelper;
	FPhysAnimControllerStabilityValidationResult ControllerStability;
	FPhysAnimRunArtifactSnapshot Values;
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
	FPhysAnimRunArtifactSnapshot BuildRunArtifactSnapshot(const FPhysAnimRunArtifactSnapshotInput& Input);
}
