#pragma once

#include "Components/ActorComponent.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsControlActor.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PhysAnimBridge.h"

#include "PhysAnimComponent.generated.h"

class UAnimInstance;
class UCapsuleComponent;
class UCharacterMovementComponent;
class UPhysicsControlComponent;
class UPoseSearchDatabase;
class USkeletalMeshComponent;

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
			bApplyTrainingAlignedToeLimitPolicy == Other.bApplyTrainingAlignedToeLimitPolicy &&
			FMath::IsNearlyEqual(TrainingAlignedToeLimitPolicyBlend, Other.TrainingAlignedToeLimitPolicyBlend) &&
			bApplyTrainingAlignedLowerLimbTargetRangePolicy == Other.bApplyTrainingAlignedLowerLimbTargetRangePolicy &&
			FMath::IsNearlyEqual(TrainingAlignedLowerLimbTargetRangePolicyBlend, Other.TrainingAlignedLowerLimbTargetRangePolicyBlend) &&
			bApplyTrainingAlignedDistalLocomotionTargetPolicy == Other.bApplyTrainingAlignedDistalLocomotionTargetPolicy &&
			FMath::IsNearlyEqual(TrainingAlignedDistalLocomotionTargetPolicyBlend, Other.TrainingAlignedDistalLocomotionTargetPolicyBlend) &&
			FMath::IsNearlyEqual(DistalLocomotionTargetPolicyActivationSpeedCmPerSec, Other.DistalLocomotionTargetPolicyActivationSpeedCmPerSec) &&
			bApplyTrainingAlignedDistalLocomotionCompositionPolicy == Other.bApplyTrainingAlignedDistalLocomotionCompositionPolicy &&
			FMath::IsNearlyEqual(DistalLocomotionCompositionPolicyActivationSpeedCmPerSec, Other.DistalLocomotionCompositionPolicyActivationSpeedCmPerSec) &&
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
			FMath::IsNearlyEqual(InstabilityGracePeriodSeconds, Other.InstabilityGracePeriodSeconds);
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

UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent))
class PHYSANIMPLUGIN_API UPhysAnimComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhysAnimComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	FPhysAnimStabilizationSettings StabilizationSettings;

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
	bool RunInference(FString& OutError);
	FPhysAnimStabilizationSettings ResolveEffectiveStabilizationSettings() const;
	void LogBridgeStateSnapshot(const TCHAR* Context) const;
	bool ActivateRuntimePhysicsControl(FString& OutError);
	void DeactivateRuntimePhysicsControl(const TCHAR* Context);
	bool ActivateBridgeFromReadyState(const FPhysAnimStabilizationSettings& EffectiveSettings, const TCHAR* ActivationContext, FString& OutError);
	void EnterReadyForActivation(const FPhysAnimStabilizationSettings& EffectiveSettings, const TCHAR* Context, bool bLogDeferredStartupSuccess);
	void ActivateBridgePhysicsState(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ApplyTrainingAlignedMassScales(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResetTrainingAlignedMassScales();
	void ApplyTrainingAlignedToeLimitPolicy(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void ResetTrainingAlignedToeLimitPolicy();
	void ResetBridgePhysicsState();
	bool SeedControlTargetsFromCurrentPose(float DeltaTime, FString& OutError);
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
	void ApplyControlTargets(
		float PolicyStepDeltaTime,
		const FPhysAnimStabilizationSettings& EffectiveSettings,
		bool bApplyNewPolicyStepThisTick,
		FString& OutError);
	bool IsMovementSmokeModeEnabled() const;
	void ApplyMovementSmokeInput(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void MaybeLogRuntimeDiagnostics(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	void ResetStabilizationRuntimeState();
	void FailStop(const FString& Reason);
	void UpdateStabilizationStressTestState(const FPhysAnimStabilizationSettings& EffectiveSettings);
	void TrackStabilizationStressTestObservations();
	float ResolveStabilizationStressTestMultiplier() const;
	float CalculateSimulationHandoffAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	float CalculateCurrentControlAuthorityAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	float CalculateCurrentPolicyInfluenceAlpha(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	bool IsPresentationPerturbationOverrideActive() const;

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
	FString ActiveRuntimeName;
	FPhysAnimStabilizationSettings LastAppliedStabilizationSettings;
	FPhysAnimActionDiagnostics LastActionDiagnostics;
	FPhysAnimControlTargetDiagnostics LastControlTargetDiagnostics;
	FPhysAnimRuntimeInstabilityState RuntimeInstabilityState;
	FPhysAnimRuntimeInstabilityDiagnostics LastRuntimeInstabilityDiagnostics;
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
	FVector LastMovementSmokeLocalIntent = FVector::ZeroVector;
	FVector LastMovementSmokeWorldIntent = FVector::ZeroVector;
	FVector LastMovementSmokeOwnerVelocityCmPerSecond = FVector::ZeroVector;
	FVector MovementSmokeStartLocation = FVector::ZeroVector;
	FName LastMovementSmokePhaseName = NAME_None;
	bool bMovementSmokeScriptStarted = false;
	bool bMovementSmokeCompletionLogged = false;
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
	static float ResolvePolicyControlIntervalSeconds(float PolicyControlRateHz);
	static float ResolveTrainingAlignedMassScaleForBone(FName BoneName, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedMassScales(bool bApplyTrainingAlignedMassScales, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedToeLimitPolicy(bool bApplyTrainingAlignedToeLimitPolicy, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedLowerLimbTargetRangePolicy(bool bApplyTrainingAlignedLowerLimbTargetRangePolicy, float BlendAlpha);
	static float ResolveTrainingAlignedLowerLimbTargetRangeScaleForBone(FName BoneName, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedDistalLocomotionTargetPolicy(bool bApplyTrainingAlignedDistalLocomotionTargetPolicy, float BlendAlpha, float OwnerPlanarSpeedCmPerSec, float ActivationSpeedCmPerSec);
	static float ResolveTrainingAlignedDistalLocomotionTargetScaleForBone(FName BoneName, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedDistalLocomotionCompositionPolicy(bool bApplyTrainingAlignedDistalLocomotionCompositionPolicy, float OwnerPlanarSpeedCmPerSec, float ActivationSpeedCmPerSec);
	static bool ShouldForceExplicitOnlyDistalLocomotionTargetMode(FName BoneName);
	static float ResolveTrainingAlignedControlStrengthScaleForBone(FName BoneName, float BlendAlpha);
	static float ResolveTrainingAlignedControlExtraDampingScaleForBone(FName BoneName, float BlendAlpha);
	static bool ShouldApplyTrainingAlignedControlFamilyProfile(bool bApplyTrainingAlignedControlFamilyProfile, float BlendAlpha);
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
