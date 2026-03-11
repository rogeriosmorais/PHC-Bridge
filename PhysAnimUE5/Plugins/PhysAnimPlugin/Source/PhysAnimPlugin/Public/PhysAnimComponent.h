#pragma once

#include "Components/ActorComponent.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PhysAnimBridge.h"

#include "PhysAnimComponent.generated.h"

class UAnimInstance;
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float MaxAngularStepDegreesPerSecond = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float AngularStrengthMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float AngularDampingRatioMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.0"))
	float AngularExtraDampingMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	bool bLogActionDiagnostics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization", meta = (ClampMin = "0.1"))
	float ActionDiagnosticsIntervalSeconds = 1.0f;

	bool operator==(const FPhysAnimStabilizationSettings& Other) const
	{
		return bForceZeroActions == Other.bForceZeroActions &&
			FMath::IsNearlyEqual(ActionScale, Other.ActionScale) &&
			FMath::IsNearlyEqual(ActionClampAbs, Other.ActionClampAbs) &&
			FMath::IsNearlyEqual(ActionSmoothingAlpha, Other.ActionSmoothingAlpha) &&
			FMath::IsNearlyEqual(StartupRampSeconds, Other.StartupRampSeconds) &&
			FMath::IsNearlyEqual(MaxAngularStepDegreesPerSecond, Other.MaxAngularStepDegreesPerSecond) &&
			FMath::IsNearlyEqual(AngularStrengthMultiplier, Other.AngularStrengthMultiplier) &&
			FMath::IsNearlyEqual(AngularDampingRatioMultiplier, Other.AngularDampingRatioMultiplier) &&
			FMath::IsNearlyEqual(AngularExtraDampingMultiplier, Other.AngularExtraDampingMultiplier) &&
			bLogActionDiagnostics == Other.bLogActionDiagnostics &&
			FMath::IsNearlyEqual(ActionDiagnosticsIntervalSeconds, Other.ActionDiagnosticsIntervalSeconds);
	}

	bool operator!=(const FPhysAnimStabilizationSettings& Other) const
	{
		return !(*this == Other);
	}
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

protected:
	UPROPERTY(EditAnywhere, Category = "PhysAnim")
	TSoftObjectPtr<UNNEModelData> ModelDataAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PhysAnim|Stabilization")
	FPhysAnimStabilizationSettings StabilizationSettings;

private:
	bool ResolveRuntimeContext(FString& OutError);
	bool ValidateRequiredBodies(FString& OutError) const;
	bool EnsurePreauthoredPhysicsControl(FString& OutError);
	bool ValidatePreauthoredPhysicsControl(FString& OutError) const;
	bool ValidatePoseSearchIntegration(FString& OutError);
	bool InitializeModel(FString& OutError);
	bool ValidateModelDescriptorContract(FString& OutError);
	bool QueryPoseSearch(FPoseSearchBlueprintResult& OutSearchResult, FString& OutError);
	bool GatherCurrentBodySamples(TArray<FPhysAnimBodySample>& OutBodySamples, FString& OutError) const;
	bool SampleFuturePoses(const FPoseSearchBlueprintResult& SearchResult, TArray<FPhysAnimFuturePoseSample>& OutFutureSamples, FString& OutError) const;
	bool RunInference(FString& OutError);
	FPhysAnimStabilizationSettings ResolveEffectiveStabilizationSettings() const;
	void ApplyRuntimeControlTuning(const FPhysAnimStabilizationSettings& EffectiveSettings);
	bool ConditionModelActions(const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError);
	void ApplyControlTargets(float DeltaTime, FString& OutError);
	void MaybeLogActionDiagnostics(const FPhysAnimStabilizationSettings& EffectiveSettings) const;
	void ResetStabilizationRuntimeState();
	void FailStop(const FString& Reason);

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
	bool bBridgeActive = false;
	bool bStartupReported = false;
	FString ActiveRuntimeName;
	FPhysAnimStabilizationSettings LastAppliedStabilizationSettings;
	FPhysAnimActionDiagnostics LastActionDiagnostics;
	TMap<FName, FQuat> PreviousControlTargetRotations;
	double BridgeStartTimeSeconds = 0.0;
	double LastActionDiagnosticsLogTimeSeconds = -1.0;

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
};
