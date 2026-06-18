#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

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
			if (!ValidateModelDescriptorContract(OutError))
			{
				return false;
			}
			if (!TPoseReference)
			{
				OutError = TEXT("PhysAnimComponent requires a valid TPoseReference animation to align bone axes. Please assign a 1-frame T-Pose AnimSequence.");
				return false;
			}
			if (CachedSmplObservationRestComponentTransforms.IsEmpty())
			{
				OutError = TEXT("Live T-pose capture did not populate cached rest transforms before model initialization.");
				return false;
			}
			ActivatedStandingStabilityMetrics.bPolicyModelLoaded = true;
			ActivatedStandingStabilityMetrics.PolicyModelName = LoadedModelData->GetName();
			ActivatedStandingStabilityMetrics.PolicyRuntimeName = ActiveRuntimeName;
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] PhcPolicy loaded successfully on GPU: ModelName=%s, RuntimeName=%s"), *ActivatedStandingStabilityMetrics.PolicyModelName, *ActivatedStandingStabilityMetrics.PolicyRuntimeName);
			return true;
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
			if (!ValidateModelDescriptorContract(OutError))
			{
				return false;
			}
			if (!TPoseReference)
			{
				OutError = TEXT("PhysAnimComponent requires a valid TPoseReference animation to align bone axes. Please assign a 1-frame T-Pose AnimSequence.");
				return false;
			}
			if (CachedSmplObservationRestComponentTransforms.IsEmpty())
			{
				OutError = TEXT("Live T-pose capture did not populate cached rest transforms before model initialization.");
				return false;
			}
			ActivatedStandingStabilityMetrics.bPolicyModelLoaded = true;
			ActivatedStandingStabilityMetrics.PolicyModelName = LoadedModelData->GetName();
			ActivatedStandingStabilityMetrics.PolicyRuntimeName = ActiveRuntimeName;
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] PhcPolicy loaded successfully on CPU (fallback): ModelName=%s, RuntimeName=%s"), *ActivatedStandingStabilityMetrics.PolicyModelName, *ActivatedStandingStabilityMetrics.PolicyRuntimeName);
			return true;
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

	if (!PhysAnimBridge::ValidateInputTensorDescs(InputDescs, TensorIndexMap, OutError))
	{
		return false;
	}

	const TConstArrayView<UE::NNE::FTensorDesc> OutputDescs = ModelInstance->GetOutputTensorDescs();
	if (!PhysAnimBridge::ValidateActionOutputTensorDescs(OutputDescs, OutError))
	{
		return false;
	}

	SelfObservationBuffer.Init(0.0f, PhysAnimBridge::SelfObsSize);
	MimicTargetPosesBuffer.Init(0.0f, PhysAnimBridge::MimicTargetPosesSize);
	TerrainBuffer.Init(0.0f, PhysAnimBridge::TerrainSize);
	ActionOutputBuffer.Init(0.0f, PhysAnimBridge::NumActionFloats);
	PreviousActionOutputBuffer.Init(0.0f, PhysAnimBridge::NumActionFloats);

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

