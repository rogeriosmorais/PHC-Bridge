#pragma once

#include "PhysAnimFailureArbitration.h"
#include "PhysAnimTruthTypes.h"

struct FPhysAnimContinuitySnapshot
{
	int32 TopologyChangeCount = 0;
	bool bAllCriticalBodiesValid = true;
	bool bAllCriticalBodiesSimulating = true;
	double PelvisSleepDurationMs = 0.0;
	bool bContinuityBookkeepingMismatch = false;
	bool bIsBridgeActive = false;
	bool bKineticGateActive = false;
};

struct FPhysAnimContinuityValidationResult
{
	int32 TopologyChangeCount = 0;
	bool bContinuityBookkeepingMismatch = false;
	double PelvisSleepDurationMs = 0.0;
	bool bIsBridgeActive = false;
	bool bKineticGateActive = false;
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
	bool bIsBridgeActive = false;
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
	bool bIsBridgeActive = false;
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

struct FPhysAnimSupportContractSnapshot
{
	bool bSupportStateL = false;
	bool bSupportStateR = false;
	EPhysAnimSupportMode SupportMode = EPhysAnimSupportMode::Airborne;
	double SupportGapTimerMs = 0.0;
	double SupportGapMaxMs = 100.0;
	int32 ActiveSupportSideCount = 0;
	double SupportHullAreaCm2 = 0.0;
	double SupportAreaMinCm2 = 50.0;
	double SupportPatchAreaLCm2 = 0.0;
	double SupportPatchAreaRCm2 = 0.0;
	TArray<FVector2D> SupportHullPointsCm;
	FVector2D ComProxyPosCm = FVector2D::ZeroVector;
	double MaxPenetrationCm = 0.0;
	int32 SupportChurnCount = 0;
	double SupportChurnHz = 0.0;
	TOptional<bool> ProxyInsideHull;
	TOptional<double> ProxyOutsideHullDurationMs;
	EPhysAnimTerminalReason ProxyTerminalReason = EPhysAnimTerminalReason::None;
};

struct FPhysAnimSupportContractValidationResult
{
	bool bSupportStateL = false;
	bool bSupportStateR = false;
	EPhysAnimSupportMode SupportMode = EPhysAnimSupportMode::Airborne;
	double SupportGapTimerMs = 0.0;
	int32 ActiveSupportSideCount = 0;
	double SupportHullAreaCm2 = 0.0;
	double SupportPatchAreaLCm2 = 0.0;
	double SupportPatchAreaRCm2 = 0.0;
	TArray<FVector2D> SupportHullPointsCm;
	FVector2D ComProxyPosCm = FVector2D::ZeroVector;
	double MaxPenetrationCm = 0.0;
	int32 SupportChurnCount = 0;
	double SupportChurnHz = 0.0;
	TOptional<bool> ProxyInsideHull;
	TOptional<double> ProxyOutsideHullDurationMs;
	bool bSupportContractPassed = true;
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
	bool bIsBridgeActive = false;
	bool bCapsuleContractPassed = true;

	double HoldDurationSec = 0.0;
	double BalanceActiveStandingContinuousSec = 0.0;
	int32 BalanceActiveStandingExitCount = 0;
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
	bool bPhysicsControlComponentAvailable = false;
	int32 ControlledBodyCount = 0;
	int32 PolicyInferenceSuccessCount = 0;
	int32 PolicyInferenceAttemptCount = 0;
	int32 PolicyInferenceFailureCount = 0;
	double PolicyInferenceLatencyMsMax = 0.0;
	bool bPolicyModelLoaded = false;
	FString PolicyRuntimeName;
	FString PolicyModelName;
	bool bPolicyInputBuffersFinite = false;
	int32 PolicyActionSampleCount = 0;
	double PolicyActionRawMeanAbsMax = 0.0;
	double PolicyActionConditionedMeanAbsMax = 0.0;
	int32 PolicyActionClampedFloatMax = 0;
	int32 ControlTargetSampleCount = 0;
	int32 ControlTargetNormalWrites = 0;
	int32 ControlTargetTotalWrites = 0;
	double ControlTargetMaxDeltaDeg = 0.0;
	double ControlTargetMeanDeltaDegMax = 0.0;
	double ControlTargetMaxRawPolicyOffsetDeg = 0.0;
	double ControlTargetMeanRawPolicyOffsetDegMax = 0.0;
	int32 PoseSearchQueryCount = 0;
	int32 PoseSearchValidResultCount = 0;
	FString PoseSearchSelectedAnimationName;
	double PoseSearchSelectedTime = 0.0;
	int32 PoseSearchConsecutiveInvalidFrameCount = 0;
	int32 RendererFacingMotionSampleCount = 0;
	int32 RendererFacingMotionActiveSampleCount = 0;
	double RendererFacingMotionMaxRootWorldPositionDriftCm = 0.0;
	double RendererFacingMotionMaxMeshWorldPositionDriftCm = 0.0;
	double RendererFacingMotionMaxRootYawDeltaDeg = 0.0;
	double RendererFacingMotionMaxBodyDeltaCm = 0.0;
	double RendererFacingMotionMaxBodyDeltaDeg = 0.0;
	bool bRendererFacingMotionUsedNullRhi = false;
	int32 RuntimeBodySampleCount = 0;
	int32 RuntimeSimulatingBodyCount = 0;
	int32 RuntimeMinSimulatingBodyCount = 0;
	int32 CriticalBodyValidMask = 0;
	int32 CriticalBodySimulatingMask = 0;
	int32 SupportBodyValidMask = 0;
	int32 SupportBodySimulatingMask = 0;
	int32 CriticalBodyValidAllFramesMask = 0;
	int32 CriticalBodySimulatingAllFramesMask = 0;
	int32 SupportBodyValidAllFramesMask = 0;
	int32 SupportBodySimulatingAllFramesMask = 0;
	double RuntimeMaxBodyLinearSpeedCmPerSecond = 0.0;
	double RuntimeMaxBodyAngularSpeedDegPerSecond = 0.0;
	bool bPhysicalPerturbationApplied = false;
	double PerturbationMeasuredDeltaVCmPerSecond = 0.0;
	double ThighBaselineWork = 0.0;
	double ThighActivationWork = 0.0;
	double ThighNetWork = 0.0;
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

	// Stage 2A Locomotion Telemetry
	FString RootMode;
	FString LocomotionIntent;
	bool bPolicyOutputActive = false;
	FVector CapsuleVelocity = FVector::ZeroVector;
	double ShellDivergencePeak = 0.0;

	double ActionMagnitudeVariance = 0.0;
};

struct FPhysAnimRunArtifactSnapshotInput
{
	FPhysAnimPlantContractValidationResult Plant;
	FPhysAnimCapsuleContractValidationResult Capsule;
	FPhysAnimContinuityValidationResult Continuity;
	FPhysAnimSupportContractValidationResult Support;
	FPhysAnimAuthorityValidationResult Authority;
	FPhysAnimMovementReclaimValidationResult MovementReclaim;
	FPhysAnimShellHelperValidationResult ShellHelper;
	FPhysAnimControllerStabilityValidationResult ControllerStability;
	FPhysAnimRunArtifactSnapshot Values;
	TArray<FPhysAnimFailureCandidate> FailureCandidates;
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
	FPhysAnimSupportContractValidationResult ValidateSupport(const FPhysAnimSupportContractSnapshot& Snapshot);
	FPhysAnimRunArtifactSnapshot BuildRunArtifactSnapshot(const FPhysAnimRunArtifactSnapshotInput& Input);
}
