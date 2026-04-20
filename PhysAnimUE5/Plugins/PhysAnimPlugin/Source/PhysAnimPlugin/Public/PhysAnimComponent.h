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
#include "PhysAnimBalanceReadyTransition.h"

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

enum class EBalanceTransitionShellAuthorityMode : uint8
{
	GameplayShellObservedOnly,
	TransitionOwnedShellLocked
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (EditCondition = "bLockCharacterMovementUntilStartupReady && bDelayMovementUnlockUntilPolicySettled", ClampMin = "0.0"))
	float PolicySettleRequiredSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalanceEntryMaxGroundDistanceCm = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1QuietRequiredSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1TimeoutDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BalanceEntryMinPolicyAlpha = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0"))
	int32 BalanceEntryMaxSimCount = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0"))
	int32 BalanceEntryMaxDistalSimCount = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1PrepareDuration = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1MaxRootLinearBaseline = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1MaxRootAngularBaseline = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1QuietRootLinearSpeed = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1QuietRootAngularSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1QuietShellOffsetDelta = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1QuietShellVelocityDelta = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1MaxEntryTargetDeltaDeg = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1LateValidateRequiredSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1LateValidateAdmissionMaxSimulatedBoneLinearSpeed = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1LateValidateAdmissionMaxSimulatedBoneAngularSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1LateValidateBodyMotionGraceDuration = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0"))
	int32 BalancePhase1MaxAutomaticRetries = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "1"))
	int32 BalancePhase1PrepareMaxBlockedTicks = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1RetryCooldownSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase1HipQuarantineDurationSeconds = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2EntryMaxRootLinearSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2EntryMaxRootAngularSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2EntryMaxRootTiltDeg = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2EntryMaxShellOffsetDelta = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2EntryMaxShellVelocityDelta = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2EntryMaxTargetDeltaDeg = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2RequiredShellHoldDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2PreRootOnShellProofRequiredSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2GuardWindowDuration = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortRootLinearSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortRootAngularSpeed = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortMaxBodyLinearSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortMaxBodyAngularSpeed = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortShellOffsetDelta = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortShellVelocityDelta = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0"))
	int32 BalancePhase2MaxAutomaticRetries = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2RetryCooldownSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase3RequiredStableHoldDuration = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase3TimeoutDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalanceBootstrapExtraDampingMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalanceActiveExtraDampingMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalanceSettleMaxRootLinearSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalanceSettleMaxRootAngularSpeed = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|Experimental")
	bool bPhase1DistalKinematicExperiment = true;

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
			FMath::IsNearlyEqual(PolicySettleRequiredSeconds, Other.PolicySettleRequiredSeconds) &&
			FMath::IsNearlyEqual(BalancePhase1QuietRequiredSeconds, Other.BalancePhase1QuietRequiredSeconds) &&
			FMath::IsNearlyEqual(BalancePhase1TimeoutDuration, Other.BalancePhase1TimeoutDuration) &&
			BalanceEntryMaxSimCount == Other.BalanceEntryMaxSimCount &&
			BalanceEntryMaxDistalSimCount == Other.BalanceEntryMaxDistalSimCount &&
			FMath::IsNearlyEqual(BalanceEntryMinPolicyAlpha, Other.BalanceEntryMinPolicyAlpha) &&
			FMath::IsNearlyEqual(BalancePhase1PrepareDuration, Other.BalancePhase1PrepareDuration) &&
			FMath::IsNearlyEqual(BalancePhase1MaxRootLinearBaseline, Other.BalancePhase1MaxRootLinearBaseline) &&
			FMath::IsNearlyEqual(BalancePhase1MaxRootAngularBaseline, Other.BalancePhase1MaxRootAngularBaseline) &&
			FMath::IsNearlyEqual(BalancePhase1QuietRootLinearSpeed, Other.BalancePhase1QuietRootLinearSpeed) &&
			FMath::IsNearlyEqual(BalancePhase1QuietRootAngularSpeed, Other.BalancePhase1QuietRootAngularSpeed) &&
			FMath::IsNearlyEqual(BalancePhase1QuietShellOffsetDelta, Other.BalancePhase1QuietShellOffsetDelta) &&
			FMath::IsNearlyEqual(BalancePhase1QuietShellVelocityDelta, Other.BalancePhase1QuietShellVelocityDelta) &&
			FMath::IsNearlyEqual(BalancePhase1MaxEntryTargetDeltaDeg, Other.BalancePhase1MaxEntryTargetDeltaDeg) &&
			FMath::IsNearlyEqual(BalancePhase1LateValidateRequiredSeconds, Other.BalancePhase1LateValidateRequiredSeconds) &&
			FMath::IsNearlyEqual(BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed, Other.BalancePhase1LateValidateMaxSimulatedBoneLinearSpeed) &&
			FMath::IsNearlyEqual(BalancePhase1LateValidateAdmissionMaxSimulatedBoneLinearSpeed, Other.BalancePhase1LateValidateAdmissionMaxSimulatedBoneLinearSpeed) &&
			FMath::IsNearlyEqual(BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed, Other.BalancePhase1LateValidateMaxSimulatedBoneAngularSpeed) &&
			FMath::IsNearlyEqual(BalancePhase1LateValidateAdmissionMaxSimulatedBoneAngularSpeed, Other.BalancePhase1LateValidateAdmissionMaxSimulatedBoneAngularSpeed) &&
			FMath::IsNearlyEqual(BalancePhase1LateValidateBodyMotionGraceDuration, Other.BalancePhase1LateValidateBodyMotionGraceDuration) &&
			BalancePhase1MaxAutomaticRetries == Other.BalancePhase1MaxAutomaticRetries &&
			BalancePhase1PrepareMaxBlockedTicks == Other.BalancePhase1PrepareMaxBlockedTicks &&
			FMath::IsNearlyEqual(BalancePhase1RetryCooldownSeconds, Other.BalancePhase1RetryCooldownSeconds) &&
			FMath::IsNearlyEqual(BalancePhase1HipQuarantineDurationSeconds, Other.BalancePhase1HipQuarantineDurationSeconds) &&
			FMath::IsNearlyEqual(BalancePhase2EntryMaxRootLinearSpeed, Other.BalancePhase2EntryMaxRootLinearSpeed) &&
			FMath::IsNearlyEqual(BalancePhase2EntryMaxRootAngularSpeed, Other.BalancePhase2EntryMaxRootAngularSpeed) &&
			FMath::IsNearlyEqual(BalancePhase2EntryMaxRootTiltDeg, Other.BalancePhase2EntryMaxRootTiltDeg) &&
			FMath::IsNearlyEqual(BalancePhase2EntryMaxShellOffsetDelta, Other.BalancePhase2EntryMaxShellOffsetDelta) &&
			FMath::IsNearlyEqual(BalancePhase2EntryMaxShellVelocityDelta, Other.BalancePhase2EntryMaxShellVelocityDelta) &&
			FMath::IsNearlyEqual(BalancePhase2EntryMaxTargetDeltaDeg, Other.BalancePhase2EntryMaxTargetDeltaDeg) &&
			FMath::IsNearlyEqual(BalancePhase2RequiredShellHoldDuration, Other.BalancePhase2RequiredShellHoldDuration) &&
			FMath::IsNearlyEqual(BalancePhase2PreRootOnShellProofRequiredSeconds, Other.BalancePhase2PreRootOnShellProofRequiredSeconds) &&
			FMath::IsNearlyEqual(BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm, Other.BalancePhase2PreRootOnShellProofMaxOffsetDeltaCm) &&
			FMath::IsNearlyEqual(BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond, Other.BalancePhase2PreRootOnShellProofMaxVelocityDeltaCmPerSecond) &&
			FMath::IsNearlyEqual(BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm, Other.BalancePhase2PreRootOnShellProofMaxOffsetGrowthCm) &&
			FMath::IsNearlyEqual(BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond, Other.BalancePhase2PreRootOnShellProofMaxVelocityGrowthCmPerSecond) &&
			FMath::IsNearlyEqual(BalancePhase2GuardWindowDuration, Other.BalancePhase2GuardWindowDuration) &&
			FMath::IsNearlyEqual(BalancePhase2AbortRootLinearSpeed, Other.BalancePhase2AbortRootLinearSpeed) &&
			FMath::IsNearlyEqual(BalancePhase2AbortRootAngularSpeed, Other.BalancePhase2AbortRootAngularSpeed) &&
			FMath::IsNearlyEqual(BalancePhase2AbortMaxBodyLinearSpeed, Other.BalancePhase2AbortMaxBodyLinearSpeed) &&
			FMath::IsNearlyEqual(BalancePhase2AbortMaxBodyAngularSpeed, Other.BalancePhase2AbortMaxBodyAngularSpeed) &&
			FMath::IsNearlyEqual(BalancePhase2AbortShellOffsetDelta, Other.BalancePhase2AbortShellOffsetDelta) &&
			FMath::IsNearlyEqual(BalancePhase2AbortShellVelocityDelta, Other.BalancePhase2AbortShellVelocityDelta) &&
			BalancePhase2MaxAutomaticRetries == Other.BalancePhase2MaxAutomaticRetries &&
			FMath::IsNearlyEqual(BalancePhase2RetryCooldownSeconds, Other.BalancePhase2RetryCooldownSeconds) &&
			FMath::IsNearlyEqual(BalancePhase3RequiredStableHoldDuration, Other.BalancePhase3RequiredStableHoldDuration) &&
			FMath::IsNearlyEqual(BalancePhase3TimeoutDuration, Other.BalancePhase3TimeoutDuration) &&
			FMath::IsNearlyEqual(BalanceBootstrapExtraDampingMultiplier, Other.BalanceBootstrapExtraDampingMultiplier) &&
			FMath::IsNearlyEqual(BalanceActiveExtraDampingMultiplier, Other.BalanceActiveExtraDampingMultiplier) &&
			FMath::IsNearlyEqual(BalanceSettleMaxRootLinearSpeed, Other.BalanceSettleMaxRootLinearSpeed) &&
			FMath::IsNearlyEqual(BalanceSettleMaxRootAngularSpeed, Other.BalanceSettleMaxRootAngularSpeed) &&
			FMath::IsNearlyEqual(BalanceEntryMaxGroundDistanceCm, Other.BalanceEntryMaxGroundDistanceCm) &&
			bPhase1DistalKinematicExperiment == Other.bPhase1DistalKinematicExperiment;
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
	BalanceEntry_Prepare,
	BalanceEntry_LateValidate,
	BalanceEntry_RootOn,
	BalanceEntry_Settle,
	BalanceActive_Recovery,
	BalanceSafeDeny
};

UENUM(BlueprintType)
enum class EPhysAnimBridgeTraceOutputMode : uint8
{
	Off = 0,
	MetadataAndEvents = 1,
	Full = 2,
};

UENUM(BlueprintType)
enum class EPhysAnimPerturbationDirection : uint8
{
	Forward,
	Backward,
	Left,
	Right
};

UENUM(BlueprintType)
enum class EPhysAnimPerturbationMagnitude : uint8
{
	Small,
	Medium,
	Large
};

USTRUCT(BlueprintType)
struct FPhysAnimBalanceScenario
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance")
	EPhysAnimPerturbationDirection Direction = EPhysAnimPerturbationDirection::Forward;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance")
	EPhysAnimPerturbationMagnitude Magnitude = EPhysAnimPerturbationMagnitude::Small;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance")
	float TriggerDelaySeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance")
	float RecoveryTimeoutSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance")
	float CooldownSeconds = 1.0f;

	bool bTriggered = false;
	bool bCompleted = false;
};

struct FPhysAnimPendingDistalOwnershipCheck
{
	bool bActive = false;
	EPhysicsMovementType IntendedOwnership = EPhysicsMovementType::Simulated;
	EPhysicsMovementType ModifierMovementType = EPhysicsMovementType::Simulated;
	bool bRawBodySimulatingAtWrite = false;
	EPhysAnimRuntimeState RuntimeState = EPhysAnimRuntimeState::Uninitialized;
	int32 TransitionPhase = 0;
	FString CallSiteReason;
};

struct FPhase1PelvisCouplingRotationForensics
{
	bool bLiveTiltProtected = false;
	bool bTiltProtectionForced = false;
	float UnconstrainedTiltDeg = 0.0f;
	float UnconstrainedAngularThresholdOverflowDeg = 0.0f;
	float UnconstrainedLeftThighAngularErrorDeg = 0.0f;
	float UnconstrainedRightThighAngularErrorDeg = 0.0f;
	float UnconstrainedSpineAngularErrorDeg = 0.0f;
	float AppliedTiltDeg = 0.0f;
	float AppliedAngularThresholdOverflowDeg = 0.0f;
	bool bTriggeredTiltSpineRescuePath = false;
	bool bTriggeredForensicSpineRescuePath = false;
	float TiltSpineRescueSpineAngularErrorDeg = 0.0f;
	float ForensicSpineRescueSpineAngularErrorDeg = 0.0f;
	FString TiltSpineRescueSource;
	FString ForensicSpineRescueSource;
	FString WinningSearchFamily;
	FString WinningSearchSource;
	TArray<FString> ExecutedSearchFamilies;
	bool bCoupledTradeControlWon = false;
	// Runtime-execution tracking: true only when the pass actually ran (config + runtime predicate both passed).
	bool bRanSpineBiasedDirectBlend = false;
	bool bRanPairBlendSeeds = false;
	bool bRanConstraintInterpolation = false;
	bool bRanWorstThighInterpolation = false;
	bool bRanFocusedDelta = false;
	bool bRanCoupledTradeControl = false;
	bool bRanPairBlendFrontierFollowThrough = false;
};

#if !UE_BUILD_SHIPPING
enum class EPhase1AutoCalibStrategyPreset : uint8
{
	CurrentDefault,
	SpineBiased,
	WorstThighBiased,
	BalancedCoupled,
	SpineThenWorstThigh,
	RescueOnly,
	CoupledTradeControlFamily,
	PairBlendFrontierFollowThrough
};

enum class EPhase1AutoCalibBudgetMode : uint8
{
	FullSearch,
	Smoke
};

enum class EPhase1AutoCalibFrontierClassification : uint8
{
	Unknown,
	TruthfulPassFound,
	StillThighBlocked,
	StillSpineBlocked,
	CoupledSpineThighFlip,
	FlatNoMaterialImprovement
};

enum class EPhase1AutoCalibRecommendedAction : uint8
{
	None,
	PromoteBestCandidate,
	AddCoupledTradeControlExpansion,
	InvestigateCandidateGeneration
};

struct FPhase1AutoCalibRequest
{
	FString OwnerFilter;
	int32 Seed = 1337;
	EPhase1AutoCalibBudgetMode BudgetMode = EPhase1AutoCalibBudgetMode::FullSearch;
	int32 MaxTrials = INDEX_NONE;
	FString OutputSubfolder;
	float ReadinessTimeoutSeconds = 30.0f;
};

struct FPhase1AutoCalibParams
{
	EPhase1AutoCalibStrategyPreset SourcePreset = EPhase1AutoCalibStrategyPreset::CurrentDefault;
	EPhase1AutoCalibStrategyPreset SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::CurrentDefault;
	float SpineInterpolationAlpha = 0.10f;
	float WorstThighInterpolationAlpha = 0.05f;
	float FocusedDeltaScale = 1.0f;
	float UprightnessWeightScale = 1.0f;
	float ClampStrengthScale = 1.0f;
	float PelvisPitchBiasDeg = 0.0f;
	float PelvisRollBiasDeg = 0.0f;
};

struct FPhase1AutoCalibScore
{
	bool bContractPassed = false;
	bool bTimedOut = false;
	bool bSafeDenied = false;
	bool bRestoreDeterministic = true;
	bool bReachedRootOn = false;
	bool bNoCouplingProofSatisfied = false;
	float WorstDirectLinkAngularErrorDeg = TNumericLimits<float>::Max();
	float MeanTargetDeltaDeg = TNumericLimits<float>::Max();
	float MaxTargetDeltaDeg = TNumericLimits<float>::Max();
	float ThighAsymmetryDeg = TNumericLimits<float>::Max();
	float PeakRootTiltDeg = TNumericLimits<float>::Max();
	float ShellOffsetDeltaCm = TNumericLimits<float>::Max();
	float ShellVelocityDeltaCmPerSecond = TNumericLimits<float>::Max();
	float PeakRootLinearSpeedCmPerSecond = TNumericLimits<float>::Max();
	float PeakRootAngularSpeedDegPerSecond = TNumericLimits<float>::Max();
	float StableSortScalar = TNumericLimits<float>::Max();
};

struct FPhase1AutoCalibTrialResult
{
	int32 TrialId = INDEX_NONE;
	int32 RepetitionIndex = 0;
	int32 PresetRank = INDEX_NONE;
	int32 PresetNearPassRank = INDEX_NONE;
	FString StageName;
	FPhase1AutoCalibParams Params;
	FPhase1AutoCalibScore Score;
	FString TerminalClass;
	FString TruthfulBlocker;
	float TrialTimeoutBudgetSeconds = 0.0f;
	float TimeToRootOnSeconds = -1.0f;
	float TimeToNoCouplingProofSeconds = -1.0f;
	bool bTimedOutBeforeRootOn = false;
	bool bTimedOutBeforeNoCouplingProof = false;
	FString WinningSearchFamily;
	FString WinningSearchSource;
	TArray<FString> ExecutedSearchFamilies;
	bool bCoupledTradeControlWon = false;
	bool bReproducible = false;
};

struct FPhase1AutoCalibBlockerCount
{
	FString TruthfulBlocker;
	int32 Count = 0;
};

struct FPhase1AutoCalibPresetSummary
{
	EPhase1AutoCalibStrategyPreset Preset = EPhase1AutoCalibStrategyPreset::CurrentDefault;
	int32 TrialCount = 0;
	int32 ContractPassedCount = 0;
	bool bHasReproducibleTruthfulPass = false;
	FPhase1AutoCalibTrialResult BestCandidate;
	FPhase1AutoCalibTrialResult BestNearPass;
	bool bHasBestCandidate = false;
	bool bHasBestNearPass = false;
	FString DominantTruthfulBlocker;
	float WorstDirectLinkImprovementVsCurrentDefaultDeg = 0.0f;
	float ThighAsymmetryImprovementVsCurrentDefaultDeg = 0.0f;
	bool bImprovesWorstDirectLinkVsCurrentDefault = false;
	bool bImprovesThighAsymmetryVsCurrentDefault = false;
	TArray<FPhase1AutoCalibBlockerCount> BlockerCounts;
};

struct FPhase1AutoCalibReport
{
	FString OutputDirectory;
	FString SummaryPath;
	FString TrialsCsvPath;
	FString ParetoJsonPath;
	TArray<FPhase1AutoCalibTrialResult> Trials;
	TArray<FPhase1AutoCalibTrialResult> ParetoFrontier;
	TArray<FPhase1AutoCalibPresetSummary> PresetSummaries;
	TArray<FPhase1AutoCalibBlockerCount> OverallBlockerCounts;
	FPhase1AutoCalibTrialResult BestCandidate;
	FPhase1AutoCalibTrialResult BestNearPass;
	FPhase1AutoCalibTrialResult FurthestProgressedFailure;
	bool bHasBestCandidate = false;
	bool bHasBestNearPass = false;
	bool bHasFurthestProgressedFailure = false;
	bool bHasReproducibleTruthfulPass = false;
	EPhase1AutoCalibFrontierClassification FrontierClassification = EPhase1AutoCalibFrontierClassification::Unknown;
	EPhase1AutoCalibRecommendedAction RecommendedAction = EPhase1AutoCalibRecommendedAction::None;
	FString RecommendedExpansionName;
	FString DominantTruthfulBlocker;
	bool bAnyTimedOutBeforeRootOn = false;
	bool bAnyTimedOutBeforeNoCouplingProof = false;
};

struct FPhase1AutoCalibLiveMetrics
{
	EPhysAnimRuntimeState RuntimeState = EPhysAnimRuntimeState::Uninitialized;
	EBalanceReadyTransitionPhase TransitionPhase = EBalanceReadyTransitionPhase::BRT_Inactive;
	float RootLinearSpeedCmPerSecond = 0.0f;
	float RootAngularSpeedDegPerSecond = 0.0f;
	float RootTiltDeg = 0.0f;
	float ShellOffsetDeltaCm = 0.0f;
	float ShellVelocityDeltaCmPerSecond = 0.0f;
	float MaxTargetDeltaDeg = 0.0f;
	float MeanTargetDeltaDeg = 0.0f;
};

struct FPhase1AutoCalibDeterminismFingerprint
{
	EPhysAnimRuntimeState RuntimeState = EPhysAnimRuntimeState::Uninitialized;
	EBalanceReadyTransitionPhase TransitionPhase = EBalanceReadyTransitionPhase::BRT_Inactive;
	FTransform OwnerTransform = FTransform::Identity;
	FTransform MeshTransform = FTransform::Identity;
	FTransform RootBodyTransform = FTransform::Identity;
	FVector RootLinearVelocity = FVector::ZeroVector;
	FVector RootAngularVelocity = FVector::ZeroVector;
	float PelvisThighLAngularErrorDeg = 0.0f;
	float PelvisThighRAngularErrorDeg = 0.0f;
	float PelvisSpine01AngularErrorDeg = 0.0f;
	float ShellOffsetDeltaCm = 0.0f;
	float ShellVelocityDeltaCmPerSecond = 0.0f;
	float MaxTargetDeltaDeg = 0.0f;
	float MeanTargetDeltaDeg = 0.0f;
	int32 PendingResetCount = 0;
};

struct FPhase1AutoCalibBodyState
{
	FName BoneName = NAME_None;
	FTransform WorldTransform = FTransform::Identity;
	FVector LinearVelocity = FVector::ZeroVector;
	FVector AngularVelocityRad = FVector::ZeroVector;
	bool bSimulating = false;
	bool bSleeping = false;
};

struct FPhase1AutoCalibBodyModifierState
{
	FName ModifierName = NAME_None;
	EPhysicsMovementType MovementType = EPhysicsMovementType::Static;
	float PhysicsBlendWeight = 0.0f;
	ECollisionEnabled::Type CollisionType = ECollisionEnabled::NoCollision;
	bool bUpdateKinematicFromSimulation = false;
};

struct FPhase1AutoCalibBaselineSnapshot
{
	FTransform OwnerActorTransform = FTransform::Identity;
	FVector CharacterVelocity = FVector::ZeroVector;
	FTransform MeshWorldTransform = FTransform::Identity;
	TArray<FPhase1AutoCalibBodyState> Bodies;
	TArray<FPhase1AutoCalibBodyModifierState> BodyModifiers;
	TMap<FName, FQuat> PreviousControlTargetRotations;
	TMap<FName, FQuat> PolicyBlendStartControlTargetRotations;
	TArray<float> ConditionedActionBuffer;
	TArray<float> PreviousConditionedActionBuffer;
	TArray<float> SelfObservationBuffer;
	TArray<float> MimicTargetPosesBuffer;
	TArray<float> TerrainBuffer;
	TArray<float> ActionOutputBuffer;
	TArray<float> PreviousActionOutputBuffer;
	FPoseSearchBlueprintResult LastValidPoseSearchResult;
	int32 ConsecutiveInvalidPoseSearchFrames = 0;
	FBridgeIntentState BridgeIntentState;
	FBridgeTrajectoryState BridgeTrajectoryState;
	FBridgeShellState BridgeShellState;
	FPhysAnimRuntimeInstabilityState RuntimeInstabilityState;
	FPhysAnimRuntimeInstabilityDiagnostics LastRuntimeInstabilityDiagnostics;
	FPhysAnimActionDiagnostics LastActionDiagnostics;
	FPhysAnimControlTargetDiagnostics LastControlTargetDiagnostics;
	FPhysAnimStabilizationSettings LastAppliedStabilizationSettings;
	FPhysAnimBalanceReadyTransitionSnapshot BalanceTransitionSnapshot;
	FPhase1AcceptedConvergenceSnapshot SafePhase1ConvergenceSnapshot;
	FPhase1PelvisCouplingRotationForensics LastPhase1PelvisCouplingRotationForensics;
	TArray<FName> PendingBodyModifierCachedResetNames;
	TArray<double> BringUpGroupActivationTimeSeconds;
	TArray<double> BringUpGroupControlRampStartTimeSeconds;
	TArray<uint8> BringUpGroupAlphaActiveLogged;
	TMap<FName, EPhysicsMovementType> PreviousDistalBoneIntendedOwnership;
	TMap<FName, EPhysicsMovementType> PreviousDistalBoneModifierOwnership;
	TMap<FName, FString> LastDistalClassification;
	TMap<FName, FPhysAnimPendingDistalOwnershipCheck> PendingDistalOwnershipChecks;
	EPhysAnimRuntimeState RuntimeState = EPhysAnimRuntimeState::Uninitialized;
	EBridgeLocomotionAuthorityState BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	EBalanceTransitionShellAuthorityMode BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly;
	float SimulationHandoffAlpha = 0.0f;
	bool bLastAppliedSimulationHandoffSettled = false;
	float LastAppliedControlAuthorityAlpha = -1.0f;
	double BridgeStartTimeSeconds = 0.0;
	double SimulationHandoffCompletedTimeSeconds = -1.0;
	double PolicyInfluenceRampStartTimeSeconds = -1.0;
	int32 HighestUnlockedBringUpGroupIndex = INDEX_NONE;
	float BringUpGroupStableAccumulatedSeconds = 0.0f;
	double LastRuntimeDiagnosticsLogTimeSeconds = -1.0;
	float PolicyUpdateAccumulatorSeconds = -1.0f;
	int32 LastPolicyElapsedSteps = 0;
	int32 PolicyControlTicksExecuted = 0;
	int32 PolicyControlTicksSkipped = 0;
	double LastPolicyControlUpdateTimeSeconds = -1.0;
	FVector ShellCouplingReferenceRootLocalOffsetCm = FVector::ZeroVector;
	bool bHasShellCouplingReferenceRootLocalOffset = false;
	bool bTransitionOwnedShellReferenceReanchored = false;
	bool bTransitionOwnedShellReferenceReseededAfterLock = false;
	bool bPolicyTargetsAppliedLastFrame = false;
	bool bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = false;
	bool bStartupBringUpFrozenByBalanceEntry = false;
	bool bPendingBalanceModeStartRequest = false;
	bool bPendingBalanceModeStartAttemptIssued = false;
	FString PendingBalanceModeStartReason;
	double PendingBalanceModeRequestTimeSeconds = -1.0;
	bool bPhase1TiltDiagnosticEmitted = false;
	bool bPhase1PelvisCouplingSkipLogged = false;
	bool bPelvisResetAppliedThisTick = false;
	int32 HipQuarantineTicksRemaining = 0;
	uint32 BalanceEntryRootOnFrameCount = 0;
	uint32 BalanceEntrySettleFrameCount = 0;
	bool bLastPelvisRawSim = false;
	int32 LastTotalSimCount = -1;
	bool bPhase2Tick4AuditArmed = false;
	float LastHipQuarantineLeftPreDeltaDegrees = 0.0f;
	float LastHipQuarantineRightPreDeltaDegrees = 0.0f;
};
#else
enum class EPhase1AutoCalibStrategyPreset : uint8 { CurrentDefault };
enum class EPhase1AutoCalibBudgetMode : uint8 { FullSearch };
enum class EPhase1AutoCalibFrontierClassification : uint8 { Unknown };
enum class EPhase1AutoCalibRecommendedAction : uint8 { None };
struct FPhase1AutoCalibRequest { float ReadinessTimeoutSeconds = 0.0f; FString OwnerFilter; int32 Seed = 0; EPhase1AutoCalibBudgetMode BudgetMode = EPhase1AutoCalibBudgetMode::FullSearch; int32 MaxTrials = 0; FString OutputSubfolder; };
struct FPhase1AutoCalibParams {};
struct FPhase1AutoCalibScore {};
struct FPhase1AutoCalibTrialResult { int32 PresetRank = INDEX_NONE; int32 PresetNearPassRank = INDEX_NONE; FPhase1AutoCalibParams Params; FPhase1AutoCalibScore Score; FString TerminalClass; FString TruthfulBlocker; float TrialTimeoutBudgetSeconds = 0.0f; float TimeToRootOnSeconds = -1.0f; float TimeToNoCouplingProofSeconds = -1.0f; bool bTimedOutBeforeRootOn = false; bool bTimedOutBeforeNoCouplingProof = false; };
struct FPhase1AutoCalibBlockerCount { FString TruthfulBlocker; int32 Count = 0; };
struct FPhase1AutoCalibPresetSummary { bool bHasReproducibleTruthfulPass = false; FString DominantTruthfulBlocker; TArray<FPhase1AutoCalibBlockerCount> BlockerCounts; };
struct FPhase1AutoCalibReport { TArray<FPhase1AutoCalibTrialResult> Trials; TArray<FPhase1AutoCalibTrialResult> ParetoFrontier; TArray<FPhase1AutoCalibPresetSummary> PresetSummaries; TArray<FPhase1AutoCalibBlockerCount> OverallBlockerCounts; FPhase1AutoCalibTrialResult BestCandidate; FPhase1AutoCalibTrialResult BestNearPass; bool bHasBestCandidate; bool bHasBestNearPass; bool bHasReproducibleTruthfulPass; EPhase1AutoCalibFrontierClassification FrontierClassification = EPhase1AutoCalibFrontierClassification::Unknown; EPhase1AutoCalibRecommendedAction RecommendedAction = EPhase1AutoCalibRecommendedAction::None; FString RecommendedExpansionName; FString DominantTruthfulBlocker; bool bAnyTimedOutBeforeRootOn = false; bool bAnyTimedOutBeforeNoCouplingProof = false; };
struct FPhase1AutoCalibLiveMetrics { EPhysAnimRuntimeState RuntimeState; EBalanceReadyTransitionPhase TransitionPhase; float RootLinearSpeedCmPerSecond; float RootAngularSpeedDegPerSecond; float RootTiltDeg; float ShellOffsetDeltaCm; float ShellVelocityDeltaCmPerSecond; float MaxTargetDeltaDeg; float MeanTargetDeltaDeg; };
struct FPhase1AutoCalibDeterminismFingerprint {};
struct FPhase1AutoCalibBodyState {};
struct FPhase1AutoCalibBodyModifierState {};
struct FPhase1AutoCalibBaselineSnapshot { TArray<FPhase1AutoCalibBodyState> Bodies; };
#endif

UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent))
class PHYSANIMPLUGIN_API UPhysAnimComponent : public UActorComponent, public IPoseSearchTrajectoryPredictorInterface
{
	GENERATED_BODY()
	friend class FPhysAnimBalanceReadyTransition;
	friend class UPhysAnimPhase1AutoCalibSubsystem;

public:
	UPhysAnimComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void Predict(FTransformTrajectory& InOutTrajectory, int32 NumPredictionSamples, float SecondsPerPredictionSample, int32 NumHistorySamples) override;
	virtual void GetGravity(FVector& OutGravityAccel) override;
	virtual void GetCurrentState(FVector& OutPosition, FQuat& OutFacing, FVector& OutVelocity) override;
	virtual void GetVelocity(FVector& OutVelocity) override;

	bool IsIdlePoseActive() const;
	EBridgeLocomotionAuthorityState GetLocomotionAuthorityState() const { return BridgeLocomotionAuthorityState; }
	FVector GetAcceptedShellPlanarVelocity() const { return BridgeShellState.AcceptedPlanarVelocityCmPerSecond; }
	float GetCurrentShellPlanarOffsetDeltaCm() const;
	float GetCurrentShellPlanarVelocityDeltaCmPerSecond() const;
	void ReanchorShellCouplingReferenceToCurrentRoot(const TCHAR* Source = TEXT("unknown"));
	void ActivateTransitionOwnedShellLock();
	void ReleaseTransitionOwnedShellLock();
	EBalanceTransitionShellAuthorityMode GetBalanceTransitionShellAuthorityMode() const { return BalanceTransitionShellAuthorityMode; }
	bool IsTransitionOwnedShellLocked() const;
	bool IsStartupMovementLockActive() const { return bStartupMovementLockActive; }
	bool WasTransitionShellReferenceReanchored() const { return bTransitionOwnedShellReferenceReanchored; }
	bool WasTransitionShellReferenceReseededAfterLock() const { return bTransitionOwnedShellReferenceReseededAfterLock; }
	const TArray<FName>& GetPendingBodyModifierCachedResetNames() const { return PendingBodyModifierCachedResetNames; }
	void ConsumeUpperBodyPendingResets();
	USkeletalMeshComponent* GetMeshComponent() const { return MeshComponent.Get(); }
	const FPhysAnimControlTargetDiagnostics& GetLastControlTargetDiagnostics() const { return LastControlTargetDiagnostics; }
	bool WasPelvisResetAppliedThisTick() const { return bPelvisResetAppliedThisTick; }
	bool WasPolicyTargetAppliedLastFrame() const { return bPolicyTargetsAppliedLastFrame; }
	bool WasPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame() const { return bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame; }
	bool IsStartupBringUpFrozenByBalanceEntry() const { return bStartupBringUpFrozenByBalanceEntry; }
	void SetStartupBringUpFrozenByBalanceEntry(bool bFrozen, const FString& InReason = TEXT("unknown"));
	int32 GetHighestUnlockedBringUpGroupIndex() const { return HighestUnlockedBringUpGroupIndex; }
	bool WasPelvisSimulatingLastFrame() const { return bLastAppliedPresentationRootSimulationEnabled; }
	bool IsPelvisSimulatingNow() const;
	bool TryGetPublicBalanceEntryRuntimeState(EPhysAnimRuntimeState& OutState) const;
	bool HasBalanceReadyTransitionFailed() const { return BalanceReadyTransition.HasFailed() || !LastPublishedBalanceTransitionFailureReason.IsEmpty(); }
	bool HasSafePhase2Denial() const { return BalanceReadyTransition.HasSafePhase2Denial(); }
	const FString& GetBalanceReadyTransitionFailureReason() const
	{
		const FString& LiveFailureReason = BalanceReadyTransition.GetFailureReason();
		return LiveFailureReason.IsEmpty() ? LastPublishedBalanceTransitionFailureReason : LiveFailureReason;
	}
	const FString& GetSafePhase2DenialReason() const { return BalanceReadyTransition.GetSafePhase2DenialReason(); }

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	bool StartBridge();

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void StopBridge();

	UFUNCTION(BlueprintPure, Category = "PhysAnim")
	EPhysAnimRuntimeState GetRuntimeState() const { return RuntimeState; }

	static EPhysAnimRuntimeState MapBalanceTransitionPhaseToRuntimeState(EBalanceReadyTransitionPhase TransitionPhase);

	UFUNCTION(BlueprintPure, Category = "PhysAnim")
	bool IsReadyForScriptedPresentation() const;

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void SetPresentationPerturbationOverrideSeconds(float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void ClearPresentationPerturbationOverride();

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void StartBalancePerturbationMode();

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void StopBalancePerturbationMode();

#if !UE_BUILD_SHIPPING
	const FPhysAnimStabilizationSettings& GetConfiguredStabilizationSettings() const { return StabilizationSettings; }
	EBalanceReadyTransitionPhase GetBalanceReadyTransitionPhase() const { return BalanceReadyTransition.GetPhase(); }
	FPhysAnimBalanceReadyTransitionSnapshot ExportBalanceReadyTransitionSnapshot() const { return BalanceReadyTransition.ExportSnapshot(); }
	bool CapturePhase1AutoCalibBaseline(FPhase1AutoCalibBaselineSnapshot& OutSnapshot, FString& OutError) const;
	bool RestorePhase1AutoCalibBaseline(const FPhase1AutoCalibBaselineSnapshot& Snapshot, FString& OutError);
	void ApplyPhase1AutoCalibParams(const FPhase1AutoCalibParams& Params);
	void ClearPhase1AutoCalibParams();
	bool CapturePhase1AutoCalibLiveMetrics(FPhase1AutoCalibLiveMetrics& OutMetrics, FString& OutError) const;
	bool CapturePhase1AutoCalibDeterminismFingerprint(FPhase1AutoCalibDeterminismFingerprint& OutFingerprint, FString& OutError) const;
	bool StartPhase1AutoCalibTrial(FString& OutError);
	void SetPhase1AutoCalibOwnsStartRequests(bool bOwned);
	static bool IsBetterPhase1AutoCalibScore(const FPhase1AutoCalibScore& Candidate, const FPhase1AutoCalibScore& CurrentBest);
	static void FinalizePhase1AutoCalibScore(FPhase1AutoCalibScore& InOutScore);
	static void StorePhase1AutoCalibActionHistory(
		FPhase1AutoCalibBaselineSnapshot& Snapshot,
		const TArray<float>& ConditionedActions,
		const TArray<float>& PreviousConditionedActions,
		const TArray<float>& ActionOutputs,
		const TArray<float>& PreviousActionOutputs);
	static void RestorePhase1AutoCalibActionHistory(
		const FPhase1AutoCalibBaselineSnapshot& Snapshot,
		TArray<float>& ConditionedActions,
		TArray<float>& PreviousConditionedActions,
		TArray<float>& ActionOutputs,
		TArray<float>& PreviousActionOutputs);
#endif

#if WITH_DEV_AUTOMATION_TESTS
	static bool TestOnlyIsBalanceEntryState(EPhysAnimRuntimeState State) { return IsBalanceEntryState(State); }
	static bool TestOnlyIsBalanceActiveState(EPhysAnimRuntimeState State) { return IsBalanceActiveState(State); }
	static bool TestOnlyShouldUseAuthoritativePerBoneBodyModifierSync(
		EPhysAnimRuntimeState RuntimeState,
		bool bDistalKinematicAccepted)
	{
		return ShouldUseAuthoritativePerBoneBodyModifierSync(RuntimeState, bDistalKinematicAccepted);
	}
	static bool TestOnlyShouldUpdateBodyOnPerBoneBodyModifierSync(EPhysAnimRuntimeState RuntimeState)
	{
		return ShouldUpdateBodyOnPerBoneBodyModifierSync(RuntimeState);
	}
	static bool TestOnlyShouldAttemptAutoTriggeredBalanceStart(
		EPhysAnimRuntimeState RuntimeState,
		bool bPendingBalanceModeStartRequest,
		bool bTransitionStarted,
		bool bPhase1AutoCalibOwnsStartRequests,
		bool bPhase1AutoCalibSubsystemActive = false)
	{
		return ShouldAttemptAutoTriggeredBalanceStart(
			RuntimeState,
			bPendingBalanceModeStartRequest,
			bTransitionStarted,
			bPhase1AutoCalibOwnsStartRequests,
			bPhase1AutoCalibSubsystemActive);
	}
	static bool TestOnlyShouldTreatInstabilityPrecursorAsTransitionBlocker(
		EPhysAnimRuntimeState RuntimeState,
		float UnstableAccumulatedSeconds)
	{
		return UnstableAccumulatedSeconds > 0.0f &&
			RuntimeState != EPhysAnimRuntimeState::BalanceEntry_RootOn &&
			RuntimeState != EPhysAnimRuntimeState::BalanceEntry_Settle;
	}
	static bool TestOnlyShouldRebaselineBridgeStateAfterTransitionFailure(const FString& FailureReason)
	{
		return ShouldRebaselineBridgeStateAfterTransitionFailure(FailureReason);
	}
	static void TestOnlyStorePhase1AutoCalibActionHistory(
		FPhase1AutoCalibBaselineSnapshot& Snapshot,
		const TArray<float>& ConditionedActions,
		const TArray<float>& PreviousConditionedActions,
		const TArray<float>& ActionOutputs,
		const TArray<float>& PreviousActionOutputs)
	{
		StorePhase1AutoCalibActionHistory(
			Snapshot,
			ConditionedActions,
			PreviousConditionedActions,
			ActionOutputs,
			PreviousActionOutputs);
	}
	static void TestOnlyRestorePhase1AutoCalibActionHistory(
		const FPhase1AutoCalibBaselineSnapshot& Snapshot,
		TArray<float>& ConditionedActions,
		TArray<float>& PreviousConditionedActions,
		TArray<float>& ActionOutputs,
		TArray<float>& PreviousActionOutputs)
	{
		RestorePhase1AutoCalibActionHistory(
			Snapshot,
			ConditionedActions,
			PreviousConditionedActions,
			ActionOutputs,
			PreviousActionOutputs);
	}
	static bool TestOnlyShouldRunRootOnReadinessUltraFineMarginSweep(float RootOnReadinessTotalDeficitDeg)
	{
		return ShouldRunRootOnReadinessUltraFineMarginSweep(RootOnReadinessTotalDeficitDeg);
	}
	static bool TestOnlyShouldAcceptStepLimitedPhase1PelvisRotation(
		bool bBestTiltAdmissible,
		bool bBestRootOnAngularReady,
		bool bBestRootOnReadinessMarginSatisfied,
		bool bStepTiltAdmissible,
		bool bStepRootOnAngularReady,
		bool bStepRootOnReadinessMarginSatisfied)
	{
		return ShouldAcceptStepLimitedPhase1PelvisRotation(
			bBestTiltAdmissible,
			bBestRootOnAngularReady,
			bBestRootOnReadinessMarginSatisfied,
			bStepTiltAdmissible,
			bStepRootOnAngularReady,
			bStepRootOnReadinessMarginSatisfied);
	}
	static bool TestOnlyShouldRunSpineOnlyRootOnReadinessRescueSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg)
	{
		return ShouldRunSpineOnlyRootOnReadinessRescueSweep(
			LeftThighAngularErrorDeg,
			RightThighAngularErrorDeg,
			SpineAngularErrorDeg);
	}
	static bool TestOnlyShouldRunSpineBiasedDirectConstraintBlendSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg)
	{
		return ShouldRunSpineBiasedDirectConstraintBlendSweep(
			LeftThighAngularErrorDeg,
			RightThighAngularErrorDeg,
			SpineAngularErrorDeg);
	}
	static bool TestOnlyShouldRunSpineFocusedPairBlendSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg)
	{
		return ShouldRunSpineFocusedPairBlendSweep(
			LeftThighAngularErrorDeg,
			RightThighAngularErrorDeg,
			SpineAngularErrorDeg);
	}
	static bool TestOnlyShouldRunAlternateReferenceDirectConstraintBlendSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg)
	{
		return ShouldRunAlternateReferenceDirectConstraintBlendSweep(
			LeftThighAngularErrorDeg,
			RightThighAngularErrorDeg,
			SpineAngularErrorDeg);
	}
	static bool TestOnlyShouldRunSpineConstraintInterpolationSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg)
	{
		return ShouldRunSpineConstraintInterpolationSweep(
			LeftThighAngularErrorDeg,
			RightThighAngularErrorDeg,
			SpineAngularErrorDeg);
	}
	static bool TestOnlyShouldRunWorstThighConstraintInterpolationSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg)
	{
		return ShouldRunWorstThighConstraintInterpolationSweep(
			LeftThighAngularErrorDeg,
			RightThighAngularErrorDeg,
			SpineAngularErrorDeg);
	}
	static bool TestOnlyShouldRunSpineSafeWorstThighFocusedDelta(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg)
	{
		return ShouldRunSpineSafeWorstThighFocusedDelta(
			LeftThighAngularErrorDeg,
			RightThighAngularErrorDeg,
			SpineAngularErrorDeg);
	}
	static bool TestOnlyIsConstraintSampleRelevantToFocusedBone(
		FName SampleChildBoneName,
		const FString& SampleSource,
		FName FocusChildBone)
	{
		return IsConstraintSampleRelevantToFocusedBone(
			SampleChildBoneName,
			SampleSource,
			FocusChildBone);
	}
	static bool TestOnlyShouldAcceptWorstThighConstraintInterpolationCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg)
	{
		return ShouldAcceptWorstThighConstraintInterpolationCandidate(
			CurrentLeftThighAngularErrorDeg,
			CurrentRightThighAngularErrorDeg,
			CurrentSpineAngularErrorDeg,
			CandidateLeftThighAngularErrorDeg,
			CandidateRightThighAngularErrorDeg,
			CandidateSpineAngularErrorDeg);
	}
	static bool TestOnlyShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg)
	{
		return ShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
			CurrentLeftThighAngularErrorDeg,
			CurrentRightThighAngularErrorDeg,
			CurrentSpineAngularErrorDeg,
			CandidateLeftThighAngularErrorDeg,
			CandidateRightThighAngularErrorDeg,
			CandidateSpineAngularErrorDeg);
	}
	static bool TestOnlyShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg)
	{
		return ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
			CurrentLeftThighAngularErrorDeg,
			CurrentRightThighAngularErrorDeg,
			CurrentSpineAngularErrorDeg,
			CandidateLeftThighAngularErrorDeg,
			CandidateRightThighAngularErrorDeg,
			CandidateSpineAngularErrorDeg);
	}
	static bool TestOnlyShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg)
	{
		return ShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
			CurrentLeftThighAngularErrorDeg,
			CurrentRightThighAngularErrorDeg,
			CurrentSpineAngularErrorDeg,
			CandidateLeftThighAngularErrorDeg,
			CandidateRightThighAngularErrorDeg,
			CandidateSpineAngularErrorDeg);
	}
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceQuietLinearSpeedThresholdCmPerSec = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceQuietTiltThresholdDeg = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceQuietWindowRequiredSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BalanceReadyPolicyInfluenceThreshold = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceBridgeActivePreEntrySettleSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceResponseVelocityThresholdCmPerSec = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceRecoveryVelocityThresholdCmPerSec = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceRecoveryTiltThresholdDeg = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceRecoveryHeightToleranceCm = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceRecoveryStableHoldSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceRecoveryTimeoutSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceShellContaminationDisplacementCm = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceFallHeightThresholdCm = 40.0f;

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
	void ApplyTrainingAlignedSpineLimitPolicy(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResetTrainingAlignedSpineLimitPolicy();
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
	void GetSimulatingBodies(TArray<FName>& OutBones) const;
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
	void ApplyTransitionOwnedShellLock();
	void CommitTransitionOwnedShellDrop();
	void MaintainTransitionOwnedShellLock();
	void ReleaseTransitionOwnedShellLockInternal(bool bRestoreCharacterMovement);
	void ResetStartupQuietWindowState();
	bool UpdateStartupQuietWindow(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, float& OutLinearSpeedCmPerSecond, float& OutAngularSpeedDegPerSecond);
	void ResetPolicySettleWindowState();
	bool UpdatePolicySettleWindow(const FPhysAnimStabilizationSettings& EffectiveSettings, float& OutShellOffsetCm, float& OutRootLinearSpeedCmPerSecond, float& OutRootAngularSpeedDegPerSecond);
	bool ShouldUseBridgeOwnedMovementDrive(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	void CaptureBridgeIntent(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ApplyBridgeOwnedMovementDrive(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResetBridgeLocomotionAuthorityState();
	void RecoverBridgeActiveStateAfterBalanceTransitionFailure(const FString& FailureReason);
	void PublishBalanceTransitionFailureReason(const FString& FailureReason);
	void ClearPublishedBalanceTransitionFailureReason();
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
	void UpdateBalancePerturbation(float DeltaTime);
	void ApplyPelvisImpulse(EPhysAnimPerturbationDirection Direction, EPhysAnimPerturbationMagnitude Magnitude);
	void FinalizeBalanceScenario(bool bSuccess, const FString& Reason);
	bool ShouldAllowBalanceSimulation(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	static bool ShouldRebaselineBridgeStateAfterTransitionFailure(const FString& FailureReason);
	bool IsBalancePerturbationRuntimeReady(
		const FPhysAnimStabilizationSettings& EffectiveSettings,
		float* OutPolicyInfluenceAlpha = nullptr,
		FString* OutFailureReason = nullptr) const;
	bool IsBalanceScenarioQuietEnough(
		const FVector& PelvisLinearVelocity,
		const FVector& PelvisAngularVelocityDegPerSec,
		float TiltDeg,
		bool bIdlePoseActive,
		bool bNoLocomotionStateActive) const;

	static bool IsBalanceEntryState(EPhysAnimRuntimeState State);
	EPhysAnimRuntimeState GetPublicBalanceEntryRuntimeState() const;
	static bool IsBalanceActiveState(EPhysAnimRuntimeState State);
	void ResetBalanceScenarioQuietGate(const FString& Reason);
	void CompleteBalanceModeEntry();
	bool EvaluateBalanceModeQueueGates(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutReason) const;
	bool EvaluateBalanceBridgeActivePreEntryPrerequisites(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutReason) const;
	void QueueBalanceModeStartRequest(const FString& Reason);
	void TryStartPendingBalanceModeRequest(const FPhysAnimStabilizationSettings& EffectiveSettings);
	bool IsInstabilityPrecursorActive() const;

	void CacheRestPoses(UAnimSequence* TPoseAnim);
	bool BeginStartupTPoseCapture(FString& OutError);
	bool FinalizeStartupTPoseCaptureAndStartBridge(FString& OutError);
	void SaveStartupAnimationState(USkeletalMeshComponent* SkeletalMesh);
	void RestoreStartupAnimationState(USkeletalMeshComponent* SkeletalMesh);
	void LogTPoseIdentityCheck() const;

	void ReconcilePhase1DistalModifierRecords(const FPhysAnimStabilizationSettings& EffectiveSettings);

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
	TArray<float> PreviousActionOutputBuffer;
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
	FString LastPublishedBalanceTransitionFailureReason;
	TSharedPtr<class FPhysAnimBridgeTraceWriter> BridgeTraceWriter;
	FString CurrentBridgeTraceSessionId;
	int64 BridgeTraceTickCounter = 0;
	double BridgeTraceLastFlushTimeSeconds = -1.0;
	TMap<FName, FQuat> PreviousControlTargetRotations;
	TMap<FName, FQuat> PolicyBlendStartControlTargetRotations;
	bool bPolicyTargetsAppliedLastFrame = false;
	bool bPolicyInfluenceRampReanchoredOnFirstPolicyEnabledFrame = false;
	bool bStartupBringUpFrozenByBalanceEntry = false;
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
	mutable TArray<uint8> BringUpGroupAlphaActiveLogged;
	mutable bool bShellCorrectionStateLogged = false;
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
	EBalanceTransitionShellAuthorityMode BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly;
	bool bTransitionOwnedShellReferenceReanchored = false;
	bool bTransitionOwnedShellReferenceReseededAfterLock = false;
	FName LastMovementSmokePhaseName = NAME_None;
	bool bMovementSmokeScriptStarted = false;
	bool bMovementSmokeCompletionLogged = false;
	bool bHasShellCouplingReferenceRootLocalOffset = false;
	double PresentationPerturbationOverrideEndTimeSeconds = -1.0;
	bool bLastAppliedPresentationRootSimulationEnabled = false;
	int32 HipQuarantineTicksRemaining = 0;
	uint32 BalanceEntryRootOnFrameCount = 0;
	uint32 BalanceEntrySettleFrameCount = 0;
	bool bLastPelvisRawSim = false;
	int32 LastTotalSimCount = -1;
	bool bPhase2Tick4AuditArmed = false;
	float LastHipQuarantineLeftPreDeltaDegrees = 0.0f;
	float LastHipQuarantineRightPreDeltaDegrees = 0.0f;
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
	TMap<FName, uint8> OriginalSpineTwistMotions;
	TMap<FName, uint8> OriginalSpineSwing1Motions;
	TMap<FName, uint8> OriginalSpineSwing2Motions;
	TMap<FName, float> OriginalSpineTwistLimits;
	TMap<FName, float> OriginalSpineSwing1Limits;
	TMap<FName, float> OriginalSpineSwing2Limits;
	bool bHasSavedSpineConstraintLimits = false;
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
	
	TArray<FPhysAnimBalanceScenario> BalanceScenarios;
	int32 ActiveBalanceScenarioIndex = INDEX_NONE;
	double BalanceScenarioStartTimeSeconds = -1.0;
	double LastBalanceScenarioImpactTimeSeconds = -1.0;
	FVector BalanceScenarioImpactPelvisLinearVelPre = FVector::ZeroVector;
	FVector BalanceScenarioImpactPelvisLinearVelPost = FVector::ZeroVector;
	float BalanceScenarioPeakPelvisVel = 0.0f;
	float BalanceScenarioPeakPelvisTilt = 0.0f;
	FVector BalanceScenarioStartActorLocation = FVector::ZeroVector;
	FVector BalanceScenarioStartPelvisLocation = FVector::ZeroVector;
	FQuat BalanceScenarioStartPelvisRotation = FQuat::Identity;
	bool bBalanceScenarioAwaitingStableWindow = false;
	double BalanceScenarioStableWindowStartTimeSeconds = -1.0;
	double BalanceScenarioQuietWindowAccumulatedSeconds = 0.0;
	double BalanceScenarioRecoveryStableAccumulatedSeconds = 0.0;
	double LastBalanceStabilizationLogTimeSeconds = -1.0;
	FVector BalanceScenarioImpactPelvisAngularVelPre = FVector::ZeroVector;
	FVector BalanceScenarioImpactPelvisAngularVelPost = FVector::ZeroVector;

	FPhysAnimBalanceReadyTransition BalanceReadyTransition;
	FPhase1AcceptedConvergenceSnapshot SafePhase1ConvergenceSnapshot;
	bool bPendingBalanceModeStartRequest = false;
	bool bPendingBalanceModeStartAttemptIssued = false;
	FString PendingBalanceModeStartReason;
	double PendingBalanceModeRequestTimeSeconds = -1.0;
	bool bPhase1TiltDiagnosticEmitted = false;
	bool bPhase1PelvisCouplingSkipLogged = false;
	FPhase1PelvisCouplingRotationForensics LastPhase1PelvisCouplingRotationForensics;
	bool bPelvisResetAppliedThisTick = false;
#if !UE_BUILD_SHIPPING
	TOptional<FPhase1AutoCalibParams> ActivePhase1AutoCalibParams;
	bool bPhase1AutoCalibOwnsStartRequests = false;
#endif
	float BalanceScenarioPeakPelvisAngularSpeed = 0.0f;
	float BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
	float BalanceScenarioPeakActorDisplacementCm = 0.0f;
	FPoseSearchBlueprintResult BalanceIdlePoseSearchResult;
	bool bHasBalanceIdlePoseSearchResult = false;

	void TrackDistalBoneOwnershipChange(FName BoneName, EPhysicsMovementType NewOwnership, const FString& CallSiteReason);
	void TrackDistalModifierWrite(FName BoneName, EPhysicsMovementType NewMovementType, bool bUpdateBody, const FString& CallSiteReason);
	void ApplyPhase1PelvisRootCouplingSolve();
	TMap<FName, EPhysicsMovementType> PreviousDistalBoneIntendedOwnership;
	TMap<FName, EPhysicsMovementType> PreviousDistalBoneModifierOwnership;
	TMap<FName, FString> LastDistalClassification;
	TMap<FName, FPhysAnimPendingDistalOwnershipCheck> PendingDistalOwnershipChecks;
	static bool ShouldAttemptAutoTriggeredBalanceStart(
		EPhysAnimRuntimeState RuntimeState,
		bool bPendingBalanceModeStartRequest,
		bool bTransitionStarted,
		bool bPhase1AutoCalibOwnsStartRequests,
		bool bPhase1AutoCalibSubsystemActive);

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

	static bool EvaluateBalancePerturbationRuntimeReadiness(
		EPhysAnimRuntimeState RuntimeState,
		int32 HighestUnlockedBringUpGroupIndex,
		int32 BringUpGroupCount,
		bool bFinalBringUpRampActive,
		float PolicyInfluenceAlpha,
		float PolicyInfluenceThreshold,
		bool bHasPendingBodyModifierCachedResets,
		bool bHasPelvisBody,
		bool bPelvisBodySimulating,
		FString* OutFailureReason = nullptr);

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
		EPhysAnimRuntimeState RuntimeState,
		bool bForceZeroActions,
		bool bSimulationHandoffSettled,
		bool bBringUpGroupUnlocked,
		bool bIsRootBodyModifier,
		bool bAllowRootBodyModifierSimulation,
		EPhysicsMovementType& OutMovementType,
		float& OutPhysicsBlendWeight,
		bool& bOutUpdateKinematicFromSimulation);
	static ECollisionEnabled::Type ResolveBodyModifierCollisionType(
		EPhysAnimRuntimeState RuntimeState,
		bool bForceZeroActions,
		bool bSimulationHandoffSettled,
		bool bBringUpGroupUnlocked,
		bool bIsRootBodyModifier,
		bool bAllowRootBodyModifierSimulation);
	static bool ShouldUseAuthoritativePerBoneBodyModifierSync(
		EPhysAnimRuntimeState RuntimeState,
		bool bDistalKinematicAccepted);
	static bool ShouldUpdateBodyOnPerBoneBodyModifierSync(EPhysAnimRuntimeState RuntimeState);
	static bool ShouldResetBodyModifierToCachedBoneTransform(
		FName BoneName,
		EPhysAnimRuntimeState InRuntimeState,
		bool bForceZeroActions,
		bool bBodyModifierActivatedThisTick,
		bool bBringUpGroupUnlocked,
		bool bIsRootBodyModifier,
		bool bAllowRootBodyModifierSimulation,
		float PolicyAlpha,
		bool bIsDistalKinematicAccepted);
	static int32 ResolveBringUpGroupIndex(FName BoneName);
	static int32 GetBringUpGroupCount();
	static bool ShouldDelayBringUpGroupControlRamp(int32 GroupIndex, int32 NumBringUpGroups);
	static bool ShouldStartBringUpGroupControlRamp(
		bool bForceZeroActions,
		bool bBringUpGroupUnlocked,
		bool bDelayBringUpGroupControlRamp,
		bool bPostUnlockSettleComplete,
		bool bStartupBringUpFrozenByBalanceEntry = false);
	static bool ShouldStartPolicyInfluenceRamp(
		EPhysAnimRuntimeState RuntimeState,
		bool bForceZeroActions,
		bool bAllBringUpGroupsUnlocked,
		bool bFinalBringUpGroupControlRampActive,
		bool bPostFinalGroupControlSettleComplete,
		bool bStartupBringUpFrozenByBalanceEntry = false);
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
	static const TCHAR* GetPhysicsMovementTypeName(EPhysicsMovementType MovementType);
	static bool ShouldRunRootOnReadinessUltraFineMarginSweep(float RootOnReadinessTotalDeficitDeg);
	static bool ShouldAcceptStepLimitedPhase1PelvisRotation(
		bool bBestTiltAdmissible,
		bool bBestRootOnAngularReady,
		bool bBestRootOnReadinessMarginSatisfied,
		bool bStepTiltAdmissible,
		bool bStepRootOnAngularReady,
		bool bStepRootOnReadinessMarginSatisfied);
	static bool ShouldRunSpineOnlyRootOnReadinessRescueSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg);
	static bool ShouldRunSpineBiasedDirectConstraintBlendSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg);
	static bool ShouldRunSpineFocusedPairBlendSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg);
	static bool ShouldRunAlternateReferenceDirectConstraintBlendSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg);
	static bool ShouldRunSpineConstraintInterpolationSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg);
	static bool ShouldRunWorstThighConstraintInterpolationSweep(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg);
	static bool ShouldRunSpineSafeWorstThighFocusedDelta(
		float LeftThighAngularErrorDeg,
		float RightThighAngularErrorDeg,
		float SpineAngularErrorDeg);
	static bool IsConstraintSampleRelevantToFocusedBone(
		FName SampleChildBoneName,
		const FString& SampleSource,
		FName FocusChildBone);
	static bool ShouldAcceptWorstThighConstraintInterpolationCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg);
	static bool ShouldAcceptSpineSafeWorstThighMarginSweepCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg);
	static bool ShouldPreferSpineOnlyRootOnReadinessRescueCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg);
	static bool ShouldAcceptSpineOnlyRootOnReadinessRescueCandidate(
		float CurrentLeftThighAngularErrorDeg,
		float CurrentRightThighAngularErrorDeg,
		float CurrentSpineAngularErrorDeg,
		float CandidateLeftThighAngularErrorDeg,
		float CandidateRightThighAngularErrorDeg,
		float CandidateSpineAngularErrorDeg);

	static float ResolvePhase1Uprightness(
		class USkeletalMeshComponent* SkeletalMesh,
		class AActor* Owner,
		const FName& PelvisBoneName,
		FString& OutSourceName);


private:
	void UpdateBridgeStatusIndicator(float DisplayDurationSeconds) const;
	void TransitionRuntimeState(EPhysAnimRuntimeState NewState);
	EPhysAnimRuntimeState RuntimeState = EPhysAnimRuntimeState::Uninitialized;
	double InitialPoseSearchWaitStartTimeSeconds = 0.0;
};



