#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::RunInference(FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysAnimComponent_RunInference);

	UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync();
	if (!ModelInstance)
	{
		OutError = TEXT("No active model instance exists.");
		return false;
	}

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("self_obs"), SelfObservationBuffer, OutError))
	{
		return false;
	}

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("mimic_target_poses"), MimicTargetPosesBuffer, OutError))
	{
		return false;
	}

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("terrain"), TerrainBuffer, OutError))
	{
		return false;
	}

	const TArray<float> ActionOutputsBeforeRun = ActionOutputBuffer;
	const double RunSyncStartSeconds = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PhysAnim_RunSync);
		if (ModelInstance->RunSync(InputBindings, OutputBindings) != UE::NNE::EResultStatus::Ok)
		{
			TRACE_COUNTER_SET(COUNTER_PhysAnim_RunSyncMs, static_cast<float>((FPlatformTime::Seconds() - RunSyncStartSeconds) * 1000.0));
			OutError = TEXT("RunSync failed.");
			return false;
		}
	}
	TRACE_COUNTER_SET(COUNTER_PhysAnim_RunSyncMs, static_cast<float>((FPlatformTime::Seconds() - RunSyncStartSeconds) * 1000.0));

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("Model action output"), ActionOutputBuffer, OutError))
	{
		return false;
	}
	PreviousActionOutputBuffer = ActionOutputsBeforeRun;
	if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
	{
		++ActivatedStandingStabilityMetrics.PolicyInferenceSuccessCount;
	}

	return true;
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
		if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
		{
			++ActivatedStandingStabilityMetrics.PolicyActionSampleCount;
			ActivatedStandingStabilityMetrics.PolicyActionRawMeanAbsMax = FMath::Max(
				ActivatedStandingStabilityMetrics.PolicyActionRawMeanAbsMax,
				static_cast<double>(LastActionDiagnostics.RawMeanAbs));
			ActivatedStandingStabilityMetrics.PolicyActionConditionedMeanAbsMax = FMath::Max(
				ActivatedStandingStabilityMetrics.PolicyActionConditionedMeanAbsMax,
				static_cast<double>(LastActionDiagnostics.ConditionedMeanAbs));
			ActivatedStandingStabilityMetrics.PolicyActionClampedFloatMax = FMath::Max(
				ActivatedStandingStabilityMetrics.PolicyActionClampedFloatMax,
				LastActionDiagnostics.NumClampedActionFloats);
		}
	}

	return bSuccess;
}

