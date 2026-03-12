#include "PhysAnimComponent.h"

#include "PhysAnimBridge.h"
#include "PhysAnimStage1InitializerComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
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
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PoseSearch/PoseSearchAssetSamplerLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PhysicsEngine/BodyInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogPhysAnimBridge, Log, All);

namespace PhysAnimComponentInternal
{
	const FName PoseHistoryName(TEXT("PoseHistory_Stage1"));
	const TCHAR* ExpectedMeshPath = TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple");
	const TCHAR* ExpectedPhysicsAssetPath = TEXT("/Game/Characters/Mannequins/Rigs/PA_Mannequin.PA_Mannequin");
	const TCHAR* ExpectedAnimBlueprintPath = TEXT("/Game/Characters/Mannequins/Animations/ABP_PhysAnim.ABP_PhysAnim_C");
	const TCHAR* ExpectedPoseSearchDatabasePath = TEXT("/Game/PoseSearch/Databases/PSDB_Stage1_Locomotion.PSDB_Stage1_Locomotion");
	const TCHAR* ExpectedPoseSearchSchemaPath = TEXT("/Game/PoseSearch/Schemas/PSS_Stage1_Locomotion.PSS_Stage1_Locomotion");
	const TCHAR* DefaultModelPath = TEXT("/Game/NNEModels/phc_policy.phc_policy");
	const TCHAR* PreferredGpuRuntime = TEXT("NNERuntimeORTDml");
	const TCHAR* FallbackCpuRuntime = TEXT("NNERuntimeORTCpu");
	constexpr double InitialPoseSearchWaitTimeoutSeconds = 2.0;
	constexpr int32 NumBringUpGroups = 5;
	constexpr float MovementSmokeIdleDurationSeconds = 3.0f;
	constexpr float MovementSmokeMoveDurationSeconds = 5.0f;
	constexpr float MovementSmokeTimelineDurationSeconds = 32.0f;
	constexpr float PresentationPerturbationStrengthRelaxationMultiplier = 0.72f;
	constexpr float PresentationPerturbationDampingRatioRelaxationMultiplier = 0.78f;
	constexpr float PresentationPerturbationExtraDampingRelaxationMultiplier = 0.74f;

	TAutoConsoleVariable<float> CVarPhysAnimActionScale(
		TEXT("physanim.ActionScale"),
		-1.0f,
		TEXT("Override for PhysAnim action scale. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimActionClampAbs(
		TEXT("physanim.ActionClampAbs"),
		-1.0f,
		TEXT("Override for PhysAnim absolute action clamp. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimActionSmoothingAlpha(
		TEXT("physanim.ActionSmoothingAlpha"),
		-1.0f,
		TEXT("Override for PhysAnim action smoothing alpha. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimStartupRampSeconds(
		TEXT("physanim.StartupRampSeconds"),
		-1.0f,
		TEXT("Override for PhysAnim startup ramp duration in seconds. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimPolicyControlRateHz(
		TEXT("physanim.PolicyControlRateHz"),
		-1.0f,
		TEXT("Override for the fixed PhysAnim policy/control update rate in Hz. Negative values keep the component default."));
	TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedMassScales(
		TEXT("physanim.ApplyTrainingAlignedMassScales"),
		-1,
		TEXT("Override for the Stage 1 training-aligned Manny family mass policy. -1 keeps the component default, 0 disables, 1 enables."));
	TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedMassScaleBlend(
		TEXT("physanim.TrainingAlignedMassScaleBlend"),
		-1.0f,
		TEXT("Override for the Stage 1 training-aligned Manny family mass policy blend. Negative values keep the component default."));
	TAutoConsoleVariable<int32> CVarPhysAnimApplyTrainingAlignedControlFamilyProfile(
		TEXT("physanim.ApplyTrainingAlignedControlFamilyProfile"),
		-1,
		TEXT("Override for the Stage 1 training-aligned control family profile. -1 keeps the component default, 0 disables, 1 enables."));
	TAutoConsoleVariable<float> CVarPhysAnimTrainingAlignedControlFamilyProfileBlend(
		TEXT("physanim.TrainingAlignedControlFamilyProfileBlend"),
		-1.0f,
		TEXT("Override for the Stage 1 training-aligned control family profile blend. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimMaxAngularStepDegPerSec(
		TEXT("physanim.MaxAngularStepDegPerSec"),
		-1.0f,
		TEXT("Override for PhysAnim maximum target rotation step in degrees per second. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimAngularStrengthMultiplier(
		TEXT("physanim.AngularStrengthMultiplier"),
		-1.0f,
		TEXT("Override for PhysAnim angular strength multiplier. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimAngularDampingRatioMultiplier(
		TEXT("physanim.AngularDampingRatioMultiplier"),
		-1.0f,
		TEXT("Override for PhysAnim angular damping ratio multiplier. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimAngularExtraDampingMultiplier(
		TEXT("physanim.AngularExtraDampingMultiplier"),
		-1.0f,
		TEXT("Override for PhysAnim angular extra damping multiplier. Negative values keep the component default."));
	TAutoConsoleVariable<int32> CVarPhysAnimUseSkeletalAnimationTargets(
		TEXT("physanim.UseSkeletalAnimationTargets"),
		-1,
		TEXT("Override for PhysAnim skeletal-animation target blending. -1 keeps the component default, 0 disables, 1 enables."));
	TAutoConsoleVariable<int32> CVarPhysAnimForceZeroActions(
		TEXT("physanim.ForceZeroActions"),
		-1,
		TEXT("Override for PhysAnim zero-action mode. -1 keeps the component default, 0 disables, 1 forces zero actions."));
	TAutoConsoleVariable<int32> CVarPhysAnimLogActionDiagnostics(
		TEXT("physanim.LogActionDiagnostics"),
		-1,
		TEXT("Override for PhysAnim action diagnostics logging. -1 keeps the component default, 0 disables, 1 enables."));
	TAutoConsoleVariable<float> CVarPhysAnimMaxRootHeightDeltaCm(
		TEXT("physanim.MaxRootHeightDeltaCm"),
		-1.0f,
		TEXT("Override for PhysAnim runtime root height delta fail-stop threshold in cm. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimMaxRootLinearSpeedCmPerSec(
		TEXT("physanim.MaxRootLinearSpeedCmPerSec"),
		-1.0f,
		TEXT("Override for PhysAnim runtime root linear speed fail-stop threshold in cm/s. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimMaxRootAngularSpeedDegPerSec(
		TEXT("physanim.MaxRootAngularSpeedDegPerSec"),
		-1.0f,
		TEXT("Override for PhysAnim runtime root angular speed fail-stop threshold in deg/s. Negative values keep the component default."));
	TAutoConsoleVariable<float> CVarPhysAnimInstabilityGracePeriodSeconds(
		TEXT("physanim.InstabilityGracePeriodSeconds"),
		-1.0f,
		TEXT("Override for PhysAnim runtime instability grace period in seconds. Negative values keep the component default."));
	TAutoConsoleVariable<int32> CVarPhysAnimEnableInstabilityFailStop(
		TEXT("physanim.EnableInstabilityFailStop"),
		-1,
		TEXT("Override for PhysAnim runtime instability fail-stop. -1 keeps the component default, 0 disables, 1 enables."));
	TAutoConsoleVariable<int32> CVarPhysAnimLogBridgeStateSnapshots(
		TEXT("physanim.LogBridgeStateSnapshots"),
		1,
		TEXT("Whether PhysAnim emits startup and fail-stop bridge state snapshots."));
	TAutoConsoleVariable<int32> CVarPhysAnimMovementSmokeMode(
		TEXT("physanim.MovementSmokeMode"),
		0,
		TEXT("Enables the deterministic PIE movement smoke mode that preserves the gameplay shell and applies scripted WASD-equivalent input."));
	TAutoConsoleVariable<int32> CVarPhysAnimMovementSmokeLoopCount(
		TEXT("physanim.MovementSmokeLoopCount"),
		1,
		TEXT("How many times the deterministic PIE movement smoke timeline repeats before completing."));
	TAutoConsoleVariable<int32> CVarPhysAnimAllowCharacterMovementInBridgeActive(
		TEXT("physanim.AllowCharacterMovementInBridgeActive"),
		1,
		TEXT("When enabled, BridgeActive preserves capsule collision and CharacterMovement so the player can drive the character with normal gameplay input."));
	TAutoConsoleVariable<int32> CVarPhysAnimShowBridgeStatusIndicator(
		TEXT("physanim.ShowBridgeStatusIndicator"),
		1,
		TEXT("Whether PhysAnim shows an always-visible on-screen bridge status indicator."));
	TAutoConsoleVariable<int32> CVarPaStabilizationStressTest(
		TEXT("pa.StabilizationStressTest"),
		0,
		TEXT("Enable the idle stabilization stress-test ramp. 0 disables, 1 linearly relaxes all angular stabilization gains after the bridge is fully settled."));
	TAutoConsoleVariable<float> CVarPaStabilizationStressTestRampSeconds(
		TEXT("pa.StabilizationStressTestRampSeconds"),
		45.0f,
		TEXT("Duration in seconds for the stabilization stress-test gain ramp from 1.0 to 0.0. Values <= 0 clamp the ramp immediately to zero."));
	TAutoConsoleVariable<int32> CVarPaStabilizationStressTestProfile(
		TEXT("pa.StabilizationStressTestProfile"),
		0,
		TEXT("Stabilization stress-test profile. 0 = ramp down only, 1 = ramp down / hold / ramp back up."));
	TAutoConsoleVariable<float> CVarPaStabilizationStressTestTargetMultiplier(
		TEXT("pa.StabilizationStressTestTargetMultiplier"),
		0.0f,
		TEXT("Target floor multiplier for the stabilization stress-test profile. Used as the hold floor for down/hold/up mode."));
	TAutoConsoleVariable<float> CVarPaStabilizationStressTestHoldSeconds(
		TEXT("pa.StabilizationStressTestHoldSeconds"),
		3.0f,
		TEXT("Hold duration in seconds for the stabilization stress-test recovery profile."));
	TAutoConsoleVariable<float> CVarPaStabilizationStressTestRecoveryRampSeconds(
		TEXT("pa.StabilizationStressTestRecoveryRampSeconds"),
		5.0f,
		TEXT("Ramp-up duration in seconds for the stabilization stress-test recovery profile."));
	TAutoConsoleVariable<int32> CVarPaStabilizationStressTestSweepMode(
		TEXT("pa.StabilizationStressTestSweepMode"),
		0,
		TEXT("Which stabilization parameter the stress-test ramps. 0 = all, 1 = strength only, 2 = damping ratio only, 3 = extra damping only."));
	TAutoConsoleVariable<float> CVarPaStabilizationStressTestAngularSpikeThreshold(
		TEXT("pa.StabilizationStressTestAngularSpikeThresholdDegPerSec"),
		500.0f,
		TEXT("Angular velocity threshold used to mark the first stress-test per-bone spike."));
	TAutoConsoleVariable<float> CVarPaStabilizationStressTestLinearSpikeThreshold(
		TEXT("pa.StabilizationStressTestLinearSpikeThresholdCmPerSec"),
		100.0f,
		TEXT("Linear velocity threshold used to mark the first stress-test per-bone spike."));

	float ResolveFloatOverride(const TAutoConsoleVariable<float>& CVar, float DefaultValue)
	{
		const float OverrideValue = CVar.GetValueOnGameThread();
		return OverrideValue >= 0.0f ? OverrideValue : DefaultValue;
	}

	bool ResolveBoolOverride(const TAutoConsoleVariable<int32>& CVar, bool DefaultValue)
	{
		const int32 OverrideValue = CVar.GetValueOnGameThread();
		if (OverrideValue < 0)
		{
			return DefaultValue;
		}

		return OverrideValue != 0;
	}

	FString BuildStabilizationSummary(const FPhysAnimStabilizationSettings& Settings)
	{
		return FString::Printf(
			TEXT("Zero=%s Scale=%.3f Clamp=%.3f Smooth=%.3f Ramp=%.3f PolicyHz=%.1f MassPolicy=%s MassBlend=%.2f FamilyPd=%s FamilyPdBlend=%.2f StepDegPerSec=%.1f GainMul=%.3f DampMul=%.3f ExtraDampMul=%.3f SkeletalTargets=%s InstabilityStop=%s HeightCm=%.1f LinCmPerSec=%.1f AngDegPerSec=%.1f Grace=%.2f"),
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

	FString JoinNames(const TArray<FName>& Names)
	{
		TArray<FString> Strings;
		Strings.Reserve(Names.Num());
		for (const FName Name : Names)
		{
			Strings.Add(Name.ToString());
		}
		return FString::Join(Strings, TEXT(", "));
	}

	bool ValidateInitialPhysicsControlAuthoring(
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

UPhysAnimComponent::UPhysAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	ModelDataAsset = TSoftObjectPtr<UNNEModelData>(FSoftObjectPath(PhysAnimComponentInternal::DefaultModelPath));
}

bool UPhysAnimComponent::BuildConditionedActions(
	const TArray<float>& RawActions,
	const TArray<float>* PreviousConditionedActions,
	const FPhysAnimActionConditioningSettings& Settings,
	TArray<float>& OutConditionedActions,
	FPhysAnimActionDiagnostics& OutDiagnostics,
	FString& OutError)
{
	return PhysAnimBridge::ConditionModelActions(
		RawActions,
		PreviousConditionedActions,
		Settings,
		OutConditionedActions,
		OutDiagnostics,
		OutError);
}

FQuat UPhysAnimComponent::LimitTargetRotationStep(
	const FQuat& PreviousRotation,
	const FQuat& TargetRotation,
	float MaxAngularStepDegrees)
{
	return PhysAnimBridge::LimitControlRotationStep(PreviousRotation, TargetRotation, MaxAngularStepDegrees);
}

bool UPhysAnimComponent::EvaluateRuntimeInstability(
	const FVector& RootLocationCm,
	const FVector& RootLinearVelocityCmPerSecond,
	const FVector& RootAngularVelocityDegPerSecond,
	float DeltaTimeSeconds,
	const FPhysAnimRuntimeInstabilitySettings& Settings,
	FPhysAnimRuntimeInstabilityState& InOutState,
	FPhysAnimRuntimeInstabilityDiagnostics& OutDiagnostics,
	FString& OutError)
{
	return PhysAnimBridge::UpdateRuntimeInstabilityState(
		RootLocationCm,
		RootLinearVelocityCmPerSecond,
		RootAngularVelocityDegPerSecond,
		DeltaTimeSeconds,
		Settings,
		InOutState,
		OutDiagnostics,
		OutError);
}

void UPhysAnimComponent::BeginPlay()
{
	Super::BeginPlay();
	StartBridge();
}

void UPhysAnimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBridge();
	Super::EndPlay(EndPlayReason);
}

void UPhysAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	UpdateBridgeStatusIndicator(0.25f);

	if (RuntimeState != EPhysAnimRuntimeState::WaitingForPoseSearch &&
		RuntimeState != EPhysAnimRuntimeState::ReadyForActivation &&
		RuntimeState != EPhysAnimRuntimeState::BridgeActive)
	{
		return;
	}

	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	if (!PhysicsControl || !SkeletalMesh || !LocalAnimInstance)
	{
		FailStop(TEXT("Runtime context became invalid after startup."));
		return;
	}

	UpdateStabilizationStressTestState(StabilizationSettings);
	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	ApplyMovementSmokeInput(EffectiveSettings);
	FString TickError;
	FPoseSearchBlueprintResult SearchResult;
	if (RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch)
	{
		if (!QueryPoseSearch(SearchResult, TickError))
		{
			const double WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
			const double WaitSeconds = WorldTimeSeconds - InitialPoseSearchWaitStartTimeSeconds;
			if (IsInitialPoseSearchWaitTimedOut(WaitSeconds, PhysAnimComponentInternal::InitialPoseSearchWaitTimeoutSeconds))
			{
				FailStop(FString::Printf(TEXT("Initial PoseSearch result was never produced within %.2fs. %s"), PhysAnimComponentInternal::InitialPoseSearchWaitTimeoutSeconds, *TickError));
			}
			return;
		}

		LastValidPoseSearchResult = SearchResult;
		ConsecutiveInvalidPoseSearchFrames = 0;
		if (ResolveInitialPoseSearchSuccessState(EffectiveSettings.bForceZeroActions) == EPhysAnimRuntimeState::ReadyForActivation)
		{
			EnterReadyForActivation(EffectiveSettings, TEXT("StartupReadyForActivation"), true);
			return;
		}

		if (!ActivateBridgeFromReadyState(EffectiveSettings, TEXT("StartupActivation"), TickError))
		{
			FailStop(TickError);
			return;
		}
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Startup success. Runtime=%s Model=%s"),
			*ActiveRuntimeName,
			*GetPathNameSafe(LoadedModelData));
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Stabilization %s"), *PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings));
		return;
	}

	if (ShouldDeactivateBridgeToSafeMode(RuntimeState, EffectiveSettings.bForceZeroActions))
	{
		DeactivateRuntimePhysicsControl(TEXT("ReadyForActivation"));
		ResetBridgePhysicsState();
		EnterReadyForActivation(EffectiveSettings, TEXT("AfterDeferredDeactivation"), false);
		return;
	}

	if (ShouldActivateBridgeFromSafeMode(RuntimeState, EffectiveSettings.bForceZeroActions))
	{
		if (!ActivateBridgeFromReadyState(EffectiveSettings, TEXT("DeferredActivation"), TickError))
		{
			FailStop(TickError);
			return;
		}

		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Deferred activation complete. Runtime=%s Model=%s"),
			*ActiveRuntimeName,
			*GetPathNameSafe(LoadedModelData));
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Stabilization %s"), *PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings));
		return;
	}

	if (RuntimeState == EPhysAnimRuntimeState::ReadyForActivation)
	{
		return;
	}

	const float PreviousSimulationHandoffAlpha = SimulationHandoffAlpha;
	SimulationHandoffAlpha = CalculateSimulationHandoffAlpha(EffectiveSettings);
	const bool bSimulationHandoffCompletedThisTick =
		PreviousSimulationHandoffAlpha < 1.0f && SimulationHandoffAlpha >= 1.0f;
	if (bSimulationHandoffCompletedThisTick)
	{
		SimulationHandoffCompletedTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
		UnlockBringUpGroup(0, TEXT("SimulationHandoff"));
	}

	PhysicsControl->UpdateTargetCaches(DeltaTime);
	PhysicsControl->GetCachedBoneTransforms(SkeletalMesh, PhysAnimBridge::GetControlledBoneNames());

	const float PolicyControlIntervalSeconds = ResolvePolicyControlIntervalSeconds(EffectiveSettings.PolicyControlRateHz);
	int32 ElapsedPolicySteps = 0;
	const bool bRunPolicyUpdateThisTick = AdvancePolicyControlAccumulator(
		DeltaTime,
		PolicyControlIntervalSeconds,
		PolicyUpdateAccumulatorSeconds,
		ElapsedPolicySteps);
	LastPolicyElapsedSteps = ElapsedPolicySteps;
	if (bRunPolicyUpdateThisTick)
	{
		++PolicyControlTicksExecuted;
		PolicyControlTicksSkipped += FMath::Max(ElapsedPolicySteps - 1, 0);
		LastPolicyControlUpdateTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;

		if (QueryPoseSearch(SearchResult, TickError))
		{
			LastValidPoseSearchResult = SearchResult;
			ConsecutiveInvalidPoseSearchFrames = 0;
		}
		else
		{
			++ConsecutiveInvalidPoseSearchFrames;
			if (ConsecutiveInvalidPoseSearchFrames > 1 || LastValidPoseSearchResult.SelectedAnim == nullptr)
			{
				FailStop(FString::Printf(TEXT("PoseSearch query was invalid for two consecutive policy ticks. %s"), *TickError));
				return;
			}

			UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] Reusing last valid PoseSearch result for one grace policy tick. Reason: %s"), *TickError);
			SearchResult = LastValidPoseSearchResult;
		}

		TArray<FPhysAnimBodySample> CurrentBodySamples;
		if (!GatherCurrentBodySamples(CurrentBodySamples, TickError))
		{
			FailStop(TickError);
			return;
		}

		TArray<FPhysAnimFuturePoseSample> FuturePoseSamples;
		if (!SampleFuturePoses(SearchResult, FuturePoseSamples, TickError))
		{
			FailStop(TickError);
			return;
		}

		if (!PhysAnimBridge::BuildSelfObservation(CurrentBodySamples, 0.0f, SelfObservationBuffer, TickError))
		{
			FailStop(TickError);
			return;
		}

		if (!PhysAnimBridge::BuildMimicTargetPoses(CurrentBodySamples, FuturePoseSamples, MimicTargetPosesBuffer, TickError))
		{
			FailStop(TickError);
			return;
		}

		PhysAnimBridge::BuildZeroTerrain(TerrainBuffer);

		if (!RunInference(TickError))
		{
			FailStop(TickError);
			return;
		}
	}

	ApplyRuntimeControlTuning(EffectiveSettings);
	if (!PendingBodyModifierCachedResetNames.IsEmpty())
	{
		ResetPendingBodyModifiersToCachedTargets();
	}
	if (bRunPolicyUpdateThisTick && !ConditionModelActions(EffectiveSettings, TickError))
	{
		FailStop(TickError);
		return;
	}

	ApplyControlTargets(
		bRunPolicyUpdateThisTick
			? (PolicyControlIntervalSeconds * FMath::Max(ElapsedPolicySteps, 1))
			: 0.0f,
		EffectiveSettings,
		bRunPolicyUpdateThisTick,
		TickError);
	if (!TickError.IsEmpty())
	{
		FailStop(TickError);
		return;
	}

	PhysicsControl->UpdateControls(DeltaTime);
	if (!CheckRuntimeInstability(DeltaTime, EffectiveSettings, TickError))
	{
		FailStop(TickError);
		return;
	}
	TrackStabilizationStressTestObservations();
	AdvanceBringUpState(DeltaTime, EffectiveSettings);
	if (bSimulationHandoffCompletedThisTick)
	{
		LogBridgeStateSnapshot(TEXT("AfterSimulationHandoff"));
		LogBodyModifierTelemetrySnapshot(TEXT("AfterSimulationHandoff"));
		LogActivationSummary(EffectiveSettings, TEXT("SimulationHandoffComplete"), true, true, SimulationHandoffAlpha);
	}
	MaybeLogRuntimeDiagnostics(EffectiveSettings);
}

bool UPhysAnimComponent::StartBridge()
{
	if (RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
		RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		RuntimeState == EPhysAnimRuntimeState::ReadyForActivation)
	{
		return true;
	}

	FString Error;
	if (!ResolveRuntimeContext(Error))
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Startup blocked: %s"), *Error);
		SetComponentTickEnabled(false);
		TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
		return false;
	}

	DeactivateRuntimePhysicsControl(TEXT("StartupReset"));

	if (!ValidateRequiredBodies(Error) ||
		!ValidatePhysicsControlAuthoring(Error) ||
		!ValidatePoseSearchIntegration(Error) ||
		!InitializeModel(Error))
	{
		UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Startup blocked: %s"), *Error);
		SetComponentTickEnabled(false);
		TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
		return false;
	}

	bStartupReported = true;
	SetComponentTickEnabled(true);
	TransitionRuntimeState(EPhysAnimRuntimeState::RuntimeReady);
	TransitionRuntimeState(EPhysAnimRuntimeState::WaitingForPoseSearch);
	InitialPoseSearchWaitStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	ConsecutiveInvalidPoseSearchFrames = 0;
	LastValidPoseSearchResult = FPoseSearchBlueprintResult();
	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Startup pending initial PoseSearch result before taking physics ownership."));
	UpdateBridgeStatusIndicator(1.0f);
	return true;
}

bool UPhysAnimComponent::ValidatePhysicsControlAuthoring(FString& OutError) const
{
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutError = TEXT("Physics Control authoring validation requires an owning actor.");
		return false;
	}

	if (const UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
	{
		return PhysAnimComponentInternal::ValidateInitialPhysicsControlAuthoring(
			Stage1Initializer->InitialControls,
			Stage1Initializer->InitialBodyModifiers,
			OutError);
	}

	if (const UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
	{
		return PhysAnimComponentInternal::ValidateInitialPhysicsControlAuthoring(
			Initializer->InitialControls,
			Initializer->InitialBodyModifiers,
			OutError);
	}

	OutError = TEXT("Owning actor is missing a Stage 1 Physics Control initializer.");
	return false;
}

void UPhysAnimComponent::StopBridge()
{
	DeactivateRuntimePhysicsControl(TEXT("StopBridge"));
	ResetBridgePhysicsState();
	TransitionRuntimeState(EPhysAnimRuntimeState::Uninitialized);
	UpdateBridgeStatusIndicator(5.0f);
	SetComponentTickEnabled(false);
	ConsecutiveInvalidPoseSearchFrames = 0;
	LastValidPoseSearchResult = FPoseSearchBlueprintResult();
	ResetStabilizationRuntimeState();
}

bool UPhysAnimComponent::IsReadyForScriptedPresentation() const
{
	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive)
	{
		return false;
	}

	if (!AreAllBringUpGroupsUnlocked())
	{
		return false;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	return CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings) >= (1.0f - KINDA_SMALL_NUMBER);
}

void UPhysAnimComponent::SetPresentationPerturbationOverrideSeconds(float DurationSeconds)
{
	if (DurationSeconds <= 0.0f)
	{
		ClearPresentationPerturbationOverride();
		return;
	}

	const UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	PresentationPerturbationOverrideEndTimeSeconds =
		FMath::Max(PresentationPerturbationOverrideEndTimeSeconds, World->GetTimeSeconds() + DurationSeconds);
}

void UPhysAnimComponent::ClearPresentationPerturbationOverride()
{
	PresentationPerturbationOverrideEndTimeSeconds = -1.0;
}

void UPhysAnimComponent::UpdateStabilizationStressTestState(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	const bool bStressTestEnabled = PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() > 0;
	if (!bStressTestEnabled)
	{
		StabilizationStressTestStartTimeSeconds = -1.0;
		bStabilizationStressTestCompletionLogged = false;
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	if (RuntimeState != EPhysAnimRuntimeState::BridgeActive || !AreAllBringUpGroupsUnlocked())
	{
		return;
	}

	FPhysAnimStabilizationSettings PolicyRampSettings = EffectiveSettings;
	PolicyRampSettings.bForceZeroActions =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimForceZeroActions,
			PolicyRampSettings.bForceZeroActions);
	PolicyRampSettings.StartupRampSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimStartupRampSeconds,
			PolicyRampSettings.StartupRampSeconds);
	if (CalculateCurrentPolicyInfluenceAlpha(PolicyRampSettings) < (1.0f - KINDA_SMALL_NUMBER))
	{
		return;
	}

	if (StabilizationStressTestStartTimeSeconds < 0.0)
	{
		StabilizationStressTestStartTimeSeconds = World->GetTimeSeconds();
		bStabilizationStressTestCompletionLogged = false;
		StabilizationStressTestFirstAngularSpikeTimeSeconds = -1.0;
		StabilizationStressTestFirstLinearSpikeTimeSeconds = -1.0;
		StabilizationStressTestFirstInstabilitySignTimeSeconds = -1.0;
		StabilizationStressTestFirstAngularSpikeMultiplier = 1.0f;
		StabilizationStressTestFirstLinearSpikeMultiplier = 1.0f;
		StabilizationStressTestFirstInstabilityMultiplier = 1.0f;
		StabilizationStressTestFirstAngularSpikeBoneName = NAME_None;
		StabilizationStressTestFirstLinearSpikeBoneName = NAME_None;
		if (const AActor* const OwnerActor = GetOwner())
		{
			StabilizationStressTestBaselineActorLocation = OwnerActor->GetActorLocation();
		}
		if (const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get())
		{
			StabilizationStressTestBaselineSpineLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("spine_01")) - StabilizationStressTestBaselineActorLocation;
			StabilizationStressTestBaselineHeadLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("head")) - StabilizationStressTestBaselineActorLocation;
			StabilizationStressTestBaselineLeftFootLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("foot_l")) - StabilizationStressTestBaselineActorLocation;
			StabilizationStressTestBaselineRightFootLocalOffset =
				SkeletalMesh->GetBoneLocation(TEXT("foot_r")) - StabilizationStressTestBaselineActorLocation;
		}
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test started: profile=%d sweep=%d rampSeconds=%.1f targetMultiplier=%.2f holdSeconds=%.1f recoveryRampSeconds=%.1f baseStrength=%.2f baseDampingRatio=%.2f baseExtraDamping=%.2f"),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestProfile.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestSweepMode.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestRampSeconds.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestTargetMultiplier.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestHoldSeconds.GetValueOnGameThread(),
			PhysAnimComponentInternal::CVarPaStabilizationStressTestRecoveryRampSeconds.GetValueOnGameThread(),
			PolicyRampSettings.AngularStrengthMultiplier,
			PolicyRampSettings.AngularDampingRatioMultiplier,
			PolicyRampSettings.AngularExtraDampingMultiplier);
	}

	const float StressMultiplier = ResolveStabilizationStressTestMultiplier();
	if (!bStabilizationStressTestCompletionLogged && StressMultiplier <= KINDA_SMALL_NUMBER)
	{
		bStabilizationStressTestCompletionLogged = true;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test reached zero gain multiplier after %.2fs."),
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds);
	}
}

float UPhysAnimComponent::ResolveStabilizationStressTestMultiplier() const
{
	if (PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() <= 0 ||
		StabilizationStressTestStartTimeSeconds < 0.0)
	{
		return 1.0f;
	}

	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : StabilizationStressTestStartTimeSeconds;
	const float ElapsedSeconds = static_cast<float>(FMath::Max(CurrentTimeSeconds - StabilizationStressTestStartTimeSeconds, 0.0));
	return CalculateStabilizationStressTestMultiplier(
		PhysAnimComponentInternal::CVarPaStabilizationStressTestProfile.GetValueOnGameThread(),
		ElapsedSeconds,
		PhysAnimComponentInternal::CVarPaStabilizationStressTestRampSeconds.GetValueOnGameThread(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestTargetMultiplier.GetValueOnGameThread(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestHoldSeconds.GetValueOnGameThread(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestRecoveryRampSeconds.GetValueOnGameThread());
}

void UPhysAnimComponent::TrackStabilizationStressTestObservations()
{
	if (PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() <= 0 ||
		StabilizationStressTestStartTimeSeconds < 0.0)
	{
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	const float StressMultiplier = ResolveStabilizationStressTestMultiplier();
	const float AngularSpikeThresholdDegPerSec =
		PhysAnimComponentInternal::CVarPaStabilizationStressTestAngularSpikeThreshold.GetValueOnGameThread();
	const float LinearSpikeThresholdCmPerSec =
		PhysAnimComponentInternal::CVarPaStabilizationStressTestLinearSpikeThreshold.GetValueOnGameThread();

	if (StabilizationStressTestFirstAngularSpikeTimeSeconds < 0.0 &&
		LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond >= AngularSpikeThresholdDegPerSec)
	{
		StabilizationStressTestFirstAngularSpikeTimeSeconds = World->GetTimeSeconds();
		StabilizationStressTestFirstAngularSpikeMultiplier = StressMultiplier;
		StabilizationStressTestFirstAngularSpikeBoneName = LastRuntimeInstabilityDiagnostics.MaxAngularSpeedBoneName;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test first angular spike: bone=%s multiplier=%.2f elapsed=%.2fs angularDegPerSec=%.1f"),
			*StabilizationStressTestFirstAngularSpikeBoneName.ToString(),
			StressMultiplier,
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds,
			LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond);
	}

	if (StabilizationStressTestFirstLinearSpikeTimeSeconds < 0.0 &&
		LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond >= LinearSpikeThresholdCmPerSec)
	{
		StabilizationStressTestFirstLinearSpikeTimeSeconds = World->GetTimeSeconds();
		StabilizationStressTestFirstLinearSpikeMultiplier = StressMultiplier;
		StabilizationStressTestFirstLinearSpikeBoneName = LastRuntimeInstabilityDiagnostics.MaxLinearSpeedBoneName;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test first linear spike: bone=%s multiplier=%.2f elapsed=%.2fs linearCmPerSec=%.1f"),
			*StabilizationStressTestFirstLinearSpikeBoneName.ToString(),
			StressMultiplier,
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds,
			LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond);
	}

	if (StabilizationStressTestFirstInstabilitySignTimeSeconds < 0.0 &&
		RuntimeInstabilityState.UnstableAccumulatedSeconds > 0.0f)
	{
		StabilizationStressTestFirstInstabilitySignTimeSeconds = World->GetTimeSeconds();
		StabilizationStressTestFirstInstabilityMultiplier = StressMultiplier;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Stabilization stress-test first instability sign: multiplier=%.2f elapsed=%.2fs rootHeightDeltaCm=%.1f rootLinearCmPerSec=%.1f rootAngularDegPerSec=%.1f"),
			StressMultiplier,
			World->GetTimeSeconds() - StabilizationStressTestStartTimeSeconds,
			LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm,
			LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond,
			LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond);
	}
}

bool UPhysAnimComponent::ActivateBridgeFromReadyState(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* ActivationContext,
	FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Bridge activation requires a valid Physics Control component.");
		return false;
	}

	if (!ActivateRuntimePhysicsControl(OutError))
	{
		return false;
	}

	TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
	LogBridgeStateSnapshot(TEXT("BeforeActivateBridgePhysicsState"));
	ActivateBridgePhysicsState(EffectiveSettings);
	LogBridgeStateSnapshot(TEXT("AfterActivateBridgePhysicsState"));
	ResetStabilizationRuntimeState();
	BridgeStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	if (const AActor* const OwnerActor = GetOwner())
	{
		MovementSmokeStartLocation = OwnerActor->GetActorLocation();
	}
	SimulationHandoffAlpha = CalculateSimulationHandoffAlpha(EffectiveSettings);
	PhysicsControl->UpdateTargetCaches(0.0f);
	if (!SeedControlTargetsFromCurrentPose(0.0f, OutError))
	{
		return false;
	}
	ApplyRuntimeControlTuning(EffectiveSettings);
	PhysicsControl->UpdateControls(0.0f);
	LogBridgeStateSnapshot(TEXT("AfterActivationPrepass"));
	LogActivationSummary(EffectiveSettings, ActivationContext, true, true, SimulationHandoffAlpha);

	UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Bridge physics activation[%s] complete."), ActivationContext);
	return true;
}

void UPhysAnimComponent::EnterReadyForActivation(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* Context,
	bool bLogDeferredStartupSuccess)
{
	ResetStabilizationRuntimeState();
	TransitionRuntimeState(EPhysAnimRuntimeState::ReadyForActivation);
	LogBridgeStateSnapshot(Context);

	if (bLogDeferredStartupSuccess)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Startup success. Runtime=%s Model=%s DeferredActivation=true"),
			*ActiveRuntimeName,
			*GetPathNameSafe(LoadedModelData));
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Bridge physics activation deferred by zero-action safe mode."));
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnim] Stabilization %s"), *PhysAnimComponentInternal::BuildStabilizationSummary(EffectiveSettings));
	}
}

bool UPhysAnimComponent::SeedControlTargetsFromCurrentPose(float DeltaTime, FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!PhysicsControl || !SkeletalMesh || !OwnerActor)
	{
		OutError = TEXT("Current-pose target seeding requires the owner, skeletal mesh, and Physics Control component.");
		return false;
	}

	auto FindInitialControl = [OwnerActor](const FName ControlName) -> const FInitialPhysicsControl*
	{
		if (const UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
		{
			return Stage1Initializer->InitialControls.Find(ControlName);
		}

		if (const UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
		{
			return Initializer->InitialControls.Find(ControlName);
		}

		return nullptr;
	};

	PreviousControlTargetRotations.Reset();

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		if (!PhysicsControl->GetControlExists(ControlName))
		{
			OutError = FString::Printf(TEXT("Missing required control '%s' while seeding current-pose targets."), *ControlName.ToString());
			return false;
		}

		const FInitialPhysicsControl* const InitialControl = FindInitialControl(ControlName);
		if (!InitialControl)
		{
			OutError = FString::Printf(TEXT("Missing authored control definition for '%s' while seeding current-pose targets."), *ControlName.ToString());
			return false;
		}

		if (SkeletalMesh->GetBoneIndex(InitialControl->ParentBoneName) == INDEX_NONE ||
			SkeletalMesh->GetBoneIndex(InitialControl->ChildBoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(
				TEXT("Could not resolve current-pose bones for control '%s' (parent='%s', child='%s')."),
				*ControlName.ToString(),
				*InitialControl->ParentBoneName.ToString(),
				*InitialControl->ChildBoneName.ToString());
			return false;
		}

		const FQuat ParentWorldRotation =
			SkeletalMesh->GetBoneQuaternion(InitialControl->ParentBoneName, EBoneSpaces::WorldSpace);
		const FQuat ChildWorldRotation =
			SkeletalMesh->GetBoneQuaternion(InitialControl->ChildBoneName, EBoneSpaces::WorldSpace);
		const FQuat TargetOrientation =
			BuildCurrentPoseControlTargetOrientation(ParentWorldRotation, ChildWorldRotation);

		PreviousControlTargetRotations.Add(ControlName, TargetOrientation);
		PhysicsControl->SetControlTargetOrientation(
			ControlName,
			TargetOrientation.Rotator(),
			DeltaTime,
			true,
			false,
			true,
			false);
	}

	return true;
}

bool UPhysAnimComponent::ResolveRuntimeContext(FString& OutError)
{
	AActor* const OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		OutError = TEXT("PhysAnimComponent has no owning actor.");
		return false;
	}

	USkeletalMeshComponent* const SkeletalMesh = OwnerActor->FindComponentByClass<USkeletalMeshComponent>();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Owning actor is missing a skeletal mesh component.");
		return false;
	}

	UPhysicsControlComponent* const PhysicsControl = OwnerActor->FindComponentByClass<UPhysicsControlComponent>();
	if (!PhysicsControl)
	{
		OutError = TEXT("Owning actor is missing a pre-authored Physics Control component.");
		return false;
	}

	UAnimInstance* const LocalAnimInstance = SkeletalMesh->GetAnimInstance();
	if (!LocalAnimInstance)
	{
		OutError = TEXT("The live AnimInstance was not resolved from the skeletal mesh.");
		return false;
	}

	MeshComponent = SkeletalMesh;
	PhysicsControlComponent = PhysicsControl;
	this->AnimInstance = LocalAnimInstance;

	AddTickPrerequisiteComponent(SkeletalMesh);
	PhysicsControl->SetComponentTickEnabled(false);
	return true;
}

bool UPhysAnimComponent::ValidateRequiredBodies(FString& OutError) const
{
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved.");
		return false;
	}

	const FString MeshAssetPath = GetPathNameSafe(SkeletalMesh->GetSkeletalMeshAsset());
	if (MeshAssetPath != PhysAnimComponentInternal::ExpectedMeshPath)
	{
		OutError = FString::Printf(TEXT("Expected Manny mesh '%s' but found '%s'."), PhysAnimComponentInternal::ExpectedMeshPath, *MeshAssetPath);
		return false;
	}

	const UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	const FString PhysicsAssetPath = PhysicsAsset ? PhysicsAsset->GetPathName() : FString();
	if (PhysicsAssetPath != PhysAnimComponentInternal::ExpectedPhysicsAssetPath)
	{
		OutError = FString::Printf(TEXT("Expected physics asset '%s' but found '%s'."), PhysAnimComponentInternal::ExpectedPhysicsAssetPath, *PhysicsAssetPath);
		return false;
	}

	TArray<FName> MissingBodies;
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		if (SkeletalMesh->GetBoneIndex(BoneName) == INDEX_NONE || SkeletalMesh->GetBodyInstance(BoneName) == nullptr)
		{
			MissingBodies.Add(BoneName);
		}
	}

	if (MissingBodies.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Missing required physics bodies: %s"), *PhysAnimComponentInternal::JoinNames(MissingBodies));
		return false;
	}

	return true;
}

bool UPhysAnimComponent::ValidateRuntimePhysicsControl(FString& OutError) const
{
	const UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Physics Control component was not resolved.");
		return false;
	}

	TArray<FName> MissingControls;
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		if (!PhysicsControl->GetControlExists(ControlName))
		{
			MissingControls.Add(ControlName);
		}
	}

	TArray<FName> MissingModifiers;
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			MissingModifiers.Add(ModifierName);
		}
	}

	if (MissingControls.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Missing required runtime controls: %s"), *PhysAnimComponentInternal::JoinNames(MissingControls));
		return false;
	}

	if (MissingModifiers.Num() > 0)
	{
		OutError = FString::Printf(TEXT("Missing required runtime body modifiers: %s"), *PhysAnimComponentInternal::JoinNames(MissingModifiers));
		return false;
	}

	return true;
}

bool UPhysAnimComponent::ValidatePoseSearchIntegration(FString& OutError)
{
	const UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	if (!LocalAnimInstance)
	{
		OutError = TEXT("AnimInstance was not resolved.");
		return false;
	}

	const FString AnimClassPath = GetPathNameSafe(LocalAnimInstance->GetClass());
	if (AnimClassPath != PhysAnimComponentInternal::ExpectedAnimBlueprintPath)
	{
		OutError = FString::Printf(TEXT("Expected AnimBlueprint '%s' but found '%s'."), PhysAnimComponentInternal::ExpectedAnimBlueprintPath, *AnimClassPath);
		return false;
	}

	if (UPoseSearchLibrary::FindPoseHistoryNode(PhysAnimComponentInternal::PoseHistoryName, LocalAnimInstance) == nullptr)
	{
		OutError = TEXT("PoseHistory_Stage1 was not found on the live AnimInstance.");
		return false;
	}

	LoadedPoseSearchDatabase = LoadObject<UPoseSearchDatabase>(nullptr, PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath);
	if (!LoadedPoseSearchDatabase)
	{
		OutError = FString::Printf(TEXT("Failed to load PoseSearch database '%s'."), PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath);
		return false;
	}

	if (GetPathNameSafe(LoadedPoseSearchDatabase) != PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath)
	{
		OutError = FString::Printf(
			TEXT("Expected PoseSearch database '%s' but found '%s'."),
			PhysAnimComponentInternal::ExpectedPoseSearchDatabasePath,
			*GetPathNameSafe(LoadedPoseSearchDatabase));
		return false;
	}

	if (GetPathNameSafe(LoadedPoseSearchDatabase->Schema.Get()) != PhysAnimComponentInternal::ExpectedPoseSearchSchemaPath)
	{
		OutError = FString::Printf(
			TEXT("Expected PoseSearch schema '%s' but found '%s'."),
			PhysAnimComponentInternal::ExpectedPoseSearchSchemaPath,
			*GetPathNameSafe(LoadedPoseSearchDatabase->Schema.Get()));
		return false;
	}

	return true;
}

bool UPhysAnimComponent::QueryPoseSearch(FPoseSearchBlueprintResult& OutSearchResult, FString& OutError)
{
	UAnimInstance* const LocalAnimInstance = AnimInstance.Get();
	if (!LocalAnimInstance)
	{
		OutError = TEXT("AnimInstance was not resolved.");
		return false;
	}

	if (!LoadedPoseSearchDatabase)
	{
		OutError = TEXT("PoseSearch database was not loaded.");
		return false;
	}

	TArray<UObject*> AssetsToSearch;
	AssetsToSearch.Add(LoadedPoseSearchDatabase);

	FPoseSearchContinuingProperties ContinuingProperties;
	if (LastValidPoseSearchResult.SelectedAnim != nullptr)
	{
		ContinuingProperties.InitFrom(LastValidPoseSearchResult, EPoseSearchInterruptMode::DoNotInterrupt);
	}

	FPoseSearchFutureProperties FutureProperties;
	OutSearchResult = FPoseSearchBlueprintResult();
	UPoseSearchLibrary::MotionMatch(
		LocalAnimInstance,
		AssetsToSearch,
		PhysAnimComponentInternal::PoseHistoryName,
		ContinuingProperties,
		FutureProperties,
		OutSearchResult);

	if (OutSearchResult.SelectedAnim == nullptr)
	{
		OutError = TEXT("UPoseSearchLibrary::MotionMatch returned no selected animation.");
		return false;
	}

	return true;
}

bool UPhysAnimComponent::InitializeModel(FString& OutError)
{
	LoadedModelData = ModelDataAsset.LoadSynchronous();
	if (!LoadedModelData)
	{
		OutError = FString::Printf(TEXT("Failed to load model asset '%s'."), *ModelDataAsset.ToSoftObjectPath().ToString());
		return false;
	}

	RuntimeGPU = UE::NNE::GetRuntime<INNERuntimeGPU>(PhysAnimComponentInternal::PreferredGpuRuntime);
	if (RuntimeGPU.IsValid() && RuntimeGPU->CanCreateModelGPU(LoadedModelData) == UE::NNE::EResultStatus::Ok)
	{
		ModelGPU = RuntimeGPU->CreateModelGPU(LoadedModelData);
		if (ModelGPU.IsValid())
		{
			ModelInstanceGPU = ModelGPU->CreateModelInstanceGPU();
		}
		if (ModelInstanceGPU.IsValid())
		{
			ActiveRuntimeName = PhysAnimComponentInternal::PreferredGpuRuntime;
			return ValidateModelDescriptorContract(OutError);
		}
	}

	RuntimeCPU = UE::NNE::GetRuntime<INNERuntimeCPU>(PhysAnimComponentInternal::FallbackCpuRuntime);
	if (RuntimeCPU.IsValid() && RuntimeCPU->CanCreateModelCPU(LoadedModelData) == UE::NNE::EResultStatus::Ok)
	{
		ModelCPU = RuntimeCPU->CreateModelCPU(LoadedModelData);
		if (ModelCPU.IsValid())
		{
			ModelInstanceCPU = ModelCPU->CreateModelInstanceCPU();
		}
		if (ModelInstanceCPU.IsValid())
		{
			ActiveRuntimeName = PhysAnimComponentInternal::FallbackCpuRuntime;
			return ValidateModelDescriptorContract(OutError);
		}
	}

	OutError = TEXT("Could not create an NNE model instance from NNERuntimeORTDml or NNERuntimeORTCpu.");
	return false;
}

bool UPhysAnimComponent::ValidateModelDescriptorContract(FString& OutError)
{
	UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync();
	if (!ModelInstance)
	{
		OutError = TEXT("No active model instance exists.");
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorDesc> InputDescsView = ModelInstance->GetInputTensorDescs();
	TArray<UE::NNE::FTensorDesc> InputDescs;
	InputDescs.Append(InputDescsView.GetData(), InputDescsView.Num());

	if (!PhysAnimBridge::BuildInputTensorIndexMap(InputDescs, TensorIndexMap, OutError))
	{
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorDesc> OutputDescs = ModelInstance->GetOutputTensorDescs();
	if (OutputDescs.Num() != 1)
	{
		OutError = FString::Printf(TEXT("Expected exactly one output tensor but found %d."), OutputDescs.Num());
		return false;
	}

	SelfObservationBuffer.Init(0.0f, PhysAnimBridge::SelfObsSize);
	MimicTargetPosesBuffer.Init(0.0f, PhysAnimBridge::MimicTargetPosesSize);
	TerrainBuffer.Init(0.0f, PhysAnimBridge::TerrainSize);
	ActionOutputBuffer.Init(0.0f, PhysAnimBridge::NumActionFloats);

	TArray<UE::NNE::FTensorShape> InputShapes;
	InputShapes.SetNum(InputDescs.Num());
	InputShapes[TensorIndexMap.SelfObs] = UE::NNE::FTensorShape::Make({1u, static_cast<uint32>(PhysAnimBridge::SelfObsSize)});
	InputShapes[TensorIndexMap.MimicTargetPoses] = UE::NNE::FTensorShape::Make({1u, static_cast<uint32>(PhysAnimBridge::MimicTargetPosesSize)});
	InputShapes[TensorIndexMap.Terrain] = UE::NNE::FTensorShape::Make({1u, static_cast<uint32>(PhysAnimBridge::TerrainSize)});

	if (ModelInstance->SetInputTensorShapes(InputShapes) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("SetInputTensorShapes failed for the locked Stage 1 tensor contract.");
		return false;
	}

	InputBindings.SetNum(InputDescs.Num());
	InputBindings[TensorIndexMap.SelfObs] = { SelfObservationBuffer.GetData(), static_cast<uint64>(SelfObservationBuffer.Num() * sizeof(float)) };
	InputBindings[TensorIndexMap.MimicTargetPoses] = { MimicTargetPosesBuffer.GetData(), static_cast<uint64>(MimicTargetPosesBuffer.Num() * sizeof(float)) };
	InputBindings[TensorIndexMap.Terrain] = { TerrainBuffer.GetData(), static_cast<uint64>(TerrainBuffer.Num() * sizeof(float)) };

	OutputBindings.SetNum(1);
	OutputBindings[0] = { ActionOutputBuffer.GetData(), static_cast<uint64>(ActionOutputBuffer.Num() * sizeof(float)) };
	return true;
}

bool UPhysAnimComponent::GatherCurrentBodySamples(TArray<FPhysAnimBodySample>& OutBodySamples, FString& OutError) const
{
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved.");
		return false;
	}

	OutBodySamples.Reset();
	OutBodySamples.Reserve(PhysAnimBridge::NumSmplBodies);

	for (const FName BoneName : PhysAnimBridge::GetSmplObservationBoneNames())
	{
		if (SkeletalMesh->GetBoneIndex(BoneName) == INDEX_NONE)
		{
			OutError = FString::Printf(TEXT("Missing observation bone '%s' on the skeletal mesh."), *BoneName.ToString());
			return false;
		}

		const FTransform BoneWorldTransform = SkeletalMesh->GetBoneTransform(BoneName, RTS_World);

		FPhysAnimBodySample BodySample;
		BodySample.Position = PhysAnimBridge::UeVectorToSmpl(BoneWorldTransform.GetLocation());
		BodySample.Rotation = PhysAnimBridge::UeQuaternionToSmpl(BoneWorldTransform.GetRotation());
		USkeletalMeshComponent* const MutableMesh = const_cast<USkeletalMeshComponent*>(SkeletalMesh);
		BodySample.LinearVelocity = PhysAnimBridge::UeVectorToSmpl(MutableMesh->GetPhysicsLinearVelocity(BoneName));
		BodySample.AngularVelocity = PhysAnimBridge::UeVectorToSmpl(MutableMesh->GetPhysicsAngularVelocityInRadians(BoneName));
		OutBodySamples.Add(BodySample);
	}

	return true;
}

bool UPhysAnimComponent::SampleFuturePoses(
	const FPoseSearchBlueprintResult& SearchResult,
	TArray<FPhysAnimFuturePoseSample>& OutFutureSamples,
	FString& OutError) const
{
	const UAnimInstance* const LocalAnimInstance = this->AnimInstance.Get();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!LocalAnimInstance || !SkeletalMesh)
	{
		OutError = TEXT("Pose sampling requires both the AnimInstance and skeletal mesh.");
		return false;
	}

	const UAnimationAsset* const AnimationAsset = Cast<UAnimationAsset>(SearchResult.SelectedAnim);
	if (!AnimationAsset)
	{
		OutError = TEXT("PoseSearch result did not return a UAnimationAsset.");
		return false;
	}

	const float AnimationLength = AnimationAsset->GetPlayLength();
	const TArray<float> FutureOffsets = PhysAnimBridge::BuildFutureSampleTimeSchedule();

	OutFutureSamples.Reset();
	OutFutureSamples.Reserve(FutureOffsets.Num());

	for (const float FutureOffset : FutureOffsets)
	{
		FPoseSearchAssetSamplerInput SamplerInput;
		SamplerInput.Animation = AnimationAsset;
		SamplerInput.AnimationTime = FMath::Clamp(SearchResult.SelectedTime + FutureOffset, 0.0f, AnimationLength);
		SamplerInput.bMirrored = SearchResult.bIsMirrored;
		SamplerInput.BlendParameters = SearchResult.BlendParameters;
		SamplerInput.RootTransformOrigin = SkeletalMesh->GetComponentTransform();

		FPoseSearchAssetSamplerPose SampledPose = UPoseSearchAssetSamplerLibrary::SamplePose(LocalAnimInstance, SamplerInput);

		FPhysAnimFuturePoseSample FutureSample;
		FutureSample.FutureTimeSeconds = FutureOffset;
		FutureSample.BodyTransforms.Reserve(PhysAnimBridge::NumSmplBodies);

		for (const FName BoneName : PhysAnimBridge::GetSmplObservationBoneNames())
		{
			const FTransform WorldTransform =
				UPoseSearchAssetSamplerLibrary::GetTransformByName(SampledPose, BoneName, EPoseSearchAssetSamplerSpace::World);
			FutureSample.BodyTransforms.Add(FTransform(
				PhysAnimBridge::UeQuaternionToSmpl(WorldTransform.GetRotation()),
				PhysAnimBridge::UeVectorToSmpl(WorldTransform.GetLocation()),
				WorldTransform.GetScale3D()));
		}

		OutFutureSamples.Add(MoveTemp(FutureSample));
	}

	return true;
}

bool UPhysAnimComponent::RunInference(FString& OutError)
{
	UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync();
	if (!ModelInstance)
	{
		OutError = TEXT("No active model instance exists.");
		return false;
	}

	for (const float Value : SelfObservationBuffer)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("self_obs contained NaN or Inf.");
			return false;
		}
	}

	for (const float Value : MimicTargetPosesBuffer)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("mimic_target_poses contained NaN or Inf.");
			return false;
		}
	}

	if (ModelInstance->RunSync(InputBindings, OutputBindings) != UE::NNE::EResultStatus::Ok)
	{
		OutError = TEXT("RunSync failed.");
		return false;
	}

	for (const float Value : ActionOutputBuffer)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("Model action output contained NaN or Inf.");
			return false;
		}
	}

	return true;
}

FPhysAnimStabilizationSettings UPhysAnimComponent::ResolveEffectiveStabilizationSettings() const
{
	FPhysAnimStabilizationSettings EffectiveSettings = StabilizationSettings;
	EffectiveSettings.ActionScale =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimActionScale, EffectiveSettings.ActionScale);
	EffectiveSettings.ActionClampAbs =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimActionClampAbs, EffectiveSettings.ActionClampAbs);
	EffectiveSettings.ActionSmoothingAlpha =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimActionSmoothingAlpha, EffectiveSettings.ActionSmoothingAlpha);
	EffectiveSettings.StartupRampSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(PhysAnimComponentInternal::CVarPhysAnimStartupRampSeconds, EffectiveSettings.StartupRampSeconds);
	EffectiveSettings.PolicyControlRateHz =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimPolicyControlRateHz,
			EffectiveSettings.PolicyControlRateHz);
	EffectiveSettings.bApplyTrainingAlignedMassScales =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedMassScales,
			EffectiveSettings.bApplyTrainingAlignedMassScales);
	EffectiveSettings.TrainingAlignedMassScaleBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedMassScaleBlend,
			EffectiveSettings.TrainingAlignedMassScaleBlend);
	EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimApplyTrainingAlignedControlFamilyProfile,
			EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile);
	EffectiveSettings.TrainingAlignedControlFamilyProfileBlend =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimTrainingAlignedControlFamilyProfileBlend,
			EffectiveSettings.TrainingAlignedControlFamilyProfileBlend);
	EffectiveSettings.MaxAngularStepDegreesPerSecond =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxAngularStepDegPerSec,
			EffectiveSettings.MaxAngularStepDegreesPerSecond);
	EffectiveSettings.AngularStrengthMultiplier =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimAngularStrengthMultiplier,
			EffectiveSettings.AngularStrengthMultiplier);
	EffectiveSettings.AngularDampingRatioMultiplier =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimAngularDampingRatioMultiplier,
			EffectiveSettings.AngularDampingRatioMultiplier);
	EffectiveSettings.AngularExtraDampingMultiplier =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimAngularExtraDampingMultiplier,
			EffectiveSettings.AngularExtraDampingMultiplier);
	EffectiveSettings.bUseSkeletalAnimationTargets =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimUseSkeletalAnimationTargets,
			EffectiveSettings.bUseSkeletalAnimationTargets);
	EffectiveSettings.bForceZeroActions =
		PhysAnimComponentInternal::ResolveBoolOverride(PhysAnimComponentInternal::CVarPhysAnimForceZeroActions, EffectiveSettings.bForceZeroActions);
	EffectiveSettings.bLogActionDiagnostics =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimLogActionDiagnostics,
			EffectiveSettings.bLogActionDiagnostics);
	EffectiveSettings.MaxRootHeightDeltaCm =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxRootHeightDeltaCm,
			EffectiveSettings.MaxRootHeightDeltaCm);
	EffectiveSettings.MaxRootLinearSpeedCmPerSecond =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxRootLinearSpeedCmPerSec,
			EffectiveSettings.MaxRootLinearSpeedCmPerSecond);
	EffectiveSettings.MaxRootAngularSpeedDegPerSecond =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimMaxRootAngularSpeedDegPerSec,
			EffectiveSettings.MaxRootAngularSpeedDegPerSecond);
	EffectiveSettings.InstabilityGracePeriodSeconds =
		PhysAnimComponentInternal::ResolveFloatOverride(
			PhysAnimComponentInternal::CVarPhysAnimInstabilityGracePeriodSeconds,
			EffectiveSettings.InstabilityGracePeriodSeconds);
	EffectiveSettings.bEnableInstabilityFailStop =
		PhysAnimComponentInternal::ResolveBoolOverride(
			PhysAnimComponentInternal::CVarPhysAnimEnableInstabilityFailStop,
			EffectiveSettings.bEnableInstabilityFailStop);
	ApplyPresentationPerturbationStabilizationOverride(IsPresentationPerturbationOverrideActive(), EffectiveSettings);
	ApplyStabilizationStressTestRamp(
		ResolveStabilizationStressTestMultiplier(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestSweepMode.GetValueOnGameThread(),
		EffectiveSettings);
	return EffectiveSettings;
}

void UPhysAnimComponent::LogBridgeStateSnapshot(const TCHAR* Context) const
{
	if (PhysAnimComponentInternal::CVarPhysAnimLogBridgeStateSnapshots.GetValueOnGameThread() == 0)
	{
		return;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	const ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	const UCapsuleComponent* const CapsuleComponent = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr;
	const UCharacterMovementComponent* const CharacterMovement = CharacterOwner ? CharacterOwner->GetCharacterMovement() : nullptr;
	const UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	const FBodyInstance* const RootBody = SkeletalMesh ? SkeletalMesh->GetBodyInstance(RootBoneName) : nullptr;
	const FVector RootLinearVelocity = RootBody ? RootBody->GetUnrealWorldVelocity() : FVector::ZeroVector;
	const FVector RootAngularVelocity = RootBody ? FMath::RadiansToDegrees(RootBody->GetUnrealWorldAngularVelocityInRadians()) : FVector::ZeroVector;

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Snapshot[%s] state=%s bridgeOwnsPhysics=%s liveControls=%d liveBodyModifiers=%d forceZero=%s controlsDesiredEnabled=%s meshProfile=%s meshCollision=%s meshPawnResponse=%s capsuleCollision=%s charMoveTick=%s movementMode=%s rootBodyValid=%s rootBodySim=%s rootLinCmPerSec=(%.1f,%.1f,%.1f) rootAngDegPerSec=(%.1f,%.1f,%.1f)"),
		Context,
		GetRuntimeStateName(RuntimeState),
		RuntimeStateOwnsBridgePhysics(RuntimeState) ? TEXT("true") : TEXT("false"),
		PhysicsControl ? PhysicsControl->GetAllControlNames().Num() : 0,
		PhysicsControl ? PhysicsControl->GetAllBodyModifierNames().Num() : 0,
		EffectiveSettings.bForceZeroActions ? TEXT("true") : TEXT("false"),
		EffectiveSettings.bForceZeroActions ? TEXT("false") : TEXT("true"),
		SkeletalMesh ? *SkeletalMesh->GetCollisionProfileName().ToString() : TEXT("None"),
		SkeletalMesh ? *UEnum::GetValueAsString(SkeletalMesh->GetCollisionEnabled()) : TEXT("None"),
		SkeletalMesh ? *UEnum::GetValueAsString(SkeletalMesh->GetCollisionResponseToChannel(ECC_Pawn)) : TEXT("None"),
		CapsuleComponent ? *UEnum::GetValueAsString(CapsuleComponent->GetCollisionEnabled()) : TEXT("None"),
		CharacterMovement && CharacterMovement->IsComponentTickEnabled() ? TEXT("true") : TEXT("false"),
		CharacterMovement ? *UEnum::GetValueAsString(static_cast<EMovementMode>(CharacterMovement->MovementMode)) : TEXT("None"),
		RootBody && RootBody->IsValidBodyInstance() ? TEXT("true") : TEXT("false"),
		RootBody && RootBody->IsValidBodyInstance() && RootBody->IsInstanceSimulatingPhysics() ? TEXT("true") : TEXT("false"),
		RootLinearVelocity.X,
		RootLinearVelocity.Y,
		RootLinearVelocity.Z,
		RootAngularVelocity.X,
		RootAngularVelocity.Y,
		RootAngularVelocity.Z);
}

void UPhysAnimComponent::LogActivationSummary(
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	const TCHAR* Context,
	bool bCurrentPoseTargetsSeeded,
	bool bActivationPrepassCompleted,
	float SimulationHandoffProgress) const
{
	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Activation[%s]: skeletalTargets=%s currentPoseTargetsSeeded=%s activationPrepassCompleted=%s simulationHandoffAlpha=%.2f"),
		Context,
		EffectiveSettings.bUseSkeletalAnimationTargets ? TEXT("true") : TEXT("false"),
		bCurrentPoseTargetsSeeded ? TEXT("true") : TEXT("false"),
		bActivationPrepassCompleted ? TEXT("true") : TEXT("false"),
		SimulationHandoffProgress);
}

bool UPhysAnimComponent::ActivateRuntimePhysicsControl(FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	AActor* const OwnerActor = GetOwner();
	if (!PhysicsControl || !OwnerActor)
	{
		OutError = TEXT("Runtime Physics Control activation requires both the owning actor and Physics Control component.");
		return false;
	}

	DeactivateRuntimePhysicsControl(TEXT("ActivateRuntimePhysicsControl"));

	if (UPhysAnimStage1InitializerComponent* const Stage1Initializer = OwnerActor->FindComponentByClass<UPhysAnimStage1InitializerComponent>())
	{
		Stage1Initializer->CreateControls(PhysicsControl);
	}
	else if (UPhysicsControlInitializerComponent* const Initializer = OwnerActor->FindComponentByClass<UPhysicsControlInitializerComponent>())
	{
		Initializer->CreateControls(PhysicsControl);
	}
	else
	{
		OutError = TEXT("Owning actor is missing a runtime Physics Control initializer.");
		return false;
	}

	if (!ValidateRuntimePhysicsControl(OutError))
	{
		return false;
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime operator activation: controls=%d bodyModifiers=%d"),
		PhysicsControl->GetAllControlNames().Num(),
		PhysicsControl->GetAllBodyModifierNames().Num());
	return true;
}

void UPhysAnimComponent::DeactivateRuntimePhysicsControl(const TCHAR* Context)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		return;
	}

	const TArray<FName> ControlNames = PhysicsControl->GetAllControlNames();
	const TArray<FName> BodyModifierNames = PhysicsControl->GetAllBodyModifierNames();
	if (ControlNames.Num() == 0 && BodyModifierNames.Num() == 0)
	{
		return;
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime operator deactivation[%s]: controls=%d bodyModifiers=%d"),
		Context,
		ControlNames.Num(),
		BodyModifierNames.Num());

	if (BodyModifierNames.Num() > 0)
	{
		PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(BodyModifierNames);
		PhysicsControl->SetCachedBoneVelocitiesToZero();
		PhysicsControl->DestroyBodyModifiers(BodyModifierNames, true, false);
	}

	if (ControlNames.Num() > 0)
	{
		PhysicsControl->DestroyControls(ControlNames, true, false);
	}
}

void UPhysAnimComponent::ActivateBridgePhysicsState(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	if (!bHasSavedMeshCollisionState)
	{
		OriginalMeshCollisionProfileName = SkeletalMesh->GetCollisionProfileName();
		OriginalMeshCollisionEnabled = SkeletalMesh->GetCollisionEnabled();
		OriginalMeshPawnResponse = SkeletalMesh->GetCollisionResponseToChannel(ECC_Pawn);
		bHasSavedMeshCollisionState = true;
	}

	SkeletalMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	SkeletalMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	SkeletalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SkeletalMesh->RecreatePhysicsState();
	SkeletalMesh->SetEnablePhysicsBlending(true);
	SkeletalMesh->WakeAllRigidBodies();
	ApplyTrainingAlignedMassScales(EffectiveSettings);

	const bool bPreserveGameplayShell = ShouldPreserveGameplayShellDuringBridgeActive(
		IsMovementSmokeModeEnabled(),
		PhysAnimComponentInternal::CVarPhysAnimAllowCharacterMovementInBridgeActive.GetValueOnGameThread() != 0);
	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (!bHasSavedCapsuleCollisionState)
			{
				OriginalCapsuleCollisionEnabled = CapsuleComponent->GetCollisionEnabled();
				bHasSavedCapsuleCollisionState = true;
			}

			if (!bPreserveGameplayShell)
			{
				CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
		}

		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			if (!bHasSavedCharacterMovementState)
			{
				OriginalCharacterMovementMode = CharacterMovement->MovementMode;
				OriginalCharacterCustomMovementMode = CharacterMovement->CustomMovementMode;
				bOriginalCharacterMovementTickEnabled = CharacterMovement->IsComponentTickEnabled();
				bHasSavedCharacterMovementState = true;
			}

			if (!bPreserveGameplayShell)
			{
				CharacterMovement->DisableMovement();
				CharacterMovement->SetComponentTickEnabled(false);
			}
			else
			{
				CharacterMovement->SetComponentTickEnabled(true);
				if (CharacterMovement->MovementMode == MOVE_None)
				{
					CharacterMovement->SetMovementMode(MOVE_Walking);
				}
			}
		}
	}

	if (bPreserveGameplayShell)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] BridgeActive preserving capsule collision and CharacterMovement during bridge ownership."));
	}
}

void UPhysAnimComponent::ApplyTrainingAlignedMassScales(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return;
	}

	UPhysicsAsset* const PhysicsAsset = SkeletalMesh->GetPhysicsAsset();
	if (!PhysicsAsset)
	{
		return;
	}

	const bool bApplyMassPolicy = ShouldApplyTrainingAlignedMassScales(
		EffectiveSettings.bApplyTrainingAlignedMassScales,
		EffectiveSettings.TrainingAlignedMassScaleBlend);
	if (!bApplyMassPolicy)
	{
		return;
	}

	if (!bHasSavedBodyMassScales)
	{
		OriginalBodyMassScales.Reset();
		for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
		{
			if (!BodySetup)
			{
				continue;
			}

			OriginalBodyMassScales.Add(BodySetup->BoneName, SkeletalMesh->GetMassScale(BodySetup->BoneName));
		}
		bHasSavedBodyMassScales = OriginalBodyMassScales.Num() > 0;
	}

	int32 NumAdjustedBodies = 0;
	for (const USkeletalBodySetup* const BodySetup : PhysicsAsset->SkeletalBodySetups)
	{
		if (!BodySetup)
		{
			continue;
		}

		const float MassScale =
			ResolveTrainingAlignedMassScaleForBone(
				BodySetup->BoneName,
				EffectiveSettings.TrainingAlignedMassScaleBlend);
		SkeletalMesh->SetMassScale(BodySetup->BoneName, MassScale);
		++NumAdjustedBodies;
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Applied training-aligned Manny mass scales: bodies=%d blend=%.2f"),
		NumAdjustedBodies,
		EffectiveSettings.TrainingAlignedMassScaleBlend);
}

void UPhysAnimComponent::ResetTrainingAlignedMassScales()
{
	if (!bHasSavedBodyMassScales)
	{
		return;
	}

	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OriginalBodyMassScales.Reset();
		bHasSavedBodyMassScales = false;
		return;
	}

	for (const TPair<FName, float>& Pair : OriginalBodyMassScales)
	{
		SkeletalMesh->SetMassScale(Pair.Key, Pair.Value);
	}

	OriginalBodyMassScales.Reset();
	bHasSavedBodyMassScales = false;
}

void UPhysAnimComponent::ResetBridgePhysicsState()
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		bHasSavedMeshCollisionState = false;
		OriginalBodyMassScales.Reset();
		bHasSavedBodyMassScales = false;
		return;
	}

	ResetTrainingAlignedMassScales();
	SkeletalMesh->SetEnablePhysicsBlending(false);
	if (bHasSavedMeshCollisionState)
	{
		SkeletalMesh->SetCollisionProfileName(OriginalMeshCollisionProfileName);
		SkeletalMesh->SetCollisionEnabled(OriginalMeshCollisionEnabled);
		SkeletalMesh->SetCollisionResponseToChannel(ECC_Pawn, OriginalMeshPawnResponse);
		bHasSavedMeshCollisionState = false;
	}

	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (bHasSavedCapsuleCollisionState)
			{
				CapsuleComponent->SetCollisionEnabled(OriginalCapsuleCollisionEnabled);
				bHasSavedCapsuleCollisionState = false;
			}
		}

		if (UCharacterMovementComponent* const CharacterMovement = CharacterOwner->GetCharacterMovement())
		{
			if (bHasSavedCharacterMovementState)
			{
				CharacterMovement->SetComponentTickEnabled(bOriginalCharacterMovementTickEnabled);
				CharacterMovement->SetMovementMode(static_cast<EMovementMode>(OriginalCharacterMovementMode), OriginalCharacterCustomMovementMode);
				bHasSavedCharacterMovementState = false;
			}
		}
	}
}

void UPhysAnimComponent::ApplyRuntimeControlTuning(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	const bool bSimulationHandoffSettled = SimulationHandoffAlpha >= (1.0f - KINDA_SMALL_NUMBER);
	const bool bSimulationHandoffCompletedThisTick = bSimulationHandoffSettled && !bLastAppliedSimulationHandoffSettled;
	const bool bPresentationPerturbationOverrideActive = IsPresentationPerturbationOverrideActive();
	const bool bPolicyInfluenceActive = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings) > KINDA_SMALL_NUMBER;
	const bool bUseSkeletalAnimationTargetRepresentation =
		ShouldUseSkeletalAnimationTargetRepresentation(
			EffectiveSettings.bUseSkeletalAnimationTargets,
			bPolicyInfluenceActive);
	if (!PhysicsControl)
	{
		return;
	}

	PhysicsControl->SetControlsInSetEnabled(TEXT("All"), false);
	PhysicsControl->SetControlsInSetUseSkeletalAnimation(
		TEXT("All"),
		bUseSkeletalAnimationTargetRepresentation,
		0.0f,
		0.0f);

	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		const int32 BringUpGroupIndex = ResolveBringUpGroupIndex(BoneName);
		const bool bBringUpGroupUnlocked = IsBringUpGroupUnlocked(BringUpGroupIndex);
		const float ControlAuthorityAlpha =
			CalculateBringUpGroupControlAuthorityAlpha(BringUpGroupIndex, EffectiveSettings);
		const bool bApplyTrainingAlignedControlProfile =
			ShouldApplyTrainingAlignedControlFamilyProfile(
				EffectiveSettings.bApplyTrainingAlignedControlFamilyProfile,
				EffectiveSettings.TrainingAlignedControlFamilyProfileBlend);
		const float FamilyStrengthScale =
			bApplyTrainingAlignedControlProfile
				? ResolveTrainingAlignedControlStrengthScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedControlFamilyProfileBlend)
				: 1.0f;
		const float FamilyExtraDampingScale =
			bApplyTrainingAlignedControlProfile
				? ResolveTrainingAlignedControlExtraDampingScaleForBone(
					BoneName,
					EffectiveSettings.TrainingAlignedControlFamilyProfileBlend)
				: 1.0f;

		FPhysicsControlMultiplier ControlMultiplier;
		ControlMultiplier.AngularStrengthMultiplier =
			EffectiveSettings.AngularStrengthMultiplier * FamilyStrengthScale * ControlAuthorityAlpha;
		ControlMultiplier.AngularDampingRatioMultiplier = EffectiveSettings.AngularDampingRatioMultiplier;
		ControlMultiplier.AngularExtraDampingMultiplier =
			EffectiveSettings.AngularExtraDampingMultiplier * FamilyExtraDampingScale;

		const FName ControlName = PhysAnimBridge::MakeControlName(BoneName);
		PhysicsControl->SetControlMultiplier(
			ControlName,
			ControlMultiplier,
			bBringUpGroupUnlocked && !EffectiveSettings.bForceZeroActions,
			true,
			false);
	}

	PhysicsControl->SetBodyModifiersInSetMovementType(TEXT("All"), EPhysicsMovementType::Kinematic);
	PhysicsControl->SetBodyModifiersInSetPhysicsBlendWeight(TEXT("All"), 0.0f);
	PhysicsControl->SetBodyModifiersInSetCollisionType(TEXT("All"), ECollisionEnabled::NoCollision);
	PhysicsControl->SetBodyModifiersInSetUpdateKinematicFromSimulation(TEXT("All"), false);

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			continue;
		}

		const int32 BringUpGroupIndex = ResolveBringUpGroupIndex(BoneName);
		const bool bIsRootBodyModifier = BoneName == RootBoneName;
		const bool bAllowRootBodyModifierSimulation = false;
		const bool bBringUpGroupUnlocked =
			bIsRootBodyModifier ? bAllowRootBodyModifierSimulation : IsBringUpGroupUnlocked(BringUpGroupIndex);
		const bool bBodyModifierActivatedThisTick =
			(!bIsRootBodyModifier && bSimulationHandoffCompletedThisTick) ||
			(bAllowRootBodyModifierSimulation && !bLastAppliedPresentationRootSimulationEnabled);
		EPhysicsMovementType BodyModifierMovementType = EPhysicsMovementType::Kinematic;
		float BodyModifierPhysicsBlendWeight = 0.0f;
		bool bUpdateKinematicFromSimulation = false;
		const ECollisionEnabled::Type BodyModifierCollisionType =
			ResolveBodyModifierCollisionType(
				EffectiveSettings.bForceZeroActions,
				bSimulationHandoffSettled,
				bBringUpGroupUnlocked,
				bIsRootBodyModifier,
				bAllowRootBodyModifierSimulation);
		ResolveBodyModifierRuntimeMode(
			EffectiveSettings.bForceZeroActions,
			bSimulationHandoffSettled,
			bBringUpGroupUnlocked,
			bIsRootBodyModifier,
			bAllowRootBodyModifierSimulation,
			BodyModifierMovementType,
			BodyModifierPhysicsBlendWeight,
			bUpdateKinematicFromSimulation);
		PhysicsControl->SetBodyModifierMovementType(ModifierName, BodyModifierMovementType, true, false);
		PhysicsControl->SetBodyModifierPhysicsBlendWeight(ModifierName, BodyModifierPhysicsBlendWeight, true, false);
		PhysicsControl->SetBodyModifierCollisionType(ModifierName, BodyModifierCollisionType, true, false);
		PhysicsControl->SetBodyModifierUpdateKinematicFromSimulation(
			ModifierName,
			bUpdateKinematicFromSimulation,
			true,
			false);

		if (ShouldResetBodyModifierToCachedBoneTransform(
				EffectiveSettings.bForceZeroActions,
				bBodyModifierActivatedThisTick,
				bBringUpGroupUnlocked,
				bIsRootBodyModifier,
				bAllowRootBodyModifierSimulation) &&
			!PendingBodyModifierCachedResetNames.Contains(ModifierName))
		{
			PendingBodyModifierCachedResetNames.Add(ModifierName);
		}
	}

	LastAppliedStabilizationSettings = EffectiveSettings;
	bLastAppliedSimulationHandoffSettled = bSimulationHandoffSettled;
	LastAppliedControlAuthorityAlpha = CalculateCurrentControlAuthorityAlpha(EffectiveSettings);
	bLastAppliedPresentationRootSimulationEnabled = false;
}

bool UPhysAnimComponent::ConditionModelActions(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError)
{
	FPhysAnimActionConditioningSettings ConditioningSettings;
	ConditioningSettings.bForceZeroActions = EffectiveSettings.bForceZeroActions;
	ConditioningSettings.ActionClampAbs = EffectiveSettings.ActionClampAbs;
	ConditioningSettings.ActionSmoothingAlpha = EffectiveSettings.ActionSmoothingAlpha;
	ConditioningSettings.ActionScale =
		EffectiveSettings.ActionScale * CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
	const bool bSuccess = BuildConditionedActions(
		ActionOutputBuffer,
		PreviousConditionedActionBuffer.Num() == ActionOutputBuffer.Num() ? &PreviousConditionedActionBuffer : nullptr,
		ConditioningSettings,
		ConditionedActionBuffer,
		LastActionDiagnostics,
		OutError);
	if (bSuccess)
	{
		PreviousConditionedActionBuffer = ConditionedActionBuffer;
	}

	return bSuccess;
}

float UPhysAnimComponent::CalculateSimulationHandoffAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (EffectiveSettings.bForceZeroActions)
	{
		return 0.0f;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	const double ElapsedSeconds = FMath::Max(CurrentTimeSeconds - BridgeStartTimeSeconds, 0.0);
	const double HandoffDurationSeconds = FMath::Max(static_cast<double>(EffectiveSettings.StartupRampSeconds), UE_DOUBLE_SMALL_NUMBER);
	return FMath::Clamp(static_cast<float>(ElapsedSeconds / HandoffDurationSeconds), 0.0f, 1.0f);
}

float UPhysAnimComponent::CalculateCurrentControlAuthorityAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (EffectiveSettings.bForceZeroActions)
	{
		return 0.0f;
	}

	float MaxControlAuthorityAlpha = 0.0f;
	for (int32 GroupIndex = 0; GroupIndex <= HighestUnlockedBringUpGroupIndex; ++GroupIndex)
	{
		MaxControlAuthorityAlpha = FMath::Max(
			MaxControlAuthorityAlpha,
			CalculateBringUpGroupControlAuthorityAlpha(GroupIndex, EffectiveSettings));
	}

	return MaxControlAuthorityAlpha;
}

float UPhysAnimComponent::CalculateCurrentPolicyInfluenceAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (ShouldSuspendPolicyInfluenceDuringPresentationPerturbation(IsPresentationPerturbationOverrideActive()))
	{
		return 0.0f;
	}

	if (!AreAllBringUpGroupsUnlocked())
	{
		return 0.0f;
	}

	if (PolicyInfluenceRampStartTimeSeconds < 0.0)
	{
		return 0.0f;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	const float ElapsedSincePolicyRampStartSeconds = static_cast<float>(
		FMath::Max(CurrentTimeSeconds - PolicyInfluenceRampStartTimeSeconds, 0.0));
	return CalculatePolicyInfluenceAlpha(
		EffectiveSettings.bForceZeroActions,
		true,
		ElapsedSincePolicyRampStartSeconds,
		EffectiveSettings.StartupRampSeconds);
}

bool UPhysAnimComponent::IsPresentationPerturbationOverrideActive() const
{
	const UWorld* const World = GetWorld();
	return World &&
		PresentationPerturbationOverrideEndTimeSeconds >= 0.0 &&
		World->GetTimeSeconds() < PresentationPerturbationOverrideEndTimeSeconds;
}

void UPhysAnimComponent::UnlockBringUpGroup(int32 GroupIndex, const TCHAR* Context)
{
	if (GroupIndex < 0 || GroupIndex >= GetBringUpGroupCount() || GroupIndex <= HighestUnlockedBringUpGroupIndex)
	{
		return;
	}

	if (!BringUpGroupActivationTimeSeconds.IsValidIndex(GroupIndex))
	{
		BringUpGroupActivationTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	}
	if (!BringUpGroupControlRampStartTimeSeconds.IsValidIndex(GroupIndex))
	{
		BringUpGroupControlRampStartTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	}

	const double ActivationTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	BringUpGroupActivationTimeSeconds[GroupIndex] = ActivationTimeSeconds;
	const bool bDelayControlRamp = ShouldDelayBringUpGroupControlRamp(GroupIndex, GetBringUpGroupCount());
	const bool bStartControlRampImmediately = ShouldStartBringUpGroupControlRamp(
		false,
		true,
		bDelayControlRamp,
		false);
	BringUpGroupControlRampStartTimeSeconds[GroupIndex] =
		bStartControlRampImmediately ? ActivationTimeSeconds : -1.0;
	HighestUnlockedBringUpGroupIndex = GroupIndex;
	BringUpGroupStableAccumulatedSeconds = 0.0f;

	TArray<FString> GroupBoneNames;
	for (const FName BoneName : PhysAnimBridge::GetControlledBoneNames())
	{
		if (ResolveBringUpGroupIndex(BoneName) != GroupIndex)
		{
			continue;
		}

		GroupBoneNames.Add(BoneName.ToString());

		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (!PendingBodyModifierCachedResetNames.Contains(ModifierName))
		{
			PendingBodyModifierCachedResetNames.Add(ModifierName);
		}
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Stabilization bring-up unlocked group %d/%d [%s] context=%s"),
		GroupIndex + 1,
		GetBringUpGroupCount(),
		*FString::Join(GroupBoneNames, TEXT(", ")),
		Context);
}

void UPhysAnimComponent::AdvanceBringUpState(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	if (EffectiveSettings.bForceZeroActions || !IsBringUpGroupUnlocked(0))
	{
		return;
	}

	const bool bWithinBodyVelocityBounds =
		LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond <= EffectiveSettings.MaxRootLinearSpeedCmPerSecond &&
		LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond <= EffectiveSettings.MaxRootAngularSpeedDegPerSecond;
	const bool bWithinRootBounds =
		LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm <= EffectiveSettings.MaxRootHeightDeltaCm &&
		LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond <= EffectiveSettings.MaxRootLinearSpeedCmPerSecond &&
		LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond <= EffectiveSettings.MaxRootAngularSpeedDegPerSecond;
	if (!bWithinBodyVelocityBounds || !bWithinRootBounds)
	{
		BringUpGroupStableAccumulatedSeconds = 0.0f;
		return;
	}

	BringUpGroupStableAccumulatedSeconds += DeltaTime;
	const float StableDwellSeconds = FMath::Max(EffectiveSettings.StartupRampSeconds, 0.25f);
	if (BringUpGroupStableAccumulatedSeconds < StableDwellSeconds)
	{
		return;
	}

	const int32 FinalGroupIndex = GetBringUpGroupCount() - 1;
	if (AreAllBringUpGroupsUnlocked())
	{
		if (BringUpGroupControlRampStartTimeSeconds.IsValidIndex(FinalGroupIndex) &&
			BringUpGroupControlRampStartTimeSeconds[FinalGroupIndex] < 0.0 &&
			ShouldStartBringUpGroupControlRamp(
				EffectiveSettings.bForceZeroActions,
				true,
				ShouldDelayBringUpGroupControlRamp(FinalGroupIndex, GetBringUpGroupCount()),
				true))
		{
			BringUpGroupControlRampStartTimeSeconds[FinalGroupIndex] =
				GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
			BringUpGroupStableAccumulatedSeconds = 0.0f;
			UE_LOG(
				LogPhysAnimBridge,
				Log,
				TEXT("[PhysAnim] Stabilization final-group control ramp enabled for group %d/%d [hand_l, hand_r]."),
				FinalGroupIndex + 1,
				GetBringUpGroupCount());
		}
		else if (PolicyInfluenceRampStartTimeSeconds < 0.0 &&
			ShouldStartPolicyInfluenceRamp(
				EffectiveSettings.bForceZeroActions,
				true,
				IsBringUpGroupControlRampActive(FinalGroupIndex),
				true))
		{
			PolicyInfluenceRampStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
			BringUpGroupStableAccumulatedSeconds = 0.0f;
			UE_LOG(
				LogPhysAnimBridge,
				Log,
				TEXT("[PhysAnim] Stabilization policy influence ramp enabled after final-group control settle."));
		}
		return;
	}

	UnlockBringUpGroup(HighestUnlockedBringUpGroupIndex + 1, TEXT("StableRuntimeWindow"));
}

bool UPhysAnimComponent::AreAllBringUpGroupsUnlocked() const
{
	return HighestUnlockedBringUpGroupIndex >= (GetBringUpGroupCount() - 1);
}

bool UPhysAnimComponent::IsBringUpGroupUnlocked(int32 GroupIndex) const
{
	return GroupIndex >= 0 && GroupIndex <= HighestUnlockedBringUpGroupIndex;
}

bool UPhysAnimComponent::IsBringUpGroupControlRampActive(int32 GroupIndex) const
{
	return GroupIndex >= 0 &&
		BringUpGroupControlRampStartTimeSeconds.IsValidIndex(GroupIndex) &&
		BringUpGroupControlRampStartTimeSeconds[GroupIndex] >= 0.0;
}

bool UPhysAnimComponent::IsBoneInUnlockedBringUpGroup(FName BoneName) const
{
	return IsBringUpGroupUnlocked(ResolveBringUpGroupIndex(BoneName));
}

float UPhysAnimComponent::CalculateBringUpGroupControlAuthorityAlpha(
	int32 GroupIndex,
	const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (!IsBringUpGroupUnlocked(GroupIndex) ||
		!BringUpGroupControlRampStartTimeSeconds.IsValidIndex(GroupIndex) ||
		BringUpGroupControlRampStartTimeSeconds[GroupIndex] < 0.0)
	{
		return 0.0f;
	}

	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
	const float ElapsedSinceGroupControlRampStartSeconds = static_cast<float>(
		FMath::Max(CurrentTimeSeconds - BringUpGroupControlRampStartTimeSeconds[GroupIndex], 0.0));
	return CalculateControlAuthorityAlpha(
		EffectiveSettings.bForceZeroActions,
		true,
		ElapsedSinceGroupControlRampStartSeconds,
		EffectiveSettings.StartupRampSeconds);
}

bool UPhysAnimComponent::CheckRuntimeInstability(
	float DeltaTime,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	FString& OutError)
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		OutError = TEXT("Skeletal mesh component was not resolved.");
		return false;
	}

	FPhysAnimRuntimeInstabilitySettings InstabilitySettings;
	InstabilitySettings.bEnableAutomaticFailStop = EffectiveSettings.bEnableInstabilityFailStop;
	InstabilitySettings.MaxRootHeightDeltaCm = EffectiveSettings.MaxRootHeightDeltaCm;
	InstabilitySettings.MaxRootLinearSpeedCmPerSecond = EffectiveSettings.MaxRootLinearSpeedCmPerSecond;
	InstabilitySettings.MaxRootAngularSpeedDegPerSecond = EffectiveSettings.MaxRootAngularSpeedDegPerSecond;
	InstabilitySettings.UnstableGracePeriodSeconds = EffectiveSettings.InstabilityGracePeriodSeconds;

	const FName RootBoneName = PhysAnimBridge::GetRootBoneName();
	const FVector RootLocationCm = SkeletalMesh->GetBoneLocation(RootBoneName, EBoneSpaces::WorldSpace);
	const FVector RootLinearVelocityCmPerSecond = SkeletalMesh->GetPhysicsLinearVelocity(RootBoneName);
	const FVector RootAngularVelocityDegPerSecond = SkeletalMesh->GetPhysicsAngularVelocityInDegrees(RootBoneName);
	const AActor* const OwnerActor = GetOwner();
	const bool bPreserveGameplayShell = ShouldPreserveGameplayShellDuringBridgeActive(
		IsMovementSmokeModeEnabled(),
		PhysAnimComponentInternal::CVarPhysAnimAllowCharacterMovementInBridgeActive.GetValueOnGameThread() != 0);
	const FVector OwnerLocationCm = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
	const FVector OwnerLinearVelocityCmPerSecond = OwnerActor ? OwnerActor->GetVelocity() : FVector::ZeroVector;
	FVector EffectiveRootLocationCm = RootLocationCm;
	FVector EffectiveRootLinearVelocityCmPerSecond = RootLinearVelocityCmPerSecond;
	ResolveRuntimeInstabilityRootFrame(
		bPreserveGameplayShell,
		RootLocationCm,
		RootLinearVelocityCmPerSecond,
		OwnerLocationCm,
		OwnerLinearVelocityCmPerSecond,
		EffectiveRootLocationCm,
		EffectiveRootLinearVelocityCmPerSecond);

	FString InstabilityError;
	const bool bStable = EvaluateRuntimeInstability(
		EffectiveRootLocationCm,
		EffectiveRootLinearVelocityCmPerSecond,
		RootAngularVelocityDegPerSecond,
		DeltaTime,
		InstabilitySettings,
		RuntimeInstabilityState,
		LastRuntimeInstabilityDiagnostics,
		InstabilityError);

	TArray<FPhysAnimBodyInstabilitySample> BodySamples;
	if (GatherRuntimeInstabilityBodySamples(BodySamples))
	{
		if (bPreserveGameplayShell && OwnerActor)
		{
			for (FPhysAnimBodyInstabilitySample& Sample : BodySamples)
			{
				Sample.Location -= OwnerLocationCm;
				Sample.LinearVelocity -= OwnerLinearVelocityCmPerSecond;
			}
		}

		const FVector ReferenceRootLocationCm = RuntimeInstabilityState.bHasReferenceRootLocation
			? RuntimeInstabilityState.ReferenceRootLocation
			: EffectiveRootLocationCm;
		PhysAnimBridge::EvaluatePerBodyInstabilitySamples(
			BodySamples,
			ReferenceRootLocationCm,
			LastRuntimeInstabilityDiagnostics);
	}
	if (!bStable)
	{
		OutError = InstabilityError;
	}

	return bStable;
}

bool UPhysAnimComponent::GatherRuntimeInstabilityBodySamples(TArray<FPhysAnimBodyInstabilitySample>& OutSamples) const
{
	OutSamples.Reset();

	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		return false;
	}

	OutSamples.Reserve(PhysAnimBridge::GetRequiredBodyModifierBoneNames().Num());
	for (const FName BoneName : PhysAnimBridge::GetRequiredBodyModifierBoneNames())
	{
		const FBodyInstance* const BodyInstance = SkeletalMesh->GetBodyInstance(BoneName);
		if (!BodyInstance || !BodyInstance->IsValidBodyInstance())
		{
			continue;
		}

		FPhysAnimBodyInstabilitySample& Sample = OutSamples.AddDefaulted_GetRef();
		Sample.BoneName = BoneName;
		Sample.Location = BodyInstance->GetUnrealWorldTransform().GetLocation();
		Sample.LinearVelocity = BodyInstance->GetUnrealWorldVelocity();
		Sample.AngularVelocity = FMath::RadiansToDegrees(BodyInstance->GetUnrealWorldAngularVelocityInRadians());
		Sample.bIsSimulatingPhysics = BodyInstance->IsInstanceSimulatingPhysics();
	}

	return true;
}

void UPhysAnimComponent::LogBodyModifierTelemetrySnapshot(const TCHAR* Context) const
{
	TArray<FPhysAnimBodyInstabilitySample> BodySamples;
	if (!GatherRuntimeInstabilityBodySamples(BodySamples))
	{
		return;
	}

	FPhysAnimRuntimeInstabilityDiagnostics BodyDiagnostics;
	const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	const FVector CurrentRootLocationCm = SkeletalMesh
		? SkeletalMesh->GetBoneLocation(PhysAnimBridge::GetRootBoneName(), EBoneSpaces::WorldSpace)
		: FVector::ZeroVector;
	const FVector ReferenceRootLocationCm = RuntimeInstabilityState.bHasReferenceRootLocation
		? RuntimeInstabilityState.ReferenceRootLocation
		: CurrentRootLocationCm;
	PhysAnimBridge::EvaluatePerBodyInstabilitySamples(
		BodySamples,
		ReferenceRootLocationCm,
		BodyDiagnostics);

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] BodyTelemetry[%s]: bodies=%d simulating=%d referenceRootZ=%.1f"),
		Context,
		BodyDiagnostics.NumBodiesConsidered,
		BodyDiagnostics.NumSimulatingBodies,
		ReferenceRootLocationCm.Z);

	for (const FPhysAnimBodyInstabilitySample& Sample : BodySamples)
	{
		const float HeightDeltaCm = FMath::Abs(Sample.Location.Z - ReferenceRootLocationCm.Z);
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] BodyTelemetry[%s] bone=%s sim=%s locZ=%.1f heightDeltaCm=%.1f linearCmPerSec=%.1f angularDegPerSec=%.1f"),
			Context,
			*Sample.BoneName.ToString(),
			Sample.bIsSimulatingPhysics ? TEXT("true") : TEXT("false"),
			Sample.Location.Z,
			HeightDeltaCm,
			Sample.LinearVelocity.Size(),
			Sample.AngularVelocity.Size());
	}
}

void UPhysAnimComponent::ResetPendingBodyModifiersToCachedTargets()
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl || PendingBodyModifierCachedResetNames.IsEmpty())
	{
		return;
	}

	TArray<FName> ModifierNamesToReset;
	ModifierNamesToReset.Reserve(PendingBodyModifierCachedResetNames.Num());
	for (const FName ModifierName : PendingBodyModifierCachedResetNames)
	{
		if (PhysicsControl->GetBodyModifierExists(ModifierName))
		{
			ModifierNamesToReset.Add(ModifierName);
		}
	}

	if (ModifierNamesToReset.IsEmpty())
	{
		PendingBodyModifierCachedResetNames.Reset();
		return;
	}

	PhysicsControl->ResetBodyModifiersToCachedBoneTransforms(
		ModifierNamesToReset,
		EResetToCachedTargetBehavior::ResetDuringUpdateControls,
		true,
		false);

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Scheduled deferred cached-target reset for %d promoted body modifiers."),
		ModifierNamesToReset.Num());
	PendingBodyModifierCachedResetNames.Reset();
}

void UPhysAnimComponent::ApplyControlTargets(
	float PolicyStepDeltaTime,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	bool bApplyNewPolicyStepThisTick,
	FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Physics Control component was not resolved.");
		return;
	}

	const float PolicyInfluenceAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
	const bool bPolicyInfluenceActive = PolicyInfluenceAlpha > KINDA_SMALL_NUMBER;
	FPhysAnimControlTargetDiagnostics ControlTargetDiagnostics;
	ControlTargetDiagnostics.bPolicyInfluenceActive = bPolicyInfluenceActive;
	ControlTargetDiagnostics.bFirstPolicyEnabledFrame = bPolicyInfluenceActive && !bPolicyTargetsAppliedLastFrame;
	if (EffectiveSettings.bForceZeroActions)
	{
		PreviousControlTargetRotations.Reset();
		PolicyBlendStartControlTargetRotations.Reset();
		LastControlTargetDiagnostics = {};
		bPolicyTargetsAppliedLastFrame = false;
		return;
	}

	if (!bPolicyInfluenceActive)
	{
		PolicyBlendStartControlTargetRotations.Reset();
		LastControlTargetDiagnostics = ControlTargetDiagnostics;
		bPolicyTargetsAppliedLastFrame = false;
		return;
	}

	if (!bApplyNewPolicyStepThisTick)
	{
		LastControlTargetDiagnostics.bPolicyInfluenceActive = bPolicyInfluenceActive;
		LastControlTargetDiagnostics.bFirstPolicyEnabledFrame = false;
		return;
	}

	if (ControlTargetDiagnostics.bFirstPolicyEnabledFrame)
	{
		PreviousControlTargetRotations.Reset();
		PolicyBlendStartControlTargetRotations.Reset();
	}

	TMap<FName, FQuat> ControlRotations;
	if (!PhysAnimBridge::ConvertModelActionsToControlRotations(ConditionedActionBuffer, ControlRotations, OutError))
	{
		return;
	}

	const float MaxAngularStepDegrees =
		FMath::Max(0.0f, EffectiveSettings.MaxAngularStepDegreesPerSecond) * PolicyStepDeltaTime;
	const bool bUseSkeletalAnimationTargetRepresentation =
		ShouldUseSkeletalAnimationTargetRepresentation(
			EffectiveSettings.bUseSkeletalAnimationTargets,
			bPolicyInfluenceActive);
	const float TargetWriteDeltaTime =
		ResolvePolicyTargetWriteDeltaTime(
			bUseSkeletalAnimationTargetRepresentation,
			ControlTargetDiagnostics.bFirstPolicyEnabledFrame,
			PolicyStepDeltaTime);

	if (ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(
		bUseSkeletalAnimationTargetRepresentation,
		ControlTargetDiagnostics.bFirstPolicyEnabledFrame))
	{
		PhysicsControl->SetControlTargetOrientationsInSet(TEXT("All"), FRotator::ZeroRotator, 0.0f, true, false);
	}

	for (const TPair<FName, FQuat>& Pair : ControlRotations)
	{
		if (!ShouldApplyPolicyTargetToBone(Pair.Key, bPolicyInfluenceActive))
		{
			continue;
		}

		const FName ControlName = PhysAnimBridge::MakeControlName(Pair.Key);
		if (!PhysicsControl->GetControlExists(ControlName))
		{
			OutError = FString::Printf(TEXT("Missing required control '%s' during target write."), *ControlName.ToString());
			return;
		}
		const FQuat IdentityRotation = FQuat::Identity;
		const FQuat* const PreviousRotation =
			ControlTargetDiagnostics.bFirstPolicyEnabledFrame ? &IdentityRotation : PreviousControlTargetRotations.Find(ControlName);
		const FQuat* const BlendStartRotation =
			ControlTargetDiagnostics.bFirstPolicyEnabledFrame ? &IdentityRotation : PolicyBlendStartControlTargetRotations.Find(ControlName);
		if (ControlTargetDiagnostics.bFirstPolicyEnabledFrame)
		{
			PolicyBlendStartControlTargetRotations.Add(ControlName, IdentityRotation);
		}
		const float RawPolicyOffsetDegrees = BlendStartRotation
			? CalculateControlTargetDeltaDegrees(*BlendStartRotation, Pair.Value)
			: 0.0f;
		const FQuat BlendedPolicyRotation = BlendStartRotation
			? BlendPolicyTargetRotation(*BlendStartRotation, Pair.Value, PolicyInfluenceAlpha)
			: Pair.Value;
		const float TargetDeltaDegrees = PreviousRotation
			? CalculateControlTargetDeltaDegrees(*PreviousRotation, BlendedPolicyRotation)
			: 0.0f;
		const FQuat LimitedRotation = PreviousRotation
			? LimitTargetRotationStep(*PreviousRotation, BlendedPolicyRotation, MaxAngularStepDegrees)
			: BlendedPolicyRotation;

		++ControlTargetDiagnostics.NumPolicyTargetsWritten;
		ControlTargetDiagnostics.MeanTargetDeltaDegrees += TargetDeltaDegrees;
		ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees += RawPolicyOffsetDegrees;
		if (TargetDeltaDegrees > ControlTargetDiagnostics.MaxTargetDeltaDegrees)
		{
			ControlTargetDiagnostics.MaxTargetDeltaDegrees = TargetDeltaDegrees;
			ControlTargetDiagnostics.MaxTargetDeltaBoneName = Pair.Key;
		}
		if (RawPolicyOffsetDegrees > ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees)
		{
			ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees = RawPolicyOffsetDegrees;
			ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName = Pair.Key;
		}

		PreviousControlTargetRotations.Add(ControlName, LimitedRotation);

		PhysicsControl->SetControlTargetOrientation(
			ControlName,
			LimitedRotation.Rotator(),
			TargetWriteDeltaTime,
			true,
			false,
			true,
			false);
	}

	if (ControlTargetDiagnostics.NumPolicyTargetsWritten > 0)
	{
		ControlTargetDiagnostics.MeanTargetDeltaDegrees /=
			static_cast<float>(ControlTargetDiagnostics.NumPolicyTargetsWritten);
		ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees /=
			static_cast<float>(ControlTargetDiagnostics.NumPolicyTargetsWritten);
	}

	LastControlTargetDiagnostics = ControlTargetDiagnostics;
	bPolicyTargetsAppliedLastFrame = bPolicyInfluenceActive;

	if (ControlTargetDiagnostics.bFirstPolicyEnabledFrame)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] First policy-enabled frame: targets=%d maxTargetDelta=%s:%.1fdeg meanTargetDelta=%.1fdeg maxRawPolicyOffset=%s:%.1fdeg meanRawPolicyOffset=%.1fdeg"),
			ControlTargetDiagnostics.NumPolicyTargetsWritten,
			*ControlTargetDiagnostics.MaxTargetDeltaBoneName.ToString(),
			ControlTargetDiagnostics.MaxTargetDeltaDegrees,
			ControlTargetDiagnostics.MeanTargetDeltaDegrees,
			*ControlTargetDiagnostics.MaxRawPolicyOffsetBoneName.ToString(),
			ControlTargetDiagnostics.MaxRawPolicyOffsetDegrees,
			ControlTargetDiagnostics.MeanRawPolicyOffsetDegrees);
	}
}

bool UPhysAnimComponent::IsMovementSmokeModeEnabled() const
{
	return PhysAnimComponentInternal::CVarPhysAnimMovementSmokeMode.GetValueOnGameThread() != 0;
}

void UPhysAnimComponent::ApplyMovementSmokeInput(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	LastMovementSmokeLocalIntent = FVector::ZeroVector;
	LastMovementSmokeWorldIntent = FVector::ZeroVector;
	LastMovementSmokePhaseName = NAME_None;

	if (!IsMovementSmokeModeEnabled() || RuntimeState != EPhysAnimRuntimeState::BridgeActive)
	{
		return;
	}

	ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner());
	if (!CharacterOwner)
	{
		return;
	}

	const UWorld* const World = GetWorld();
	const double CurrentTimeSeconds = World ? World->GetTimeSeconds() : BridgeStartTimeSeconds;
	const double ScriptStartTimeSeconds = PolicyInfluenceRampStartTimeSeconds >= 0.0
		? (PolicyInfluenceRampStartTimeSeconds + EffectiveSettings.StartupRampSeconds)
		: -1.0;
	if (ScriptStartTimeSeconds < 0.0 || CurrentTimeSeconds < ScriptStartTimeSeconds)
	{
		LastMovementSmokePhaseName = TEXT("WaitingForPolicy");
		LastMovementSmokeOwnerVelocityCmPerSecond = CharacterOwner->GetVelocity();
		return;
	}

	const float ScriptElapsedSeconds = static_cast<float>(CurrentTimeSeconds - ScriptStartTimeSeconds);
	const int32 NumLoops = FMath::Max(PhysAnimComponentInternal::CVarPhysAnimMovementSmokeLoopCount.GetValueOnGameThread(), 1);
	const float TotalDurationSeconds = GetMovementSmokeTotalDurationSeconds(NumLoops);
	const bool bScriptComplete = ScriptElapsedSeconds >= TotalDurationSeconds;
	const float PhaseElapsedSeconds = bScriptComplete
		? GetMovementSmokeDurationSeconds()
		: FMath::Fmod(ScriptElapsedSeconds, GetMovementSmokeDurationSeconds());
	const FVector LocalIntent = bScriptComplete
		? FVector::ZeroVector
		: ResolveMovementSmokeLocalIntent(PhaseElapsedSeconds);
	const FName PhaseName = bScriptComplete
		? TEXT("Complete")
		: ResolveMovementSmokePhaseName(PhaseElapsedSeconds);

	FRotator IntentRotation = CharacterOwner->GetActorRotation();
	if (const AController* const Controller = CharacterOwner->GetController())
	{
		IntentRotation = Controller->GetControlRotation();
	}
	IntentRotation.Pitch = 0.0f;
	IntentRotation.Roll = 0.0f;

	const FVector Forward = FRotationMatrix(IntentRotation).GetScaledAxis(EAxis::X).GetSafeNormal2D();
	const FVector Right = FRotationMatrix(IntentRotation).GetScaledAxis(EAxis::Y).GetSafeNormal2D();
	const FVector WorldIntent = ((Forward * LocalIntent.X) + (Right * LocalIntent.Y)).GetClampedToMaxSize(1.0f);

	if (!bMovementSmokeScriptStarted)
	{
		MovementSmokeStartLocation = CharacterOwner->GetActorLocation();
		bMovementSmokeScriptStarted = true;
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Movement smoke script started after policy settle."));
	}

	LastMovementSmokeLocalIntent = LocalIntent;
	LastMovementSmokeWorldIntent = WorldIntent;
	LastMovementSmokePhaseName = PhaseName;
	LastMovementSmokeOwnerVelocityCmPerSecond = CharacterOwner->GetVelocity();

	if (!WorldIntent.IsNearlyZero())
	{
		CharacterOwner->AddMovementInput(WorldIntent, 1.0f, true);
	}

	if (!bMovementSmokeCompletionLogged && bScriptComplete)
	{
		const FVector CurrentLocation = CharacterOwner->GetActorLocation();
		const float TotalDisplacementCm = FVector::Dist2D(CurrentLocation, MovementSmokeStartLocation);
		UE_LOG(
			LogPhysAnimBridge,
			Log,
			TEXT("[PhysAnim] Movement smoke complete: loops=%d totalDisplacementCm=%.1f finalPhase=%s runtime=%s"),
			NumLoops,
			TotalDisplacementCm,
			*PhaseName.ToString(),
			GetRuntimeStateName(RuntimeState));
		bMovementSmokeCompletionLogged = true;
	}
}

void UPhysAnimComponent::MaybeLogRuntimeDiagnostics(const FPhysAnimStabilizationSettings& EffectiveSettings) const
{
	if (!EffectiveSettings.bLogActionDiagnostics)
	{
		return;
	}

	const UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	const double CurrentTimeSeconds = World->GetTimeSeconds();
	if (LastRuntimeDiagnosticsLogTimeSeconds >= 0.0 &&
		(CurrentTimeSeconds - LastRuntimeDiagnosticsLogTimeSeconds) < EffectiveSettings.ActionDiagnosticsIntervalSeconds)
	{
		return;
	}

	const_cast<UPhysAnimComponent*>(this)->LastRuntimeDiagnosticsLogTimeSeconds = CurrentTimeSeconds;
	const bool bStressTestEnabled = PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() > 0;
	const bool bStressTestActive = bStressTestEnabled && StabilizationStressTestStartTimeSeconds >= 0.0;
	const float StressTestMultiplier = ResolveStabilizationStressTestMultiplier();
	const float PolicyControlIntervalSeconds = ResolvePolicyControlIntervalSeconds(EffectiveSettings.PolicyControlRateHz);
	const float StressTestElapsedSeconds = bStressTestActive
		? static_cast<float>(FMath::Max(CurrentTimeSeconds - StabilizationStressTestStartTimeSeconds, 0.0))
		: 0.0f;
	float StressSpineLocalDeltaCm = 0.0f;
	float StressHeadLocalDeltaCm = 0.0f;
	float StressFootLocalDeltaCm = 0.0f;
	if (bStressTestActive)
	{
		if (const AActor* const OwnerActor = GetOwner())
		{
			const FVector ActorLocation = OwnerActor->GetActorLocation();
			if (const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get())
			{
				StressSpineLocalDeltaCm = FVector::Dist(
					SkeletalMesh->GetBoneLocation(TEXT("spine_01")) - ActorLocation,
					StabilizationStressTestBaselineSpineLocalOffset);
				StressHeadLocalDeltaCm = FVector::Dist(
					SkeletalMesh->GetBoneLocation(TEXT("head")) - ActorLocation,
					StabilizationStressTestBaselineHeadLocalOffset);
				StressFootLocalDeltaCm = FMath::Max(
					FVector::Dist(
						SkeletalMesh->GetBoneLocation(TEXT("foot_l")) - ActorLocation,
						StabilizationStressTestBaselineLeftFootLocalOffset),
					FVector::Dist(
						SkeletalMesh->GetBoneLocation(TEXT("foot_r")) - ActorLocation,
						StabilizationStressTestBaselineRightFootLocalOffset));
			}
		}
	}
	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime diagnostics: handoffAlpha=%.2f bringUpGroup=%d/%d controlAuthorityAlpha=%.2f currentGroupControlAuthorityAlpha=%.2f policyInfluenceAlpha=%.2f policyStep[rateHz=%.1f intervalMs=%.1f updated=%s elapsedSteps=%d skipped=%d accumMs=%.1f] perturbOverride=%s stressTest[enabled=%s active=%s profile=%d sweep=%d multiplier=%.2f elapsed=%.1f firstAngSpike=%s:%.2f firstLinSpike=%s:%.2f firstInstability=%.2f localSpine=%.1f localHead=%.1f localFoot=%.1f] moveSmoke[active=%s phase=%s local=(%.1f,%.1f) world=(%.2f,%.2f) ownerVelCmPerSec=%.1f] action[rawMin=%.3f rawMax=%.3f rawMeanAbs=%.3f conditionedMeanAbs=%.3f clamped=%d] targets[policyActive=%s firstPolicyFrame=%s written=%d maxDelta=%s:%.1fdeg meanDelta=%.1fdeg maxRawOffset=%s:%.1fdeg meanRawOffset=%.1fdeg] root[heightDeltaCm=%.1f linearCmPerSec=%.1f angularDegPerSec=%.1f unstableFor=%.2f] bodies[count=%d sim=%d maxLin=%s(%s):%.1f maxAng=%s(%s):%.1f maxHeight=%s(%s):%.1f]"),
		SimulationHandoffAlpha,
		FMath::Max(HighestUnlockedBringUpGroupIndex + 1, 0),
		GetBringUpGroupCount(),
		CalculateCurrentControlAuthorityAlpha(EffectiveSettings),
		CalculateBringUpGroupControlAuthorityAlpha(HighestUnlockedBringUpGroupIndex, EffectiveSettings),
		CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings),
		EffectiveSettings.PolicyControlRateHz,
		PolicyControlIntervalSeconds * 1000.0f,
		LastPolicyElapsedSteps > 0 ? TEXT("true") : TEXT("false"),
		LastPolicyElapsedSteps,
		PolicyControlTicksSkipped,
		PolicyUpdateAccumulatorSeconds * 1000.0f,
		IsPresentationPerturbationOverrideActive() ? TEXT("true") : TEXT("false"),
		bStressTestEnabled ? TEXT("true") : TEXT("false"),
		bStressTestActive ? TEXT("true") : TEXT("false"),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestProfile.GetValueOnGameThread(),
		PhysAnimComponentInternal::CVarPaStabilizationStressTestSweepMode.GetValueOnGameThread(),
		StressTestMultiplier,
		StressTestElapsedSeconds,
		*StabilizationStressTestFirstAngularSpikeBoneName.ToString(),
		StabilizationStressTestFirstAngularSpikeMultiplier,
		*StabilizationStressTestFirstLinearSpikeBoneName.ToString(),
		StabilizationStressTestFirstLinearSpikeMultiplier,
		StabilizationStressTestFirstInstabilityMultiplier,
		StressSpineLocalDeltaCm,
		StressHeadLocalDeltaCm,
		StressFootLocalDeltaCm,
		IsMovementSmokeModeEnabled() ? TEXT("true") : TEXT("false"),
		*LastMovementSmokePhaseName.ToString(),
		LastMovementSmokeLocalIntent.X,
		LastMovementSmokeLocalIntent.Y,
		LastMovementSmokeWorldIntent.X,
		LastMovementSmokeWorldIntent.Y,
		LastMovementSmokeOwnerVelocityCmPerSecond.Size2D(),
		LastActionDiagnostics.RawMin,
		LastActionDiagnostics.RawMax,
		LastActionDiagnostics.RawMeanAbs,
		LastActionDiagnostics.ConditionedMeanAbs,
		LastActionDiagnostics.NumClampedActionFloats,
		LastControlTargetDiagnostics.bPolicyInfluenceActive ? TEXT("true") : TEXT("false"),
		LastControlTargetDiagnostics.bFirstPolicyEnabledFrame ? TEXT("true") : TEXT("false"),
		LastControlTargetDiagnostics.NumPolicyTargetsWritten,
		*LastControlTargetDiagnostics.MaxTargetDeltaBoneName.ToString(),
		LastControlTargetDiagnostics.MaxTargetDeltaDegrees,
		LastControlTargetDiagnostics.MeanTargetDeltaDegrees,
		*LastControlTargetDiagnostics.MaxRawPolicyOffsetBoneName.ToString(),
		LastControlTargetDiagnostics.MaxRawPolicyOffsetDegrees,
		LastControlTargetDiagnostics.MeanRawPolicyOffsetDegrees,
		LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm,
		LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond,
		LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond,
		LastRuntimeInstabilityDiagnostics.UnstableAccumulatedSeconds,
		LastRuntimeInstabilityDiagnostics.NumBodiesConsidered,
		LastRuntimeInstabilityDiagnostics.NumSimulatingBodies,
		*LastRuntimeInstabilityDiagnostics.MaxLinearSpeedBoneName.ToString(),
		LastRuntimeInstabilityDiagnostics.bMaxLinearSpeedBoneSimulatingPhysics ? TEXT("sim") : TEXT("kin"),
		LastRuntimeInstabilityDiagnostics.MaxBodyLinearSpeedCmPerSecond,
		*LastRuntimeInstabilityDiagnostics.MaxAngularSpeedBoneName.ToString(),
		LastRuntimeInstabilityDiagnostics.bMaxAngularSpeedBoneSimulatingPhysics ? TEXT("sim") : TEXT("kin"),
		LastRuntimeInstabilityDiagnostics.MaxBodyAngularSpeedDegPerSecond,
		*LastRuntimeInstabilityDiagnostics.MaxHeightDeltaBoneName.ToString(),
		LastRuntimeInstabilityDiagnostics.bMaxHeightDeltaBoneSimulatingPhysics ? TEXT("sim") : TEXT("kin"),
		LastRuntimeInstabilityDiagnostics.MaxBodyHeightDeltaCm);
}

void UPhysAnimComponent::ResetStabilizationRuntimeState()
{
	ConditionedActionBuffer.Reset();
	PreviousConditionedActionBuffer.Reset();
	PreviousControlTargetRotations.Reset();
	LastActionDiagnostics = {};
	LastControlTargetDiagnostics = {};
	RuntimeInstabilityState = {};
	LastRuntimeInstabilityDiagnostics = {};
	SimulationHandoffAlpha = 0.0f;
	bLastAppliedSimulationHandoffSettled = false;
	LastAppliedControlAuthorityAlpha = -1.0f;
	BridgeStartTimeSeconds = 0.0;
	SimulationHandoffCompletedTimeSeconds = -1.0;
	PolicyInfluenceRampStartTimeSeconds = -1.0;
	HighestUnlockedBringUpGroupIndex = INDEX_NONE;
	BringUpGroupStableAccumulatedSeconds = 0.0f;
	BringUpGroupActivationTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	BringUpGroupControlRampStartTimeSeconds.Init(-1.0, GetBringUpGroupCount());
	PendingBodyModifierCachedResetNames.Reset();
	LastRuntimeDiagnosticsLogTimeSeconds = -1.0;
	PolicyUpdateAccumulatorSeconds = -1.0f;
	LastPolicyElapsedSteps = 0;
	PolicyControlTicksExecuted = 0;
	PolicyControlTicksSkipped = 0;
	LastPolicyControlUpdateTimeSeconds = -1.0;
	LastMovementSmokeLocalIntent = FVector::ZeroVector;
	LastMovementSmokeWorldIntent = FVector::ZeroVector;
	LastMovementSmokeOwnerVelocityCmPerSecond = FVector::ZeroVector;
	MovementSmokeStartLocation = FVector::ZeroVector;
	LastMovementSmokePhaseName = NAME_None;
	bMovementSmokeScriptStarted = false;
	bMovementSmokeCompletionLogged = false;
	PresentationPerturbationOverrideEndTimeSeconds = -1.0;
	bLastAppliedPresentationRootSimulationEnabled = false;
	StabilizationStressTestStartTimeSeconds = -1.0;
	bStabilizationStressTestCompletionLogged = false;
	StabilizationStressTestFirstAngularSpikeTimeSeconds = -1.0;
	StabilizationStressTestFirstLinearSpikeTimeSeconds = -1.0;
	StabilizationStressTestFirstInstabilitySignTimeSeconds = -1.0;
	StabilizationStressTestFirstAngularSpikeMultiplier = 1.0f;
	StabilizationStressTestFirstLinearSpikeMultiplier = 1.0f;
	StabilizationStressTestFirstInstabilityMultiplier = 1.0f;
	StabilizationStressTestFirstAngularSpikeBoneName = NAME_None;
	StabilizationStressTestFirstLinearSpikeBoneName = NAME_None;
	StabilizationStressTestBaselineActorLocation = FVector::ZeroVector;
	StabilizationStressTestBaselineSpineLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineHeadLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineLeftFootLocalOffset = FVector::ZeroVector;
	StabilizationStressTestBaselineRightFootLocalOffset = FVector::ZeroVector;
	OriginalBodyMassScales.Reset();
	bHasSavedBodyMassScales = false;
	PolicyBlendStartControlTargetRotations.Reset();
	bPolicyTargetsAppliedLastFrame = false;
	LastAppliedStabilizationSettings = {};
}

void UPhysAnimComponent::FailStop(const FString& Reason)
{
	if (PhysAnimComponentInternal::CVarPaStabilizationStressTest.GetValueOnGameThread() > 0 &&
		StabilizationStressTestStartTimeSeconds >= 0.0)
	{
		const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : StabilizationStressTestStartTimeSeconds;
		const double ElapsedSinceStartSeconds = CurrentTimeSeconds - StabilizationStressTestStartTimeSeconds;
		const double CascadeSeconds =
			(StabilizationStressTestFirstInstabilitySignTimeSeconds >= 0.0)
				? (CurrentTimeSeconds - StabilizationStressTestFirstInstabilitySignTimeSeconds)
				: -1.0;
		UE_LOG(
			LogPhysAnimBridge,
			Error,
			TEXT("[PhysAnim] Stabilization stress-test collapse: multiplier=%.2f elapsed=%.2fs firstAngularSpike=%s:%.2f firstLinearSpike=%s:%.2f firstInstability=%.2f onsetToCollapse=%.2fs"),
			ResolveStabilizationStressTestMultiplier(),
			ElapsedSinceStartSeconds,
			*StabilizationStressTestFirstAngularSpikeBoneName.ToString(),
			StabilizationStressTestFirstAngularSpikeMultiplier,
			*StabilizationStressTestFirstLinearSpikeBoneName.ToString(),
			StabilizationStressTestFirstLinearSpikeMultiplier,
			StabilizationStressTestFirstInstabilityMultiplier,
			CascadeSeconds);
	}

	LogBridgeStateSnapshot(TEXT("FailStop"));
	UE_LOG(LogPhysAnimBridge, Error, TEXT("[PhysAnim] Fail-stop: %s"), *Reason);
	DeactivateRuntimePhysicsControl(TEXT("FailStop"));
	ResetBridgePhysicsState();
	TransitionRuntimeState(EPhysAnimRuntimeState::FailStopped);
	SetComponentTickEnabled(false);
	ResetStabilizationRuntimeState();
}

bool UPhysAnimComponent::IsInitialPoseSearchWaitTimedOut(double ElapsedSeconds, double TimeoutSeconds)
{
	return TimeoutSeconds > 0.0 && ElapsedSeconds >= TimeoutSeconds;
}

FQuat UPhysAnimComponent::BuildCurrentPoseControlTargetOrientation(
	const FQuat& ParentWorldRotation,
	const FQuat& ChildWorldRotation)
{
	return (ParentWorldRotation.Inverse() * ChildWorldRotation).GetNormalized();
}

void UPhysAnimComponent::ResolveBodyModifierRuntimeMode(
	bool bForceZeroActions,
	bool bSimulationHandoffSettled,
	bool bBringUpGroupUnlocked,
	bool bIsRootBodyModifier,
	bool bAllowRootBodyModifierSimulation,
	EPhysicsMovementType& OutMovementType,
	float& OutPhysicsBlendWeight,
	bool& bOutUpdateKinematicFromSimulation)
{
	if (bForceZeroActions || !bSimulationHandoffSettled || !bBringUpGroupUnlocked || (bIsRootBodyModifier && !bAllowRootBodyModifierSimulation))
	{
		OutMovementType = EPhysicsMovementType::Kinematic;
		OutPhysicsBlendWeight = 0.0f;
		bOutUpdateKinematicFromSimulation = false;
		return;
	}

	OutMovementType = EPhysicsMovementType::Simulated;
	OutPhysicsBlendWeight = 1.0f;
	bOutUpdateKinematicFromSimulation = false;
}

ECollisionEnabled::Type UPhysAnimComponent::ResolveBodyModifierCollisionType(
	bool bForceZeroActions,
	bool bSimulationHandoffSettled,
	bool bBringUpGroupUnlocked,
	bool bIsRootBodyModifier,
	bool bAllowRootBodyModifierSimulation)
{
	if (bForceZeroActions || !bSimulationHandoffSettled || !bBringUpGroupUnlocked || (bIsRootBodyModifier && !bAllowRootBodyModifierSimulation))
	{
		return ECollisionEnabled::NoCollision;
	}

	return ECollisionEnabled::QueryAndPhysics;
}

bool UPhysAnimComponent::ShouldResetBodyModifierToCachedBoneTransform(
	bool bForceZeroActions,
	bool bBodyModifierActivatedThisTick,
	bool bBringUpGroupUnlocked,
	bool bIsRootBodyModifier,
	bool bAllowRootBodyModifierSimulation)
{
	return !bForceZeroActions &&
		bBodyModifierActivatedThisTick &&
		bBringUpGroupUnlocked &&
		(!bIsRootBodyModifier || bAllowRootBodyModifierSimulation);
}

int32 UPhysAnimComponent::ResolveBringUpGroupIndex(FName BoneName)
{
	if (BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03") ||
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("thigh_r"))
	{
		return 0;
	}

	if (BoneName == TEXT("calf_l") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("foot_r") ||
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("ball_r"))
	{
		return 1;
	}

	if (BoneName == TEXT("clavicle_l") ||
		BoneName == TEXT("clavicle_r") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("lowerarm_r"))
	{
		return 2;
	}

	if (BoneName == TEXT("neck_01") ||
		BoneName == TEXT("head"))
	{
		return 3;
	}

	if (BoneName == TEXT("hand_l") ||
		BoneName == TEXT("hand_r"))
	{
		return 4;
	}

	return INDEX_NONE;
}

int32 UPhysAnimComponent::GetBringUpGroupCount()
{
	return PhysAnimComponentInternal::NumBringUpGroups;
}

bool UPhysAnimComponent::ShouldDelayBringUpGroupControlRamp(int32 GroupIndex, int32 NumBringUpGroups)
{
	return NumBringUpGroups > 0 && GroupIndex == (NumBringUpGroups - 1);
}

bool UPhysAnimComponent::ShouldStartBringUpGroupControlRamp(
	bool bForceZeroActions,
	bool bBringUpGroupUnlocked,
	bool bDelayBringUpGroupControlRamp,
	bool bPostUnlockSettleComplete)
{
	if (bForceZeroActions || !bBringUpGroupUnlocked)
	{
		return false;
	}

	return !bDelayBringUpGroupControlRamp || bPostUnlockSettleComplete;
}

bool UPhysAnimComponent::ShouldStartPolicyInfluenceRamp(
	bool bForceZeroActions,
	bool bAllBringUpGroupsUnlocked,
	bool bFinalBringUpGroupControlRampActive,
	bool bPostFinalGroupControlSettleComplete)
{
	if (bForceZeroActions || !bAllBringUpGroupsUnlocked || !bFinalBringUpGroupControlRampActive)
	{
		return false;
	}

	return bPostFinalGroupControlSettleComplete;
}

bool UPhysAnimComponent::ShouldApplyPolicyTargetToBone(FName BoneName, bool bPolicyInfluenceActive)
{
	if (!bPolicyInfluenceActive)
	{
		return false;
	}

	return BoneName != TEXT("clavicle_l") &&
		BoneName != TEXT("spine_01") &&
		BoneName != TEXT("spine_02") &&
		BoneName != TEXT("spine_03") &&
		BoneName != TEXT("upperarm_l") &&
		BoneName != TEXT("lowerarm_l") &&
		BoneName != TEXT("hand_l") &&
		BoneName != TEXT("neck_01") &&
		BoneName != TEXT("head") &&
		BoneName != TEXT("clavicle_r") &&
		BoneName != TEXT("upperarm_r") &&
		BoneName != TEXT("lowerarm_r") &&
		BoneName != TEXT("hand_r");
}

bool UPhysAnimComponent::ShouldUseSkeletalAnimationTargetRepresentation(
	bool bConfiguredUseSkeletalAnimationTargets,
	bool bPolicyInfluenceActive)
{
	return bConfiguredUseSkeletalAnimationTargets || bPolicyInfluenceActive;
}

bool UPhysAnimComponent::ShouldResetAllControlOffsetsForPolicyTargetRepresentationSwitch(
	bool bUseSkeletalAnimationTargetRepresentation,
	bool bFirstPolicyEnabledFrame)
{
	return bUseSkeletalAnimationTargetRepresentation && bFirstPolicyEnabledFrame;
}

float UPhysAnimComponent::ResolvePolicyTargetWriteDeltaTime(
	bool bUseSkeletalAnimationTargetRepresentation,
	bool bFirstPolicyEnabledFrame,
	float DeltaTime)
{
	return (bUseSkeletalAnimationTargetRepresentation && bFirstPolicyEnabledFrame) ? 0.0f : DeltaTime;
}

float UPhysAnimComponent::ResolvePolicyControlIntervalSeconds(float PolicyControlRateHz)
{
	const float ClampedRateHz = FMath::Max(PolicyControlRateHz, 1.0f);
	return 1.0f / ClampedRateHz;
}

float UPhysAnimComponent::ResolveTrainingAlignedMassScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (BoneName == TEXT("pelvis"))
	{
		TargetScale = 0.815f;
	}
	else if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("thigh_r") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_r") ||
		BoneName == TEXT("ball_r"))
	{
		TargetScale = 1.569f;
	}
	else if (
		BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03") ||
		BoneName == TEXT("spine_04") ||
		BoneName == TEXT("spine_05"))
	{
		TargetScale = 0.855f;
	}
	else if (
		BoneName == TEXT("neck_01") ||
		BoneName == TEXT("neck_02") ||
		BoneName == TEXT("head"))
	{
		TargetScale = 0.762f;
	}
	else if (
		BoneName == TEXT("clavicle_l") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("hand_l") ||
		BoneName == TEXT("clavicle_r") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_r") ||
		BoneName == TEXT("hand_r"))
	{
		TargetScale = 0.725f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}

bool UPhysAnimComponent::ShouldApplyTrainingAlignedMassScales(bool bApplyTrainingAlignedMassScales, float BlendAlpha)
{
	return bApplyTrainingAlignedMassScales && BlendAlpha > UE_SMALL_NUMBER;
}

float UPhysAnimComponent::ResolveTrainingAlignedControlStrengthScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (
		BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03"))
	{
		TargetScale = 1.25f;
	}
	else if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("thigh_r") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_r"))
	{
		TargetScale = 1.0f;
	}
	else if (
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("ball_r") ||
		BoneName == TEXT("neck_01") ||
		BoneName == TEXT("head") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_r"))
	{
		TargetScale = 0.625f;
	}
	else if (
		BoneName == TEXT("hand_l") ||
		BoneName == TEXT("hand_r"))
	{
		TargetScale = 0.375f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}

float UPhysAnimComponent::ResolveTrainingAlignedControlExtraDampingScaleForBone(FName BoneName, float BlendAlpha)
{
	const float ClampedBlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);
	float TargetScale = 1.0f;

	if (
		BoneName == TEXT("spine_01") ||
		BoneName == TEXT("spine_02") ||
		BoneName == TEXT("spine_03"))
	{
		TargetScale = 1.25f;
	}
	else if (
		BoneName == TEXT("thigh_l") ||
		BoneName == TEXT("calf_l") ||
		BoneName == TEXT("foot_l") ||
		BoneName == TEXT("thigh_r") ||
		BoneName == TEXT("calf_r") ||
		BoneName == TEXT("foot_r"))
	{
		TargetScale = 1.0f;
	}
	else if (
		BoneName == TEXT("ball_l") ||
		BoneName == TEXT("ball_r") ||
		BoneName == TEXT("neck_01") ||
		BoneName == TEXT("head") ||
		BoneName == TEXT("upperarm_l") ||
		BoneName == TEXT("lowerarm_l") ||
		BoneName == TEXT("upperarm_r") ||
		BoneName == TEXT("lowerarm_r"))
	{
		TargetScale = 0.625f;
	}
	else if (
		BoneName == TEXT("hand_l") ||
		BoneName == TEXT("hand_r"))
	{
		TargetScale = 0.375f;
	}

	return FMath::Lerp(1.0f, TargetScale, ClampedBlendAlpha);
}

bool UPhysAnimComponent::ShouldApplyTrainingAlignedControlFamilyProfile(bool bApplyTrainingAlignedControlFamilyProfile, float BlendAlpha)
{
	return bApplyTrainingAlignedControlFamilyProfile && BlendAlpha > UE_SMALL_NUMBER;
}

bool UPhysAnimComponent::AdvancePolicyControlAccumulator(
	float DeltaTimeSeconds,
	float PolicyControlIntervalSeconds,
	float& InOutAccumulatorSeconds,
	int32& OutElapsedSteps)
{
	OutElapsedSteps = 0;

	if (PolicyControlIntervalSeconds <= UE_SMALL_NUMBER)
	{
		InOutAccumulatorSeconds = 0.0f;
		OutElapsedSteps = 1;
		return true;
	}

	if (InOutAccumulatorSeconds < 0.0f)
	{
		InOutAccumulatorSeconds = PolicyControlIntervalSeconds;
	}

	InOutAccumulatorSeconds += FMath::Max(DeltaTimeSeconds, 0.0f);
	OutElapsedSteps = FMath::FloorToInt(InOutAccumulatorSeconds / PolicyControlIntervalSeconds);
	if (OutElapsedSteps <= 0)
	{
		return false;
	}

	InOutAccumulatorSeconds = FMath::Fmod(InOutAccumulatorSeconds, PolicyControlIntervalSeconds);
	return true;
}

FQuat UPhysAnimComponent::BlendPolicyTargetRotation(
	const FQuat& BaselineRotation,
	const FQuat& PolicyTargetRotation,
	float PolicyAlpha)
{
	const float ClampedPolicyAlpha = FMath::Clamp(PolicyAlpha, 0.0f, 1.0f);
	if (ClampedPolicyAlpha <= KINDA_SMALL_NUMBER)
	{
		return BaselineRotation;
	}

	if (ClampedPolicyAlpha >= (1.0f - KINDA_SMALL_NUMBER))
	{
		return PolicyTargetRotation;
	}

	return FQuat::Slerp(BaselineRotation, PolicyTargetRotation, ClampedPolicyAlpha).GetNormalized();
}

float UPhysAnimComponent::CalculateControlTargetDeltaDegrees(const FQuat& PreviousRotation, const FQuat& TargetRotation)
{
	return FMath::RadiansToDegrees(static_cast<float>(PreviousRotation.AngularDistance(TargetRotation)));
}

float UPhysAnimComponent::CalculateControlAuthorityAlpha(
	bool bForceZeroActions,
	bool bSimulationHandoffSettled,
	float ElapsedSinceHandoffSettledSeconds,
	float RampDurationSeconds)
{
	if (bForceZeroActions || !bSimulationHandoffSettled)
	{
		return 0.0f;
	}

	if (RampDurationSeconds <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp(ElapsedSinceHandoffSettledSeconds / RampDurationSeconds, 0.0f, 1.0f);
}

float UPhysAnimComponent::CalculateStabilizationStressTestMultiplier(
	int32 ProfileMode,
	float ElapsedSeconds,
	float RampDurationSeconds,
	float TargetMultiplier,
	float HoldSeconds,
	float RecoveryRampSeconds)
{
	const float ClampedTargetMultiplier = FMath::Clamp(TargetMultiplier, 0.0f, 1.0f);
	if (ElapsedSeconds <= 0.0f)
	{
		return 1.0f;
	}

	if (ProfileMode == 1)
	{
		const float ClampedRampDurationSeconds = FMath::Max(RampDurationSeconds, 0.0f);
		const float ClampedHoldSeconds = FMath::Max(HoldSeconds, 0.0f);
		const float ClampedRecoveryRampSeconds = FMath::Max(RecoveryRampSeconds, 0.0f);

		if (ClampedRampDurationSeconds <= UE_SMALL_NUMBER)
		{
			if (ElapsedSeconds < ClampedHoldSeconds)
			{
				return ClampedTargetMultiplier;
			}

			if (ClampedRecoveryRampSeconds <= UE_SMALL_NUMBER)
			{
				return 1.0f;
			}

			const float RecoveryAlpha =
				FMath::Clamp((ElapsedSeconds - ClampedHoldSeconds) / ClampedRecoveryRampSeconds, 0.0f, 1.0f);
			return FMath::Lerp(ClampedTargetMultiplier, 1.0f, RecoveryAlpha);
		}

		if (ElapsedSeconds <= ClampedRampDurationSeconds)
		{
			const float RampAlpha = FMath::Clamp(ElapsedSeconds / ClampedRampDurationSeconds, 0.0f, 1.0f);
			return FMath::Lerp(1.0f, ClampedTargetMultiplier, RampAlpha);
		}

		const float HoldEndSeconds = ClampedRampDurationSeconds + ClampedHoldSeconds;
		if (ElapsedSeconds <= HoldEndSeconds)
		{
			return ClampedTargetMultiplier;
		}

		if (ClampedRecoveryRampSeconds <= UE_SMALL_NUMBER)
		{
			return 1.0f;
		}

		const float RecoveryAlpha =
			FMath::Clamp((ElapsedSeconds - HoldEndSeconds) / ClampedRecoveryRampSeconds, 0.0f, 1.0f);
		return FMath::Lerp(ClampedTargetMultiplier, 1.0f, RecoveryAlpha);
	}

	if (RampDurationSeconds <= UE_SMALL_NUMBER)
	{
		return ClampedTargetMultiplier;
	}

	return FMath::Clamp(1.0f - ((1.0f - ClampedTargetMultiplier) * (ElapsedSeconds / RampDurationSeconds)), ClampedTargetMultiplier, 1.0f);
}

void UPhysAnimComponent::ApplyPresentationPerturbationStabilizationOverride(
	bool bOverrideActive,
	FPhysAnimStabilizationSettings& InOutSettings)
{
	if (!bOverrideActive)
	{
		return;
	}

	InOutSettings.AngularStrengthMultiplier *= PhysAnimComponentInternal::PresentationPerturbationStrengthRelaxationMultiplier;
	InOutSettings.AngularDampingRatioMultiplier *= PhysAnimComponentInternal::PresentationPerturbationDampingRatioRelaxationMultiplier;
	InOutSettings.AngularExtraDampingMultiplier *= PhysAnimComponentInternal::PresentationPerturbationExtraDampingRelaxationMultiplier;
}

void UPhysAnimComponent::ApplyStabilizationStressTestRamp(
	float Multiplier,
	int32 SweepMode,
	FPhysAnimStabilizationSettings& InOutSettings)
{
	const float ClampedMultiplier = FMath::Clamp(Multiplier, 0.0f, 1.0f);
	switch (SweepMode)
	{
	case 1:
		InOutSettings.AngularStrengthMultiplier *= ClampedMultiplier;
		break;
	case 2:
		InOutSettings.AngularDampingRatioMultiplier *= ClampedMultiplier;
		break;
	case 3:
		InOutSettings.AngularExtraDampingMultiplier *= ClampedMultiplier;
		break;
	default:
		InOutSettings.AngularStrengthMultiplier *= ClampedMultiplier;
		InOutSettings.AngularDampingRatioMultiplier *= ClampedMultiplier;
		InOutSettings.AngularExtraDampingMultiplier *= ClampedMultiplier;
		break;
	}
}

bool UPhysAnimComponent::ShouldPreserveGameplayShellDuringBridgeActive(
	bool bMovementSmokeModeEnabled,
	bool bAllowCharacterMovementInBridgeActive)
{
	return bMovementSmokeModeEnabled || bAllowCharacterMovementInBridgeActive;
}

FString UPhysAnimComponent::BuildBridgeStatusIndicatorText(EPhysAnimRuntimeState State, bool bBridgeOwnsPhysics)
{
	const TCHAR* const StateName = GetRuntimeStateName(State);
	return FString::Printf(
		TEXT("PhysAnim Bridge: %s (%s)"),
		StateName,
		bBridgeOwnsPhysics ? TEXT("ACTIVE") : TEXT("INACTIVE"));
}

FColor UPhysAnimComponent::ResolveBridgeStatusIndicatorColor(EPhysAnimRuntimeState State, bool bBridgeOwnsPhysics)
{
	if (State == EPhysAnimRuntimeState::FailStopped)
	{
		return FColor::Red;
	}

	if (bBridgeOwnsPhysics)
	{
		return FColor::Green;
	}

	if (State == EPhysAnimRuntimeState::ReadyForActivation)
	{
		return FColor::Yellow;
	}

	return FColor(160, 160, 160);
}

void UPhysAnimComponent::UpdateBridgeStatusIndicator(float DisplayDurationSeconds) const
{
	if (!GEngine || PhysAnimComponentInternal::CVarPhysAnimShowBridgeStatusIndicator.GetValueOnGameThread() == 0)
	{
		return;
	}

	const uint64 MessageKey = static_cast<uint64>(reinterpret_cast<UPTRINT>(this));
	const bool bBridgeOwnsPhysics = RuntimeStateOwnsBridgePhysics(RuntimeState);
	const FString Message = BuildBridgeStatusIndicatorText(RuntimeState, bBridgeOwnsPhysics);
	const FColor Color = ResolveBridgeStatusIndicatorColor(RuntimeState, bBridgeOwnsPhysics);
	GEngine->AddOnScreenDebugMessage(MessageKey, DisplayDurationSeconds, Color, Message, false, FVector2D(1.25f, 1.25f));
}

void UPhysAnimComponent::ResolveRuntimeInstabilityRootFrame(
	bool bPreserveGameplayShell,
	const FVector& RootLocationCm,
	const FVector& RootLinearVelocityCmPerSecond,
	const FVector& OwnerLocationCm,
	const FVector& OwnerLinearVelocityCmPerSecond,
	FVector& OutEffectiveRootLocationCm,
	FVector& OutEffectiveRootLinearVelocityCmPerSecond)
{
	OutEffectiveRootLocationCm = RootLocationCm;
	OutEffectiveRootLinearVelocityCmPerSecond = RootLinearVelocityCmPerSecond;

	if (!bPreserveGameplayShell)
	{
		return;
	}

	OutEffectiveRootLocationCm -= OwnerLocationCm;
	OutEffectiveRootLinearVelocityCmPerSecond -= OwnerLinearVelocityCmPerSecond;
}

FVector UPhysAnimComponent::ResolveMovementSmokeLocalIntent(float ElapsedSeconds)
{
	if (ElapsedSeconds < 0.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 3.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 8.0f)
	{
		return FVector(1.0f, 0.0f, 0.0f);
	}
	if (ElapsedSeconds < 11.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 16.0f)
	{
		return FVector(0.0f, -1.0f, 0.0f);
	}
	if (ElapsedSeconds < 19.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < 24.0f)
	{
		return FVector(0.0f, 1.0f, 0.0f);
	}
	if (ElapsedSeconds < 27.0f)
	{
		return FVector::ZeroVector;
	}
	if (ElapsedSeconds < PhysAnimComponentInternal::MovementSmokeTimelineDurationSeconds)
	{
		return FVector(-1.0f, 0.0f, 0.0f);
	}

	return FVector::ZeroVector;
}

FName UPhysAnimComponent::ResolveMovementSmokePhaseName(float ElapsedSeconds)
{
	if (ElapsedSeconds < 0.0f)
	{
		return TEXT("Inactive");
	}
	if (ElapsedSeconds < 3.0f)
	{
		return TEXT("Idle_00");
	}
	if (ElapsedSeconds < 8.0f)
	{
		return TEXT("Forward");
	}
	if (ElapsedSeconds < 11.0f)
	{
		return TEXT("Idle_01");
	}
	if (ElapsedSeconds < 16.0f)
	{
		return TEXT("StrafeLeft");
	}
	if (ElapsedSeconds < 19.0f)
	{
		return TEXT("Idle_02");
	}
	if (ElapsedSeconds < 24.0f)
	{
		return TEXT("StrafeRight");
	}
	if (ElapsedSeconds < 27.0f)
	{
		return TEXT("Idle_03");
	}
	if (ElapsedSeconds < PhysAnimComponentInternal::MovementSmokeTimelineDurationSeconds)
	{
		return TEXT("Backward");
	}

	return TEXT("Complete");
}

float UPhysAnimComponent::GetMovementSmokeDurationSeconds()
{
	return PhysAnimComponentInternal::MovementSmokeTimelineDurationSeconds;
}

float UPhysAnimComponent::GetMovementSmokeTotalDurationSeconds(int32 NumLoops)
{
	return GetMovementSmokeDurationSeconds() * FMath::Max(NumLoops, 1);
}

float UPhysAnimComponent::CalculatePolicyInfluenceAlpha(
	bool bForceZeroActions,
	bool bAllBringUpGroupsUnlocked,
	float ElapsedSinceAllBringUpGroupsUnlockedSeconds,
	float RampDurationSeconds)
{
	if (bForceZeroActions || !bAllBringUpGroupsUnlocked)
	{
		return 0.0f;
	}

	if (RampDurationSeconds <= 0.0f)
	{
		return 1.0f;
	}

	return FMath::Clamp(ElapsedSinceAllBringUpGroupsUnlockedSeconds / RampDurationSeconds, 0.0f, 1.0f);
}

bool UPhysAnimComponent::ShouldSuspendPolicyInfluenceDuringPresentationPerturbation(
	bool bPresentationPerturbationOverrideActive)
{
	(void)bPresentationPerturbationOverrideActive;
	return false;
}

EPhysAnimRuntimeState UPhysAnimComponent::ResolveInitialPoseSearchSuccessState(bool bForceZeroActions)
{
	return bForceZeroActions
		? EPhysAnimRuntimeState::ReadyForActivation
		: EPhysAnimRuntimeState::BridgeActive;
}

bool UPhysAnimComponent::ShouldActivateBridgeFromSafeMode(EPhysAnimRuntimeState State, bool bForceZeroActions)
{
	return State == EPhysAnimRuntimeState::ReadyForActivation && !bForceZeroActions;
}

bool UPhysAnimComponent::ShouldDeactivateBridgeToSafeMode(EPhysAnimRuntimeState State, bool bForceZeroActions)
{
	return State == EPhysAnimRuntimeState::BridgeActive && bForceZeroActions;
}

bool UPhysAnimComponent::RuntimeStateOwnsBridgePhysics(EPhysAnimRuntimeState State)
{
	return State == EPhysAnimRuntimeState::BridgeActive;
}

const TCHAR* UPhysAnimComponent::GetRuntimeStateName(EPhysAnimRuntimeState State)
{
	switch (State)
	{
	case EPhysAnimRuntimeState::Uninitialized:
		return TEXT("Uninitialized");
	case EPhysAnimRuntimeState::RuntimeReady:
		return TEXT("RuntimeReady");
	case EPhysAnimRuntimeState::WaitingForPoseSearch:
		return TEXT("WaitingForPoseSearch");
	case EPhysAnimRuntimeState::ReadyForActivation:
		return TEXT("ReadyForActivation");
	case EPhysAnimRuntimeState::BridgeActive:
		return TEXT("BridgeActive");
	case EPhysAnimRuntimeState::FailStopped:
		return TEXT("FailStopped");
	default:
		return TEXT("Unknown");
	}
}

void UPhysAnimComponent::TransitionRuntimeState(EPhysAnimRuntimeState NewState)
{
	if (RuntimeState == NewState)
	{
		return;
	}

	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime state: %s -> %s"),
		GetRuntimeStateName(RuntimeState),
		GetRuntimeStateName(NewState));
	RuntimeState = NewState;
	UpdateBridgeStatusIndicator(60.0f);
}

UE::NNE::IModelInstanceRunSync* UPhysAnimComponent::GetModelInstanceRunSync() const
{
	if (ModelInstanceGPU.IsValid())
	{
		return ModelInstanceGPU.Get();
	}

	if (ModelInstanceCPU.IsValid())
	{
		return ModelInstanceCPU.Get();
	}

	return nullptr;
}

TConstArrayView<UE::NNE::FTensorDesc> UPhysAnimComponent::GetInputTensorDescs() const
{
	if (const UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync())
	{
		return ModelInstance->GetInputTensorDescs();
	}

	return {};
}

TConstArrayView<UE::NNE::FTensorDesc> UPhysAnimComponent::GetOutputTensorDescs() const
{
	if (const UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync())
	{
		return ModelInstance->GetOutputTensorDescs();
	}

	return {};
}
