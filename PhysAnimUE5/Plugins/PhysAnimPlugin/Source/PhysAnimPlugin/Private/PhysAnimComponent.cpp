#include "PhysAnimComponent.h"

#include "PhysAnimBridge.h"
#include "PhysAnimStage1InitializerComponent.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"
#include "NNEStatus.h"
#include "PhysicsControlActor.h"
#include "PhysicsControlComponent.h"
#include "PhysicsEngine/PhysicsAsset.h"
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
			TEXT("Zero=%s Scale=%.3f Clamp=%.3f Smooth=%.3f Ramp=%.3f StepDegPerSec=%.1f GainMul=%.3f DampMul=%.3f ExtraDampMul=%.3f SkeletalTargets=%s InstabilityStop=%s HeightCm=%.1f LinCmPerSec=%.1f AngDegPerSec=%.1f Grace=%.2f"),
			Settings.bForceZeroActions ? TEXT("true") : TEXT("false"),
			Settings.ActionScale,
			Settings.ActionClampAbs,
			Settings.ActionSmoothingAlpha,
			Settings.StartupRampSeconds,
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

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
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

	if (bPendingSimulationHandoff)
	{
		bPendingSimulationHandoff = false;
		ApplyRuntimeControlTuning(EffectiveSettings);
		PhysicsControl->UpdateControls(0.0f);
		LogBridgeStateSnapshot(TEXT("AfterSimulationHandoff"));
		LogActivationSummary(EffectiveSettings, TEXT("SimulationHandoffComplete"), true, true, false);
		return;
	}

	PhysicsControl->UpdateTargetCaches(DeltaTime);
	PhysicsControl->GetCachedBoneTransforms(SkeletalMesh, PhysAnimBridge::GetControlledBoneNames());

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
			FailStop(FString::Printf(TEXT("PoseSearch query was invalid for two consecutive ticks. %s"), *TickError));
			return;
		}

		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnim] Reusing last valid PoseSearch result for one grace tick. Reason: %s"), *TickError);
		SearchResult = LastValidPoseSearchResult;
	}

	TArray<FPhysAnimBodySample> CurrentBodySamples;
	if (!GatherCurrentBodySamples(CurrentBodySamples, TickError))
	{
		FailStop(TickError);
		return;
	}

	if (!CheckRuntimeInstability(DeltaTime, EffectiveSettings, TickError))
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

	ApplyRuntimeControlTuning(EffectiveSettings);
	if (!ConditionModelActions(EffectiveSettings, TickError))
	{
		FailStop(TickError);
		return;
	}

	ApplyControlTargets(DeltaTime, TickError);
	if (!TickError.IsEmpty())
	{
		FailStop(TickError);
		return;
	}

	PhysicsControl->UpdateControls(DeltaTime);
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
	SetComponentTickEnabled(false);
	ConsecutiveInvalidPoseSearchFrames = 0;
	LastValidPoseSearchResult = FPoseSearchBlueprintResult();
	ResetStabilizationRuntimeState();
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
	ActivateBridgePhysicsState();
	LogBridgeStateSnapshot(TEXT("AfterActivateBridgePhysicsState"));
	ResetStabilizationRuntimeState();
	BridgeStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	PhysicsControl->UpdateTargetCaches(0.0f);
	if (!SeedControlTargetsFromCurrentPose(0.0f, OutError))
	{
		return false;
	}
	bPendingSimulationHandoff = true;
	ApplyRuntimeControlTuning(EffectiveSettings);
	PhysicsControl->UpdateControls(0.0f);
	LogBridgeStateSnapshot(TEXT("AfterActivationPrepass"));
	LogActivationSummary(EffectiveSettings, ActivationContext, true, true, true);

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
	bool bSimulationHandoffPending) const
{
	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Activation[%s]: skeletalTargets=%s currentPoseTargetsSeeded=%s activationPrepassCompleted=%s simulationHandoff=%s"),
		Context,
		EffectiveSettings.bUseSkeletalAnimationTargets ? TEXT("true") : TEXT("false"),
		bCurrentPoseTargetsSeeded ? TEXT("true") : TEXT("false"),
		bActivationPrepassCompleted ? TEXT("true") : TEXT("false"),
		bSimulationHandoffPending ? TEXT("pending") : TEXT("complete"));
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

void UPhysAnimComponent::ActivateBridgePhysicsState()
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

	if (ACharacter* const CharacterOwner = Cast<ACharacter>(GetOwner()))
	{
		if (UCapsuleComponent* const CapsuleComponent = CharacterOwner->GetCapsuleComponent())
		{
			if (!bHasSavedCapsuleCollisionState)
			{
				OriginalCapsuleCollisionEnabled = CapsuleComponent->GetCollisionEnabled();
				bHasSavedCapsuleCollisionState = true;
			}

			CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

			CharacterMovement->DisableMovement();
			CharacterMovement->SetComponentTickEnabled(false);
		}
	}
}

void UPhysAnimComponent::ResetBridgePhysicsState()
{
	USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
	if (!SkeletalMesh)
	{
		bHasSavedMeshCollisionState = false;
		return;
	}

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
	if (!PhysicsControl ||
		(EffectiveSettings == LastAppliedStabilizationSettings &&
		 bPendingSimulationHandoff == bLastAppliedPendingSimulationHandoff))
	{
		return;
	}

	FPhysicsControlMultiplier ControlMultiplier;
	ControlMultiplier.AngularStrengthMultiplier = EffectiveSettings.AngularStrengthMultiplier;
	ControlMultiplier.AngularDampingRatioMultiplier = EffectiveSettings.AngularDampingRatioMultiplier;
	ControlMultiplier.AngularExtraDampingMultiplier = EffectiveSettings.AngularExtraDampingMultiplier;
	PhysicsControl->SetControlMultipliersInSet(TEXT("All"), ControlMultiplier, !EffectiveSettings.bForceZeroActions);
	PhysicsControl->SetControlsInSetEnabled(TEXT("All"), !EffectiveSettings.bForceZeroActions);
	PhysicsControl->SetControlsInSetUseSkeletalAnimation(
		TEXT("All"),
		EffectiveSettings.bUseSkeletalAnimationTargets,
		0.0f,
		0.0f);

	EPhysicsMovementType BodyModifierMovementType = EPhysicsMovementType::Kinematic;
	float BodyModifierPhysicsBlendWeight = 0.0f;
	ResolveBodyModifierRuntimeMode(
		EffectiveSettings.bForceZeroActions,
		bPendingSimulationHandoff,
		BodyModifierMovementType,
		BodyModifierPhysicsBlendWeight);
	PhysicsControl->SetBodyModifiersInSetMovementType(TEXT("All"), BodyModifierMovementType);
	PhysicsControl->SetBodyModifiersInSetPhysicsBlendWeight(TEXT("All"), BodyModifierPhysicsBlendWeight);

	LastAppliedStabilizationSettings = EffectiveSettings;
	bLastAppliedPendingSimulationHandoff = bPendingSimulationHandoff;
}

bool UPhysAnimComponent::ConditionModelActions(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError)
{
	FPhysAnimActionConditioningSettings ConditioningSettings;
	ConditioningSettings.bForceZeroActions = EffectiveSettings.bForceZeroActions;
	ConditioningSettings.ActionClampAbs = EffectiveSettings.ActionClampAbs;
	ConditioningSettings.ActionSmoothingAlpha = EffectiveSettings.ActionSmoothingAlpha;

	float RampAlpha = 1.0f;
	if (EffectiveSettings.StartupRampSeconds > 0.0f)
	{
		const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : BridgeStartTimeSeconds;
		const double ElapsedSeconds = CurrentTimeSeconds - BridgeStartTimeSeconds;
		RampAlpha = FMath::Clamp(
			static_cast<float>(ElapsedSeconds / static_cast<double>(EffectiveSettings.StartupRampSeconds)),
			0.0f,
			1.0f);
	}

	ConditioningSettings.ActionScale = EffectiveSettings.ActionScale * RampAlpha;
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

	FString InstabilityError;
	const bool bStable = EvaluateRuntimeInstability(
		RootLocationCm,
		RootLinearVelocityCmPerSecond,
		RootAngularVelocityDegPerSecond,
		DeltaTime,
		InstabilitySettings,
		RuntimeInstabilityState,
		LastRuntimeInstabilityDiagnostics,
		InstabilityError);
	if (!bStable)
	{
		OutError = InstabilityError;
	}

	return bStable;
}

void UPhysAnimComponent::ApplyControlTargets(float DeltaTime, FString& OutError)
{
	UPhysicsControlComponent* const PhysicsControl = PhysicsControlComponent.Get();
	if (!PhysicsControl)
	{
		OutError = TEXT("Physics Control component was not resolved.");
		return;
	}

	TMap<FName, FQuat> ControlRotations;
	if (!PhysAnimBridge::ConvertModelActionsToControlRotations(ConditionedActionBuffer, ControlRotations, OutError))
	{
		return;
	}

	const FPhysAnimStabilizationSettings EffectiveSettings = ResolveEffectiveStabilizationSettings();
	if (EffectiveSettings.bForceZeroActions)
	{
		PreviousControlTargetRotations.Reset();
		return;
	}

	const float MaxAngularStepDegrees = FMath::Max(0.0f, EffectiveSettings.MaxAngularStepDegreesPerSecond) * DeltaTime;

	for (const TPair<FName, FQuat>& Pair : ControlRotations)
	{
		const FName ControlName = PhysAnimBridge::MakeControlName(Pair.Key);
		if (!PhysicsControl->GetControlExists(ControlName))
		{
			OutError = FString::Printf(TEXT("Missing required control '%s' during target write."), *ControlName.ToString());
			return;
		}

		const FQuat* const PreviousRotation = PreviousControlTargetRotations.Find(ControlName);
		const FQuat LimitedRotation = PreviousRotation
			? LimitTargetRotationStep(*PreviousRotation, Pair.Value, MaxAngularStepDegrees)
			: Pair.Value;
		PreviousControlTargetRotations.Add(ControlName, LimitedRotation);

		PhysicsControl->SetControlTargetOrientation(
			ControlName,
			LimitedRotation.Rotator(),
			DeltaTime,
			true,
			false,
			true,
			false);
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
	UE_LOG(
		LogPhysAnimBridge,
		Log,
		TEXT("[PhysAnim] Runtime diagnostics: action[rawMin=%.3f rawMax=%.3f rawMeanAbs=%.3f conditionedMeanAbs=%.3f clamped=%d] root[heightDeltaCm=%.1f linearCmPerSec=%.1f angularDegPerSec=%.1f unstableFor=%.2f]"),
		LastActionDiagnostics.RawMin,
		LastActionDiagnostics.RawMax,
		LastActionDiagnostics.RawMeanAbs,
		LastActionDiagnostics.ConditionedMeanAbs,
		LastActionDiagnostics.NumClampedActionFloats,
		LastRuntimeInstabilityDiagnostics.RootHeightDeltaCm,
		LastRuntimeInstabilityDiagnostics.RootLinearSpeedCmPerSecond,
		LastRuntimeInstabilityDiagnostics.RootAngularSpeedDegPerSecond,
		LastRuntimeInstabilityDiagnostics.UnstableAccumulatedSeconds);
}

void UPhysAnimComponent::ResetStabilizationRuntimeState()
{
	ConditionedActionBuffer.Reset();
	PreviousConditionedActionBuffer.Reset();
	PreviousControlTargetRotations.Reset();
	LastActionDiagnostics = {};
	RuntimeInstabilityState = {};
	LastRuntimeInstabilityDiagnostics = {};
	bPendingSimulationHandoff = false;
	bLastAppliedPendingSimulationHandoff = false;
	BridgeStartTimeSeconds = 0.0;
	LastRuntimeDiagnosticsLogTimeSeconds = -1.0;
	LastAppliedStabilizationSettings = {};
}

void UPhysAnimComponent::FailStop(const FString& Reason)
{
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
	bool bPendingSimulationHandoff,
	EPhysicsMovementType& OutMovementType,
	float& OutPhysicsBlendWeight)
{
	if (bForceZeroActions || bPendingSimulationHandoff)
	{
		OutMovementType = EPhysicsMovementType::Kinematic;
		OutPhysicsBlendWeight = 0.0f;
		return;
	}

	OutMovementType = EPhysicsMovementType::Simulated;
	OutPhysicsBlendWeight = 1.0f;
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
