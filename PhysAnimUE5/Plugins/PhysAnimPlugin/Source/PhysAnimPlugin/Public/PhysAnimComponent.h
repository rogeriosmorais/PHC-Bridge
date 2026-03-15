#pragma once

#include "Components/ActorComponent.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsControlActor.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchTrajectoryLibrary.h"
#include "PoseSearch/PoseSearchTrajectoryPredictor.h"
#include "Animation/TrajectoryTypes.h"
#include "PhysAnimBridge.h"

#include "PhysAnimComponent.generated.h"

class UAnimInstance;
class UAnimationAsset;
class UAnimSequence;
class UCapsuleComponent;
class UCharacterMovementComponent;
class UPhysicsControlComponent;
class UPoseSearchDatabase;
class USkeletalMeshComponent;

struct FBridgeIntentState
{
	FVector WorldMoveDirection = FVector::ZeroVector;
	FVector LocalMoveDirection = FVector::ZeroVector;
	float IntentMagnitude = 0.0f;
	float DesiredSpeedCmPerSecond = 0.0f;
	float DesiredFacingYawDegrees = 0.0f;
	bool bHasDesiredFacing = false;
};

struct FBridgeTrajectoryState
{
	FVector DesiredVelocityCmPerSecond = FVector::ZeroVector;
	FVector AcceptedVelocityCmPerSecond = FVector::ZeroVector;
	FVector QueryVelocityCmPerSecond = FVector::ZeroVector;
	FTransformTrajectory QueryTrajectory;
	float DesiredControllerYawLastUpdate = 0.0f;
	float LastDeltaTimeSeconds = 1.0f / 30.0f;
	bool bInitialized = false;
};

struct FBridgeShellState
{
	FVector PendingPlanarVelocityCmPerSecond = FVector::ZeroVector;
	FVector AcceptedPlanarVelocityCmPerSecond = FVector::ZeroVector;
	FVector AcceptedWorldDeltaCm = FVector::ZeroVector;
	FVector LastAcceptedActorLocation = FVector::ZeroVector;
	bool bInitialized = false;
	bool bLastMoveBlocked = false;
};

enum class EBridgeLocomotionAuthorityState : uint8
{
	Idle,
	StartupLocomotion,
	Locomoting
};

USTRUCT(BlueprintType)
struct FPhysAnimStabilizationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bForceZeroActions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float ActionScale = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActionClampAbs = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ActionSmoothingAlpha = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float StartupRampSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "1.0"))
	float PolicyControlRateHz = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bApplyTrainingAlignedMassScales = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TrainingAlignedMassScaleBlend = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bApplyTrainingAlignedControlFamilyProfile = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TrainingAlignedControlFamilyProfileBlend = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TrainingAlignedLocomotionLowerLimbResponsePolicyBlend = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bApplyTrainingAlignedToeLimitPolicy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TrainingAlignedToeLimitPolicyBlend = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bApplyTrainingAlignedLowerLimbTargetRangePolicy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TrainingAlignedLowerLimbTargetRangePolicyBlend = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bApplyTrainingAlignedDistalLocomotionTargetPolicy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TrainingAlignedDistalLocomotionTargetPolicyBlend = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float DistalLocomotionTargetPolicyActivationSpeedCmPerSec = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bApplyTrainingAlignedDistalLocomotionCompositionPolicy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float DistalLocomotionCompositionPolicyActivationSpeedCmPerSec = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float DistalLocomotionCompositionPolicyExitSpeedCmPerSec = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float DistalLocomotionCompositionPolicyEnterHoldSeconds = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float DistalLocomotionCompositionPolicyExitHoldSeconds = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float DistalLocomotionCompositionPolicyIntentGraceSeconds = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float MaxAngularStepDegreesPerSecond = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float AngularStrengthMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float AngularDampingRatioMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float AngularExtraDampingMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bUseSkeletalAnimationTargets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bLogActionDiagnostics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.1"))
	float ActionDiagnosticsIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bEnableInstabilityFailStop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float MaxRootHeightDeltaCm = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float MaxRootLinearSpeedCmPerSecond = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float MaxRootAngularSpeedDegPerSecond = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float InstabilityGracePeriodSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "-1", ClampMax = "4"))
	int32 MaxAutoUnlockBringUpGroup = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bEnablePrePolicyShellRecovery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float PrePolicyShellRecoveryOffsetThresholdCm = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float PrePolicyShellRecoveryRootAngularSpeedThresholdDegPerSec = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bLockCharacterMovementUntilStartupReady = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady", ClampMin = "0.0"))
	float StartupQuietLinearSpeedThresholdCmPerSecond = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady", ClampMin = "0.0"))
	float StartupQuietAngularSpeedThresholdDegPerSec = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady", ClampMin = "0.0"))
	float StartupQuietRequiredSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady"))
	bool bDelayMovementUnlockUntilPolicySettled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady"))
	bool bRestoreCharacterMovementAfterStartupReady = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady"))
	bool bEnableBridgeOwnedMovementWhileCharacterMovementLocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgeOwnedMovementMaxPlanarSpeedCmPerSecond = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgeOwnedMovementAccelerationCmPerSecondSq = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgeOwnedMovementDecelerationCmPerSecondSq = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked"))
	bool bBridgeOwnedMovementUseControllerYaw = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgeOwnedMovementRotationInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked"))
	bool bBridgePoseSearchUseStabilizedWalkQuerySpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0", ClampMax = "1.0"))
	float BridgePoseSearchWalkIntentThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgePoseSearchStabilizedWalkSpeedCmPerSecond = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgePoseSearchWalkContinuationSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0", ClampMax = "180.0"))
	float BridgePoseSearchContinuationMaxDirectionDeltaDegrees = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgePoseSearchContinuationMaxSpeedDeltaCmPerSecond = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgePoseSearchStartupLocomotionSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgePoseSearchSustainAcceptedSpeedThresholdCmPerSecond = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && !bRestoreCharacterMovementAfterStartupReady && bEnableBridgeOwnedMovementWhileCharacterMovementLocked", ClampMin = "0.0"))
	float BridgePoseSearchExitHoldSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && bDelayMovementUnlockUntilPolicySettled", ClampMin = "0.0", ClampMax = "1.0"))
	float PolicySettleMinInfluenceAlpha = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && bDelayMovementUnlockUntilPolicySettled", ClampMin = "0.0"))
	float PolicySettleMaxShellOffsetCm = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && bDelayMovementUnlockUntilPolicySettled", ClampMin = "0.0"))
	float PolicySettleMaxRootLinearSpeedCmPerSecond = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && bDelayMovementUnlockUntilPolicySettled", ClampMin = "0.0"))
	float PolicySettleMaxRootAngularSpeedDegPerSec = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && bDelayMovementUnlockUntilPolicySettled", ClampMin = "0.0"))
	float PolicySettleRequiredSeconds = 0.15f;

	bool operator==(const FPhysAnimStabilizationSettings& Other) const
	{
		return bForceZeroActions == Other.bForceZeroActions &&
			FMath::IsNearlyEqual(ActionScale, Other.ActionScale) &&
			FMath::IsNearlyEqual(ActionClampAbs, Other.ActionClampAbs) &&
			FMath::IsNearlyEqual(ActionSmoothingAlpha, Other.ActionSmoothingAlpha) &&
			FMath::IsNearlyEqual(StartupRampSeconds, Other.StartupRampSeconds) &&
			FMath::IsNearlyEqual(PolicyControlRateHz, Other.PolicyControlRateHz) &&
			bApplyTrainingAlignedMassScales == Other.bApplyTrainingAlignedMassScales &&
			FMath::IsNearlyEqual(TrainingAlignedMassScaleBlend, Other.TrainingAlignedMassScaleBlend) &&
			bApplyTrainingAlignedControlFamilyProfile == Other.bApplyTrainingAlignedControlFamilyProfile &&
			FMath::IsNearlyEqual(TrainingAlignedControlFamilyProfileBlend, Other.TrainingAlignedControlFamilyProfileBlend) &&
			bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy == Other.bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy &&
			FMath::IsNearlyEqual(TrainingAlignedLocomotionLowerLimbResponsePolicyBlend, Other.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend) &&
			bApplyTrainingAlignedToeLimitPolicy == Other.bApplyTrainingAlignedToeLimitPolicy &&
			FMath::IsNearlyEqual(TrainingAlignedToeLimitPolicyBlend, Other.TrainingAlignedToeLimitPolicyBlend) &&
			bApplyTrainingAlignedLowerLimbTargetRangePolicy == Other.bApplyTrainingAlignedLowerLimbTargetRangePolicy &&
			FMath::IsNearlyEqual(TrainingAlignedLowerLimbTargetRangePolicyBlend, Other.TrainingAlignedLowerLimbTargetRangePolicyBlend) &&
			bApplyTrainingAlignedDistalLocomotionTargetPolicy == Other.bApplyTrainingAlignedDistalLocomotionTargetPolicy &&
			FMath::IsNearlyEqual(TrainingAlignedDistalLocomotionTargetPolicyBlend, Other.TrainingAlignedDistalLocomotionTargetPolicyBlend) &&
			FMath::IsNearlyEqual(DistalLocomotionTargetPolicyActivationSpeedCmPerSec, Other.DistalLocomotionTargetPolicyActivationSpeedCmPerSec) &&
			bApplyTrainingAlignedDistalLocomotionCompositionPolicy == Other.bApplyTrainingAlignedDistalLocomotionCompositionPolicy &&
			FMath::IsNearlyEqual(DistalLocomotionCompositionPolicyActivationSpeedCmPerSec, Other.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec) &&
			FMath::IsNearlyEqual(DistalLocomotionCompositionPolicyExitSpeedCmPerSec, Other.DistalLocomotionCompositionPolicyExitSpeedCmPerSec) &&
			FMath::IsNearlyEqual(DistalLocomotionCompositionPolicyEnterHoldSeconds, Other.DistalLocomotionCompositionPolicyEnterHoldSeconds) &&
			FMath::IsNearlyEqual(DistalLocomotionCompositionPolicyIntentGraceSeconds, Other.DistalLocomotionCompositionPolicyIntentGraceSeconds) &&
			FMath::IsNearlyEqual(DistalLocomotionCompositionPolicyExitHoldSeconds, Other.DistalLocomotionCompositionPolicyExitHoldSeconds) &&
			FMath::IsNearlyEqual(MaxAngularStepDegreesPerSecond, Other.MaxAngularStepDegreesPerSecond) &&
			FMath::IsNearlyEqual(AngularStrengthMultiplier, Other.AngularStrengthMultiplier) &&
			FMath::IsNearlyEqual(AngularDampingRatioMultiplier, Other.AngularDampingRatioMultiplier) &&
			FMath::IsNearlyEqual(AngularExtraDampingMultiplier, Other.AngularExtraDampingMultiplier) &&
			bUseSkeletalAnimationTargets == Other.bUseSkeletalAnimationTargets &&
			bLogActionDiagnostics == Other.bLogActionDiagnostics &&
			FMath::IsNearlyEqual(ActionDiagnosticsIntervalSeconds, Other.ActionDiagnosticsIntervalSeconds) &&
			bEnableInstabilityFailStop == Other.bEnableInstabilityFailStop &&
			FMath::IsNearlyEqual(MaxRootHeightDeltaCm, Other.MaxRootHeightDeltaCm) &&
			FMath::IsNearlyEqual(MaxRootLinearSpeedCmPerSecond, Other.MaxRootLinearSpeedCmPerSecond) &&
			FMath::IsNearlyEqual(MaxRootAngularSpeedDegPerSecond, Other.MaxRootAngularSpeedDegPerSecond) &&
			FMath::IsNearlyEqual(InstabilityGracePeriodSeconds, Other.InstabilityGracePeriodSeconds) &&
			MaxAutoUnlockBringUpGroup == Other.MaxAutoUnlockBringUpGroup &&
			bEnablePrePolicyShellRecovery == Other.bEnablePrePolicyShellRecovery &&
			FMath::IsNearlyEqual(PrePolicyShellRecoveryOffsetThresholdCm, Other.PrePolicyShellRecoveryOffsetThresholdCm) &&
			FMath::IsNearlyEqual(PrePolicyShellRecoveryRootAngularSpeedThresholdDegPerSec, Other.PrePolicyShellRecoveryRootAngularSpeedThresholdDegPerSec) &&
			bLockCharacterMovementUntilStartupReady == Other.bLockCharacterMovementUntilStartupReady &&
			FMath::IsNearlyEqual(StartupQuietLinearSpeedThresholdCmPerSecond, Other.StartupQuietLinearSpeedThresholdCmPerSecond) &&
			FMath::IsNearlyEqual(StartupQuietAngularSpeedThresholdDegPerSec, Other.StartupQuietAngularSpeedThresholdDegPerSec) &&
			FMath::IsNearlyEqual(StartupQuietRequiredSeconds, Other.StartupQuietRequiredSeconds) &&
			bDelayMovementUnlockUntilPolicySettled == Other.bDelayMovementUnlockUntilPolicySettled &&
			bRestoreCharacterMovementAfterStartupReady == Other.bRestoreCharacterMovementAfterStartupReady &&
			bEnableBridgeOwnedMovementWhileCharacterMovementLocked == Other.bEnableBridgeOwnedMovementWhileCharacterMovementLocked &&
			FMath::IsNearlyEqual(BridgeOwnedMovementMaxPlanarSpeedCmPerSecond, Other.BridgeOwnedMovementMaxPlanarSpeedCmPerSecond) &&
			FMath::IsNearlyEqual(BridgeOwnedMovementAccelerationCmPerSecondSq, Other.BridgeOwnedMovementAccelerationCmPerSecondSq) &&
			FMath::IsNearlyEqual(BridgeOwnedMovementDecelerationCmPerSecondSq, Other.BridgeOwnedMovementDecelerationCmPerSecondSq) &&
			bBridgeOwnedMovementUseControllerYaw == Other.bBridgeOwnedMovementUseControllerYaw &&
			FMath::IsNearlyEqual(BridgeOwnedMovementRotationInterpSpeed, Other.BridgeOwnedMovementRotationInterpSpeed) &&
			bBridgePoseSearchUseStabilizedWalkQuerySpeed == Other.bBridgePoseSearchUseStabilizedWalkQuerySpeed &&
			FMath::IsNearlyEqual(BridgePoseSearchWalkIntentThreshold, Other.BridgePoseSearchWalkIntentThreshold) &&
			FMath::IsNearlyEqual(BridgePoseSearchStabilizedWalkSpeedCmPerSecond, Other.BridgePoseSearchStabilizedWalkSpeedCmPerSecond) &&
			FMath::IsNearlyEqual(BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond, Other.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond) &&
			FMath::IsNearlyEqual(BridgePoseSearchWalkContinuationSeconds, Other.BridgePoseSearchWalkContinuationSeconds) &&
			FMath::IsNearlyEqual(BridgePoseSearchContinuationMaxDirectionDeltaDegrees, Other.BridgePoseSearchContinuationMaxDirectionDeltaDegrees) &&
			FMath::IsNearlyEqual(BridgePoseSearchContinuationMaxSpeedDeltaCmPerSecond, Other.BridgePoseSearchContinuationMaxSpeedDeltaCmPerSecond) &&
			FMath::IsNearlyEqual(BridgePoseSearchStartupLocomotionSeconds, Other.BridgePoseSearchStartupLocomotionSeconds) &&
			FMath::IsNearlyEqual(BridgePoseSearchSustainAcceptedSpeedThresholdCmPerSecond, Other.BridgePoseSearchSustainAcceptedSpeedThresholdCmPerSecond) &&
			FMath::IsNearlyEqual(BridgePoseSearchExitHoldSeconds, Other.BridgePoseSearchExitHoldSeconds) &&
			FMath::IsNearlyEqual(PolicySettleMinInfluenceAlpha, Other.PolicySettleMinInfluenceAlpha) &&
			FMath::IsNearlyEqual(PolicySettleMaxShellOffsetCm, Other.PolicySettleMaxShellOffsetCm) &&
			FMath::IsNearlyEqual(PolicySettleMaxRootLinearSpeedCmPerSecond, Other.PolicySettleMaxRootLinearSpeedCmPerSecond) &&
			FMath::IsNearlyEqual(PolicySettleMaxRootAngularSpeedDegPerSec, Other.PolicySettleMaxRootAngularSpeedDegPerSec) &&
			FMath::IsNearlyEqual(PolicySettleRequiredSeconds, Other.PolicySettleRequiredSeconds);
	}

	bool operator!=(const FPhysAnimStabilizationSettings& Other) const
	{
		return !(*this == Other);
	}
};

UENUM()
enum class EPhysAnimRuntimeState : uint8
{
	Uninitialized,
	RuntimeReady,
	WaitingForPoseSearch,
	ReadyForActivation,
	BridgeActive,
	FailStopped,
};

UENUM(BlueprintType)
enum class EPhysAnimBridgeTraceOutputMode : uint8
{
	Off = 0,
	MetadataAndEvents = 1,
	Full = 2,
};

UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent))
class PHYSANIMPLUGIN_API UPhysAnimComponent : public UActorComponent, public IPoseSearchTrajectoryPredictorInterface
{
	GENERATED_BODY()

public:
	UPhysAnimComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void Predict(FTransformTrajectory& InOutTrajectory, int32 NumPredictionSamples, float SecondsPerPredictionSample, int32 NumHistorySamples) override;
	virtual void GetGravity(FVector& OutGravityAccel) override;
	virtual void GetCurrentState(FVector& OutPosition, FQuat& OutFacing, FVector& OutVelocity) override;
	virtual void GetVelocity(FVector& OutVelocity) override;

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	bool StartBridge();

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void StopBridge();

	UFUNCTION(BlueprintPure, Category = "PhysAnim")
	EPhysAnimRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category = "PhysAnim")
	bool IsReadyForScriptedPresentation() const;

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void SetPresentationPerturbationOverrideSeconds(float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void ClearPresentationPerturbationOverride();

protected:
	UPROPERTY(EditAnywhere, Category = "PhysAnim")
	TSoftObjectPtr<UNNEModelData> ModelDataAsset;

	/** A 1-frame animation of the character in a perfect T-Pose, used to extract base bone alignments. */
	UPROPERTY(EditDefaultsOnly, Category = "PhysAnim | Policy")
	TObjectPtr<UAnimSequence> TPoseReference;

	UPROPERTY(EditAnywhere, Category = "PhysAnim | Debug")
	bool bRunStartupTPoseIdentityCheck = false;

	/** Number of threads the ONNX CPU backend is permitted to use for inference. */
	UPROPERTY(EditDefaultsOnly, Category = "PhysAnim | Policy", meta = (ClampMin = "1"))
	int32 InferenceThreads = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	FPhysAnimStabilizationSettings StabilizationSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Trace")
	bool bEnableBridgeTraceOutput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Trace", meta = (EditCondition = "bEnableBridgeTraceOutput"))
	EPhysAnimBridgeTraceOutputMode BridgeTraceOutputMode = EPhysAnimBridgeTraceOutputMode::Full;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Trace", meta = (EditCondition = "bEnableBridgeTraceOutput", ClampMin = "0.1"))
	float BridgeTraceFlushIntervalSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Trace", meta = (EditCondition = "bEnableBridgeTraceOutput", ClampMin = "1"))
	int32 BridgeTraceSampleEveryNthFrame = 1;

private:
	bool ResolveRuntimeContext(FString& OutError);
	bool ValidateRequiredBodies(FString& OutError) const;
	bool ValidatePhysicsControlAuthoring(FString& OutError) const;
	bool ValidateRuntimePhysicsControl(FString& OutError) const;
	bool ValidatePoseSearchIntegration(FString& OutError);
	bool InitializeModel(FString& OutError);
	bool ValidateModelDescriptorContract(FString& OutError);
	bool QueryPoseSearch(FPoseSearchBlueprintResult& OutSearchResult, FString& OutError);
	bool GatherCurrentBodySamples(TArray<FPhysAnimBodySample>& OutBodySamples, FString& OutError) const;
	bool SampleFuturePoses(const FPoseSearchBlueprintResult& SearchResult, TArray<FPhysAnimFuturePoseSample>& OutFutureSamples, FString& OutError) const;
	bool ResolveMimicTargetReferenceDataOffset(
		const FPoseSearchBlueprintResult& SearchResult,
		FVector2D& OutDataOffsetXY,
		FString& OutError) const;
	bool RunInference(FString& OutError);
	FPhysAnimStabilizationSettings ResolveEffectiveStabilizationSettings() const;
	void LogBridgeStateSnapshot(const TCHAR* Context) const;
	bool ActivateRuntimePhysicsControl(FString& OutError);
	void DeactivateRuntimePhysicsControl(const TCHAR* Context);
	bool ActivateBridgeFromReadyState(const FPhysAnimStabilizationSettings& EffectiveSettings, const TCHAR* ActivationContext, FString& OutError);
	bool PrewarmPhysicsControlActivationPose();
	void EnterReadyForActivation(const FPhysAnimStabilizationSettings& EffectiveSettings, const TCHAR* Context, bool bLogDeferredStartupSuccess);
	void ActivateBridgePhysicsState(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ApplyTrainingAlignedMassScales(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResetTrainingAlignedMassScales();
	void ApplyTrainingAlignedToeLimitPolicy(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResetTrainingAlignedToeLimitPolicy();
	void ResetBridgePhysicsState();
	bool GatherCurrentPoseControlTargetOrientations(TMap<FName, FQuat>& OutTargetOrientations, FString& OutError) const;
	bool SeedControlTargetsFromCurrentPose(float DeltaTime, FString& OutError);
	void UpdateBridgeLocomotionAuthorityState(const FVector& QueryVelocity, const FPhysAnimStabilizationSettings& EffectiveSettings, double CurrentTimeSeconds);
	bool IsBridgeLocomotionQueryActive() const;
	bool IsBridgeLocomotionEntryRequested(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	void ApplyRuntimeControlTuning(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void LogActivationSummary(
		const FPhysAnimStabilizationSettings& EffectiveSettings,
		const TCHAR* Context,
		bool bCurrentPoseTargetsSeeded,
		bool bActivationPrepassCompleted,
		float SimulationHandoffProgress) const;
	bool ConditionModelActions(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError);
	void UnlockBringUpGroup(int32 GroupIndex, const TCHAR* Context);
	void AdvanceBringUpState(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings);
	bool AreAllBringUpGroupsUnlocked() const;
	bool IsBringUpGroupUnlocked(int32 GroupIndex) const;
	bool IsBringUpGroupControlRampActive(int32 GroupIndex) const;
	bool IsBoneInUnlockedBringUpGroup(FName BoneName) const;
	float CalculateBringUpGroupControlAuthorityAlpha(int32 GroupIndex, const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	bool GatherRuntimeInstabilityBodySamples(TArray<FPhysAnimBodyInstabilitySample>& OutSamples) const;
	bool CheckRuntimeInstability(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError);
	void LogBodyModifierTelemetrySnapshot(const TCHAR* Context) const;
	void ResetPendingBodyModifiersToCachedTargets();
	float ResolveSelfObservationGroundHeight(const TArray<FPhysAnimBodySample>& CurrentBodySamples) const;
	bool BuildTerrainObservation(const TArray<FPhysAnimBodySample>& CurrentBodySamples, TArray<float>& OutTerrain, FString& OutError) const;
	bool SampleTerrainGroundHeights(
		const FVector& RootLocation,
		const FQuat& RootRotation,
		float FallbackGroundHeight,
		TArray<float>& OutGroundHeights,
		FString& OutError) const;
	void ApplyControlTargets(
		float PolicyStepDeltaTime,
		const FPhysAnimStabilizationSettings& EffectiveSettings,
		bool bApplyNewPolicyStepThisTick,
		FString& OutError);
	bool IsMovementSmokeModeEnabled() const;
	void ApplyMovementSmokeInput(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void MaybeLogRuntimeDiagnostics(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	bool HandlePrePolicyShellRecovery(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ApplyStartupMovementLock();
	void ReleaseStartupMovementLock(bool bRestoreCharacterMovement = true);
	void ResetStartupQuietWindowState();
	bool UpdateStartupQuietWindow(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, float& OutLinearSpeedCmPerSecond, float& OutAngularSpeedDegPerSecond);
	void ResetPolicySettleWindowState();
	bool UpdatePolicySettleWindow(const FPhysAnimStabilizationSettings& EffectiveSettings, float& OutShellOffsetCm, float& OutRootLinearSpeedCmPerSecond, float& OutRootAngularSpeedDegPerSecond);
	bool ShouldUseBridgeOwnedMovementDrive(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	void CaptureBridgeIntent(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ApplyBridgeOwnedMovementDrive(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResetBridgeLocomotionAuthorityState();
	bool QueryPoseSearchWithBridgeTrajectory(FPoseSearchBlueprintResult& OutSearchResult, FString& OutError);
	void UpdateBridgePoseSearchTrajectory(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResolveBridgePoseSearchQueryVelocity(const FPhysAnimStabilizationSettings& EffectiveSettings, FVector& OutQueryVelocity, float* OutIntentMagnitude = nullptr) const;
	void ApplyBridgePoseSearchSelectionPolicy(
		FPoseSearchBlueprintResult& InOutSearchResult,
		float QueryDeltaTimeSeconds,
		const FVector& QueryVelocity,
		const FPhysAnimStabilizationSettings& EffectiveSettings);
	void AdvanceBridgePoseSearchResultTime(FPoseSearchBlueprintResult& InOutSearchResult, float DeltaTimeSeconds) const;
	bool ShouldContinueBridgePoseSearchWalkSelection(
		const FVector& QueryVelocity,
		const FPhysAnimStabilizationSettings& EffectiveSettings,
		double CurrentTimeSeconds) const;
	static bool IsBridgePoseSearchIdleResult(const FPoseSearchBlueprintResult& SearchResult);
	void ResetStabilizationRuntimeState();
	void FailStop(const FString& Reason);
	void StartBridgeTraceSession();
	void StopBridgeTraceSession(const TCHAR* StopContext, const FString& Message);
	void FlushBridgeTrace(bool bForce);
	void EmitBridgeTraceEvent(
		const TCHAR* EventType,
		const FString& Message,
		const FString& Error = FString(),
		const TCHAR* PreviousRuntimeState = nullptr,
		const TCHAR* NewRuntimeState = nullptr);
	EPhysAnimBridgeTraceOutputMode ResolveBridgeTraceOutputMode() const;
	void UpdateStabilizationStressTestState(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void TrackStabilizationStressTestObservations();
	float ResolveStabilizationStressTestMultiplier() const;
	float CalculateSimulationHandoffAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	float CalculateCurrentControlAuthorityAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	float CalculateCurrentPolicyInfluenceAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	bool IsPresentationPerturbationOverrideActive() const;

	void CacheRestPoses(UAnimSequence* TPoseAnim);
	bool BeginStartupTPoseCapture(FString& OutError);
	bool FinalizeStartupTPoseCaptureAndStartBridge(FString& OutError);
	void SaveStartupAnimationState(USkeletalMeshComponent* SkeletalMesh);
	void RestoreStartupAnimationState(USkeletalMeshComponent* SkeletalMesh);
	void LogTPoseIdentityCheck() const;

	UE::NNE::IModelInstanceRunSync* GetModelInstanceRunSync() const;
	TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const;
	TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const;

	TWeakObjectPtr<USkeletalMeshComponent> MeshComponent;
	TWeakObjectPtr<UPhysicsControlComponent> PhysicsControlComponent;
	TWeakObjectPtr<UAnimInstance> AnimInstance;

	TWeakInterfacePtr<INNERuntimeGPU> RuntimeGPU;
	TWeakInterfacePtr<INNERuntimeCPU> RuntimeCPU;
	TSharedPtr<UE::NNE::IModelGPU> ModelGPU;
	TSharedPtr<UE::NNE::IModelCPU> ModelCPU;
	TSharedPtr<UE::NNE::IModelInstanceGPU> ModelInstanceGPU;
	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstanceCPU;

	TArray<FTransform> CachedSmplObservationRestComponentTransforms;

	TObjectPtr<UNNEModelData> LoadedModelData = nullptr;
	TObjectPtr<UPoseSearchDatabase> LoadedPoseSearchDatabase = nullptr;

	FPhysAnimTensorIndexMap TensorIndexMap;

	TArray<float> SelfObservationBuffer;
	TArray<float> MimicTargetPosesBuffer;
	TArray<float> TerrainBuffer;
	TArray<float> ActionOutputBuffer;
	TArray<float> PreviousConditionedActionBuffer;
	TArray<float> ConditionedActionBuffer;
	TArray<UE::NNE::FTensorBindingCPU> InputBindings;
	TArray<UE::NNE::FTensorBindingCPU> OutputBindings;

	FPoseSearchBlueprintResult LastValidPoseSearchResult;
	int32 ConsecutiveInvalidPoseSearchFrames = 0;
	bool bStartupReported = false;
	bool bPendingStartupRestPoseCapture = false;
	bool bHasSavedStartupMovementLockState = false;
	bool bStartupMovementLockActive = false;
	bool bStartupMovementLockOriginalTickEnabled = false;
	uint8 StartupMovementLockOriginalMode = 0;
	uint8 StartupMovementLockOriginalCustomMovementMode = 0;
	double StartupQuietWindowAccumulatedSeconds = 0.0;
	bool bHasLastStartupQuietActorRotation = false;
	FRotator LastStartupQuietActorRotation = FRotator::ZeroRotator;
	double LastStartupQuietGateLogTimeSeconds = -1.0;
	double PolicySettleWindowAccumulatedSeconds = 0.0;
	double LastPolicySettleGateLogTimeSeconds = -1.0;
	FBridgeIntentState BridgeIntentState;
	FBridgeTrajectoryState BridgeTrajectoryState;
	FBridgeShellState BridgeShellState;
	FVector BridgeOwnedMovementPlanarVelocityCmPerSecond = FVector::ZeroVector;
	FVector BridgeOwnedMovementLastWorldIntent = FVector::ZeroVector;
	FVector BridgePoseSearchQueryVelocityCmPerSecond = FVector::ZeroVector;
	double LastBridgeOwnedMovementLogTimeSeconds = -1.0;
	double LastBridgeOwnedMovementNoInputLogTimeSeconds = -1.0;
	FTransformTrajectory BridgePoseSearchTrajectory;
	float BridgePoseSearchDesiredControllerYawLastUpdate = 0.0f;
	float LastBridgePoseSearchDeltaTimeSeconds = 1.0f / 30.0f;
	double LastBridgePoseSearchTrajectoryLogTimeSeconds = -1.0;
	FPoseSearchBlueprintResult BridgePoseSearchLatchedWalkResult;
	FVector BridgePoseSearchLatchedQueryDirection = FVector::ZeroVector;
	float BridgePoseSearchLatchedQuerySpeedCmPerSecond = 0.0f;
	double BridgePoseSearchWalkLatchExpireTimeSeconds = -1.0;
	bool bHasBridgePoseSearchLatchedWalkResult = false;
	bool bBridgePoseSearchTrajectoryInitialized = false;
	EBridgeLocomotionAuthorityState BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	double BridgeLocomotionStateEnterTimeSeconds = -1.0;
	double BridgeLocomotionExitHoldStartTimeSeconds = -1.0;
	double LastPrePolicyShellRecoveryLogTimeSeconds = -1.0;
	bool bHasSavedStartupAnimationState = false;
	uint8 SavedStartupAnimationMode = 0;
	TSubclassOf<UAnimInstance> SavedStartupAnimClass;
	TObjectPtr<UAnimationAsset> SavedStartupAnimationAsset = nullptr;
	FString ActiveRuntimeName;
	FPhysAnimStabilizationSettings LastAppliedStabilizationSettings;
	FPhysAnimActionDiagnostics LastActionDiagnostics;
	FPhysAnimControlTargetDiagnostics LastControlTargetDiagnostics;
	FPhysAnimRuntimeInstabilityState RuntimeInstabilityState;
	FPhysAnimRuntimeInstabilityDiagnostics LastRuntimeInstabilityDiagnostics;
	TSharedPtr<class FPhysAnimBridgeTraceWriter> BridgeTraceWriter;
	FString CurrentBridgeTraceSessionId;
	int64 BridgeTraceTickCounter = 0;
	double BridgeTraceLastFlushTimeSeconds = -1.0;
	TMap<FName, FQuat> PreviousControlTargetRotations;
	TMap<FName, FQuat> PolicyBlendStartControlTargetRotations;
	bool bPolicyTargetsAppliedLastFrame = false;
	float SimulationHandoffAlpha = 0.0f;
	bool bLastAppliedSimulationHandoffSettled = false;
	float LastAppliedControlAuthorityAlpha = -1.0f;
	double BridgeStartTimeSeconds = 0.0;
	double SimulationHandoffCompletedTimeSeconds = -1.0;
	double PolicyInfluenceRampStartTimeSeconds = -1.0;
	int32 HighestUnlockedBringUpGroupIndex = INDEX_NONE;
	float BringUpGroupStableAccumulatedSeconds = 0.0f;
	TArray<double> BringUpGroupActivationTimeSeconds;
	TArray<double> BringUpGroupControlRampStartTimeSeconds;
	TArray<FName> PendingBodyModifierCachedResetNames;
	double LastRuntimeDiagnosticsLogTimeSeconds = -1.0;
	float PolicyUpdateAccumulatorSeconds = -1.0f;
	int32 LastPolicyElapsedSteps = 0;
	int32 PolicyControlTicksExecuted = 0;
	int32 PolicyControlTicksSkipped = 0;
	double LastPolicyControlUpdateTimeSeconds = -1.0;
	bool bDistalLocomotionCompositionModeActive = false;
	float DistalLocomotionCompositionTimeAboveEnterSeconds = 0.0f;
	float DistalLocomotionCompositionTimeBelowExitSeconds = 0.0f;
	float DistalLocomotionCompositionTimeSinceActiveIntentSeconds = -1.0f;
	FVector LastMovementSmokeLocalIntent = FVector::ZeroVector;
	FVector LastMovementSmokeWorldIntent = FVector::ZeroVector;
	FVector LastMovementSmokeOwnerVelocityCmPerSecond = FVector::ZeroVector;
	FVector MovementSmokeStartLocation = FVector::ZeroVector;
	FVector ShellCouplingReferenceRootLocalOffsetCm = FVector::ZeroVector;
	FName LastMovementSmokePhaseName = NAME_None;
	bool bMovementSmokeScriptStarted = false;
	bool bMovementSmokeCompletionLogged = false;
	bool bHasShellCouplingReferenceRootLocalOffset = false;
	double PresentationPerturbationOverrideEndTimeSeconds = -1.0;
	bool bLastAppliedPresentationRootSimulationEnabled = false;
	double StabilizationStressTestStartTimeSeconds = -1.0;
	bool bStabilizationStressTestCompletionLogged = false;
	double StabilizationStressTestFirstAngularSpikeTimeSeconds = -1.0;
	double StabilizationStressTestFirstLinearSpikeTimeSeconds = -1.0;
	double StabilizationStressTestFirstInstabilitySignTimeSeconds = -1.0;
	float StabilizationStressTestFirstAngularSpikeMultiplier = 1.0f;
	float StabilizationStressTestFirstLinearSpikeMultiplier = 1.0f;
	float StabilizationStressTestFirstInstabilityMultiplier = 1.0f;
	FName StabilizationStressTestFirstAngularSpikeBoneName = NAME_None;
	FName StabilizationStressTestFirstLinearSpikeBoneName = NAME_None;
	FVector StabilizationStressTestBaselineActorLocation = FVector::ZeroVector;
	FVector StabilizationStressTestBaselineSpineLocalOffset = FVector::ZeroVector;
	FVector StabilizationStressTestBaselineHeadLocalOffset = FVector::ZeroVector;
	FVector StabilizationStressTestBaselineLeftFootLocalOffset = FVector::ZeroVector;
	FVector StabilizationStressTestBaselineRightFootLocalOffset = FVector::ZeroVector;
	TMap<FName, float> OriginalBodyMassScales;
	bool bHasSavedBodyMassScales = false;
	TMap<FName, uint8> OriginalToeTwistMotions;
	TMap<FName, uint8> OriginalToeSwing1Motions;
	TMap<FName, uint8> OriginalToeSwing2Motions;
	TMap<FName, float> OriginalToeTwistLimits;
	TMap<FName, float> OriginalToeSwing1Limits;
	TMap<FName, float> OriginalToeSwing2Limits;
	bool bHasSavedToeConstraintLimits = false;
	FName OriginalMeshCollisionProfileName = NAME_None;
	ECollisionEnabled::Type OriginalMeshCollisionEnabled = ECollisionEnabled::NoCollision;
	TEnumAsByte<ECollisionResponse> OriginalMeshPawnResponse = ECollisionResponse::ECR_Block;
	bool bHasSavedMeshCollisionState = false;
	ECollisionEnabled::Type OriginalCapsuleCollisionEnabled = ECollisionEnabled::NoCollision;
	bool bHasSavedCapsuleCollisionState = false;
	bool bHasSavedCharacterMovementState = false;
	bool bOriginalCharacterMovementTickEnabled = false;
	uint8 OriginalCharacterMovementMode = 0;
	uint8 OriginalCharacterCustomMovementMode = 0;

public:
	static bool BuildConditionedActions(
		const TArray<float>& RawActions,
		const TArray<float>* PreviousConditionedActions,
		const FPhysAnimActionConditioningSettings& Settings,
		TArray<float>& OutConditionedActions,
		FPhysAnimActionDiagnostics& OutDiagnostics,
		FString& OutError);

	static FQuat LimitTargetRotationStep(
		const FQuat& PreviousRotation,
		const FQuat& TargetRotation,
		float MaxAngularStepDegrees);

	static bool EvaluateRuntimeInstability(
		const FVector& RootLocationCm,
		const FVector& RootLinearVelocityCmPerSecond,
		const FVector& RootAngularVelocityDegPerSecond,
		float DeltaTimeSeconds,
		const FPhysAnimRuntimeInstabilitySettings& Settings,
		FPhysAnimRuntimeInstabilityState& InOutState,
		FPhysAnimRuntimeInstabilityDiagnostics& OutDiagnostics,
		FString& OutError);

	static FQuat BuildCurrentPoseControlTargetOrientation(
		const FQuat& ParentWorldRotation,
		const FQuat& ChildWorldRotation);
	static float ResolveShellCouplingPlanarOffsetDeltaCm(
		const FVector& OwnerLocationCm,
		const FVector& RootLocationCm,
		const FVector& ReferenceRootLocalOffsetCm);
	static float ResolveShellCouplingPlanarVelocityDeltaCmPerSecond(
		const FVector& OwnerVelocityCmPerSecond,
		const FVector& RootVelocityCmPerSecond);
	static float ResolveShellCouplingPlanarVelocityAlignment(
		const FVector& OwnerVelocityCmPerSecond,
		const FVector& RootVelocityCmPerSecond);
	static void ResolveBodyModifierRuntimeMode(
		bool bForceZeroActions,
		bool bSimulationHandoffSettled,
		bool bBringUpGroupUnlocked,
		bool bIsRootBodyModifier,
		bool bAllowRootBodyModifierSimulation,
		EPhysicsMovementType& OutMovementType,
		float& OutPhysicsBlendWeight,
		bool& bOutUpdateKinematicFromSimulation);
	static ECollisionEnabled::Type ResolveBodyModifierCollisionType(
		bool bForceZeroActions,
		bool bSimulationHandoffSettled,
		bool bBringUpGroupUnlocked,
		bool bIsRootBodyModifier,
		bool bAllowRootBodyModifierSimulation);
	static bool ShouldResetBodyModifierToCachedBoneTransform(
		bool bForceZeroActions,
		bool bBodyModifierActivatedThisTick,
		bool bBringUpGroupUnlocked,
		bool bIsRootBodyModifier,
		bool bAllowRootBodyModifierSimulation);
	static int32 ResolveBringUpGroupIndex(FName BoneName);
	static int32 GetBringUpGroupCount();
	static bool ShouldDelayBringUpGroupControlRamp(int32 GroupIndex, int32 NumBringUpGroups);
	static bool ShouldStartBringUpGroupControlRamp(
		bool bForceZeroActions,
		bool bBringUpGroupUnlocked,
		bool bDelayBringUpGroupControlRamp,
		bool bPostUnlockSettleComplete);
	static bool ShouldStartPolicyInfluenceRamp(
		bool bForceZeroActions,
		bool bAllBringUpGroupsUnlocked,
		bool bFinalBringUpGroupControlRampActive,
		bool bPostFinalGroupControlSettleComplete);
	static bool ShouldApplyPolicyTargetToBone(FName BoneName, bool bPolicyInfluenceActive);
	static bool ShouldUseSkeletalAnimationTargetRepresentation(
		bool bConfiguredUseSkeletalAnimationTargets,
		bool bPolicyInfluenceActive);
	static bool ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(
		bool bUseSkeletalAnimationTargetRepresentation,
		bool bFirstPolicyEnabledFrame);
	static float ResolvePolicyTargetWriteDeltaTime(
		bool bUseSkeletalAnimationTargetRepresentation,
		bool bFirstPolicyEnabledFrame,
		float DeltaTime);
	static float ResolvePolicyTargetAngularVelocityDeltaTime(
		FName BoneName,
		bool bUseSkeletalAnimationTargetRepresentation,
		bool bFirstPolicyEnabledFrame,
		bool bDistalLocomotionCompositionModeActive,
		float DeltaTime);
	static float ResolveObservationGroundWorldZFromFloor(
		bool bHasWalkableFloor,
		bool bHasBlockingFloorHit,
		float FloorImpactPointZ,
		float CapsuleCenterZ,
		float CapsuleHalfHeight,
		float FloorDistance,
		float FallbackGroundWorldZ);
	static float ResolveSelfObservationSyntheticGroundHeight(
		float ObservationFrameRootZ,
		float RootWorldZ,
		float GroundWorldZ);
	static void MakeGroundRelativeCurrentReferenceBodySamples(
		const TArray<FPhysAnimBodySample>& SourceBodySamples,
		float GroundWorldZ,
		TArray<FPhysAnimBodySample>& OutBodySamples);
	static FVector2D ResolveMimicTargetReferenceDataOffsetXY(
		const FVector& CurrentSelectedWorldRootPosition,
		const FVector& CurrentSelectedDataRootPosition);
	static void MakeMimicTargetCurrentReferenceBodySamples(
		const TArray<FPhysAnimBodySample>& SourceBodySamples,
		const FVector2D& DataOffsetXY,
		float GroundWorldZ,
		TArray<FPhysAnimBodySample>& OutBodySamples);
	static float ResolvePolicyControlIntervalSeconds(float PolicyControlRateHz);
	static bool ShouldPrewarmPhysicsControlActivationPose(bool bHasSkeletalMeshComponent, bool bHasLeaderPoseComponent);
	static float ResolveTrainingAlignedMassScaleForBone(FName BoneName, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedMassScales(bool bApplyTrainingAlignedMassScales, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedToeLimitPolicy(bool bApplyTrainingAlignedToeLimitPolicy, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedLowerLimbTargetRangePolicy(bool bApplyTrainingAlignedLowerLimbTargetRangePolicy, float BlendAlpha);
	static float ResolveTrainingAlignedLowerLimbTargetRangeScaleForBone(FName BoneName, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedDistalLocomotionTargetPolicy(bool bApplyTrainingAlignedDistalLocomotionTargetPolicy, float BlendAlpha, float OwnerPlanarSpeedCmPerSec, float ActivationSpeedCmPerSec);
	static float ResolveTrainingAlignedDistalLocomotionTargetScaleForBone(FName BoneName, float BlendAlpha);
	static bool UpdateBinarySpeedModeWithHysteresis(
		bool bCurrentModeActive,
		float SpeedCmPerSec,
		float EnterThresholdCmPerSec,
		float ExitThresholdCmPerSec,
		float EnterHoldSeconds,
		float ExitHoldSeconds,
		float DeltaTimeSeconds,
		float& InOutTimeAboveEnterSeconds,
		float& InOutTimeBelowExitSeconds);
	static bool UpdateBinarySpeedModeWithIntentLatch(
		bool bCurrentModeActive,
		float SpeedCmPerSec,
		bool bHasActiveMovementIntent,
		float EnterThresholdCmPerSec,
		float ExitThresholdCmPerSec,
		float EnterHoldSeconds,
		float ExitHoldSeconds,
		float DeltaTimeSeconds,
		float& InOutTimeAboveEnterSeconds,
		float& InOutTimeBelowExitSeconds);
	static bool ShouldForceExplicitOnlyDistalLocomotionTargetMode(FName BoneName);
	static float ResolveTrainingAlignedControlStrengthScaleForBone(FName BoneName, float BlendAlpha);
	static float ResolveTrainingAlignedLocomotionLowerLimbDampingRatioScaleForBone(FName BoneName, float BlendAlpha);
	static float ResolveTrainingAlignedLocomotionLowerLimbExtraDampingScaleForBone(FName BoneName, float BlendAlpha);
	static float ResolveTrainingAlignedControlExtraDampingScaleForBone(FName BoneName, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedControlFamilyProfile(bool bApplyTrainingAlignedControlFamilyProfile, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedLocomotionLowerLimbResponsePolicy(bool bApplyTrainingAlignedLocomotionLowerLimbResponsePolicy, float BlendAlpha, bool bLocomotionModeActive);
	static float CalculateConstraintMinLimitedAngleDegrees(
		EAngularConstraintMotion TwistMotion,
		float TwistLimit,
		EAngularConstraintMotion Swing1Motion,
		float Swing1Limit,
		EAngularConstraintMotion Swing2Motion,
		float Swing2Limit);
	static bool AdvancePolicyControlAccumulator(
		float DeltaTimeSeconds,
		float PolicyControlIntervalSeconds,
		float& InOutAccumulatorSeconds,
		int32& OutElapsedSteps);
	static FQuat BlendPolicyTargetRotation(const FQuat& BaselineRotation, const FQuat& PolicyTargetRotation, float PolicyAlpha);
	static float CalculateControlTargetDeltaDegrees(const FQuat& PreviousRotation, const FQuat& TargetRotation);
	static float CalculateControlAuthorityAlpha(
		bool bForceZeroActions,
		bool bSimulationHandoffSettled,
		float ElapsedSinceHandoffSettledSeconds,
		float RampDurationSeconds);
	static void ApplyPresentationPerturbationStabilizationOverride(
		bool bOverrideActive,
		FPhysAnimStabilizationSettings& InOutSettings);
	static float CalculateStabilizationStressTestMultiplier(
		int32 ProfileMode,
		float ElapsedSeconds,
		float RampDurationSeconds,
		float TargetMultiplier,
		float HoldSeconds,
		float RecoveryRampSeconds);
	static void ApplyStabilizationStressTestRamp(
		float Multiplier,
		int32 SweepMode,
		FPhysAnimStabilizationSettings& InOutSettings);
	static void ResolveRuntimeInstabilityRootFrame(
		bool bPreserveGameplayShell,
		const FVector& RootLocationCm,
		const FVector& RootLinearVelocityCmPerSecond,
		const FVector& OwnerLocationCm,
		const FVector& OwnerLinearVelocityCmPerSecond,
		FVector& OutEffectiveRootLocationCm,
		FVector& OutEffectiveRootLinearVelocityCmPerSecond);
	static FString BuildBridgeStatusIndicatorText(EPhysAnimRuntimeState State, bool bBridgeOwnsPhysics);
	static FColor ResolveBridgeStatusIndicatorColor(EPhysAnimRuntimeState State, bool bBridgeOwnsPhysics);
	static bool ShouldPreserveGameplayShellDuringBridgeActive(
		bool bMovementSmokeModeEnabled,
		bool bAllowCharacterMovementInBridgeActive);
	static FVector ResolveMovementSmokeLocalIntent(float ElapsedSeconds);
	static FName ResolveMovementSmokePhaseName(float ElapsedSeconds);
	static float GetMovementSmokeDurationSeconds();
	static float GetMovementSmokeTotalDurationSeconds(int32 NumLoops);
	static bool ShouldSuspendPolicyInfluenceDuringPresentationPerturbation(bool bPresentationPerturbationOverrideActive);
	static float CalculatePolicyInfluenceAlpha(
		bool bForceZeroActions,
		bool bAllBringUpGroupsUnlocked,
		float ElapsedSinceAllBringUpGroupsUnlockedSeconds,
		float RampDurationSeconds);
	static bool IsInitialPoseSearchWaitTimedOut(double ElapsedSeconds, double TimeoutSeconds);
	static EPhysAnimRuntimeState ResolveInitialPoseSearchSuccessState(bool bForceZeroActions);
	static bool ShouldActivateBridgeFromSafeMode(EPhysAnimRuntimeState State, bool bForceZeroActions);
	static bool ShouldDeactivateBridgeToSafeMode(EPhysAnimRuntimeState State, bool bForceZeroActions);
	static bool RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState State);
	static const TCHAR* GetRuntimeStateName(EPhysAnimRuntimeState State);

private:
	void UpdateBridgeStatusIndicator(float DisplayDurationSeconds) const;
	void TransitionRuntimeState(EPhysAnimRuntimeState NewState);
	EPhysAnimRuntimeState RuntimeState = EPhysAnimRuntimeState::Uninitialized;
	double InitialPoseSearchWaitStartTimeSeconds = 0.0;
};
