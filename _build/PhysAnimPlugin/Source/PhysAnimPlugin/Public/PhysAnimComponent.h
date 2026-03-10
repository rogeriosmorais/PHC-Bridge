#pragma once

#include "Components/ActorComponent.h"
#include "NNE.h"
#include "NNEModelData.h"
#include "NNERuntimeCPU.h"
#include "NNERuntimeGPU.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PhysAnimBridge.h"

#include "PhysAnimComponent.generated.h"

class UPhysAnimAnimInstance;
class UPhysicsControlComponent;
class USkeletalMeshComponent;

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

private:
	bool ResolveRuntimeContext(FString& OutError);
	bool ValidateRequiredBodies(FString& OutError) const;
	bool ValidatePreauthoredPhysicsControl(FString& OutError) const;
	bool ValidatePoseSearchIntegration(FString& OutError) const;
	bool InitializeModel(FString& OutError);
	bool ValidateModelDescriptorContract(FString& OutError);
	bool GatherCurrentBodySamples(TArray<FPhysAnimBodySample>& OutBodySamples, FString& OutError) const;
	bool SampleFuturePoses(const FPoseSearchBlueprintResult& SearchResult, TArray<FPhysAnimFuturePoseSample>& OutFutureSamples, FString& OutError) const;
	bool RunInference(FString& OutError);
	void ApplyControlTargets(float DeltaTime, FString& OutError);
	void FailStop(const FString& Reason);

	UE::NNE::IModelInstanceRunSync* GetModelInstanceRunSync() const;
	TConstArrayView<UE::NNE::FTensorDesc> GetInputTensorDescs() const;
	TConstArrayView<UE::NNE::FTensorDesc> GetOutputTensorDescs() const;

	TWeakObjectPtr<USkeletalMeshComponent> MeshComponent;
	TWeakObjectPtr<UPhysicsControlComponent> PhysicsControlComponent;
	TWeakObjectPtr<UPhysAnimAnimInstance> PhysAnimAnimInstance;

	TWeakInterfacePtr<INNERuntimeGPU> RuntimeGPU;
	TWeakInterfacePtr<INNERuntimeCPU> RuntimeCPU;
	TSharedPtr<UE::NNE::IModelGPU> ModelGPU;
	TSharedPtr<UE::NNE::IModelCPU> ModelCPU;
	TSharedPtr<UE::NNE::IModelInstanceGPU> ModelInstanceGPU;
	TSharedPtr<UE::NNE::IModelInstanceCPU> ModelInstanceCPU;

	TObjectPtr<UNNEModelData> LoadedModelData = nullptr;

	FPhysAnimTensorIndexMap TensorIndexMap;

	TArray<float> SelfObservationBuffer;
	TArray<float> MimicTargetPosesBuffer;
	TArray<float> TerrainBuffer;
	TArray<float> ActionOutputBuffer;
	TArray<UE::NNE::FTensorBindingCPU> InputBindings;
	TArray<UE::NNE::FTensorBindingCPU> OutputBindings;

	FPoseSearchBlueprintResult LastValidPoseSearchResult;
	int32 ConsecutiveInvalidPoseSearchFrames = 0;
	bool bBridgeActive = false;
	bool bStartupReported = false;
	FString ActiveRuntimeName;
};
