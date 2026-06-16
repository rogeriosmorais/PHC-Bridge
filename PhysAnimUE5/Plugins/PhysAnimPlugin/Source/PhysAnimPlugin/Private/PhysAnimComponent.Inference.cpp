#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"

bool UPhysAnimComponent::RunInference(FString& OutError)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPhysAnimComponent_RunInference);

	const bool bCaptureMetrics = RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing ||
		((RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
		  RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
		  RuntimeState == EPhysAnimRuntimeState::ReadyForActivation) && bLiveRuntimeEvidenceProofActive);

	if (bCaptureMetrics)
	{
		++ActivatedStandingStabilityMetrics.PolicyInferenceAttemptCount;
	}

	UE::NNE::IModelInstanceRunSync* const ModelInstance = GetModelInstanceRunSync();
	if (!ModelInstance)
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] RunInference failed: No active model instance exists."));
		OutError = TEXT("No active model instance exists.");
		if (bCaptureMetrics)
		{
			++ActivatedStandingStabilityMetrics.PolicyInferenceFailureCount;
		}
		return false;
	}

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("self_obs"), SelfObservationBuffer, OutError))
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnim] RunInference failed: self_obs buffer contains non-finite values."));
		if (bCaptureMetrics)
		{
			++ActivatedStandingStabilityMetrics.PolicyInferenceFailureCount;
		}
		return false;
	}

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("mimic_target_poses"), MimicTargetPosesBuffer, OutError))
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnim] RunInference failed: mimic_target_poses buffer contains non-finite values."));
		if (bCaptureMetrics)
		{
			++ActivatedStandingStabilityMetrics.PolicyInferenceFailureCount;
		}
		return false;
	}

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("terrain"), TerrainBuffer, OutError))
	{
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnim] RunInference failed: terrain buffer contains non-finite values."));
		if (bCaptureMetrics)
		{
			++ActivatedStandingStabilityMetrics.PolicyInferenceFailureCount;
		}
		return false;
	}

	const TArray<float> ActionOutputsBeforeRun = ActionOutputBuffer;
	const double RunSyncStartSeconds = FPlatformTime::Seconds();
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PhysAnim_RunSync);
		if (ModelInstance->RunSync(InputBindings, OutputBindings) != UE::NNE::EResultStatus::Ok)
		{
			const double LatencyMs = (FPlatformTime::Seconds() - RunSyncStartSeconds) * 1000.0;
			TRACE_COUNTER_SET(COUNTER_PhysAnim_RunSyncMs, static_cast<float>(LatencyMs));
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, TEXT("[PhysAnim] RunInference failed: ModelInstance->RunSync returned failure status."));
			OutError = TEXT("RunSync failed.");
			if (bCaptureMetrics)
			{
				++ActivatedStandingStabilityMetrics.PolicyInferenceFailureCount;
				ActivatedStandingStabilityMetrics.PolicyInferenceLatencyMsMax = FMath::Max(
					ActivatedStandingStabilityMetrics.PolicyInferenceLatencyMsMax,
					LatencyMs);
			}
			return false;
		}
	}
	const double LatencyMs = (FPlatformTime::Seconds() - RunSyncStartSeconds) * 1000.0;
	TRACE_COUNTER_SET(COUNTER_PhysAnim_RunSyncMs, static_cast<float>(LatencyMs));

	if (!PhysAnimBridge::ValidateFiniteFloatBuffer(TEXT("Model action output"), ActionOutputBuffer, OutError))
	{
		if (bCaptureMetrics)
		{
			++ActivatedStandingStabilityMetrics.PolicyInferenceFailureCount;
			ActivatedStandingStabilityMetrics.PolicyInferenceLatencyMsMax = FMath::Max(
				ActivatedStandingStabilityMetrics.PolicyInferenceLatencyMsMax,
				LatencyMs);
		}
		return false;
	}
	PreviousActionOutputBuffer = ActionOutputsBeforeRun;
	if (bCaptureMetrics)
	{
		++ActivatedStandingStabilityMetrics.PolicyInferenceSuccessCount;
		ActivatedStandingStabilityMetrics.PolicyInferenceLatencyMsMax = FMath::Max(
			ActivatedStandingStabilityMetrics.PolicyInferenceLatencyMsMax,
			LatencyMs);
		ActivatedStandingStabilityMetrics.bPolicyInputBuffersFinite = true;

		// §S2-IMPL-SYNC-INFERENCE-01: Enforcement of 0.95 success ratio after 50 attempts
		RecentInferenceSuccessRatio = (float)ActivatedStandingStabilityMetrics.PolicyInferenceSuccessCount / 
			(float)ActivatedStandingStabilityMetrics.PolicyInferenceAttemptCount;

		if (ActivatedStandingStabilityMetrics.PolicyInferenceAttemptCount >= 50 && RecentInferenceSuccessRatio < 0.95f)
		{
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, 
				TEXT("[PhysAnim] INFERENCE_QUALITY_LOW ratio=%.3f attempts=%d success=%d — trigger diagnostic failure."),
				RecentInferenceSuccessRatio, 
				ActivatedStandingStabilityMetrics.PolicyInferenceAttemptCount,
				ActivatedStandingStabilityMetrics.PolicyInferenceSuccessCount);
			// Trigger a fail-stop if the quality is consistently below the required threshold
			FailStop(FString::Printf(TEXT("Inference success ratio %.3f is below 0.95 threshold after %d attempts."), 
				RecentInferenceSuccessRatio, ActivatedStandingStabilityMetrics.PolicyInferenceAttemptCount));
		}
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
		const bool bCaptureMetrics = RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing ||
			((RuntimeState == EPhysAnimRuntimeState::BridgeActive ||
			  RuntimeState == EPhysAnimRuntimeState::WaitingForPoseSearch ||
			  RuntimeState == EPhysAnimRuntimeState::ReadyForActivation) && bLiveRuntimeEvidenceProofActive);

		if (bCaptureMetrics)
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

			// §S2-IMPL-SYNC-INFERENCE-01: Action Magnitude Variance check
			RecentActionMagnitudeHistory.Add(LastActionDiagnostics.RawMeanAbs);
			if (RecentActionMagnitudeHistory.Num() > 10)
			{
				RecentActionMagnitudeHistory.RemoveAt(0);
			}

			if (RecentActionMagnitudeHistory.Num() >= 10)
			{
				float Sum = 0.0f;
				for (float Mag : RecentActionMagnitudeHistory) { Sum += Mag; }
				float Mean = Sum / 10.0f;
				float Variance = 0.0f;
				for (float Mag : RecentActionMagnitudeHistory) { Variance += FMath::Square(Mag - Mean); }
				
				ActivatedStandingStabilityMetrics.ActionMagnitudeVariance = static_cast<double>(Variance);

				if (Variance <= UE_SMALL_NUMBER && Mean > UE_SMALL_NUMBER)
				{
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Error, 1.0f, 
						TEXT("[PhysAnim] ACTION_FROZEN variance=0.0 mean=%.4f over 10 frames — failure of intent."), Mean);
					FailStop(TEXT("Model action output is frozen (zero variance over 10 frames)."));
				}
			}
		}
	}

	return bSuccess;
}

