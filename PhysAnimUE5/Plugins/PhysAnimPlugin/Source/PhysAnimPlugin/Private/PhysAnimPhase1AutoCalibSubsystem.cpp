#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimComponentPrivate.h"


#include "Algo/Sort.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Math/RandomStream.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr int32 StageASamplesPerPreset = 24;
	constexpr int32 StageBTopK = 8;
	constexpr int32 StageBRounds = 3;
	constexpr int32 StageCTopK = 5;
	constexpr int32 StageCRepetitions = 5;
	constexpr float RestoreFingerprintTolerance = 1.0e-3f;
	constexpr float ScoreReproTolerance = 1.0e-3f;

	bool MatchesFilter(const UPhysAnimComponent& Component, const FString& FilterLower)
	{
		if (FilterLower.IsEmpty() || FilterLower == TEXT("<all>") || FilterLower == TEXT("all"))
		{
			return true;
		}

		const AActor* const Owner = Component.GetOwner();
		if (!Owner)
		{
			return false;
		}

		const FString OwnerNameLower = Owner->GetName().ToLower();
		const FString PathNameLower = Owner->GetPathName().ToLower();
		return OwnerNameLower.Contains(FilterLower) || PathNameLower.Contains(FilterLower);
	}

	float SampleStratifiedValue(const float MinValue, const float MaxValue, const int32 StratumIndex, const int32 NumStrata, FRandomStream& RandomStream)
	{
		const float FractionMin = static_cast<float>(StratumIndex) / static_cast<float>(NumStrata);
		const float FractionMax = static_cast<float>(StratumIndex + 1) / static_cast<float>(NumStrata);
		return FMath::Lerp(MinValue, MaxValue, RandomStream.FRandRange(FractionMin, FractionMax));
	}

	FString JsonEscape(const FString& Value)
	{
		FString Escaped = Value;
		Escaped.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Escaped.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Escaped.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Escaped.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Escaped.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return Escaped;
	}

	const TCHAR* StrategyPresetToString(const EPhase1AutoCalibStrategyPreset Preset)
	{
		switch (Preset)
		{
		case EPhase1AutoCalibStrategyPreset::SpineBiased:
			return TEXT("SpineBiased");
		case EPhase1AutoCalibStrategyPreset::WorstThighBiased:
			return TEXT("WorstThighBiased");
		case EPhase1AutoCalibStrategyPreset::BalancedCoupled:
			return TEXT("BalancedCoupled");
		case EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh:
			return TEXT("SpineThenWorstThigh");
		case EPhase1AutoCalibStrategyPreset::RescueOnly:
			return TEXT("RescueOnly");
		case EPhase1AutoCalibStrategyPreset::CurrentDefault:
		default:
			return TEXT("CurrentDefault");
		}
	}

	bool IsLaterThanPhase1(const EBalanceReadyTransitionPhase Phase)
	{
		return Phase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
			Phase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3 ||
			Phase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle ||
			Phase == EBalanceReadyTransitionPhase::BRT_Succeeded;
	}

	bool AreTransformsNear(const FTransform& A, const FTransform& B, const float Tolerance)
	{
		return A.GetLocation().Equals(B.GetLocation(), Tolerance) &&
			A.GetRotation().Equals(B.GetRotation(), Tolerance) &&
			A.GetScale3D().Equals(B.GetScale3D(), Tolerance);
	}

	bool AreFingerprintsNear(
		const FPhase1AutoCalibDeterminismFingerprint& A,
		const FPhase1AutoCalibDeterminismFingerprint& B,
		const float Tolerance)
	{
		return A.RuntimeState == B.RuntimeState &&
			A.TransitionPhase == B.TransitionPhase &&
			AreTransformsNear(A.OwnerTransform, B.OwnerTransform, Tolerance) &&
			AreTransformsNear(A.MeshTransform, B.MeshTransform, Tolerance) &&
			AreTransformsNear(A.RootBodyTransform, B.RootBodyTransform, Tolerance) &&
			A.RootLinearVelocity.Equals(B.RootLinearVelocity, Tolerance) &&
			A.RootAngularVelocity.Equals(B.RootAngularVelocity, Tolerance) &&
			FMath::IsNearlyEqual(A.ShellOffsetDeltaCm, B.ShellOffsetDeltaCm, Tolerance) &&
			FMath::IsNearlyEqual(A.ShellVelocityDeltaCmPerSecond, B.ShellVelocityDeltaCmPerSecond, Tolerance) &&
			FMath::IsNearlyEqual(A.MaxTargetDeltaDeg, B.MaxTargetDeltaDeg, Tolerance) &&
			FMath::IsNearlyEqual(A.MeanTargetDeltaDeg, B.MeanTargetDeltaDeg, Tolerance) &&
			A.PendingResetCount == B.PendingResetCount;
	}

	bool ParamsEqual(const FPhase1AutoCalibParams& A, const FPhase1AutoCalibParams& B, const float Tolerance)
	{
		return A.SourcePreset == B.SourcePreset &&
			A.SeedFamilyPreset == B.SeedFamilyPreset &&
			FMath::IsNearlyEqual(A.SpineInterpolationAlpha, B.SpineInterpolationAlpha, Tolerance) &&
			FMath::IsNearlyEqual(A.WorstThighInterpolationAlpha, B.WorstThighInterpolationAlpha, Tolerance) &&
			FMath::IsNearlyEqual(A.FocusedDeltaScale, B.FocusedDeltaScale, Tolerance) &&
			FMath::IsNearlyEqual(A.UprightnessWeightScale, B.UprightnessWeightScale, Tolerance) &&
			FMath::IsNearlyEqual(A.ClampStrengthScale, B.ClampStrengthScale, Tolerance) &&
			FMath::IsNearlyEqual(A.PelvisPitchBiasDeg, B.PelvisPitchBiasDeg, Tolerance) &&
			FMath::IsNearlyEqual(A.PelvisRollBiasDeg, B.PelvisRollBiasDeg, Tolerance);
	}

	bool Dominates(const FPhase1AutoCalibTrialResult& Candidate, const FPhase1AutoCalibTrialResult& Other)
	{
		const auto ScoreGateRank = [](const FPhase1AutoCalibScore& Score) -> int32
		{
			if (Score.bContractPassed)
			{
				return 0;
			}
			if (!Score.bRestoreDeterministic)
			{
				return 4;
			}
			if (Score.bTimedOut)
			{
				return 3;
			}
			if (Score.bSafeDenied)
			{
				return 2;
			}
			return 1;
		};

		const int32 CandidateGate = ScoreGateRank(Candidate.Score);
		const int32 OtherGate = ScoreGateRank(Other.Score);
		if (CandidateGate > OtherGate)
		{
			return false;
		}

		const bool bGateStrictlyBetter = CandidateGate < OtherGate;
		const bool bAllNoWorse =
			Candidate.Score.WorstDirectLinkAngularErrorDeg <= Other.Score.WorstDirectLinkAngularErrorDeg + ScoreReproTolerance &&
			Candidate.Score.MeanTargetDeltaDeg <= Other.Score.MeanTargetDeltaDeg + ScoreReproTolerance &&
			Candidate.Score.MaxTargetDeltaDeg <= Other.Score.MaxTargetDeltaDeg + ScoreReproTolerance &&
			Candidate.Score.ThighAsymmetryDeg <= Other.Score.ThighAsymmetryDeg + ScoreReproTolerance &&
			Candidate.Score.PeakRootTiltDeg <= Other.Score.PeakRootTiltDeg + ScoreReproTolerance &&
			Candidate.Score.ShellOffsetDeltaCm <= Other.Score.ShellOffsetDeltaCm + ScoreReproTolerance &&
			Candidate.Score.ShellVelocityDeltaCmPerSecond <= Other.Score.ShellVelocityDeltaCmPerSecond + ScoreReproTolerance &&
			Candidate.Score.PeakRootLinearSpeedCmPerSecond <= Other.Score.PeakRootLinearSpeedCmPerSecond + ScoreReproTolerance &&
			Candidate.Score.PeakRootAngularSpeedDegPerSecond <= Other.Score.PeakRootAngularSpeedDegPerSecond + ScoreReproTolerance;
		const bool bAnyStrictlyBetter =
			Candidate.Score.WorstDirectLinkAngularErrorDeg + ScoreReproTolerance < Other.Score.WorstDirectLinkAngularErrorDeg ||
			Candidate.Score.MeanTargetDeltaDeg + ScoreReproTolerance < Other.Score.MeanTargetDeltaDeg ||
			Candidate.Score.MaxTargetDeltaDeg + ScoreReproTolerance < Other.Score.MaxTargetDeltaDeg ||
			Candidate.Score.ThighAsymmetryDeg + ScoreReproTolerance < Other.Score.ThighAsymmetryDeg ||
			Candidate.Score.PeakRootTiltDeg + ScoreReproTolerance < Other.Score.PeakRootTiltDeg ||
			Candidate.Score.ShellOffsetDeltaCm + ScoreReproTolerance < Other.Score.ShellOffsetDeltaCm ||
			Candidate.Score.ShellVelocityDeltaCmPerSecond + ScoreReproTolerance < Other.Score.ShellVelocityDeltaCmPerSecond ||
			Candidate.Score.PeakRootLinearSpeedCmPerSecond + ScoreReproTolerance < Other.Score.PeakRootLinearSpeedCmPerSecond ||
			Candidate.Score.PeakRootAngularSpeedDegPerSecond + ScoreReproTolerance < Other.Score.PeakRootAngularSpeedDegPerSecond;
		return bAllNoWorse && (bGateStrictlyBetter || bAnyStrictlyBetter);
	}

	FPhase1AutoCalibParams ClampParams(FPhase1AutoCalibParams Params)
	{
		Params.SpineInterpolationAlpha = FMath::Clamp(Params.SpineInterpolationAlpha, 0.01f, 0.80f);
		Params.WorstThighInterpolationAlpha = FMath::Clamp(Params.WorstThighInterpolationAlpha, 0.01f, 0.20f);
		Params.FocusedDeltaScale = FMath::Clamp(Params.FocusedDeltaScale, 0.50f, 2.00f);
		Params.UprightnessWeightScale = FMath::Clamp(Params.UprightnessWeightScale, 0.50f, 1.50f);
		Params.ClampStrengthScale = FMath::Clamp(Params.ClampStrengthScale, 0.50f, 1.50f);
		Params.PelvisPitchBiasDeg = FMath::Clamp(Params.PelvisPitchBiasDeg, -1.0f, 1.0f);
		Params.PelvisRollBiasDeg = FMath::Clamp(Params.PelvisRollBiasDeg, -1.0f, 1.0f);
		return Params;
	}

	void ShuffleIndices(TArray<int32>& OutIndices, const int32 Count, FRandomStream& RandomStream)
	{
		OutIndices.Reset(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			OutIndices.Add(Index);
		}

		for (int32 Index = Count - 1; Index > 0; --Index)
		{
			const int32 SwapIndex = RandomStream.RandRange(0, Index);
			OutIndices.Swap(Index, SwapIndex);
		}
	}
}

bool UPhysAnimPhase1AutoCalibSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::PIE || WorldType == EWorldType::Game;
}

void UPhysAnimPhase1AutoCalibSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bRunActive)
	{
		return;
	}

	if (!TargetComponent.IsValid())
	{
		StopPhase1AutoCalib(TEXT("target_component_lost"));
		LastError = TEXT("Phase1 auto-calibration target component was destroyed during the run.");
		return;
	}

	if (bTrialActive)
	{
		TickActiveTrial();
		return;
	}

	if (CurrentStage == EAutoCalibStage::AwaitingReadiness)
	{
		TickAwaitingReadiness();
		return;
	}

	if (!BeginNextTrial())
	{
		StopPhase1AutoCalib(TEXT("begin_trial_failed"));
	}
}

TStatId UPhysAnimPhase1AutoCalibSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPhysAnimPhase1AutoCalibSubsystem, STATGROUP_Tickables);
}

void UPhysAnimPhase1AutoCalibSubsystem::Deinitialize()
{
	StopPhase1AutoCalib(TEXT("deinitialize"));
	Super::Deinitialize();
}

bool UPhysAnimPhase1AutoCalibSubsystem::StartPhase1AutoCalib(const FPhase1AutoCalibRequest& Request, FString& OutError)
{
	OutError.Reset();
	LastError.Reset();
	LatestReport = FPhase1AutoCalibReport();

	if (bRunActive)
	{
		StopPhase1AutoCalib(TEXT("restart"));
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		OutError = TEXT("Phase1 auto-calibration requires a valid PIE/game world.");
		LastError = OutError;
		return false;
	}

	UPhysAnimComponent* Component = nullptr;
	if (!ResolveTargetComponent(Request.OwnerFilter, Component, OutError))
	{
		LastError = OutError;
		return false;
	}

	Component->StopBalancePerturbationMode();
	StopPhase1AutoCalib(TEXT("restart"));
	ActiveRequest = Request;
	TargetComponent = Component;
	OriginalBalanceEntryMinPolicyAlpha = Component->StabilizationSettings.BalanceEntryMinPolicyAlpha;
	Component->StabilizationSettings.BalanceEntryMinPolicyAlpha = 2.0f;
	
	CurrentStage = EAutoCalibStage::AwaitingReadiness;
	CurrentStageStartTimeSeconds = World->GetTimeSeconds();
	LastReadinessLogTimeSeconds = -1.0;
	PendingTrials.Reset();
	StageAResults.Reset();
	StageBResults.Reset();
	StageCResults.Reset();
	NextTrialId = 0;

	LatestReport.OutputDirectory = BuildOutputDirectory();
	IFileManager::Get().MakeDirectory(*LatestReport.OutputDirectory, true);
	LatestReport.TrialsCsvPath = FPaths::Combine(LatestReport.OutputDirectory, TEXT("trials.csv"));
	LatestReport.SummaryPath = FPaths::Combine(LatestReport.OutputDirectory, TEXT("summary.json"));
	LatestReport.ParetoJsonPath = FPaths::Combine(LatestReport.OutputDirectory, TEXT("pareto.json"));

	TArray<FPhase1AutoCalibParams> StageACandidates;
	BuildStageACandidates(ActiveRequest.Seed, ActiveRequest.MaxTrials, StageACandidates);
	for (const FPhase1AutoCalibParams& Params : StageACandidates)
	{
		FPendingTrial& Pending = PendingTrials.AddDefaulted_GetRef();
		Pending.Params = Params;
		Pending.StageName = TEXT("stage_a");
	}

	if (PendingTrials.IsEmpty())
	{
		OutError = TEXT("Phase1 auto-calibration did not generate any Stage A candidates.");
		LastError = OutError;
		return false;
	}

	bRunActive = true;
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimAutoCalib] Awaiting component readiness for baseline capture..."));

	return true;
}

void UPhysAnimPhase1AutoCalibSubsystem::StopPhase1AutoCalib(const FString& Reason)
{
	if (UPhysAnimComponent* const Component = TargetComponent.Get())
	{
		Component->StabilizationSettings.BalanceEntryMinPolicyAlpha = OriginalBalanceEntryMinPolicyAlpha;
		Component->ClearPhase1AutoCalibParams();

		FString RestoreError;
		if (!BaselineSnapshot.Bodies.IsEmpty())
		{
			Component->RestorePhase1AutoCalibBaseline(BaselineSnapshot, RestoreError);
		}

		if (!RestoreError.IsEmpty() && LastError.IsEmpty())
		{
			LastError = RestoreError;
		}
	}

	bRunActive = false;
	bTrialActive = false;
	CurrentStage = Reason == TEXT("completed") ? EAutoCalibStage::Completed : EAutoCalibStage::Inactive;
	TargetComponent.Reset();
	PendingTrials.Reset();
	ActiveTrial = FPendingTrial();
	ActiveTrialStartTimeSeconds = -1.0;
	ActiveTrialPeakMetrics = FPhase1AutoCalibLiveMetrics();
}

void UPhysAnimPhase1AutoCalibSubsystem::BuildStageACandidates(int32 Seed, int32 MaxTrials, TArray<FPhase1AutoCalibParams>& OutCandidates)
{
	OutCandidates.Reset();

	static const EPhase1AutoCalibStrategyPreset Presets[] =
	{
		EPhase1AutoCalibStrategyPreset::CurrentDefault,
		EPhase1AutoCalibStrategyPreset::SpineBiased,
		EPhase1AutoCalibStrategyPreset::WorstThighBiased,
		EPhase1AutoCalibStrategyPreset::BalancedCoupled,
		EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh,
		EPhase1AutoCalibStrategyPreset::RescueOnly
	};

	FRandomStream RandomStream(Seed);
	for (const EPhase1AutoCalibStrategyPreset Preset : Presets)
	{
		TArray<int32> SpineStrata;
		TArray<int32> ThighStrata;
		TArray<int32> FocusStrata;
		TArray<int32> UprightStrata;
		TArray<int32> ClampStrata;
		TArray<int32> PitchStrata;
		TArray<int32> RollStrata;
		ShuffleIndices(SpineStrata, StageASamplesPerPreset, RandomStream);
		ShuffleIndices(ThighStrata, StageASamplesPerPreset, RandomStream);
		ShuffleIndices(FocusStrata, StageASamplesPerPreset, RandomStream);
		ShuffleIndices(UprightStrata, StageASamplesPerPreset, RandomStream);
		ShuffleIndices(ClampStrata, StageASamplesPerPreset, RandomStream);
		ShuffleIndices(PitchStrata, StageASamplesPerPreset, RandomStream);
		ShuffleIndices(RollStrata, StageASamplesPerPreset, RandomStream);

		for (int32 SampleIndex = 0; SampleIndex < StageASamplesPerPreset; ++SampleIndex)
		{
			if (MaxTrials > 0 && OutCandidates.Num() >= MaxTrials)
			{
				return;
			}

			FPhase1AutoCalibParams Params;
			Params.SourcePreset = Preset;
			Params.SeedFamilyPreset = Preset;
			Params.SpineInterpolationAlpha = SampleStratifiedValue(0.01f, 0.80f, SpineStrata[SampleIndex], StageASamplesPerPreset, RandomStream);
			Params.WorstThighInterpolationAlpha = SampleStratifiedValue(0.01f, 0.20f, ThighStrata[SampleIndex], StageASamplesPerPreset, RandomStream);
			Params.FocusedDeltaScale = SampleStratifiedValue(0.50f, 2.00f, FocusStrata[SampleIndex], StageASamplesPerPreset, RandomStream);
			Params.UprightnessWeightScale = SampleStratifiedValue(0.50f, 1.50f, UprightStrata[SampleIndex], StageASamplesPerPreset, RandomStream);
			Params.ClampStrengthScale = SampleStratifiedValue(0.50f, 1.50f, ClampStrata[SampleIndex], StageASamplesPerPreset, RandomStream);
			Params.PelvisPitchBiasDeg = SampleStratifiedValue(-1.0f, 1.0f, PitchStrata[SampleIndex], StageASamplesPerPreset, RandomStream);
			Params.PelvisRollBiasDeg = SampleStratifiedValue(-1.0f, 1.0f, RollStrata[SampleIndex], StageASamplesPerPreset, RandomStream);
			OutCandidates.Add(ClampParams(Params));
		}
	}
}

void UPhysAnimPhase1AutoCalibSubsystem::BuildStageBRefinementCandidates(
	const TArray<FPhase1AutoCalibTrialResult>& StageAResults,
	int32 MaxTrials,
	TArray<FPhase1AutoCalibParams>& OutCandidates)
{
	OutCandidates.Reset();

	TArray<FPhase1AutoCalibTrialResult> Sorted = StageAResults;
	Algo::Sort(Sorted, [](const FPhase1AutoCalibTrialResult& A, const FPhase1AutoCalibTrialResult& B)
	{
		return UPhysAnimComponent::IsBetterPhase1AutoCalibScore(A.Score, B.Score);
	});

	const int32 CandidateCount = FMath::Min(StageBTopK, Sorted.Num());
	static const float SpineDeltas[StageBRounds] = { 0.10f, 0.05f, 0.025f };
	static const float ThighDeltas[StageBRounds] = { 0.05f, 0.025f, 0.0125f };
	static const float ScaleDeltas[StageBRounds] = { 0.25f, 0.125f, 0.0625f };
	static const float BiasDeltas[StageBRounds] = { 0.25f, 0.125f, 0.0625f };

	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		const FPhase1AutoCalibParams Base = Sorted[CandidateIndex].Params;
		for (int32 RoundIndex = 0; RoundIndex < StageBRounds; ++RoundIndex)
		{
			const float SpineDelta = SpineDeltas[RoundIndex];
			const float ThighDelta = ThighDeltas[RoundIndex];
			const float ScaleDelta = ScaleDeltas[RoundIndex];
			const float BiasDelta = BiasDeltas[RoundIndex];

			const auto AddCandidate = [&](FPhase1AutoCalibParams Params)
			{
				if (MaxTrials > 0 && OutCandidates.Num() >= MaxTrials)
				{
					return;
				}

				OutCandidates.Add(ClampParams(Params));
			};

			FPhase1AutoCalibParams Params = Base;
			Params.SpineInterpolationAlpha += SpineDelta;
			AddCandidate(Params);
			Params = Base;
			Params.SpineInterpolationAlpha -= SpineDelta;
			AddCandidate(Params);
			Params = Base;
			Params.WorstThighInterpolationAlpha += ThighDelta;
			AddCandidate(Params);
			Params = Base;
			Params.WorstThighInterpolationAlpha -= ThighDelta;
			AddCandidate(Params);
			Params = Base;
			Params.FocusedDeltaScale += ScaleDelta;
			AddCandidate(Params);
			Params = Base;
			Params.FocusedDeltaScale -= ScaleDelta;
			AddCandidate(Params);
			Params = Base;
			Params.UprightnessWeightScale += ScaleDelta;
			AddCandidate(Params);
			Params = Base;
			Params.UprightnessWeightScale -= ScaleDelta;
			AddCandidate(Params);
			Params = Base;
			Params.ClampStrengthScale += ScaleDelta;
			AddCandidate(Params);
			Params = Base;
			Params.ClampStrengthScale -= ScaleDelta;
			AddCandidate(Params);
			Params = Base;
			Params.PelvisPitchBiasDeg += BiasDelta;
			AddCandidate(Params);
			Params = Base;
			Params.PelvisPitchBiasDeg -= BiasDelta;
			AddCandidate(Params);
			Params = Base;
			Params.PelvisRollBiasDeg += BiasDelta;
			AddCandidate(Params);
			Params = Base;
			Params.PelvisRollBiasDeg -= BiasDelta;
			AddCandidate(Params);

			if (MaxTrials > 0 && OutCandidates.Num() >= MaxTrials)
			{
				return;
			}
		}
	}
}

void UPhysAnimPhase1AutoCalibSubsystem::BuildStageCReproCandidates(
	const TArray<FPhase1AutoCalibTrialResult>& StageBResults,
	int32 MaxTrials,
	TArray<FPhase1AutoCalibParams>& OutCandidates)
{
	OutCandidates.Reset();

	TArray<FPhase1AutoCalibTrialResult> Sorted = StageBResults;
	Algo::Sort(Sorted, [](const FPhase1AutoCalibTrialResult& A, const FPhase1AutoCalibTrialResult& B)
	{
		return UPhysAnimComponent::IsBetterPhase1AutoCalibScore(A.Score, B.Score);
	});

	const int32 CandidateCount = FMath::Min(StageCTopK, Sorted.Num());
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		for (int32 RepeatIndex = 0; RepeatIndex < StageCRepetitions; ++RepeatIndex)
		{
			if (MaxTrials > 0 && OutCandidates.Num() >= MaxTrials)
			{
				return;
			}

			OutCandidates.Add(Sorted[CandidateIndex].Params);
		}
	}
}

bool UPhysAnimPhase1AutoCalibSubsystem::AreTrialResultsReproducible(const TArray<FPhase1AutoCalibTrialResult>& Trials, float Epsilon)
{
	if (Trials.Num() <= 1)
	{
		return !Trials.IsEmpty();
	}

	const FPhase1AutoCalibTrialResult& Reference = Trials[0];
	for (int32 Index = 1; Index < Trials.Num(); ++Index)
	{
		const FPhase1AutoCalibTrialResult& Candidate = Trials[Index];
		if (Candidate.TerminalClass != Reference.TerminalClass || Candidate.TruthfulBlocker != Reference.TruthfulBlocker)
		{
			return false;
		}

		if (Candidate.Score.bContractPassed != Reference.Score.bContractPassed ||
			Candidate.Score.bTimedOut != Reference.Score.bTimedOut ||
			Candidate.Score.bSafeDenied != Reference.Score.bSafeDenied ||
			Candidate.Score.bRestoreDeterministic != Reference.Score.bRestoreDeterministic ||
			Candidate.Score.bReachedRootOn != Reference.Score.bReachedRootOn ||
			Candidate.Score.bNoCouplingProofSatisfied != Reference.Score.bNoCouplingProofSatisfied)
		{
			return false;
		}

		if (!FMath::IsNearlyEqual(Candidate.Score.WorstDirectLinkAngularErrorDeg, Reference.Score.WorstDirectLinkAngularErrorDeg, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.MeanTargetDeltaDeg, Reference.Score.MeanTargetDeltaDeg, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.MaxTargetDeltaDeg, Reference.Score.MaxTargetDeltaDeg, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.ThighAsymmetryDeg, Reference.Score.ThighAsymmetryDeg, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.PeakRootTiltDeg, Reference.Score.PeakRootTiltDeg, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.ShellOffsetDeltaCm, Reference.Score.ShellOffsetDeltaCm, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.ShellVelocityDeltaCmPerSecond, Reference.Score.ShellVelocityDeltaCmPerSecond, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.PeakRootLinearSpeedCmPerSecond, Reference.Score.PeakRootLinearSpeedCmPerSecond, Epsilon) ||
			!FMath::IsNearlyEqual(Candidate.Score.PeakRootAngularSpeedDegPerSecond, Reference.Score.PeakRootAngularSpeedDegPerSecond, Epsilon))
		{
			return false;
		}
	}

	return true;
}

bool UPhysAnimPhase1AutoCalibSubsystem::ResolveTargetComponent(
	const FString& OwnerFilter,
	UPhysAnimComponent*& OutComponent,
	FString& OutError) const
{
	OutComponent = nullptr;
	OutError.Reset();

	UWorld* const World = GetWorld();
	if (!World)
	{
		OutError = TEXT("Phase1 auto-calibration requires a valid PIE/game world.");
		return false;
	}

	const FString FilterLower = OwnerFilter.ToLower();
	for (TObjectIterator<UPhysAnimComponent> It; It; ++It)
	{
		UPhysAnimComponent* const Candidate = *It;
		if (!IsValid(Candidate) || Candidate->GetWorld() != World || !MatchesFilter(*Candidate, FilterLower))
		{
			continue;
		}

		if (OutComponent)
		{
			OutError = FString::Printf(
				TEXT("Phase1 auto-calibration matched more than one component. Refine the owner filter '%s'."),
				OwnerFilter.IsEmpty() ? TEXT("<all>") : *OwnerFilter);
			OutComponent = nullptr;
			return false;
		}

		OutComponent = Candidate;
	}

	if (!OutComponent)
	{
		OutError = FString::Printf(
			TEXT("Phase1 auto-calibration did not find a matching component for filter '%s'."),
			OwnerFilter.IsEmpty() ? TEXT("<all>") : *OwnerFilter);
		return false;
	}

	return true;
}

bool UPhysAnimPhase1AutoCalibSubsystem::RunDeterminismPreflight(
	UPhysAnimComponent& Component,
	const FPhase1AutoCalibBaselineSnapshot& Baseline,
	FString& OutError) const
{
	OutError.Reset();

	FPhase1AutoCalibDeterminismFingerprint FingerprintA;
	FPhase1AutoCalibDeterminismFingerprint FingerprintB;
	if (!Component.RestorePhase1AutoCalibBaseline(Baseline, OutError) ||
		!Component.CapturePhase1AutoCalibDeterminismFingerprint(FingerprintA, OutError) ||
		!Component.RestorePhase1AutoCalibBaseline(Baseline, OutError) ||
		!Component.CapturePhase1AutoCalibDeterminismFingerprint(FingerprintB, OutError))
	{
		return false;
	}

	if (!AreFingerprintsNear(FingerprintA, FingerprintB, RestoreFingerprintTolerance))
	{
		OutError = TEXT("Phase1 auto-calibration determinism preflight failed: baseline restore does not round-trip to a stable fingerprint.");
		return false;
	}

	const_cast<UPhysAnimPhase1AutoCalibSubsystem*>(this)->BaselineFingerprint = FingerprintA;
	return true;
}

void UPhysAnimPhase1AutoCalibSubsystem::TickAwaitingReadiness()
{
	UPhysAnimComponent* const Component = TargetComponent.Get();
	if (!Component)
	{
		StopPhase1AutoCalib(TEXT("target_component_lost"));
		return;
	}

	UWorld* const World = GetWorld();
	if (!World)
	{
		return;
	}

	const FPhysAnimStabilizationSettings& Settings = Component->GetConfiguredStabilizationSettings();
	FString QueueReason;
	const bool bQueueReady = Component->EvaluateBalanceModeQueueGates(Settings, QueueReason);

	FString PreEntryReason;
	const bool bPreEntryReady = Component->EvaluateBalanceBridgeActivePreEntryPrerequisites(Settings, PreEntryReason);

	if (bQueueReady && bPreEntryReady)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimAutoCalib] Readiness reached after %.2fs. Capturing baseline."), World->GetTimeSeconds() - CurrentStageStartTimeSeconds);
		
		FString Error;
		if (!Component->CapturePhase1AutoCalibBaseline(BaselineSnapshot, Error))
		{
			LastError = Error;
			StopPhase1AutoCalib(TEXT("baseline_capture_failed"));
			return;
		}

		if (!RunDeterminismPreflight(*Component, BaselineSnapshot, Error))
		{
			LastError = Error;
			StopPhase1AutoCalib(TEXT("determinism_preflight_failed"));
			return;
		}

		CurrentStage = EAutoCalibStage::StageA;
		CurrentStageStartTimeSeconds = World->GetTimeSeconds();
		if (!BeginNextTrial())
		{
			StopPhase1AutoCalib(TEXT("begin_trial_failed"));
		}
		return;
	}

	const double Elapsed = World->GetTimeSeconds() - CurrentStageStartTimeSeconds;
	if (Elapsed >= static_cast<double>(ActiveRequest.ReadinessTimeoutSeconds))
	{
		LastError = FString::Printf(TEXT("Timed out awaiting component readiness (%.1fs). lastReason=%s/%s"), 
			Elapsed, *QueueReason, *PreEntryReason);
		StopPhase1AutoCalib(TEXT("readiness_timeout"));
		return;
	}

	if (LastReadinessLogTimeSeconds < 0.0 || (World->GetTimeSeconds() - LastReadinessLogTimeSeconds) >= 1.0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimAutoCalib] Awaiting readiness... elapsed=%.1fs queue=%s preEntry=%s"), 
			Elapsed, *QueueReason, *PreEntryReason);
		LastReadinessLogTimeSeconds = World->GetTimeSeconds();
	}
}

bool UPhysAnimPhase1AutoCalibSubsystem::BeginNextTrial()
{
	if (!bRunActive)
	{
		return false;
	}

	if (PendingTrials.IsEmpty())
	{
		AdvanceStageOrFinish();
		return bRunActive;
	}

	UPhysAnimComponent* const Component = TargetComponent.Get();
	if (!Component)
	{
		LastError = TEXT("Phase1 auto-calibration target component is no longer valid.");
		return false;
	}

	ActiveTrial = PendingTrials[0];
	PendingTrials.RemoveAt(0);

	FString Error;
	if (!Component->RestorePhase1AutoCalibBaseline(BaselineSnapshot, Error))
	{
		LastError = Error;
		return false;
	}

	Component->ApplyPhase1AutoCalibParams(ActiveTrial.Params);
	ActiveTrialPeakMetrics = FPhase1AutoCalibLiveMetrics();
	if (!Component->CapturePhase1AutoCalibLiveMetrics(ActiveTrialPeakMetrics, Error))
	{
		LastError = Error;
		return false;
	}

	if (!Component->StartPhase1AutoCalibTrial(Error))
	{
		LastError = Error;
		return false;
	}

	ActiveTrialStartTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	bTrialActive = true;
	return true;
}

void UPhysAnimPhase1AutoCalibSubsystem::TickActiveTrial()
{
	UPhysAnimComponent* const Component = TargetComponent.Get();
	if (!Component)
	{
		LastError = TEXT("Phase1 auto-calibration target component is no longer valid.");
		StopPhase1AutoCalib(TEXT("target_component_lost"));
		return;
	}

	FString Error;
	FPhase1AutoCalibLiveMetrics LiveMetrics;
	if (!Component->CapturePhase1AutoCalibLiveMetrics(LiveMetrics, Error))
	{
		LastError = Error;
		StopPhase1AutoCalib(TEXT("metrics_capture_failed"));
		return;
	}

	UpdatePeakMetrics(LiveMetrics);

	const FPhysAnimStabilizationSettings& Settings = Component->GetConfiguredStabilizationSettings();
	const double TimeoutSeconds = static_cast<double>(Settings.BalancePhase1PrepareDuration + Settings.BalancePhase1LateValidateRequiredSeconds + 0.5f);
	const double ElapsedSeconds = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0) - ActiveTrialStartTimeSeconds;
	if (ElapsedSeconds >= TimeoutSeconds)
	{
		FinalizeActiveTrial(true);
		return;
	}

	const EBalanceReadyTransitionPhase Phase = Component->GetBalanceReadyTransitionPhase();
	if (IsLaterThanPhase1(Phase) || Component->HasBalanceReadyTransitionFailed() || Component->HasSafePhase2Denial())
	{
		FinalizeActiveTrial(false);
	}
}

void UPhysAnimPhase1AutoCalibSubsystem::FinalizeActiveTrial(bool bTimedOut)
{
	UPhysAnimComponent* const Component = TargetComponent.Get();
	if (!Component)
	{
		LastError = TEXT("Phase1 auto-calibration target component is no longer valid.");
		StopPhase1AutoCalib(TEXT("target_component_lost"));
		return;
	}

	FPhase1AutoCalibTrialResult Result = BuildTrialResult(bTimedOut);
	Result.TrialId = NextTrialId++;
	Result.StageName = ActiveTrial.StageName;
	Result.RepetitionIndex = ActiveTrial.RepetitionIndex;
	Result.Params = ActiveTrial.Params;

	FString RestoreError;
	if (!Component->RestorePhase1AutoCalibBaseline(BaselineSnapshot, RestoreError))
	{
		Result.Score.bRestoreDeterministic = false;
		if (Result.TruthfulBlocker.IsEmpty())
		{
			Result.TruthfulBlocker = TEXT("restore_failed");
		}
		LastError = RestoreError;
	}
	else
	{
		FPhase1AutoCalibDeterminismFingerprint RestoredFingerprint;
		if (!Component->CapturePhase1AutoCalibDeterminismFingerprint(RestoredFingerprint, RestoreError))
		{
			Result.Score.bRestoreDeterministic = false;
			LastError = RestoreError;
		}
		else
		{
			Result.Score.bRestoreDeterministic = AreFingerprintsNear(BaselineFingerprint, RestoredFingerprint, RestoreFingerprintTolerance);
			if (!Result.Score.bRestoreDeterministic && Result.TruthfulBlocker.IsEmpty())
			{
				Result.TruthfulBlocker = TEXT("restore_nondeterministic");
			}
		}
	}

	UPhysAnimComponent::FinalizePhase1AutoCalibScore(Result.Score);
	LatestReport.Trials.Add(Result);

	switch (CurrentStage)
	{
	case EAutoCalibStage::StageA:
		StageAResults.Add(Result);
		break;
	case EAutoCalibStage::StageB:
		StageBResults.Add(Result);
		break;
	case EAutoCalibStage::StageC:
		StageCResults.Add(Result);
		break;
	default:
		break;
	}

	Component->ClearPhase1AutoCalibParams();
	bTrialActive = false;
	ActiveTrial = FPendingTrial();
	ActiveTrialStartTimeSeconds = -1.0;
	ActiveTrialPeakMetrics = FPhase1AutoCalibLiveMetrics();

	if (!LastError.IsEmpty())
	{
		FinalizeReport();
		StopPhase1AutoCalib(TEXT("trial_finalize_failed"));
		return;
	}

	if (!PendingTrials.IsEmpty())
	{
		BeginNextTrial();
		return;
	}

	AdvanceStageOrFinish();
}

void UPhysAnimPhase1AutoCalibSubsystem::AdvanceStageOrFinish()
{
	PendingTrials.Reset();

	if (CurrentStage == EAutoCalibStage::StageA)
	{
		TArray<FPhase1AutoCalibParams> StageBCandidates;
		BuildStageBRefinementCandidates(StageAResults, ActiveRequest.MaxTrials, StageBCandidates);
		for (const FPhase1AutoCalibParams& Params : StageBCandidates)
		{
			FPendingTrial& Pending = PendingTrials.AddDefaulted_GetRef();
			Pending.Params = Params;
			Pending.StageName = TEXT("stage_b");
		}

		CurrentStage = PendingTrials.IsEmpty() ? EAutoCalibStage::StageC : EAutoCalibStage::StageB;
		if (!PendingTrials.IsEmpty())
		{
			BeginNextTrial();
			return;
		}
	}

	if (CurrentStage == EAutoCalibStage::StageB || CurrentStage == EAutoCalibStage::StageC)
	{
		if (CurrentStage == EAutoCalibStage::StageB)
		{
			TArray<FPhase1AutoCalibParams> StageCCandidates;
			BuildStageCReproCandidates(StageBResults, ActiveRequest.MaxTrials, StageCCandidates);
			for (int32 Index = 0; Index < StageCCandidates.Num(); ++Index)
			{
				FPendingTrial& Pending = PendingTrials.AddDefaulted_GetRef();
				Pending.Params = StageCCandidates[Index];
				Pending.StageName = TEXT("stage_c");
				Pending.RepetitionIndex = Index % StageCRepetitions;
			}

			CurrentStage = PendingTrials.IsEmpty() ? EAutoCalibStage::Completed : EAutoCalibStage::StageC;
			if (!PendingTrials.IsEmpty())
			{
				BeginNextTrial();
				return;
			}
		}

		FinalizeReport();
		StopPhase1AutoCalib(TEXT("completed"));
	}
}

void UPhysAnimPhase1AutoCalibSubsystem::UpdatePeakMetrics(const FPhase1AutoCalibLiveMetrics& Metrics)
{
	ActiveTrialPeakMetrics.RuntimeState = Metrics.RuntimeState;
	ActiveTrialPeakMetrics.TransitionPhase = Metrics.TransitionPhase;
	ActiveTrialPeakMetrics.RootLinearSpeedCmPerSecond = FMath::Max(ActiveTrialPeakMetrics.RootLinearSpeedCmPerSecond, Metrics.RootLinearSpeedCmPerSecond);
	ActiveTrialPeakMetrics.RootAngularSpeedDegPerSecond = FMath::Max(ActiveTrialPeakMetrics.RootAngularSpeedDegPerSecond, Metrics.RootAngularSpeedDegPerSecond);
	ActiveTrialPeakMetrics.RootTiltDeg = FMath::Max(ActiveTrialPeakMetrics.RootTiltDeg, Metrics.RootTiltDeg);
	ActiveTrialPeakMetrics.ShellOffsetDeltaCm = FMath::Max(ActiveTrialPeakMetrics.ShellOffsetDeltaCm, Metrics.ShellOffsetDeltaCm);
	ActiveTrialPeakMetrics.ShellVelocityDeltaCmPerSecond = FMath::Max(ActiveTrialPeakMetrics.ShellVelocityDeltaCmPerSecond, Metrics.ShellVelocityDeltaCmPerSecond);
	ActiveTrialPeakMetrics.MaxTargetDeltaDeg = FMath::Max(ActiveTrialPeakMetrics.MaxTargetDeltaDeg, Metrics.MaxTargetDeltaDeg);
	ActiveTrialPeakMetrics.MeanTargetDeltaDeg = FMath::Max(ActiveTrialPeakMetrics.MeanTargetDeltaDeg, Metrics.MeanTargetDeltaDeg);
}

FPhase1AutoCalibTrialResult UPhysAnimPhase1AutoCalibSubsystem::BuildTrialResult(bool bTimedOut) const
{
	FPhase1AutoCalibTrialResult Result;
	Result.Params = ActiveTrial.Params;
	Result.StageName = ActiveTrial.StageName;
	Result.RepetitionIndex = ActiveTrial.RepetitionIndex;
	Result.Score.bTimedOut = bTimedOut;

	const UPhysAnimComponent* const Component = TargetComponent.Get();
	if (!Component)
	{
		Result.TerminalClass = TEXT("failed");
		Result.TruthfulBlocker = TEXT("target_component_lost");
		Result.Score.bContractPassed = false;
		Result.Score.bRestoreDeterministic = false;
		UPhysAnimComponent::FinalizePhase1AutoCalibScore(Result.Score);
		return Result;
	}

	const FPhysAnimBalanceReadyTransitionSnapshot Snapshot = Component->ExportBalanceReadyTransitionSnapshot();
	const bool bFailed = Component->HasBalanceReadyTransitionFailed() || Snapshot.InternalPhase == EBalanceReadyTransitionPhase::BRT_Failed;
	const bool bSafeDenied = Component->HasSafePhase2Denial() || Snapshot.InternalPhase == EBalanceReadyTransitionPhase::BRT_SafeDenied;
	const bool bReachedRootOn = IsLaterThanPhase1(Snapshot.InternalPhase);
	const FPhysAnimLateValidationResult& LateValidation = Snapshot.CertifiedLateValidationResult;
	const FPhysAnimCertifiedHandoffSnapshot& Handoff = Snapshot.CertifiedHandoff;

	Result.Score.bSafeDenied = bSafeDenied;
	Result.Score.bReachedRootOn = bReachedRootOn;
	Result.Score.bNoCouplingProofSatisfied =
		LateValidation.bRootOnReadinessNoCouplingProofSatisfied ||
		Handoff.bRootOnReadinessNoCouplingProofSatisfied;

	const float LeftAngular = FMath::Max(LateValidation.PelvisThighLAngularErrorDeg, Handoff.PelvisThighLAngularErrorDeg);
	const float RightAngular = FMath::Max(LateValidation.PelvisThighRAngularErrorDeg, Handoff.PelvisThighRAngularErrorDeg);
	const float SpineAngular = FMath::Max(LateValidation.PelvisSpine01AngularErrorDeg, Handoff.PelvisSpine01AngularErrorDeg);
	Result.Score.WorstDirectLinkAngularErrorDeg = FMath::Max3(LeftAngular, RightAngular, SpineAngular);
	Result.Score.ThighAsymmetryDeg = FMath::Abs(LeftAngular - RightAngular);
	Result.Score.MeanTargetDeltaDeg = FMath::Max(ActiveTrialPeakMetrics.MeanTargetDeltaDeg, LateValidation.MeanTargetDeltaDegrees);
	Result.Score.MaxTargetDeltaDeg = FMath::Max(ActiveTrialPeakMetrics.MaxTargetDeltaDeg, LateValidation.MaxTargetDeltaDegrees);
	Result.Score.PeakRootTiltDeg = ActiveTrialPeakMetrics.RootTiltDeg;
	Result.Score.ShellOffsetDeltaCm = FMath::Max(ActiveTrialPeakMetrics.ShellOffsetDeltaCm, Handoff.ShellOffsetDeltaAtCaptureCm);
	Result.Score.ShellVelocityDeltaCmPerSecond = FMath::Max(ActiveTrialPeakMetrics.ShellVelocityDeltaCmPerSecond, Handoff.ShellVelocityDeltaAtCaptureCmPerSecond);
	Result.Score.PeakRootLinearSpeedCmPerSecond = ActiveTrialPeakMetrics.RootLinearSpeedCmPerSecond;
	Result.Score.PeakRootAngularSpeedDegPerSecond = ActiveTrialPeakMetrics.RootAngularSpeedDegPerSecond;
	Result.Score.bContractPassed =
		!bTimedOut &&
		!bFailed &&
		!bSafeDenied &&
		bReachedRootOn &&
		Result.Score.bNoCouplingProofSatisfied;

	if (bTimedOut)
	{
		Result.TerminalClass = TEXT("timed_out");
	}
	else if (bSafeDenied)
	{
		Result.TerminalClass = TEXT("safe_denied");
	}
	else if (bFailed)
	{
		Result.TerminalClass = TEXT("failed");
	}
	else if (bReachedRootOn)
	{
		Result.TerminalClass = TEXT("reached_root_on");
	}
	else
	{
		Result.TerminalClass = TEXT("incomplete");
	}

	if (Result.Score.bContractPassed)
	{
		Result.TruthfulBlocker = TEXT("ready");
	}
	else if (!Snapshot.SafePhase2DenialReason.IsEmpty())
	{
		Result.TruthfulBlocker = Snapshot.SafePhase2DenialReason;
	}
	else if (!Snapshot.Diagnostics.FailureReason.IsEmpty())
	{
		Result.TruthfulBlocker = Snapshot.Diagnostics.FailureReason;
	}
	else if (!Snapshot.Diagnostics.Phase1RootOnReadinessGateReason.IsEmpty())
	{
		Result.TruthfulBlocker = Snapshot.Diagnostics.Phase1RootOnReadinessGateReason;
	}
	else if (!LateValidation.RootOnReadinessGateReason.IsEmpty())
	{
		Result.TruthfulBlocker = LateValidation.RootOnReadinessGateReason;
	}
	else if (bTimedOut)
	{
		Result.TruthfulBlocker = TEXT("phase1_auto_calib_timeout");
	}
	else
	{
		Result.TruthfulBlocker = TEXT("phase1_auto_calib_unclassified");
	}

	UPhysAnimComponent::FinalizePhase1AutoCalibScore(Result.Score);
	return Result;
}

void UPhysAnimPhase1AutoCalibSubsystem::FinalizeReport()
{
	Algo::Sort(LatestReport.Trials, [](const FPhase1AutoCalibTrialResult& A, const FPhase1AutoCalibTrialResult& B)
	{
		return UPhysAnimComponent::IsBetterPhase1AutoCalibScore(A.Score, B.Score);
	});

	for (int32 Index = 0; Index + StageCRepetitions <= StageCResults.Num(); Index += StageCRepetitions)
	{
		TArray<FPhase1AutoCalibTrialResult> Group;
		Group.Reserve(StageCRepetitions);
		for (int32 Offset = 0; Offset < StageCRepetitions; ++Offset)
		{
			Group.Add(StageCResults[Index + Offset]);
		}

		const bool bReproducible = AreTrialResultsReproducible(Group, ScoreReproTolerance);
		for (int32 Offset = 0; Offset < StageCRepetitions; ++Offset)
		{
			StageCResults[Index + Offset].bReproducible = bReproducible;
		}
	}

	for (FPhase1AutoCalibTrialResult& Trial : LatestReport.Trials)
	{
		if (Trial.StageName != TEXT("stage_c"))
		{
			continue;
		}

		for (const FPhase1AutoCalibTrialResult& StageCTrial : StageCResults)
		{
			if (StageCTrial.TrialId == Trial.TrialId)
			{
				Trial.bReproducible = StageCTrial.bReproducible;
				break;
			}
		}
	}

	for (const FPhase1AutoCalibTrialResult& Trial : LatestReport.Trials)
	{
		if (!LatestReport.bHasBestNearPass && !Trial.Score.bContractPassed)
		{
			LatestReport.BestNearPass = Trial;
			LatestReport.bHasBestNearPass = true;
		}
	}

	for (const FPhase1AutoCalibTrialResult& Trial : LatestReport.Trials)
	{
		if (Trial.Score.bContractPassed && Trial.bReproducible)
		{
			LatestReport.BestCandidate = Trial;
			LatestReport.bHasBestCandidate = true;
			break;
		}
	}

	if (!LatestReport.bHasBestCandidate)
	{
		for (const FPhase1AutoCalibTrialResult& Trial : LatestReport.Trials)
		{
			if (Trial.Score.bContractPassed)
			{
				LatestReport.BestCandidate = Trial;
				LatestReport.bHasBestCandidate = true;
				break;
			}
		}
	}

	LatestReport.ParetoFrontier.Reset();
	for (const FPhase1AutoCalibTrialResult& Candidate : LatestReport.Trials)
	{
		bool bDominated = false;
		for (const FPhase1AutoCalibTrialResult& Other : LatestReport.Trials)
		{
			if (Candidate.TrialId == Other.TrialId)
			{
				continue;
			}

			if (Dominates(Other, Candidate))
			{
				bDominated = true;
				break;
			}
		}

		if (!bDominated)
		{
			LatestReport.ParetoFrontier.Add(Candidate);
		}
	}

	WriteArtifacts();
}

void UPhysAnimPhase1AutoCalibSubsystem::WriteArtifacts()
{
	const FString OutputDirectory = LatestReport.OutputDirectory;
	if (OutputDirectory.IsEmpty())
	{
		return;
	}

	IFileManager::Get().MakeDirectory(*OutputDirectory, true);

	FString Csv = TEXT("trial_id,stage,repetition,terminal_class,truthful_blocker,contract_passed,reproducible,source_preset,seed_family_preset,spine_alpha,worst_thigh_alpha,focused_delta_scale,uprightness_weight_scale,clamp_strength_scale,pelvis_pitch_bias_deg,pelvis_roll_bias_deg,worst_direct_link_angular_error_deg,mean_target_delta_deg,max_target_delta_deg,thigh_asymmetry_deg,peak_root_tilt_deg,shell_offset_delta_cm,shell_velocity_delta_cm_per_second,peak_root_linear_speed_cm_per_second,peak_root_angular_speed_deg_per_second\n");
	for (const FPhase1AutoCalibTrialResult& Trial : LatestReport.Trials)
	{
		Csv += FString::Printf(
			TEXT("%d,%s,%d,%s,%s,%s,%s,%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n"),
			Trial.TrialId,
			*Trial.StageName,
			Trial.RepetitionIndex,
			*Trial.TerminalClass,
			*Trial.TruthfulBlocker,
			Trial.Score.bContractPassed ? TEXT("true") : TEXT("false"),
			Trial.bReproducible ? TEXT("true") : TEXT("false"),
			StrategyPresetToString(Trial.Params.SourcePreset),
			StrategyPresetToString(Trial.Params.SeedFamilyPreset),
			Trial.Params.SpineInterpolationAlpha,
			Trial.Params.WorstThighInterpolationAlpha,
			Trial.Params.FocusedDeltaScale,
			Trial.Params.UprightnessWeightScale,
			Trial.Params.ClampStrengthScale,
			Trial.Params.PelvisPitchBiasDeg,
			Trial.Params.PelvisRollBiasDeg,
			Trial.Score.WorstDirectLinkAngularErrorDeg,
			Trial.Score.MeanTargetDeltaDeg,
			Trial.Score.MaxTargetDeltaDeg,
			Trial.Score.ThighAsymmetryDeg,
			Trial.Score.PeakRootTiltDeg,
			Trial.Score.ShellOffsetDeltaCm,
			Trial.Score.ShellVelocityDeltaCmPerSecond,
			Trial.Score.PeakRootLinearSpeedCmPerSecond,
			Trial.Score.PeakRootAngularSpeedDegPerSecond);
	}
	FFileHelper::SaveStringToFile(Csv, *LatestReport.TrialsCsvPath);

	const auto BuildTrialJson = [](const FPhase1AutoCalibTrialResult& Trial) -> FString
	{
		return FString::Printf(
			TEXT("{\"trialId\":%d,\"stage\":\"%s\",\"repetition\":%d,\"terminalClass\":\"%s\",\"truthfulBlocker\":\"%s\",\"contractPassed\":%s,\"reproducible\":%s,\"sourcePreset\":\"%s\",\"seedFamilyPreset\":\"%s\",\"score\":{\"worstDirectLinkAngularErrorDeg\":%.6f,\"meanTargetDeltaDeg\":%.6f,\"maxTargetDeltaDeg\":%.6f,\"thighAsymmetryDeg\":%.6f,\"peakRootTiltDeg\":%.6f,\"shellOffsetDeltaCm\":%.6f,\"shellVelocityDeltaCmPerSecond\":%.6f,\"peakRootLinearSpeedCmPerSecond\":%.6f,\"peakRootAngularSpeedDegPerSecond\":%.6f}}"),
			Trial.TrialId,
			*JsonEscape(Trial.StageName),
			Trial.RepetitionIndex,
			*JsonEscape(Trial.TerminalClass),
			*JsonEscape(Trial.TruthfulBlocker),
			Trial.Score.bContractPassed ? TEXT("true") : TEXT("false"),
			Trial.bReproducible ? TEXT("true") : TEXT("false"),
			StrategyPresetToString(Trial.Params.SourcePreset),
			StrategyPresetToString(Trial.Params.SeedFamilyPreset),
			Trial.Score.WorstDirectLinkAngularErrorDeg,
			Trial.Score.MeanTargetDeltaDeg,
			Trial.Score.MaxTargetDeltaDeg,
			Trial.Score.ThighAsymmetryDeg,
			Trial.Score.PeakRootTiltDeg,
			Trial.Score.ShellOffsetDeltaCm,
			Trial.Score.ShellVelocityDeltaCmPerSecond,
			Trial.Score.PeakRootLinearSpeedCmPerSecond,
			Trial.Score.PeakRootAngularSpeedDegPerSecond);
	};

	FString SummaryJson = FString::Printf(
		TEXT("{\"outputDirectory\":\"%s\",\"trialCount\":%d,\"bestCandidate\":%s,\"bestNearPass\":%s}"),
		*JsonEscape(LatestReport.OutputDirectory),
		LatestReport.Trials.Num(),
		LatestReport.bHasBestCandidate ? *BuildTrialJson(LatestReport.BestCandidate) : TEXT("null"),
		LatestReport.bHasBestNearPass ? *BuildTrialJson(LatestReport.BestNearPass) : TEXT("null"));
	FFileHelper::SaveStringToFile(SummaryJson, *LatestReport.SummaryPath);

	FString ParetoJson = TEXT("[");
	for (int32 Index = 0; Index < LatestReport.ParetoFrontier.Num(); ++Index)
	{
		if (Index > 0)
		{
			ParetoJson += TEXT(",");
		}

		ParetoJson += BuildTrialJson(LatestReport.ParetoFrontier[Index]);
	}
	ParetoJson += TEXT("]");
	FFileHelper::SaveStringToFile(ParetoJson, *LatestReport.ParetoJsonPath);
}

FString UPhysAnimPhase1AutoCalibSubsystem::BuildOutputDirectory() const
{
	const FString Timestamp = ActiveRequest.OutputSubfolder.IsEmpty()
		? FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S"))
		: ActiveRequest.OutputSubfolder;
	return FPaths::ConvertRelativePathToFull(
		FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT(".."),
			TEXT("test-results"),
			TEXT("phase1-autocalib"),
			Timestamp));
}


