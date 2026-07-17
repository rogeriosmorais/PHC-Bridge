#include "PhysAnimComponent.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimFrameContract.h"
#include "PhysAnimLogger.h"
#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimProtoMannyAdapter.h"

#include "Components/SkeletalMeshComponent.h"
#include "PhysicsControlComponent.h"

void UPhysAnimComponent::ProcessPendingDistalOwnershipChecks()
{
	if (!BalanceReadyTransition.IsDistalKinematicAccepted())
	{
		return;
	}

	USkeletalMeshComponent* const Mesh = GetMeshComponent();
	UPhysicsControlComponent* const PC = PhysicsControlComponent.Get();
	if (!Mesh || !PC)
	{
		return;
	}

	for (const FName BoneName : {TEXT("calf_l"), TEXT("calf_r"), TEXT("foot_l"), TEXT("foot_r"), TEXT("ball_l"), TEXT("ball_r")})
	{
		const FName ModifierName = PhysAnimBridge::MakeBodyModifierName(BoneName);
		if (const FPhysicsBodyModifierRecord* Record = FPhysAnimPhysicsControlAccessor::GetModifierRecord(PC, ModifierName))
		{
			const EPhysicsMovementType ModifierType = Record->BodyModifier.ModifierData.MovementType;
			FBodyInstance* const BodyInstance = Mesh->GetBodyInstance(BoneName);
			const bool bRawSimulating = BodyInstance ? BodyInstance->IsInstanceSimulatingPhysics() : false;
			
			if (ModifierType == EPhysicsMovementType::Simulated && !bRawSimulating)
			{
				static TMap<FName, bool> LoggedStaleOnce;
				if (!LoggedStaleOnce.Contains(BoneName))
				{
					PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("DISTAL_MODIFIER_RECORD_STALE bone=%s intended=Kinematic rawBody=Kinematic modifier=Simulated runtimeState=%s phase=%d"),
						*BoneName.ToString(),
						GetRuntimeStateName(RuntimeState),
						(int32)BalanceReadyTransition.GetPhase());
					LoggedStaleOnce.Add(BoneName, true);
				}
			}
		}
	}
}

void UPhysAnimComponent::TickRuntimeStateMachine(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	switch (RuntimeState)
	{
	case EPhysAnimRuntimeState::BridgeActive:
		if (LastValidPoseSearchResult.SelectedAnim != nullptr)
		{
			StandingActivation.Start();
			StandingActivationElapsedSeconds = 0.0f;
			bPendingBalanceModeStartRequest = false;
			bPendingBalanceModeStartAttemptIssued = false;
			PendingBalanceModeStartReason.Reset();
			PendingBalanceModeRequestTimeSeconds = -1.0;
			TransitionRuntimeState(EPhysAnimRuntimeState::Standing_Preparation);
		}
		break;

	case EPhysAnimRuntimeState::Standing_Preparation:
	{
		FString FailureReason;
		const bool bPrepared = PrepareStandingActivation(FailureReason);
		StandingActivation.CompletePreparation(bPrepared, FailureReason);
		if (StandingActivation.GetStatus().RuntimeState == EPhysAnimRuntimeState::FailStopped)
		{
			FailStop(StandingActivation.GetStatus().FailureReason);
			return;
		}
		TransitionRuntimeState(EPhysAnimRuntimeState::Standing_FullSimulationActivation);
		break;
	}

	case EPhysAnimRuntimeState::Standing_FullSimulationActivation:
	{
		FString FailureReason;
		const FPhysAnimStandingActivationReadback Readback =
			PublishStandingPhysicsControlState(EffectiveSettings, 0.0f, true, FailureReason);
		StandingActivation.CompleteFullSimulationActivation(Readback, FailureReason);
		if (StandingActivation.GetStatus().RuntimeState == EPhysAnimRuntimeState::FailStopped)
		{
			FailStop(StandingActivation.GetStatus().FailureReason);
			return;
		}
		StandingActivationElapsedSeconds = 0.0f;
		TransitionRuntimeState(EPhysAnimRuntimeState::Standing_PolicyBlend);
		break;
	}

	case EPhysAnimRuntimeState::Standing_PolicyBlend:
	{
		StandingActivationElapsedSeconds += FMath::Max(DeltaTime, 0.0f);
		const float Alpha = EffectiveSettings.StartupRampSeconds > SMALL_NUMBER
			? FMath::Clamp(StandingActivationElapsedSeconds / EffectiveSettings.StartupRampSeconds, 0.0f, 1.0f)
			: 1.0f;
		FString FailureReason;
		const FPhysAnimStandingActivationReadback Readback =
			PublishStandingPhysicsControlState(EffectiveSettings, Alpha, false, FailureReason);
		StandingActivation.TickPolicyBlend(
			StandingActivationElapsedSeconds,
			EffectiveSettings.StartupRampSeconds,
			Readback,
			FailureReason);
		if (StandingActivation.GetStatus().RuntimeState == EPhysAnimRuntimeState::FailStopped)
		{
			FailStop(StandingActivation.GetStatus().FailureReason);
			return;
		}
		if (StandingActivation.GetStatus().RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
		{
			CompleteBalanceModeEntry();
		}
		break;
	}

	case EPhysAnimRuntimeState::BalanceActive_Standing:
	{
		FString FailureReason;
		const FPhysAnimStandingActivationReadback Readback =
			PublishStandingPhysicsControlState(EffectiveSettings, 1.0f, false, FailureReason);
		StandingActivation.ObserveStanding(Readback, FailureReason);
		if (StandingActivation.GetStatus().RuntimeState == EPhysAnimRuntimeState::FailStopped)
		{
			FailStop(StandingActivation.GetStatus().FailureReason);
		}
		break;
	}

	case EPhysAnimRuntimeState::LocomotionActiveShell:
	{
		FString FailureReason;
		PublishStandingPhysicsControlState(EffectiveSettings, 1.0f, false, FailureReason);
		if (!FailureReason.IsEmpty())
		{
			FailStop(FString::Printf(TEXT("Could not preserve production policy-control state during locomotion: %s"), *FailureReason));
		}
		break;
	}

	default:
		break;
	}
}

void UPhysAnimComponent::TickPolicyAndUpdateMetrics(float DeltaTime, const FPhysAnimStabilizationSettings& EffectiveSettings, FString& OutError)
{
	const float PolicyControlIntervalSeconds = ResolvePolicyControlIntervalSeconds(EffectiveSettings.PolicyControlRateHz);
	int32 ElapsedPolicySteps = 0;
	const bool bRunPolicyUpdateThisTick = AdvancePolicyControlAccumulator(
		DeltaTime,
		PolicyControlIntervalSeconds,
		PolicyUpdateAccumulatorSeconds,
		ElapsedPolicySteps);
	LastPolicyElapsedSteps = ElapsedPolicySteps;
	bool bStandingVariantUsesPolicyInference = true;
	bool bStandingVariantForcesZeroActions = false;
#if WITH_DEV_AUTOMATION_TESTS
	bStandingVariantUsesPolicyInference =
		FPhysAnimStandingActivationPlan::UsesPolicyInference(StandingVariantForTesting);
	bStandingVariantForcesZeroActions = StandingVariantForTesting == EPhysAnimStandingVariant::ZeroActions;
#endif

	PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Verbose, 1.0f, TEXT("[PhysAnim] TICK_POLICY state=%s runPolicy=%d steps=%d acc=%.4f interval=%.4f dt=%.4f"),
		GetRuntimeStateName(RuntimeState), bRunPolicyUpdateThisTick ? 1 : 0, ElapsedPolicySteps, PolicyUpdateAccumulatorSeconds, PolicyControlIntervalSeconds, DeltaTime);

	if (bRunPolicyUpdateThisTick)
	{
		++PolicyControlTicksExecuted;
		PolicyControlTicksSkipped += FMath::Max(ElapsedPolicySteps - 1, 0);

		const FPoseSearchBlueprintResult PreviousPoseSearchResult = LastValidPoseSearchResult;
		FPoseSearchBlueprintResult SearchResult;
		const double PoseSearchStartSeconds = FPlatformTime::Seconds();
		const bool bPoseSearchValid = QueryPoseSearch(SearchResult, OutError);
		RecordLiveRuntimeEvidencePoseSearchQueryResult(bPoseSearchValid, SearchResult.SelectedAnim ? SearchResult.SelectedAnim->GetName() : TEXT(""));

		if (!bPoseSearchValid)
		{
			++ConsecutiveInvalidPoseSearchFrames;
			if (ConsecutiveInvalidPoseSearchFrames > 1 || LastValidPoseSearchResult.SelectedAnim == nullptr)
			{
				OutError = FString::Printf(TEXT("PoseSearch query was invalid for two consecutive policy ticks. %s"), *OutError);
				return;
			}
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Warning, 1.0f, TEXT("[PhysAnim] Reusing last valid PoseSearch result for one grace policy tick. Reason: %s"), *OutError);
			SearchResult = LastValidPoseSearchResult;
		}

		TArray<FPhysAnimBodySample> MannyCurrentBodySamples;
		if (!GatherCurrentBodySamples(MannyCurrentBodySamples, OutError))
		{
			return;
		}
#if WITH_DEV_AUTOMATION_TESTS
		const bool bRecordFirstPolicyBodySource =
			bStartupChronologyTraceEnabledForTesting &&
			StandingVariantForTesting == EPhysAnimStandingVariant::RealOnnxPolicy &&
			bEnablePolicyInference &&
			bStandingVariantUsesPolicyInference &&
			PolicyControlTicksExecuted == 1;
		FString BodySourceTraceError;
		if (!FirstPolicyBodySourceTrace.RecordFirstPolicySourceIf(
				bRecordFirstPolicyBodySource,
				TEXT("first_policy_pre_adapter"),
				GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0,
				GetRuntimeStateName(RuntimeState),
				PolicyControlTicksExecuted,
				MannyCurrentBodySamples,
				BodySourceTraceError))
		{
			OutError = FString::Printf(
				TEXT("Could not record the first-policy body-source trace: %s"),
				*BodySourceTraceError);
			return;
		}
#endif
		TArray<FPhysAnimBodySample> CurrentBodySamples;
		if (!PhysAnimProtoMannyAdapter::AdaptBodySamplesToCanonicalSmpl(
			MannyCurrentBodySamples,
			CurrentBodySamples,
			OutError))
		{
			return;
		}

#if WITH_DEV_AUTOMATION_TESTS
		MimicFrameDiagnostics.Reset();
#endif
		const bool bIdleToLocomotionTransition =
			bPoseSearchValid &&
			PreviousPoseSearchResult.SelectedAnim != nullptr &&
			IsBridgePoseSearchIdleResult(PreviousPoseSearchResult) &&
			SearchResult.SelectedAnim != nullptr &&
			!IsBridgePoseSearchIdleResult(SearchResult) &&
			PoseSearchTopLocomotionCandidateResultsForDiagnostics.Num() ==
				PoseSearchTopLocomotionCandidateCostsForDiagnostics.Num() &&
			MimicTargetPosesBuffer.Num() == PhysAnimBridge::MimicTargetPosesSize &&
			CurrentBodySamples.IsValidIndex(0) &&
			CurrentBodySamples.IsValidIndex(3) &&
			CurrentBodySamples.IsValidIndex(7);
		if (bIdleToLocomotionTransition)
		{
			constexpr int32 RotationProbeFutureIndex = 2;
			const FQuat CurrentRootRotation = CurrentBodySamples[0].Rotation.GetNormalized();
			const FQuat CurrentLeftFootRelativeRotation =
				(CurrentRootRotation.Inverse() * CurrentBodySamples[3].Rotation).GetNormalized();
			const FQuat CurrentRightFootRelativeRotation =
				(CurrentRootRotation.Inverse() * CurrentBodySamples[7].Rotation).GetNormalized();

			auto BuildCandidateMimicTarget =
				[&](
					const FPoseSearchBlueprintResult& CandidateResult,
					TArray<FPhysAnimFuturePoseSample> CandidateFuturePoses,
					TArray<float>& OutCandidateMimicTarget,
					FString& OutCandidateError) -> bool
				{
					FTransform CandidateReferenceWorldRoot = FTransform::Identity;
					FTransform CandidateReferenceDataRoot = FTransform::Identity;
					if (!ResolveMimicTargetReferenceDataFrame(
							CandidateResult,
							CandidateReferenceWorldRoot,
							CandidateReferenceDataRoot,
							OutCandidateError))
					{
						return false;
					}

					if (ShouldUseBridgeTrajectoryPoseSearchState(RuntimeState))
					{
						if (!bBridgePoseSearchTrajectoryInitialized)
						{
							OutCandidateError =
								TEXT("Transition candidate placement requires an initialized Pose Search trajectory.");
							return false;
						}
						const FTransformTrajectorySample CurrentTrajectorySample =
							BridgePoseSearchTrajectory.GetSampleAtTime(0.0f, true);
						const PhysAnimFrameContract::FWorldTransformCm CurrentQueryWorldRoot(
							CurrentTrajectorySample.GetTransform());
						TArray<PhysAnimFrameContract::FWorldTransformCm> FutureQueryWorldRoots;
						FutureQueryWorldRoots.Reserve(CandidateFuturePoses.Num());
						for (const FPhysAnimFuturePoseSample& CandidateFuturePose : CandidateFuturePoses)
						{
							const FTransformTrajectorySample FutureTrajectorySample =
								BridgePoseSearchTrajectory.GetSampleAtTime(
									CandidateFuturePose.FutureTimeSeconds,
									true);
							FutureQueryWorldRoots.Add(
								PhysAnimFrameContract::FWorldTransformCm(
									FutureTrajectorySample.GetTransform()));
						}
						TArray<FPhysAnimFuturePoseSample> PlacedCandidateFuturePoses;
						if (!PhysAnimFrameContract::PlaceProtoFuturePosesOnWorldTrajectory(
								CandidateFuturePoses,
								CurrentQueryWorldRoot,
								FutureQueryWorldRoots,
								PhysAnimFrameContract::FProtoWorldCanonicalTransformMeters(
									CandidateReferenceWorldRoot),
								PhysAnimFrameContract::FProtoAnimationDataCanonicalTransformMeters(
									CandidateReferenceDataRoot),
								PlacedCandidateFuturePoses,
								OutCandidateError))
						{
							return false;
						}
						CandidateFuturePoses = MoveTemp(PlacedCandidateFuturePoses);
					}

					TArray<FPhysAnimBodySample> CandidateCurrentReferenceBodySamples;
					MakeMimicTargetDataFrameBodySamples(
						CurrentBodySamples,
						CandidateReferenceWorldRoot,
						CandidateReferenceDataRoot,
						CandidateCurrentReferenceBodySamples);
					return PhysAnimBridge::BuildMimicTargetPoses(
						CandidateCurrentReferenceBodySamples,
						CandidateFuturePoses,
						OutCandidateMimicTarget,
						OutCandidateError);
				};

#if WITH_DEV_AUTOMATION_TESTS
			const bool bCaptureTransitionDiagnostics = bPolicyInputProvenanceTraceEnabledForTesting;
			MimicFrameDiagnostics.bIdleToLocomotionCandidateScan = bCaptureTransitionDiagnostics;
#endif
			for (int32 CandidateIndex = 0;
				CandidateIndex < PoseSearchTopLocomotionCandidateResultsForDiagnostics.Num();
				++CandidateIndex)
			{
				const FPoseSearchBlueprintResult& CandidateResult =
					PoseSearchTopLocomotionCandidateResultsForDiagnostics[CandidateIndex];
				if (!CandidateResult.SelectedAnim)
				{
					continue;
				}
				TArray<FPhysAnimFuturePoseSample> CandidateMannyFuturePoses;
				FString CandidateError;
				if (!SampleFuturePoses(
						CandidateResult,
						CandidateMannyFuturePoses,
						CandidateError,
						nullptr))
				{
					continue;
				}
				TArray<FPhysAnimFuturePoseSample> CandidateCanonicalFuturePoses;
				if (!PhysAnimProtoMannyAdapter::AdaptFuturePoseSamplesToCanonicalSmpl(
						CandidateMannyFuturePoses,
						CandidateCanonicalFuturePoses,
						CandidateError) ||
					!CandidateCanonicalFuturePoses.IsValidIndex(RotationProbeFutureIndex) ||
					!CandidateCanonicalFuturePoses[RotationProbeFutureIndex].BodyTransforms.IsValidIndex(0) ||
					!CandidateCanonicalFuturePoses[RotationProbeFutureIndex].BodyTransforms.IsValidIndex(3) ||
					!CandidateCanonicalFuturePoses[RotationProbeFutureIndex].BodyTransforms.IsValidIndex(7))
				{
					continue;
				}

				TArray<float> CandidateMimicTarget;
				if (!BuildCandidateMimicTarget(
						CandidateResult,
						CandidateCanonicalFuturePoses,
						CandidateMimicTarget,
						CandidateError) ||
					CandidateMimicTarget.Num() != MimicTargetPosesBuffer.Num())
				{
					continue;
				}
				double MimicTargetStepDeltaSquared = 0.0;
				for (int32 ValueIndex = 0; ValueIndex < CandidateMimicTarget.Num(); ++ValueIndex)
				{
					const double Delta =
						static_cast<double>(CandidateMimicTarget[ValueIndex]) -
						static_cast<double>(MimicTargetPosesBuffer[ValueIndex]);
					MimicTargetStepDeltaSquared += Delta * Delta;
				}
				const float MimicTargetStepDeltaL2 =
					static_cast<float>(FMath::Sqrt(MimicTargetStepDeltaSquared));

				const TArray<FTransform>& CandidateBodyTransforms =
					CandidateCanonicalFuturePoses[RotationProbeFutureIndex].BodyTransforms;
				const FQuat CandidateRootRotation =
					CandidateBodyTransforms[0].GetRotation().GetNormalized();
				const FQuat CandidateLeftFootRelativeRotation =
					(CandidateRootRotation.Inverse() *
						CandidateBodyTransforms[3].GetRotation()).GetNormalized();
				const FQuat CandidateRightFootRelativeRotation =
					(CandidateRootRotation.Inverse() *
						CandidateBodyTransforms[7].GetRotation()).GetNormalized();
				const float LeftFootOrientationDeltaDegrees = FMath::RadiansToDegrees(
					CurrentLeftFootRelativeRotation.AngularDistance(
						CandidateLeftFootRelativeRotation));
				const float RightFootOrientationDeltaDegrees = FMath::RadiansToDegrees(
					CurrentRightFootRelativeRotation.AngularDistance(
						CandidateRightFootRelativeRotation));
				const float MaxFootOrientationDeltaDegrees = FMath::Max(
					LeftFootOrientationDeltaDegrees,
					RightFootOrientationDeltaDegrees);
#if WITH_DEV_AUTOMATION_TESTS
				if (bCaptureTransitionDiagnostics)
				{
					PhysAnimBridge::FPhysAnimPoseSearchTransitionCandidateDiagnostics& CandidateDiagnostics =
						MimicFrameDiagnostics.LocomotionTransitionCandidates.AddDefaulted_GetRef();
					CandidateDiagnostics.Animation = GetNameSafe(CandidateResult.SelectedAnim);
					CandidateDiagnostics.SelectedTimeSeconds = CandidateResult.SelectedTime;
					CandidateDiagnostics.bMirrored = CandidateResult.bIsMirrored;
					CandidateDiagnostics.PoseCost =
						PoseSearchTopLocomotionCandidateCostsForDiagnostics[CandidateIndex];
					CandidateDiagnostics.LeftFootOrientationDeltaDegrees =
						LeftFootOrientationDeltaDegrees;
					CandidateDiagnostics.RightFootOrientationDeltaDegrees =
						RightFootOrientationDeltaDegrees;
					CandidateDiagnostics.MaxFootOrientationDeltaDegrees =
						MaxFootOrientationDeltaDegrees;
					CandidateDiagnostics.MimicTargetStepDeltaL2 =
						MimicTargetStepDeltaL2;
				}
#endif
			}
		}

		if (bIdleToLocomotionTransition)
		{
			BridgeLocomotionMimicTransitionSource = MimicTargetPosesBuffer;
			BridgeLocomotionMimicTransitionElapsedSeconds = 0.0f;
			bBridgeLocomotionMimicTransitionActive =
				BridgeLocomotionMimicTransitionSource.Num() == PhysAnimBridge::MimicTargetPosesSize;
			PHYSANIM_LOG(
				LogPhysAnimBridge,
				Display,
				TEXT("[PhysAnim] Started locomotion mimic transition ramp from %s to %s@%.3f."),
				*GetNameSafe(PreviousPoseSearchResult.SelectedAnim),
				*GetNameSafe(SearchResult.SelectedAnim),
				SearchResult.SelectedTime);
		}
		else if (SearchResult.SelectedAnim && IsBridgePoseSearchIdleResult(SearchResult))
		{
			BridgeLocomotionMimicTransitionSource.Reset();
			BridgeLocomotionMimicTransitionElapsedSeconds = 0.0f;
			bBridgeLocomotionMimicTransitionActive = false;
		}

		if (bPoseSearchValid)
		{
			LastValidPoseSearchResult = SearchResult;
			ConsecutiveInvalidPoseSearchFrames = 0;
		}

		TArray<FPhysAnimFuturePoseSample> MannyFuturePoseSamples;
#if WITH_DEV_AUTOMATION_TESTS
		PhysAnimBridge::FPhysAnimMimicFrameDiagnostics* const MimicFrameDiagnosticsForSampling =
			bPolicyInputProvenanceTraceEnabledForTesting ? &MimicFrameDiagnostics : nullptr;
#else
		PhysAnimBridge::FPhysAnimMimicFrameDiagnostics* const MimicFrameDiagnosticsForSampling = nullptr;
#endif
		if (!SampleFuturePoses(
				SearchResult,
				MannyFuturePoseSamples,
				OutError,
				MimicFrameDiagnosticsForSampling))
		{
			return;
		}
		TArray<FPhysAnimFuturePoseSample> FuturePoseSamples;
		if (!PhysAnimProtoMannyAdapter::AdaptFuturePoseSamplesToCanonicalSmpl(
			MannyFuturePoseSamples,
			FuturePoseSamples,
			OutError))
		{
			return;
		}
#if WITH_DEV_AUTOMATION_TESTS
		const TArray<FPhysAnimFuturePoseSample> RawCanonicalFuturePoseSamplesForReplay = FuturePoseSamples;
		TArray<float> RawQueryTrajectorySampleTimesSecondsForReplay;
		TArray<FTransform> RawQueryTrajectoryWorldTransformsCmForReplay;
		TArray<float> QueryTrajectorySampleTimesSecondsForReplay;
		TArray<FTransform> QueryTrajectoryWorldTransformsCmForReplay;
#endif

		FTransform MimicTargetReferenceWorldRoot = FTransform::Identity;
		FTransform MimicTargetReferenceDataRoot = FTransform::Identity;
		if (!ResolveMimicTargetReferenceDataFrame(
			SearchResult,
			MimicTargetReferenceWorldRoot,
			MimicTargetReferenceDataRoot,
			OutError))
		{
			return;
		}

		if (ShouldUseBridgeTrajectoryPoseSearchState(RuntimeState))
		{
			if (!bBridgePoseSearchTrajectoryInitialized)
			{
				OutError = TEXT("E80 locomotion mimic placement requires an initialized Pose Search trajectory.");
				return;
			}
#if WITH_DEV_AUTOMATION_TESTS
			RawQueryTrajectorySampleTimesSecondsForReplay.Reserve(BridgePoseSearchTrajectory.Samples.Num());
			RawQueryTrajectoryWorldTransformsCmForReplay.Reserve(BridgePoseSearchTrajectory.Samples.Num());
			for (const FTransformTrajectorySample& RawTrajectorySample : BridgePoseSearchTrajectory.Samples)
			{
				RawQueryTrajectorySampleTimesSecondsForReplay.Add(RawTrajectorySample.TimeInSeconds);
				RawQueryTrajectoryWorldTransformsCmForReplay.Add(RawTrajectorySample.GetTransform());
			}
#endif
			const FTransformTrajectorySample CurrentTrajectorySample =
				BridgePoseSearchTrajectory.GetSampleAtTime(0.0f, true);
			const PhysAnimFrameContract::FWorldTransformCm CurrentQueryWorldRoot(
				CurrentTrajectorySample.GetTransform());
#if WITH_DEV_AUTOMATION_TESTS
			QueryTrajectorySampleTimesSecondsForReplay.Reserve(FuturePoseSamples.Num() + 1);
			QueryTrajectorySampleTimesSecondsForReplay.Add(0.0f);
			QueryTrajectoryWorldTransformsCmForReplay.Reserve(FuturePoseSamples.Num() + 1);
			QueryTrajectoryWorldTransformsCmForReplay.Add(CurrentTrajectorySample.GetTransform());
#endif
			TArray<PhysAnimFrameContract::FWorldTransformCm> FutureQueryWorldRoots;
			FutureQueryWorldRoots.Reserve(FuturePoseSamples.Num());
			for (const FPhysAnimFuturePoseSample& FuturePose : FuturePoseSamples)
			{
				const FTransformTrajectorySample FutureTrajectorySample =
					BridgePoseSearchTrajectory.GetSampleAtTime(
						FuturePose.FutureTimeSeconds,
						true);
				FutureQueryWorldRoots.Add(
					PhysAnimFrameContract::FWorldTransformCm(
						FutureTrajectorySample.GetTransform()));
#if WITH_DEV_AUTOMATION_TESTS
				QueryTrajectorySampleTimesSecondsForReplay.Add(FuturePose.FutureTimeSeconds);
				QueryTrajectoryWorldTransformsCmForReplay.Add(FutureTrajectorySample.GetTransform());
#endif
			}

			TArray<FPhysAnimFuturePoseSample> TrajectoryPlacedFuturePoses;
			if (!PhysAnimFrameContract::PlaceProtoFuturePosesOnWorldTrajectory(
				FuturePoseSamples,
				CurrentQueryWorldRoot,
				FutureQueryWorldRoots,
				PhysAnimFrameContract::FProtoWorldCanonicalTransformMeters(
					MimicTargetReferenceWorldRoot),
				PhysAnimFrameContract::FProtoAnimationDataCanonicalTransformMeters(
					MimicTargetReferenceDataRoot),
				TrajectoryPlacedFuturePoses,
				OutError))
			{
				return;
			}
			FuturePoseSamples = MoveTemp(TrajectoryPlacedFuturePoses);
		}

#if WITH_DEV_AUTOMATION_TESTS
		constexpr int32 CanonicalRootBodyIndex = 0;
		constexpr int32 CanonicalLeftFootBodyIndex = 3;
		if (bPolicyInputProvenanceTraceEnabledForTesting &&
			CurrentBodySamples.IsValidIndex(CanonicalRootBodyIndex) &&
			CurrentBodySamples.IsValidIndex(CanonicalLeftFootBodyIndex) &&
			RawCanonicalFuturePoseSamplesForReplay.IsValidIndex(0) &&
			FuturePoseSamples.IsValidIndex(0) &&
			RawCanonicalFuturePoseSamplesForReplay[0].BodyTransforms.IsValidIndex(CanonicalRootBodyIndex) &&
			RawCanonicalFuturePoseSamplesForReplay[0].BodyTransforms.IsValidIndex(CanonicalLeftFootBodyIndex) &&
			FuturePoseSamples[0].BodyTransforms.IsValidIndex(CanonicalRootBodyIndex) &&
			FuturePoseSamples[0].BodyTransforms.IsValidIndex(CanonicalLeftFootBodyIndex))
		{
			MimicFrameDiagnostics.bValid = true;
			MimicFrameDiagnostics.SelectedAnimation = GetNameSafe(SearchResult.SelectedAnim);
			MimicFrameDiagnostics.SelectedTimeSeconds = SearchResult.SelectedTime;
			MimicFrameDiagnostics.bMirrored = SearchResult.bIsMirrored;
			MimicFrameDiagnostics.FirstFutureTimeSeconds = FuturePoseSamples[0].FutureTimeSeconds;
			MimicFrameDiagnostics.ReferenceWorldRootProtoMeters = MimicTargetReferenceWorldRoot;
			MimicFrameDiagnostics.ReferenceDataRootProtoMeters = MimicTargetReferenceDataRoot;
			MimicFrameDiagnostics.CurrentRootProtoMeters = CurrentBodySamples[CanonicalRootBodyIndex].Position;
			MimicFrameDiagnostics.CurrentLeftFootProtoMeters = CurrentBodySamples[CanonicalLeftFootBodyIndex].Position;
			MimicFrameDiagnostics.RawFirstFutureRootProtoMeters =
				RawCanonicalFuturePoseSamplesForReplay[0].BodyTransforms[CanonicalRootBodyIndex].GetLocation();
			MimicFrameDiagnostics.RawFirstFutureLeftFootProtoMeters =
				RawCanonicalFuturePoseSamplesForReplay[0].BodyTransforms[CanonicalLeftFootBodyIndex].GetLocation();
			MimicFrameDiagnostics.PlacedFirstFutureRootProtoMeters =
				FuturePoseSamples[0].BodyTransforms[CanonicalRootBodyIndex].GetLocation();
			MimicFrameDiagnostics.PlacedFirstFutureLeftFootProtoMeters =
				FuturePoseSamples[0].BodyTransforms[CanonicalLeftFootBodyIndex].GetLocation();

			constexpr int32 RotationProbeFutureIndex = 2;
			constexpr int32 CanonicalRightFootBodyIndex = 7;
			if (RawCanonicalFuturePoseSamplesForReplay.IsValidIndex(RotationProbeFutureIndex) &&
				FuturePoseSamples.IsValidIndex(RotationProbeFutureIndex) &&
				RawCanonicalFuturePoseSamplesForReplay[RotationProbeFutureIndex].BodyTransforms.IsValidIndex(CanonicalRootBodyIndex) &&
				RawCanonicalFuturePoseSamplesForReplay[RotationProbeFutureIndex].BodyTransforms.IsValidIndex(CanonicalRightFootBodyIndex) &&
				FuturePoseSamples[RotationProbeFutureIndex].BodyTransforms.IsValidIndex(CanonicalRootBodyIndex) &&
				FuturePoseSamples[RotationProbeFutureIndex].BodyTransforms.IsValidIndex(CanonicalRightFootBodyIndex))
			{
				MimicFrameDiagnostics.RotationProbeFutureIndex = RotationProbeFutureIndex;
				MimicFrameDiagnostics.RotationProbeFutureTimeSeconds =
					FuturePoseSamples[RotationProbeFutureIndex].FutureTimeSeconds;
				MimicFrameDiagnostics.RawProbeFutureRootRotation =
					RawCanonicalFuturePoseSamplesForReplay[RotationProbeFutureIndex].BodyTransforms[CanonicalRootBodyIndex].GetRotation();
				MimicFrameDiagnostics.RawProbeFutureRightFootRotation =
					RawCanonicalFuturePoseSamplesForReplay[RotationProbeFutureIndex].BodyTransforms[CanonicalRightFootBodyIndex].GetRotation();
				MimicFrameDiagnostics.PlacedProbeFutureRootRotation =
					FuturePoseSamples[RotationProbeFutureIndex].BodyTransforms[CanonicalRootBodyIndex].GetRotation();
				MimicFrameDiagnostics.PlacedProbeFutureRightFootRotation =
					FuturePoseSamples[RotationProbeFutureIndex].BodyTransforms[CanonicalRightFootBodyIndex].GetRotation();
			}
		}

		const bool bRecordFirstPolicyGroundReference =
			bStartupChronologyTraceEnabledForTesting &&
			StandingVariantForTesting == EPhysAnimStandingVariant::RealOnnxPolicy &&
			bEnablePolicyInference &&
			bStandingVariantUsesPolicyInference &&
			PolicyControlTicksExecuted == 1 &&
			!FirstPolicyGroundReferenceTrace.bFirstPolicyRecorded;
		PhysAnimBridge::FPhysAnimSelfObservationGroundReferenceValues GroundReferenceValues;
		const float MimicTargetReferenceGroundHeight = ResolveSelfObservationGroundHeight(
			CurrentBodySamples,
			bRecordFirstPolicyGroundReference ? &GroundReferenceValues : nullptr);
		FString GroundReferenceTraceError;
		// This diagnostic must remain observational: validation failures are published
		// by the trace artifact and must not abort or otherwise alter policy execution.
		FirstPolicyGroundReferenceTrace.RecordFirstPolicyIf(
			bRecordFirstPolicyGroundReference,
			TEXT("first_policy_self_observation"),
			GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0,
			GetRuntimeStateName(RuntimeState),
			PolicyControlTicksExecuted,
			GroundReferenceValues,
			GroundReferenceTraceError);
#else
		const float MimicTargetReferenceGroundHeight = ResolveSelfObservationGroundHeight(CurrentBodySamples);
#endif
		if (!PhysAnimBridge::BuildSelfObservation(CurrentBodySamples, MimicTargetReferenceGroundHeight, SelfObservationBuffer, OutError))
		{
			return;
		}

		TArray<FPhysAnimBodySample> MimicCurrentReferenceBodySamples;
		MakeMimicTargetDataFrameBodySamples(
			CurrentBodySamples,
			MimicTargetReferenceWorldRoot,
			MimicTargetReferenceDataRoot,
			MimicCurrentReferenceBodySamples);
#if WITH_DEV_AUTOMATION_TESTS
		if (MimicFrameDiagnostics.bValid &&
			MimicCurrentReferenceBodySamples.IsValidIndex(0) &&
			MimicCurrentReferenceBodySamples.IsValidIndex(7))
		{
			MimicFrameDiagnostics.CurrentDataRootRotation =
				MimicCurrentReferenceBodySamples[0].Rotation;
			MimicFrameDiagnostics.CurrentDataRightFootRotation =
				MimicCurrentReferenceBodySamples[7].Rotation;
		}
#endif

		if (!PhysAnimBridge::BuildMimicTargetPoses(MimicCurrentReferenceBodySamples, FuturePoseSamples, MimicTargetPosesBuffer, OutError))
		{
			return;
		}

		if (bBridgeLocomotionMimicTransitionActive)
		{
			constexpr float LocomotionMimicTransitionDurationSeconds = 0.30f;
			const float TransitionAdvanceSeconds =
				FMath::Max(ElapsedPolicySteps, 1) * PolicyControlIntervalSeconds;
			BridgeLocomotionMimicTransitionElapsedSeconds = FMath::Min(
				BridgeLocomotionMimicTransitionElapsedSeconds + TransitionAdvanceSeconds,
				LocomotionMimicTransitionDurationSeconds);
			const float TransitionAlpha = FMath::Clamp(
				BridgeLocomotionMimicTransitionElapsedSeconds /
					LocomotionMimicTransitionDurationSeconds,
				0.0f,
				1.0f);
			TArray<float> BlendedMimicTarget;
			FString TransitionBlendError;
			if (!PhysAnimBridge::BlendMimicTargetPosesForTransition(
					BridgeLocomotionMimicTransitionSource,
					MimicTargetPosesBuffer,
					TransitionAlpha,
					BlendedMimicTarget,
					TransitionBlendError))
			{
				OutError = FString::Printf(
					TEXT("Could not blend the locomotion mimic transition: %s"),
					*TransitionBlendError);
				return;
			}
			MimicTargetPosesBuffer = MoveTemp(BlendedMimicTarget);
			if (TransitionAlpha >= 1.0f - KINDA_SMALL_NUMBER)
			{
				BridgeLocomotionMimicTransitionSource.Reset();
				BridgeLocomotionMimicTransitionElapsedSeconds = 0.0f;
				bBridgeLocomotionMimicTransitionActive = false;
				PHYSANIM_LOG(
					LogPhysAnimBridge,
					Display,
					TEXT("[PhysAnim] Completed locomotion mimic transition ramp."));
			}
		}

#if WITH_DEV_AUTOMATION_TESTS
		const bool bCapturePolicyInputProvenanceThisStep =
			bPolicyInputProvenanceTraceEnabledForTesting &&
			bEnablePolicyInference &&
			bStandingVariantUsesPolicyInference &&
			!FirstPolicyInputProvenanceSnapshot.bCaptured;
		TArray<float> TerrainGroundHeightsForDiagnostics;
		FTransform TerrainRootWorldTransformForDiagnostics = FTransform::Identity;
#endif
		if (!BuildTerrainObservation(
			CurrentBodySamples,
			TerrainBuffer,
			OutError
#if WITH_DEV_AUTOMATION_TESTS
			,
			bCapturePolicyInputProvenanceThisStep ? &TerrainGroundHeightsForDiagnostics : nullptr,
			bCapturePolicyInputProvenanceThisStep ? &TerrainRootWorldTransformForDiagnostics : nullptr
#endif
			))
		{
			return;
		}

		const int32 V0PlantReviewMode = PhysAnimComponentInternal::CVarPhysAnimV0PlantReviewMode.GetValueOnGameThread();
		const bool bV0PlantReviewStaticTargets = V0PlantReviewMode == 1 && RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing;
#if WITH_DEV_AUTOMATION_TESTS
		if (bCapturePolicyInputProvenanceThisStep && !bV0PlantReviewStaticTargets)
		{
			const AActor* const OwnerActor = GetOwner();
			const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
			const UWorld* const World = GetWorld();
			FirstPolicyInputProvenanceSnapshot.CaptureFirstIf(
				true,
				GetRuntimeStateName(RuntimeState),
				World ? World->GetTimeSeconds() : 0.0,
				PolicyControlTicksExecuted,
				GetNameSafe(SearchResult.SelectedAnim),
				SearchResult.SelectedTime,
				SearchResult.bIsMirrored,
				OwnerActor ? OwnerActor->GetActorTransform() : FTransform::Identity,
				SkeletalMesh ? SkeletalMesh->GetComponentTransform() : FTransform::Identity,
				TerrainRootWorldTransformForDiagnostics,
				MimicTargetReferenceWorldRoot,
				MimicTargetReferenceDataRoot,
				MimicTargetReferenceGroundHeight,
				MannyCurrentBodySamples,
				CurrentBodySamples,
				MimicCurrentReferenceBodySamples,
				MannyFuturePoseSamples,
				FuturePoseSamples,
				TerrainGroundHeightsForDiagnostics,
				ActionOutputBuffer);
		}
#endif
		if (!bEnablePolicyInference || !bStandingVariantUsesPolicyInference)
		{
			// SafetyGrip fallback: neural inference is disabled. Zero the raw action buffer
			// so ConditionModelActions produces zero conditioned actions, keeping joints at
			// their current reference-pose PD targets with no neural actuation.
			ActionOutputBuffer.Init(0.0f, PhysAnimBridge::NumActionFloats);
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Verbose, 1.0f,
				TEXT("[PhysAnim] INFERENCE_SKIPPED bEnablePolicyInference=false state=%s — holding reference pose."),
				GetRuntimeStateName(RuntimeState));
		}
		else if (!bV0PlantReviewStaticTargets && !RunInference(OutError))
		{
			return;
		}

		if ((V0PlantReviewMode == 2 || bStandingVariantForcesZeroActions) &&
			IsCausalPolicyControlRuntimeState(RuntimeState))
		{
			ActionOutputBuffer.Init(0.0f, PhysAnimBridge::NumActionFloats);
		}

		if (!ConditionModelActions(EffectiveSettings, OutError))
		{
			return;
		}
#if WITH_DEV_AUTOMATION_TESTS
		const bool bCaptureLocomotionFrameReplayThisStep =
			PhysAnimBridge::ShouldCaptureLocomotionFrameReplay(
				bPolicyInputProvenanceTraceEnabledForTesting,
				bEnablePolicyInference,
				bStandingVariantUsesPolicyInference,
				ShouldUseBridgeTrajectoryPoseSearchState(RuntimeState),
				FirstLocomotionFrameReplaySnapshot.bCaptured);
		if (bCaptureLocomotionFrameReplayThisStep)
		{
			TArray<FPhysAnimBodySample> PhysicalBodySamplesForReplay;
			if (!GatherCurrentPhysicalBodySamplesForReplay(
				PhysicalBodySamplesForReplay,
				OutError))
			{
				return;
			}

			const AActor* const OwnerActor = GetOwner();
			const USkeletalMeshComponent* const SkeletalMesh = MeshComponent.Get();
			const UWorld* const World = GetWorld();
			if (!FirstLocomotionFrameReplaySnapshot.CaptureFirstIf(
				true,
				GetRuntimeStateName(RuntimeState),
				World ? World->GetTimeSeconds() : 0.0,
				PolicyControlTicksExecuted,
				GetNameSafe(SearchResult.SelectedAnim),
				SearchResult.SelectedTime,
				SearchResult.bIsMirrored,
				OwnerActor ? OwnerActor->GetActorTransform() : FTransform::Identity,
				SkeletalMesh ? SkeletalMesh->GetComponentTransform() : FTransform::Identity,
				MimicTargetReferenceWorldRoot,
				MimicTargetReferenceDataRoot,
				BridgeIntentState.IntentMagnitude,
				BridgeTrajectoryState.DesiredVelocityCmPerSecond,
				BridgeTrajectoryState.AcceptedVelocityCmPerSecond,
				BridgeTrajectoryState.QueryVelocityCmPerSecond,
				EffectiveSettings.bBridgePoseSearchUseStabilizedWalkQuerySpeed,
				EffectiveSettings.BridgePoseSearchWalkIntentThreshold,
				EffectiveSettings.BridgePoseSearchStabilizedWalkSpeedCmPerSecond,
				EffectiveSettings.BridgePoseSearchIdlePredictedSpeedCutoffCmPerSecond,
				RawQueryTrajectorySampleTimesSecondsForReplay,
				RawQueryTrajectoryWorldTransformsCmForReplay,
				QueryTrajectorySampleTimesSecondsForReplay,
				QueryTrajectoryWorldTransformsCmForReplay,
				MannyCurrentBodySamples,
				PhysicalBodySamplesForReplay,
				CurrentBodySamples,
				RawCanonicalFuturePoseSamplesForReplay,
				FuturePoseSamples,
				SelfObservationBuffer,
				MimicTargetPosesBuffer,
				TerrainBuffer,
				ConditionedActionBuffer))
			{
				OutError = TEXT("Could not capture the first locomotion frame replay step.");
				return;
			}
			FString ReplayValidationError;
			if (!PhysAnimBridge::ValidateLocomotionFrameReplaySnapshot(
				FirstLocomotionFrameReplaySnapshot,
				ReplayValidationError))
			{
				OutError = FString::Printf(
					TEXT("Locomotion frame replay validation failed: %s"),
					*ReplayValidationError);
				return;
			}
		}
#endif
	}

	// Standing topology and gains are published and read back by TickRuntimeStateMachine
	// before target dispatch. Keep the legacy tuning path only for disconnected future states.
	if (!IsCausalPolicyControlRuntimeState(RuntimeState))
	{
		ApplyRuntimeControlTuning(EffectiveSettings);
	}
	if (RuntimeState == EPhysAnimRuntimeState::Standing_Preparation ||
		RuntimeState == EPhysAnimRuntimeState::Standing_FullSimulationActivation)
	{
		return;
	}
	ApplyControlTargets(
		bRunPolicyUpdateThisTick ? (PolicyControlIntervalSeconds * FMath::Max(ElapsedPolicySteps, 1)) : 0.0f,
		EffectiveSettings,
		bRunPolicyUpdateThisTick,
		OutError);
}

void UPhysAnimComponent::UpdateStartupMovementLockState(const FPhysAnimStabilizationSettings& EffectiveSettings)
{
	if (!bStartupMovementLockActive)
	{
		return;
	}

	const bool bInStableState = RuntimeState == EPhysAnimRuntimeState::BridgeActive || IsBalanceActiveState(RuntimeState);
	if (!bInStableState)
	{
		return;
	}

	if (!EffectiveSettings.bLockCharacterMovementUntilStartupReady)
	{
		ReleaseStartupMovementLock(true);
		PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Released startup movement lock because startup movement locking is disabled."));
		return;
	}

	const float PolicyInfluenceAlpha = CalculateCurrentPolicyInfluenceAlpha(EffectiveSettings);
	if (!EffectiveSettings.bDelayMovementUnlockUntilPolicySettled)
	{
		if (PolicyInfluenceAlpha > KINDA_SMALL_NUMBER)
		{
			ReleaseStartupMovementLock(EffectiveSettings.bRestoreCharacterMovementAfterStartupReady);
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Released startup movement lock after bridge became ready."));
		}
	}
	else if (PolicyInfluenceAlpha >= EffectiveSettings.PolicySettleMinInfluenceAlpha)
	{
		float ShellOffset, LinSpeed, AngSpeed;
		if (UpdatePolicySettleWindow(EffectiveSettings, ShellOffset, LinSpeed, AngSpeed))
		{
			ReleaseStartupMovementLock(EffectiveSettings.bRestoreCharacterMovementAfterStartupReady);
			PHYSANIM_LOG_RATE_LIMITED(LogPhysAnimBridge, Log, 1.0f, TEXT("[PhysAnim] Released startup movement lock after policy settled."));
		}
	}
}

void UPhysAnimComponent::HandleInitialPoseSearchWait(
	float DeltaTime,
	const FPhysAnimStabilizationSettings& EffectiveSettings,
	FString& OutError,
	FPoseSearchBlueprintResult& OutSearchResult)
{
	(void)DeltaTime;
	(void)EffectiveSettings;
	const bool bPoseSearchValid = QueryPoseSearch(OutSearchResult, OutError);
	RecordLiveRuntimeEvidencePoseSearchQueryResult(
		bPoseSearchValid,
		OutSearchResult.SelectedAnim ? OutSearchResult.SelectedAnim->GetName() : TEXT(""));

	if (!bPoseSearchValid)
	{
		const double WaitSeconds = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - InitialPoseSearchWaitStartTimeSeconds;
		if (IsInitialPoseSearchWaitTimedOut(WaitSeconds, PhysAnimComponentInternal::InitialPoseSearchWaitTimeoutSeconds))
		{
			FailStop(FString::Printf(TEXT("Initial PoseSearch result was never produced. %s"), *OutError));
		}
		return;
	}

	LastValidPoseSearchResult = OutSearchResult;
	ConsecutiveInvalidPoseSearchFrames = 0;
	if (!ActivateRuntimePhysicsControl(OutError))
	{
		FailStop(FString::Printf(TEXT("Failed to create standing Physics Control records: %s"), *OutError));
		return;
	}
	TransitionRuntimeState(EPhysAnimRuntimeState::BridgeActive);
}
