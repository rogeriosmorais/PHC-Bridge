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
	float BalancePhase2AbortRootLinearSpeed = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortRootAngularSpeed = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortMaxBodyLinearSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization|BalanceEntry", meta = (ClampMin = "0.0"))
	float BalancePhase2AbortMaxBodyAngularSpeed = 300.0f;

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

UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent))
class PHYSANIMPLUGIN_API UPhysAnimComponent : public UActorComponent, public IPoseSearchTrajectoryPredictorInterface
{
	GENERATED_BODY()
	friend class FPhysAnimBalanceReadyTransition;

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
	bool HasBalanceReadyTransitionFailed() const { return BalanceReadyTransition.HasFailed(); }
	bool HasSafePhase2Denial() const { return BalanceReadyTransition.HasSafePhase2Denial(); }
	const FString& GetBalanceReadyTransitionFailureReason() const { return BalanceReadyTransition.GetFailureReason(); }
	const FString& GetSafePhase2DenialReason() const { return BalanceReadyTransition.GetSafePhase2DenialReason(); }

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

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void StartBalancePerturbationMode();

	UFUNCTION(BlueprintCallable, Category = "PhysAnim")
	void StopBalancePerturbationMode();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceQuietLinearSpeedThresholdCmPerSec = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Balance", meta = (ClampMin = "0.0"))
	float BalanceQuietTiltThresholdDeg = 10.0f;

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
	bool bPelvisResetAppliedThisTick = false;
	float BalanceScenarioPeakPelvisAngularSpeed = 0.0f;
	float BalanceScenarioPeakPelvisDisplacementCm = 0.0f;
	float BalanceScenarioPeakActorDisplacementCm = 0.0f;
	FPoseSearchBlueprintResult BalanceIdlePoseSearchResult;
	bool bHasBalanceIdlePoseSearchResult = false;

	void TrackDistalBoneOwnershipChange(FName BoneName, EPhysicsMovementType NewOwnership, const FString& CallSiteReason);
	void TrackDistalModifierWrite(FName BoneName, EPhysicsMovementType NewMovementType, bool bUpdateBody, const FString& CallSiteReason);
	TMap<FName, EPhysicsMovementType> PreviousDistalBoneIntendedOwnership;
	TMap<FName, EPhysicsMovementType> PreviousDistalBoneModifierOwnership;
	TMap<FName, FString> LastDistalClassification;
	TMap<FName, FPhysAnimPendingDistalOwnershipCheck> PendingDistalOwnershipChecks;

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



