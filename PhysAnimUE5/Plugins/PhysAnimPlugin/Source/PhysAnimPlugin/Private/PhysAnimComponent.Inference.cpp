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
	FirstPolicyInferenceSnapshot.CaptureFirst(
		SelfObservationBuffer,
		MimicTargetPosesBuffer,
		TerrainBuffer,
		ActionOutputBuffer);
#if WITH_DEV_AUTOMATION_TESTS
	bFirstActiveStandingPolicyCapturedBeforeCurrentInferenceForTesting =
		FirstActiveStandingPolicyInferenceSnapshot.bCaptured;
	FirstActiveStandingPolicyInferenceSnapshot.CaptureFirstIf(
		RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing,
		SelfObservationBuffer,
		MimicTargetPosesBuffer,
		TerrainBuffer,
		ActionOutputBuffer);
#endif
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
			return false;
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
	// Standing influence is applied once in parent-relative target space. Keep raw policy
	// conditioning variant-specific, but do not multiply the shared activation alpha here.
	ConditioningSettings.ActionScale = EffectiveSettings.ActionScale;
	const TArray<float>* ActionsForConditioning = &ActionOutputBuffer;
#if WITH_DEV_AUTOMATION_TESTS
	TArray<float> BaselineResidualActions;
	const bool bResidualModeConfigured =
		bExperimentalPolicyActionBaselineResidualEnabledForTesting ||
		bExperimentalPolicyActionZeroUntilBaselineEnabledForTesting;
	if (bResidualModeConfigured)
	{
		BaselineResidualActions = ActionOutputBuffer;
		const bool bActionsExplicitlyZero = !ActionOutputBuffer.ContainsByPredicate(
			[](const float Value)
			{
				return !FMath::IsNearlyZero(Value);
			});
		const bool bForceZeroActions = EffectiveSettings.bForceZeroActions || bActionsExplicitlyZero;
		const bool bBaselineAvailable =
			FirstActiveStandingPolicyInferenceSnapshot.Actions.Num() == ActionOutputBuffer.Num();
		const bool bPreCaptureZeroApplied = ApplyExperimentalPolicyActionZeroUntilBaselineForTesting(
			bExperimentalPolicyActionZeroUntilBaselineEnabledForTesting,
			bBaselineAvailable,
			bForceZeroActions,
			BaselineResidualActions);
		const bool bResidualApplied = !bPreCaptureZeroApplied &&
			ApplyExperimentalPolicyActionBaselineResidualForTesting(
				true,
				bForceZeroActions,
				FirstActiveStandingPolicyInferenceSnapshot.Actions,
				BaselineResidualActions);
		if (bPreCaptureZeroApplied || bResidualApplied)
		{
			ActionsForConditioning = &BaselineResidualActions;
		}
		if (bPreCaptureZeroApplied)
		{
			PreviousConditionedActionBuffer.Reset();
		}
		else if (bResidualApplied && !bExperimentalPolicyActionBaselineResidualStartedForTesting)
		{
			PreviousConditionedActionBuffer.Reset();
			bExperimentalPolicyActionBaselineResidualStartedForTesting = true;
		}
	}
#endif
	const bool bSuccess = BuildConditionedActions(
		*ActionsForConditioning,
		PreviousConditionedActionBuffer.Num() == ActionsForConditioning->Num() ? &PreviousConditionedActionBuffer : nullptr,
		ConditioningSettings,
		ConditionedActionBuffer,
		LastActionDiagnostics,
		OutError);
	if (bSuccess)
	{
		bool bRestoreCausalStandingSpineChest = false;
		bool bRestoreCausalStandingNeck = false;
		bool bRestoreCausalStandingHead = false;
#if WITH_DEV_AUTOMATION_TESTS
		bRestoreCausalStandingSpineChest =
			bExperimentalCausalStandingSpineChestEnabledForTesting;
		bRestoreCausalStandingNeck =
			bExperimentalCausalStandingNeckEnabledForTesting;
		bRestoreCausalStandingHead =
			bExperimentalCausalStandingHeadAfterFirstPolicyEnabledForTesting
				? ShouldRestoreExperimentalCausalStandingHeadAfterFirstPolicyForTesting(
					bExperimentalCausalStandingHeadEnabledForTesting,
					bFirstActiveStandingPolicyCapturedBeforeCurrentInferenceForTesting,
					RuntimeState)
				: ShouldRestoreExperimentalCausalStandingHeadForRuntimeStateForTesting(
					bExperimentalCausalStandingHeadEnabledForTesting,
					bExperimentalCausalStandingHeadActiveOnlyEnabledForTesting,
					RuntimeState);
#endif
		ApplyCausalStandingPolicyActionCompatibility(
			IsStandingActivationRuntimeState(RuntimeState),
			bRestoreCausalStandingSpineChest,
			bRestoreCausalStandingNeck,
			bRestoreCausalStandingHead,
			ConditionedActionBuffer);
#if WITH_DEV_AUTOMATION_TESTS
		ApplyExperimentalActionFamilyMaskForTesting(
			ExperimentalActionFamilyMaskForTesting,
			ConditionedActionBuffer);
		ApplyExperimentalActionJointRangeForTesting(
			ExperimentalActionJointRangeStartForTesting,
			ExperimentalActionJointRangeCountForTesting,
			ConditionedActionBuffer);
		CaptureFirstActiveStandingConditionedActionsForTesting(
			RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing,
			ConditionedActionBuffer,
			bFirstActiveStandingConditionedActionsCapturedForTesting,
			FirstActiveStandingConditionedActionsForTesting);
#endif
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

			// A steady observation may legitimately produce a steady deterministic action.
			// Record scalar magnitude variance for evidence, but do not turn convergence
			// into a runtime failure; causal responsiveness is a protocol-level property.
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
			}
		}
	}

	return bSuccess;
}

