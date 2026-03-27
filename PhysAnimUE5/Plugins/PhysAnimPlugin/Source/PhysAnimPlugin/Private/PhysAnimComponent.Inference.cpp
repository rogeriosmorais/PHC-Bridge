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

	for (const float Value : ActionOutputBuffer)
	{
		if (!FMath::IsFinite(Value))
		{
			OutError = TEXT("Model action output contained NaN or Inf.");
			return false;
		}
	}
	PreviousActionOutputBuffer = ActionOutputsBeforeRun;

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
	}

	return bSuccess;
}

