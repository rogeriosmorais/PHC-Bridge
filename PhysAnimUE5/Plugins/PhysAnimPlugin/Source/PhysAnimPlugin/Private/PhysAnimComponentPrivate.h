#pragma once

#include "PhysAnimComponent.h"

#include "PhysAnimBridge.h"
#include "PhysAnimBridgeTrace.h"
#include "PhysAnimStage1InitializerComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "CollisionQueryParams.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "Math/RotationMatrix.h"
#include "NNEStatus.h"
#include "PhysicsControlActor.h"
#include "PhysicsControlComponent.h"
#include "PhysicsControlRecord.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "PoseSearch/PoseSearchAssetSamplerLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchContext.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "Math/Interval.h"
#include "PhysicsEngine/BodyInstance.h"

#include "Misc/DateTime.h"
#include "Misc/Paths.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPhysAnimBridge, Log, All);

TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_PoseSearchQueryMs);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_FuturePoseSampleMs);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_ObservationPackMs);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_RunSyncMs);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_ControlWritesMs);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_UpdateControlsMs);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_MaxBodyAngularSpeedDegPerSec);
TRACE_DECLARE_FLOAT_COUNTER_EXTERN(COUNTER_PhysAnim_MaxLowerLimbLimitOccupancy);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_PhysAnim_NumNormalPolicyTargetsWritten);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_PhysAnim_NumHeldTargetsWritten);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_PhysAnim_NumTotalTargetsWritten);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_PhysAnim_RuntimeState);
TRACE_DECLARE_INT_COUNTER_EXTERN(COUNTER_PhysAnim_FailStopCount);

extern int32 GStrictPhase1Certification;
extern FAutoConsoleVariableRef CVarStrictPhase1Certification;
extern int32 GVerbosePhase1Forensics;
extern FAutoConsoleVariableRef CVarVerbosePhase1Forensics;
extern int32 GVerbosePhase2Forensics;
extern FAutoConsoleVariableRef CVarVerbosePhase2Forensics;

namespace PhysAnimComponentInternal
{
	inline const FName PoseHistoryName(TEXT("PoseHistory_Stage1"));
	inline const TCHAR* ExpectedMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	inline const TCHAR* ExpectedPhysicsAssetPath = TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin");
	inline const TCHAR* ExpectedAnimBlueprintPath = TEXT("/Game/Characters/Mannequins/Animations/ABP_PhysAnim.ABP_PhysAnim_C");
	inline const TCHAR* ExpectedPoseSearchDatabasePath = TEXT("/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion.PSDB_Stage1_Locomotion");
	inline const TCHAR* ExpectedPoseSearchSchemaPath = TEXT("/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion.PSS_Stage1_Locomotion");
	inline const TCHAR* DefaultModelPath = TEXT("/Game/NNEModels/phc_policy.phc_policy");
	inline const TCHAR* PreferredGpuRuntime = TEXT("NNERuntimeORTDml");
	inline const TCHAR* FallbackCpuRuntime = TEXT("NNERuntimeORTCpu");

	// Private accessor to read back modifier records from PhysicsControlComponent
	struct FPhysAnimPhysicsControlAccessor : public UPhysicsControlComponent
	{
	public:
		static const FPhysicsBodyModifierRecord* GetModifierRecord(const UPhysicsControlComponent* ControlComp, const FName Name)
		{
			return ((const FPhysAnimPhysicsControlAccessor*)ControlComp)->FindBodyModifierRecord(Name);
		}

		static FPhysicsBodyModifierRecord* GetMutableModifierRecord(UPhysicsControlComponent* ControlComp, const FName Name)
		{
			return ((FPhysAnimPhysicsControlAccessor*)ControlComp)->FindBodyModifierRecord(Name);
		}
	};

	inline void ForceBodyModifierRecordState(
		UPhysicsControlComponent* ControlComp,
		const FName ModifierName,
		const EPhysicsMovementType MovementType,
		const float PhysicsBlendWeight,
		const ECollisionEnabled::Type CollisionType,
		const bool bUpdateKinematicFromSimulation)
	{
		if (FPhysicsBodyModifierRecord* const Record = FPhysAnimPhysicsControlAccessor::GetMutableModifierRecord(ControlComp, ModifierName))
		{
			Record->BodyModifier.ModifierData.MovementType = MovementType;
			Record->BodyModifier.ModifierData.PhysicsBlendWeight = PhysicsBlendWeight;
			Record->BodyModifier.ModifierData.CollisionType = CollisionType;
			Record->BodyModifier.ModifierData.bUpdateKinematicFromSimulation = bUpdateKinematicFromSimulation;
		}
	}

	constexpr double InitialPoseSearchWaitTimeoutSeconds = 2.0;
	constexpr int32 NumBringUpGroups = 5;
	constexpr float MovementSmokeIdleDurationSeconds = 3.0f;
	constexpr float MovementSmokeMoveDurationSeconds = 5.0f;
	constexpr float MovementSmokeTimelineDurationSeconds = 32.0f;
	constexpr float PresentationPerturbationStrengthRelaxationMultiplier = 0.72f;
	constexpr float PresentationPerturbationDampingRatioRelaxationMultiplier = 0.78f;
	constexpr float PresentationPerturbationExtraDampingRelaxationMultiplier = 0.74f;
	constexpr float TerrainTraceStartAboveRootCm = 200.0f;
	constexpr float TerrainTraceEndBelowRootCm = 300.0f;

	// Balance Perturbation Mode Thresholds (Design Aligned)
	constexpr float BalanceResponseVelocityThresholdCmPerSec = 5.0f;
	constexpr float BalanceRecoveryVelocityThresholdCmPerSec = 10.0f;
	constexpr float BalanceRecoveryTiltThresholdDeg = 10.0f;
	constexpr float BalanceRecoveryHeightToleranceCm = 15.0f;
	constexpr float BalanceRecoveryStableHoldSeconds = 1.0f;
	constexpr float BalanceShellContaminationDisplacementCm = 2.0f;
	constexpr float BalanceModeTotalTimeoutSeconds = 120.0f;

	// Target Delta-V (m/s) for mass-normalized impulses
	constexpr float BalanceTargetDeltaVSmall = 0.4e2f; // cm/s
	constexpr float BalanceTargetDeltaVMedium = 1.0e2f; // cm/s
	constexpr float BalanceTargetDeltaVLarge = 2.5e2f; // cm/s

	inline float ResolveLoopingAnimationTime(const float AnimationTime, const float PlayLength, const bool bLoop)
	{
		if (PlayLength <= UE_SMALL_NUMBER)
		{
			return 0.0f;
		}

		if (!bLoop)
		{
			return FMath::Clamp(AnimationTime, 0.0f, PlayLength);
		}

		float WrappedTime = FMath::Fmod(AnimationTime, PlayLength);
		if (WrappedTime < 0.0f)
		{
			WrappedTime += PlayLength;
		}

		return WrappedTime;
	}

	struct FFloatBufferSummary
	{
		float Mean = 0.0f;
		float MeanAbs = 0.0f;
		float Min = 0.0f;
		float Max = 0.0f;
		bool bHasValues = false;
	};

	inline FFloatBufferSummary SummarizeFloatBuffer(const TArray<float>& Buffer)
	{
		FFloatBufferSummary Summary;
		if (Buffer.IsEmpty())
		{
			return Summary;
		}

		Summary.bHasValues = true;
		Summary.Min = Buffer[0];
		Summary.Max = Buffer[0];
		double Sum = 0.0;
		double SumAbs = 0.0;
		for (const float Value : Buffer)
		{
			Summary.Min = FMath::Min(Summary.Min, Value);
			Summary.Max = FMath::Max(Summary.Max, Value);
			Sum += static_cast<double>(Value);
			SumAbs += FMath::Abs(static_cast<double>(Value));
		}

		Summary.Mean = static_cast<float>(Sum / static_cast<double>(Buffer.Num()));
		Summary.MeanAbs = static_cast<float>(SumAbs / static_cast<double>(Buffer.Num()));
		return Summary;
	}

	inline float ResolveTerrainCenterSampleValue(const TArray<float>& TerrainBuffer)
	{
		constexpr int32 CenterAxisIndex = PhysAnimBridge::TerrainSamplesPerAxis / 2;
		constexpr int32 CenterSampleIndex = (CenterAxisIndex * PhysAnimBridge::TerrainSamplesPerAxis) + CenterAxisIndex;
		return TerrainBuffer.IsValidIndex(CenterSampleIndex) ? TerrainBuffer[CenterSampleIndex] : 0.0f;
	}

	inline void ResolveMimicTargetPoseTimeRange(
		const TArray<float>& MimicTargetPosesBuffer,
		float& OutMinFutureTimeSeconds,
		float& OutMaxFutureTimeSeconds)
	{
		OutMinFutureTimeSeconds = 0.0f;
		OutMaxFutureTimeSeconds = 0.0f;

		constexpr int32 FloatsPerTargetPose = PhysAnimBridge::MimicTargetPosesSize / PhysAnimBridge::NumFutureSteps;
		static_assert(
			(FloatsPerTargetPose * PhysAnimBridge::NumFutureSteps) == PhysAnimBridge::MimicTargetPosesSize,
			"MimicTargetPosesSize must divide cleanly by NumFutureSteps.");

		if (MimicTargetPosesBuffer.Num() != PhysAnimBridge::MimicTargetPosesSize)
		{
			return;
		}

		OutMinFutureTimeSeconds = TNumericLimits<float>::Max();
		OutMaxFutureTimeSeconds = -TNumericLimits<float>::Max();
		for (int32 FutureStepIndex = 0; FutureStepIndex < PhysAnimBridge::NumFutureSteps; ++FutureStepIndex)
		{
			const int32 TimeChannelIndex = (FutureStepIndex * FloatsPerTargetPose) + (FloatsPerTargetPose - 1);
			const float FutureTimeSeconds = MimicTargetPosesBuffer[TimeChannelIndex];
			OutMinFutureTimeSeconds = FMath::Min(OutMinFutureTimeSeconds, FutureTimeSeconds);
			OutMaxFutureTimeSeconds = FMath::Max(OutMaxFutureTimeSeconds, FutureTimeSeconds);
		}
	}

	inline TAutoConsoleVariable<float> CVarPhysAnimActionScale(
		TEXT("physanim.ActionScale"),
		-1.0f,
		TEXT("Override for PhysAnim action scale. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimActionClampAbs(
		TEXT("physanim.ActionClampAbs"),
		-1.0f,
		TEXT("Override for PhysAnim absolute action clamp. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimActionSmoothingAlpha(
		TEXT("physanim.ActionSmoothingAlpha"),
		-1.0f,
		TEXT("Override for PhysAnim action smoothing alpha. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimStartupRampSeconds(
		TEXT("physanim.StartupRampSeconds"),
		-1.0f,
		TEXT("Override for PhysAnim startup ramp duration in seconds. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimPolicyControlRateHz(
		TEXT("physanim.PolicyControlRateHz"),
		-1.0f,
		TEXT("Override for the fixed PhysAnim policy/control update rate in Hz. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedMassScales(
		TEXT("physanim.ApplyTrainingAlignedMassScales"),
		-1,
		TEXT("Override for the Stage 1 training-aligned Manny family mass policy. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedMassScaleBlend(
		TEXT("physanim.TrainingAlignedMassScaleBlend"),
		-1.0f,
		TEXT("Override for the Stage 1 training-aligned Manny family mass policy blend. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedControlFamilyProfile(
		TEXT("physanim.ApplyTrainingAlignedControlFamilyProfile"),
		-1,
		TEXT("Override for the Stage 1 training-aligned control family profile. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedControlFamilyProfileBlend(
		TEXT("physanim.TrainingAlignedControlFamilyProfileBlend"),
		-1.0f,
		TEXT("Override for the Stage 1 training-aligned control family profile blend. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedLocomotionLowerLimbResponsePolicy(
		TEXT("physanim.ApplyTrainingAlignedLocomotionLowerLimbResponsePolicy"),
		-1,
		TEXT("Override for the Stage 1 locomotion-time proximal lower-limb response policy. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedLocomotionLowerLimbResponsePolicyBlend(
		TEXT("physanim.TrainingAlignedLocomotionLowerLimbResponsePolicyBlend"),
		-1.0f,
		TEXT("Override for the Stage 1 locomotion-time proximal lower-limb response policy blend. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedToeLimitPolicy(
		TEXT("physanim.ApplyTrainingAlignedToeLimitPolicy"),
		-1,
		TEXT("Override for the Stage 1 training-aligned toe operating-limit policy. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedToeLimitPolicyBlend(
		TEXT("physanim.TrainingAlignedToeLimitPolicyBlend"),
		0.5f,
		TEXT("Override for the Stage 1 training-aligned toe operating-limit policy blend. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedLowerLimbTargetRangePolicy(
		TEXT("physanim.ApplyTrainingAlignedLowerLimbTargetRangePolicy"),
		-1,
		TEXT("Override for the Stage 1 training-aligned lower-limb target-range policy. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedLowerLimbTargetRangePolicyBlend(
		TEXT("physanim.TrainingAlignedLowerLimbTargetRangePolicyBlend"),
		-1.0f,
		TEXT("Override for the Stage 1 training-aligned lower-limb target-range policy blend. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedDistalLocomotionTargetPolicy(
		TEXT("physanim.ApplyTrainingAlignedDistalLocomotionTargetPolicy"),
		-1,
		TEXT("Override for the Stage 1 training-aligned distal locomotion target policy. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedDistalLocomotionTargetPolicyBlend(
		TEXT("physanim.TrainingAlignedDistalLocomotionTargetPolicyBlend"),
		-1.0f,
		TEXT("Override for the Stage 1 training-aligned distal locomotion target policy blend. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimDistalLocomotionTargetPolicyActivationSpeedCmPerSec(
		TEXT("physanim.DistalLocomotionTargetPolicyActivationSpeedCmPerSec"),
		-1.0f,
		TEXT("Override for the Stage 1 distal locomotion target policy activation speed in cm/s. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedDistalLocomotionCompositionPolicy(
		TEXT("physanim.ApplyTrainingAlignedDistalLocomotionCompositionPolicy"),
		-1,
		TEXT("Override for the Stage 1 distal locomotion target composition policy. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimDistalLocomotionCompositionPolicyActivationSpeedCmPerSec(
		TEXT("physanim.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec"),
		-1.0f,
		TEXT("Override for the Stage 1 distal locomotion target composition activation speed in cm/s. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimDistalLocomotionCompositionPolicyExitSpeedCmPerSec(
		TEXT("physanim.DistalLocomotionCompositionPolicyExitSpeedCmPerSec"),
		-1.0f,
		TEXT("Override for the Stage 1 distal locomotion target composition exit speed in cm/s. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimDistalLocomotionCompositionPolicyEnterHoldSeconds(
		TEXT("physanim.DistalLocomotionCompositionPolicyEnterHoldSeconds"),
		-1.0f,
		TEXT("Override for the Stage 1 distal locomotion target composition enter-hold in seconds. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimDistalLocomotionCompositionPolicyIntentGraceSeconds(
		TEXT("physanim.DistalLocomotionCompositionPolicyIntentGraceSeconds"),
		-1.0f,
		TEXT("Override for the Stage 1 distal locomotion target composition movement-intent grace in seconds. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimDistalLocomotionCompositionPolicyExitHoldSeconds(
		TEXT("physanim.DistalLocomotionCompositionPolicyExitHoldSeconds"),
		-1.0f,
		TEXT("Override for the Stage 1 distal locomotion target composition exit-hold in seconds. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimMaxAngularStepDegPerSec(
		TEXT("physanim.MaxAngularStepDegPerSec"),
		-1.0f,
		TEXT("Override for PhysAnim maximum target rotation step in degrees per second. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimAngularStrengthMultiplier(
		TEXT("physanim.AngularStrengthMultiplier"),
		-1.0f,
		TEXT("Override for PhysAnim angular strength multiplier. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimAngularDampingRatioMultiplier(
		TEXT("physanim.AngularDampingRatioMultiplier"),
		-1.0f,
		TEXT("Override for PhysAnim angular damping ratio multiplier. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimAngularExtraDampingMultiplier(
		TEXT("physanim.AngularExtraDampingMultiplier"),
		-1.0f,
		TEXT("Override for PhysAnim angular extra damping multiplier. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimUseSkeletalAnimationTargets(
		TEXT("physanim.UseSkeletalAnimationTargets"),
		-1,
		TEXT("Override for PhysAnim skeletal-animation target blending. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimForceZeroActions(
		TEXT("physanim.ForceZeroActions"),
		-1,
		TEXT("Override for PhysAnim zero-action mode. -1 keeps the component default, 0 disables, 1 forces zero actions."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimLogActionDiagnostics(
		TEXT("physanim.LogActionDiagnostics"),
		-1,
		TEXT("Override for PhysAnim action diagnostics logging. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<float> CVarPhysAnimMaxRootHeightDeltaCm(
		TEXT("physanim.MaxRootHeightDeltaCm"),
		-1.0f,
		TEXT("Override for PhysAnim runtime root height delta fail-stop threshold in cm. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimMaxRootLinearSpeedCmPerSec(
		TEXT("physanim.MaxRootLinearSpeedCmPerSec"),
		-1.0f,
		TEXT("Override for PhysAnim runtime root linear speed fail-stop threshold in cm/s. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimMaxRootAngularSpeedDegPerSec(
		TEXT("physanim.MaxRootAngularSpeedDegPerSec"),
		-1.0f,
		TEXT("Override for PhysAnim runtime root angular speed fail-stop threshold in deg/s. Negative values keep the component default."));
	inline TAutoConsoleVariable<float> CVarPhysAnimInstabilityGracePeriodSeconds(
		TEXT("physanim.InstabilityGracePeriodSeconds"),
		-1.0f,
		TEXT("Override for PhysAnim runtime instability grace period in seconds. Negative values keep the component default."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimEnableInstabilityFailStop(
		TEXT("physanim.EnableInstabilityFailStop"),
		-1,
		TEXT("Override for PhysAnim runtime instability fail-stop. -1 keeps the component default, 0 disables, 1 enables."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimLogBridgeStateSnapshots(
		TEXT("physanim.LogBridgeStateSnapshots"),
		1,
		TEXT("Whether PhysAnim emits startup and fail-stop bridge state snapshots."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimMovementSmokeMode(
		TEXT("physanim.MovementSmokeMode"),
		0,
		TEXT("Enables the deterministic PIE movement smoke mode that preserves the gameplay shell and applies scripted WASD-equivalent input."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimMovementSmokeLoopCount(
		TEXT("physanim.MovementSmokeLoopCount"),
		1,
		TEXT("How many times the deterministic PIE movement smoke timeline repeats before completing."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimAllowCharacterMovementInBridgeActive(
		TEXT("physanim.AllowCharacterMovementInBridgeActive"),
		1,
		TEXT("When enabled, BridgeActive preserves capsule collision and CharacterMovement so the player can drive the character with normal gameplay input."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimShowBridgeStatusIndicator(
		TEXT("physanim.ShowBridgeStatusIndicator"),
		1,
		TEXT("Whether PhysAnim shows an always-visible on-screen bridge status indicator."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimTraceOutput(
		TEXT("physanim.TraceOutput"),
		-1,
		TEXT("Override for PhysAnim bridge trace output. -1 keeps the component default, 0 disables, 1 writes metadata+events, 2 writes metadata+events+frames."));
	inline TAutoConsoleVariable<int32> CVarPaStabilizationStressTest(
		TEXT("pa.StabilizationStressTest"),
		0,
		TEXT("Enable the idle stabilization stress-test ramp. 0 disables, 1 linearly relaxes all angular stabilization gains after the bridge is fully settled."));
	inline TAutoConsoleVariable<float> CVarPaStabilizationStressTestRampSeconds(
		TEXT("pa.StabilizationStressTestRampSeconds"),
		45.0f,
		TEXT("Duration in seconds for the stabilization stress-test gain ramp from 1.0 to 0.0. Values <= 0 clamp the ramp immediately to zero."));
	inline TAutoConsoleVariable<int32> CVarPaStabilizationStressTestProfile(
		TEXT("pa.StabilizationStressTestProfile"),
		0,
		TEXT("Stabilization stress-test profile. 0 = ramp down only, 1 = ramp down / hold / ramp back up."));
	inline TAutoConsoleVariable<float> CVarPaStabilizationStressTestTargetMultiplier(
		TEXT("pa.StabilizationStressTestTargetMultiplier"),
		0.0f,
		TEXT("Target floor multiplier for the stabilization stress-test profile. Used as the hold floor for down/hold/up mode."));
	inline TAutoConsoleVariable<float> CVarPaStabilizationStressTestHoldSeconds(
		TEXT("pa.StabilizationStressTestHoldSeconds"),
		3.0f,
		TEXT("Hold duration in seconds for the stabilization stress-test recovery profile."));
	inline TAutoConsoleVariable<float> CVarPaStabilizationStressTestRecoveryRampSeconds(
		TEXT("pa.StabilizationStressTestRecoveryRampSeconds"),
		5.0f,
		TEXT("Ramp-up duration in seconds for the stabilization stress-test recovery profile."));
	inline TAutoConsoleVariable<int32> CVarPaStabilizationStressTestSweepMode(
		TEXT("pa.StabilizationStressTestSweepMode"),
		0,
		TEXT("Which stabilization parameter the stress-test ramps. 0 = all, 1 = strength only, 2 = damping ratio only, 3 = extra damping only."));
	inline TAutoConsoleVariable<float> CVarPaStabilizationStressTestAngularSpikeThreshold(
		TEXT("pa.StabilizationStressTestAngularSpikeThresholdDegPerSec"),
		500.0f,
		TEXT("Angular velocity threshold used to mark the first stress-test per-bone spike."));
	inline TAutoConsoleVariable<float> CVarPaStabilizationStressTestLinearSpikeThreshold(
		TEXT("pa.StabilizationStressTestLinearSpikeThresholdCmPerSec"),
		100.0f,
		TEXT("Linear velocity threshold used to mark the first stress-test per-bone spike."));
	inline TAutoConsoleVariable<int32> CVarPhysAnimPhase1DistalKinematicExperiment(
		TEXT("physanim.Phase1DistalKinematicExperiment"),
		-1,
		TEXT("Override for Phase 1 distal-chain kinematic experiment. -1 keeps the component default, 0 disables (distal=sim), 1 enables (distal=kin)."));

	inline float ResolveFloatOverride(const TAutoConsoleVariable<float>& CVar, float DefaultValue)
	{
		const float OverrideValue = CVar.GetValueOnGameThread();
		return OverrideValue >= 0.0f ? OverrideValue : DefaultValue;
	}

	inline bool ResolveBoolOverride(const TAutoConsoleVariable<int32>& CVar, bool DefaultValue)
	{
		const int32 OverrideValue = CVar.GetValueOnGameThread();
		if (OverrideValue < 0)
		{
			return DefaultValue;
		}

		return OverrideValue != 0;
	}

	inline EPhysAnimBridgeTraceMode ToTraceMode(const EPhysAnimBridgeTraceOutputMode Mode)
	{
		switch (Mode)
		{
		case EPhysAnimBridgeTraceOutputMode::MetadataAndEvents:
			return EPhysAnimBridgeTraceMode::MetadataAndEvents;
		case EPhysAnimBridgeTraceOutputMode::Full:
			return EPhysAnimBridgeTraceMode::Full;
		default:
			return EPhysAnimBridgeTraceMode::Off;
		}
	}

	inline FString SanitizeTraceName(const FString& Value)
	{
		FString Sanitized = FPaths::MakeValidFileName(Value);
		Sanitized.ReplaceInline(TEXT(" "), TEXT("_"));
		return Sanitized.IsEmpty() ? TEXT("Unknown") : Sanitized;
	}

	inline FString GetTraceMapName(const UWorld* const World)
	{
		return World ? SanitizeTraceName(UWorld::RemovePIEPrefix(World->GetMapName())) : TEXT("UnknownMap");
	}

	inline FString GetTraceActorName(const AActor* const Actor)
	{
		return Actor ? SanitizeTraceName(Actor->GetName()) : TEXT("UnknownActor");
	}

	inline FString BuildTraceSessionId(const UWorld* const World, const AActor* const Actor)
	{
		const FString Timestamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"));
		return FString::Printf(TEXT("%s-%s-%s"), *Timestamp, *GetTraceMapName(World), *GetTraceActorName(Actor));
	}

	inline FString GetTraceRootPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PhysAnim"), TEXT("Traces"));
	}

	inline bool IsLowerLimbControlBone(const FName BoneName)
	{
		return BoneName == TEXT("thigh_l") ||
			BoneName == TEXT("calf_l") ||
			BoneName == TEXT("foot_l") ||
			BoneName == TEXT("ball_l") ||
			BoneName == TEXT("thigh_r") ||
			BoneName == TEXT("calf_r") ||
			BoneName == TEXT("foot_r") ||
			BoneName == TEXT("ball_r");
	}

	inline FName ResolveDirectConstraintParentBoneName(const FName ChildBoneName)
	{
		if (ChildBoneName == TEXT("thigh_l") || ChildBoneName == TEXT("thigh_r"))
		{
			return TEXT("pelvis");
		}
		if (ChildBoneName == TEXT("calf_l"))
		{
			return TEXT("thigh_l");
		}
		if (ChildBoneName == TEXT("calf_r"))
		{
			return TEXT("thigh_r");
		}
		if (ChildBoneName == TEXT("foot_l"))
		{
			return TEXT("calf_l");
		}
		if (ChildBoneName == TEXT("foot_r"))
		{
			return TEXT("calf_r");
		}
		if (ChildBoneName == TEXT("ball_l"))
		{
			return TEXT("foot_l");
		}
		if (ChildBoneName == TEXT("ball_r"))
		{
			return TEXT("foot_r");
		}
		return NAME_None;
	}

	inline float CalculateConstraintMinLimitedAngleDegrees(
		const EAngularConstraintMotion TwistMotion,
		const float TwistLimit,
		const EAngularConstraintMotion Swing1Motion,
		const float Swing1Limit,
		const EAngularConstraintMotion Swing2Motion,
		const float Swing2Limit)
	{
		float MinLimitedAngleDegrees = TNumericLimits<float>::Max();
		bool bHasLimitedAxis = false;

		auto AccumulateLimit = [&MinLimitedAngleDegrees, &bHasLimitedAxis](const EAngularConstraintMotion Motion, const float Limit)
		{
			if (Motion == ACM_Limited && Limit > UE_SMALL_NUMBER)
			{
				MinLimitedAngleDegrees = FMath::Min(MinLimitedAngleDegrees, Limit);
				bHasLimitedAxis = true;
			}
		};

		AccumulateLimit(TwistMotion, TwistLimit);
		AccumulateLimit(Swing1Motion, Swing1Limit);
		AccumulateLimit(Swing2Motion, Swing2Limit);
		return bHasLimitedAxis ? MinLimitedAngleDegrees : 0.0f;
	}

	inline float ResolveLowerLimbConstraintLimitProxyDegrees(const UPhysicsAsset* const PhysicsAsset, const FName ChildBoneName)
	{
		if (!PhysicsAsset || !IsLowerLimbControlBone(ChildBoneName))
		{
			return 0.0f;
		}

		const FName ParentBoneName = ResolveDirectConstraintParentBoneName(ChildBoneName);
		if (ParentBoneName.IsNone())
		{
			return 0.0f;
		}

		const int32 ConstraintIndex = PhysicsAsset->FindConstraintIndex(ChildBoneName, ParentBoneName);
		if (ConstraintIndex == INDEX_NONE || !PhysicsAsset->ConstraintSetup.IsValidIndex(ConstraintIndex))
		{
			return 0.0f;
		}

		const UPhysicsConstraintTemplate* const ConstraintTemplate = PhysicsAsset->ConstraintSetup[ConstraintIndex];
		if (!ConstraintTemplate)
		{
			return 0.0f;
		}

		const FConstraintInstance& Constraint = ConstraintTemplate->DefaultInstance;
		return CalculateConstraintMinLimitedAngleDegrees(
			Constraint.GetAngularTwistMotion(),
			Constraint.GetAngularTwistLimit(),
			Constraint.GetAngularSwing1Motion(),
			Constraint.GetAngularSwing1Limit(),
			Constraint.GetAngularSwing2Motion(),
			Constraint.GetAngularSwing2Limit());
	}

	inline FString BuildStabilizationSummary(const FPhysAnimStabilizationSettings& Settings)
	{
		return FString::Printf(
			TEXT("Zero=%s Scale=%.3f Clamp=%.3f Smooth=%.3f Ramp=%.3f PolicyHz=%.1f MassPolicy=%s MassBlend=%.2f FamilyPd=%s FamilyPdBlend=%.2f ToePolicy=%s ToeBlend=%.2f LowerLimbRangePolicy=%s LowerLimbRangeBlend=%.2f DistalMovePolicy=%s DistalMoveBlend=%.2f DistalMoveSpeed=%.1f DistalMoveCompose=%s DistalMoveComposeEnter=%.1f DistalMoveComposeExit=%.1f EnterHold=%.2f IntentGrace=%.2f ExitHold=%.2f StepDegPerSec=%.1f GainMul=%.3f DampMul=%.3f ExtraDampMul=%.3f SkeletalTargets=%s InstabilityStop=%s HeightCm=%.1f LinCmPerSec=%.1f AngDegPerSec=%.1f Grace=%.2f"),
			Settings.bForceZeroActions ? TEXT("true") : TEXT("false"),
			Settings.ActionScale,
			Settings.ActionClampAbs,
			Settings.ActionSmoothingAlpha,
			Settings.StartupRampSeconds,
			Settings.PolicyControlRateHz,
			Settings.bApplyTrainingAlignedMassScales ? TEXT("true") : TEXT("false"),
			Settings.TrainingAlignedMassScaleBlend,
			Settings.bApplyTrainingAlignedControlFamilyProfile ? TEXT("true") : TEXT("false"),
			Settings.TrainingAlignedControlFamilyProfileBlend,
			Settings.bApplyTrainingAlignedToeLimitPolicy ? TEXT("true") : TEXT("false"),
			Settings.TrainingAlignedToeLimitPolicyBlend,
			Settings.bApplyTrainingAlignedLowerLimbTargetRangePolicy ? TEXT("true") : TEXT("false"),
			Settings.TrainingAlignedLowerLimbTargetRangePolicyBlend,
			Settings.bApplyTrainingAlignedDistalLocomotionTargetPolicy ? TEXT("true") : TEXT("false"),
			Settings.TrainingAlignedDistalLocomotionTargetPolicyBlend,
			Settings.DistalLocomotionTargetPolicyActivationSpeedCmPerSec,
			Settings.bApplyTrainingAlignedDistalLocomotionCompositionPolicy ? TEXT("true") : TEXT("false"),
			Settings.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec,
			Settings.DistalLocomotionCompositionPolicyExitSpeedCmPerSec,
			Settings.DistalLocomotionCompositionPolicyEnterHoldSeconds,
			Settings.DistalLocomotionCompositionPolicyIntentGraceSeconds,
			Settings.DistalLocomotionCompositionPolicyExitHoldSeconds,
			Settings.MaxAngularStepDegreesPerSecond,
			Settings.AngularStrengthMultiplier,
			Settings.AngularDampingRatioMultiplier,
			Settings.AngularExtraDampingMultiplier,
			Settings.bUseSkeletalAnimationTargets ? TEXT("true") : TEXT("false"),
			Settings.bEnableInstabilityFailStop ? TEXT("true") : TEXT("false"),
			Settings.MaxRootHeightDeltaCm,
			Settings.MaxRootLinearSpeedCmPerSecond,
			Settings.MaxRootAngularSpeedDegPerSecond,
			Settings.InstabilityGracePeriodSeconds);
	}

	inline FString JoinNames(const TArray<FName>& Names)
	{
		TArray<FString> Strings;
		Strings.Reserve(Names.Num());
		for (const FName Name : Names)
		{
			Strings.Add(Name.ToString());
		}
		return FString::Join(Strings, TEXT(", "));
	}

	inline bool ValidateInitialPhysicsControlAuthoring(
		const TMap<FName, FInitialPhysicsControl>& InitialControls,
		const TMap<FName, FInitialBodyModifier>& InitialBodyModifiers,
		FString& OutError)
	{
		TArray<FName> MissingControls;
		for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
		{
			const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
			if (!InitialControls.Contains(ControlName))
			{
				MissingControls.Add(ControlName);
			}
		}

		TArray<FName> MissingModifiers;
		for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
		{
			const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
			if (!InitialBodyModifiers.Contains(ModifierName))
			{
				MissingModifiers.Add(ModifierName);
			}
		}

		if (InitialControls.Num() != PhysAnimBridge::GetControlledBoneNames().Num())
		{
			OutError = FString::Printf(
				TEXT("Expected %d authored controls but found %d."),
				PhysAnimBridge::GetControlledBoneNames().Num(),
				InitialControls.Num());
			return false;
		}

		if (InitialBodyModifiers.Num() != PhysAnimBridge::GetRequiredBodyModifierBoneNames().Num())
		{
			OutError = FString::Printf(
				TEXT("Expected %d authored body modifiers but found %d."),
				PhysAnimBridge::GetRequiredBodyModifierBoneNames().Num(),
				InitialBodyModifiers.Num());
			return false;
		}

		if (MissingControls.Num() > 0)
		{
			OutError = FString::Printf(TEXT("Missing required authored controls: %s"), *JoinNames(MissingControls));
			return false;
		}

		if (MissingModifiers.Num() > 0)
		{
			OutError = FString::Printf(TEXT("Missing required authored body modifiers: %s"), *JoinNames(MissingModifiers));
			return false;
		}

		return true;
	}
}

using PhysAnimComponentInternal::FPhysAnimPhysicsControlAccessor;
using PhysAnimComponentInternal::ForceBodyModifierRecordState;

inline void ForceTPoseReferenceOntoMesh(USkeletalMeshComponent* const SkeletalMesh, UAnimSequence* const TPoseReference)
{
	if (!SkeletalMesh || !TPoseReference)
	{
		return;
	}

	SkeletalMesh->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	SkeletalMesh->SetAnimation(TPoseReference);
	SkeletalMesh->PlayAnimation(TPoseReference, false);
	SkeletalMesh->SetPosition(0.0f, false);
	SkeletalMesh->TickAnimation(0.0f, false);
	SkeletalMesh->RefreshBoneTransforms();
	SkeletalMesh->UpdateComponentToWorld();
}

inline float QuaternionAngularErrorDegrees(const FQuat& InQuat)
{
	const FQuat NormalizedQuat = InQuat.GetNormalized();
	const float SafeW = FMath::Clamp(FMath::Abs(NormalizedQuat.W), 0.0f, 1.0f);
	const float AngleRadians = 2.0f * FMath::Acos(SafeW);
	return FMath::RadiansToDegrees(AngleRadians);
}
