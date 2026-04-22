#include "PhysAnimPhase1AutoCalibSubsystem.h"
#include "PhysAnimComponentPrivate.h"
#include "PhysAnimPhase1PelvisCouplingSearch.h"

#if !UE_BUILD_SHIPPING



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
	constexpr int32 SmokeStageATrialCount = 8;
	constexpr int32 SmokeStageBTrialCount = 8;
	constexpr int32 SmokeStageCTrialCount = 5;
	constexpr float RestoreFingerprintTolerance = 1.0e-3f;
	constexpr float RestoreFingerprintTransformLocationToleranceCm = 0.25f;
	constexpr float RestoreFingerprintTransformAngularToleranceDeg = 1.0f;
	constexpr float RestoreFingerprintCriticalLinkAngularToleranceDeg = 1.0f;
	constexpr float ScoreReproTolerance = 1.0e-3f;
	constexpr float ReproWorstDirectLinkToleranceDeg = 1.0f;
	constexpr float ReproMeanTargetDeltaToleranceDeg = 8.0f;
	constexpr float ReproMaxTargetDeltaToleranceDeg = 3.0f;
	constexpr float ReproThighAsymmetryToleranceDeg = 1.0f;
	constexpr float ReproPeakRootTiltToleranceDeg = 1.0f;
	constexpr float ReproShellOffsetToleranceCm = 0.75f;
	constexpr float ReproShellVelocityToleranceCmPerSecond = 0.1f;
	constexpr float ReproPeakRootLinearSpeedToleranceCmPerSecond = 25.0f;
	constexpr float ReproPeakRootAngularSpeedToleranceDegPerSecond = 120.0f;
	constexpr float ReproBalanceActiveStandingHoldToleranceSeconds = 0.25f;

	enum class EPhase1AutoCalibBuildStage : uint8
	{
		StageA,
		StageB,
		StageC
	};

	constexpr EPhase1AutoCalibStrategyPreset GPhase1AutoCalibPresets[] =
	{
		EPhase1AutoCalibStrategyPreset::CurrentDefault,
		EPhase1AutoCalibStrategyPreset::SpineBiased,
		EPhase1AutoCalibStrategyPreset::WorstThighBiased,
		EPhase1AutoCalibStrategyPreset::BalancedCoupled,
		EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh,
		EPhase1AutoCalibStrategyPreset::RescueOnly,
		EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily,
		EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough
	};

	bool AutoCalibMatchesFilter(const UPhysAnimComponent& Component, const FString& FilterLower)
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
		return Phase1AutoCalibStrategyPresetToString(Preset);
	}

	const TCHAR* FrontierClassificationToString(const EPhase1AutoCalibFrontierClassification Classification)
	{
		switch (Classification)
		{
		case EPhase1AutoCalibFrontierClassification::TruthfulPassFound:
			return TEXT("truthful_pass_found");
		case EPhase1AutoCalibFrontierClassification::StillThighBlocked:
			return TEXT("still_thigh_blocked");
		case EPhase1AutoCalibFrontierClassification::StillSpineBlocked:
			return TEXT("still_spine_blocked");
		case EPhase1AutoCalibFrontierClassification::CoupledSpineThighFlip:
			return TEXT("coupled_spine_thigh_flip");
		case EPhase1AutoCalibFrontierClassification::FlatNoMaterialImprovement:
			return TEXT("flat_no_material_improvement");
		case EPhase1AutoCalibFrontierClassification::Unknown:
		default:
			return TEXT("unknown");
		}
	}

	const TCHAR* RecommendedActionToString(const EPhase1AutoCalibRecommendedAction Action)
	{
		switch (Action)
		{
		case EPhase1AutoCalibRecommendedAction::PromoteBestCandidate:
			return TEXT("promote_best_candidate");
		case EPhase1AutoCalibRecommendedAction::AddCoupledTradeControlExpansion:
			return TEXT("add_coupled_trade_control_expansion");
		case EPhase1AutoCalibRecommendedAction::InvestigateCandidateGeneration:
			return TEXT("investigate_candidate_generation");
		case EPhase1AutoCalibRecommendedAction::None:
		default:
			return TEXT("none");
		}
	}

	bool IsLaterThanPhase1(const EBalanceReadyTransitionPhase Phase)
	{
		return Phase == EBalanceReadyTransitionPhase::BRT_Phase2_RootOn ||
			Phase == EBalanceReadyTransitionPhase::BRT_Phase2_ReadyForPhase3 ||
			Phase == EBalanceReadyTransitionPhase::BRT_Phase3_Settle ||
			Phase == EBalanceReadyTransitionPhase::BRT_Succeeded;
	}

	bool IsStandingHoldBenchmarkSatisfied(const double HoldSeconds)
	{
		return HoldSeconds + KINDA_SMALL_NUMBER >= static_cast<double>(PhysAnimAutoCalibBenchmark::RequiredBalanceActiveStandingHoldSeconds);
	}

	bool AreTransformsNear(
		const FTransform& A,
		const FTransform& B,
		const float LocationTolerance,
		const float RotationToleranceDeg,
		const float ScaleTolerance)
	{
		return A.GetLocation().Equals(B.GetLocation(), LocationTolerance) &&
			FMath::RadiansToDegrees(A.GetRotation().AngularDistance(B.GetRotation())) <= RotationToleranceDeg &&
			A.GetScale3D().Equals(B.GetScale3D(), ScaleTolerance);
	}

	bool AreFingerprintsNear(
		const FPhase1AutoCalibDeterminismFingerprint& A,
		const FPhase1AutoCalibDeterminismFingerprint& B,
		const float Tolerance)
	{
		const float TransformLocationToleranceCm =
			FMath::Max(Tolerance, RestoreFingerprintTransformLocationToleranceCm);
		const float TransformAngularToleranceDeg =
			FMath::Max(Tolerance, RestoreFingerprintTransformAngularToleranceDeg);
		const float CriticalLinkAngularToleranceDeg =
			FMath::Max(Tolerance, RestoreFingerprintCriticalLinkAngularToleranceDeg);
		return A.RuntimeState == B.RuntimeState &&
			A.TransitionPhase == B.TransitionPhase &&
			AreTransformsNear(A.OwnerTransform, B.OwnerTransform, TransformLocationToleranceCm, TransformAngularToleranceDeg, Tolerance) &&
			AreTransformsNear(A.MeshTransform, B.MeshTransform, TransformLocationToleranceCm, TransformAngularToleranceDeg, Tolerance) &&
			AreTransformsNear(A.RootBodyTransform, B.RootBodyTransform, TransformLocationToleranceCm, TransformAngularToleranceDeg, Tolerance) &&
			A.RootLinearVelocity.Equals(B.RootLinearVelocity, Tolerance) &&
			A.RootAngularVelocity.Equals(B.RootAngularVelocity, Tolerance) &&
			FMath::IsNearlyEqual(A.PelvisThighLAngularErrorDeg, B.PelvisThighLAngularErrorDeg, CriticalLinkAngularToleranceDeg) &&
			FMath::IsNearlyEqual(A.PelvisThighRAngularErrorDeg, B.PelvisThighRAngularErrorDeg, CriticalLinkAngularToleranceDeg) &&
			FMath::IsNearlyEqual(A.PelvisSpine01AngularErrorDeg, B.PelvisSpine01AngularErrorDeg, CriticalLinkAngularToleranceDeg) &&
			FMath::IsNearlyEqual(A.ShellOffsetDeltaCm, B.ShellOffsetDeltaCm, Tolerance) &&
			FMath::IsNearlyEqual(A.ShellVelocityDeltaCmPerSecond, B.ShellVelocityDeltaCmPerSecond, Tolerance) &&
			FMath::IsNearlyEqual(A.MaxTargetDeltaDeg, B.MaxTargetDeltaDeg, Tolerance) &&
			FMath::IsNearlyEqual(A.MeanTargetDeltaDeg, B.MeanTargetDeltaDeg, Tolerance) &&
			A.PendingResetCount == B.PendingResetCount;
	}

	FString DescribeFingerprintDiff(
		const FPhase1AutoCalibDeterminismFingerprint& A,
		const FPhase1AutoCalibDeterminismFingerprint& B,
		const float Tolerance)
	{
		const float TransformLocationToleranceCm =
			FMath::Max(Tolerance, RestoreFingerprintTransformLocationToleranceCm);
		const float TransformAngularToleranceDeg =
			FMath::Max(Tolerance, RestoreFingerprintTransformAngularToleranceDeg);
		const float CriticalLinkAngularToleranceDeg =
			FMath::Max(Tolerance, RestoreFingerprintCriticalLinkAngularToleranceDeg);
		TArray<FString> Diffs;

		if (A.RuntimeState != B.RuntimeState)
		{
			Diffs.Add(FString::Printf(
				TEXT("runtime=%s->%s"),
				UPhysAnimComponent::GetRuntimeStateName(A.RuntimeState),
				UPhysAnimComponent::GetRuntimeStateName(B.RuntimeState)));
		}
		if (A.TransitionPhase != B.TransitionPhase)
		{
			Diffs.Add(FString::Printf(TEXT("phase=%d->%d"), static_cast<int32>(A.TransitionPhase), static_cast<int32>(B.TransitionPhase)));
		}
		if (!AreTransformsNear(A.OwnerTransform, B.OwnerTransform, TransformLocationToleranceCm, TransformAngularToleranceDeg, Tolerance))
		{
			const double OwnerLocDelta = FVector::Dist(A.OwnerTransform.GetLocation(), B.OwnerTransform.GetLocation());
			const double OwnerRotDeltaDeg = FMath::RadiansToDegrees(A.OwnerTransform.GetRotation().AngularDistance(B.OwnerTransform.GetRotation()));
			Diffs.Add(FString::Printf(
				TEXT("ownerLocDelta=%.4f ownerRotDeltaDeg=%.4f"),
				OwnerLocDelta,
				OwnerRotDeltaDeg));
		}
		if (!AreTransformsNear(A.MeshTransform, B.MeshTransform, TransformLocationToleranceCm, TransformAngularToleranceDeg, Tolerance))
		{
			const double MeshLocDelta = FVector::Dist(A.MeshTransform.GetLocation(), B.MeshTransform.GetLocation());
			const double MeshRotDeltaDeg = FMath::RadiansToDegrees(A.MeshTransform.GetRotation().AngularDistance(B.MeshTransform.GetRotation()));
			Diffs.Add(FString::Printf(
				TEXT("meshLocDelta=%.4f meshRotDeltaDeg=%.4f"),
				MeshLocDelta,
				MeshRotDeltaDeg));
		}
		if (!AreTransformsNear(A.RootBodyTransform, B.RootBodyTransform, TransformLocationToleranceCm, TransformAngularToleranceDeg, Tolerance))
		{
			const double RootLocDelta = FVector::Dist(A.RootBodyTransform.GetLocation(), B.RootBodyTransform.GetLocation());
			const double RootRotDeltaDeg = FMath::RadiansToDegrees(A.RootBodyTransform.GetRotation().AngularDistance(B.RootBodyTransform.GetRotation()));
			Diffs.Add(FString::Printf(
				TEXT("rootLocDelta=%.4f rootRotDeltaDeg=%.4f"),
				RootLocDelta,
				RootRotDeltaDeg));
		}
		if (!A.RootLinearVelocity.Equals(B.RootLinearVelocity, Tolerance))
		{
			Diffs.Add(FString::Printf(TEXT("rootLinearDelta=%.4f"), FVector::Dist(A.RootLinearVelocity, B.RootLinearVelocity)));
		}
		if (!A.RootAngularVelocity.Equals(B.RootAngularVelocity, Tolerance))
		{
			Diffs.Add(FString::Printf(TEXT("rootAngularDelta=%.4f"), FVector::Dist(A.RootAngularVelocity, B.RootAngularVelocity)));
		}
		if (!FMath::IsNearlyEqual(A.PelvisThighLAngularErrorDeg, B.PelvisThighLAngularErrorDeg, CriticalLinkAngularToleranceDeg))
		{
			Diffs.Add(FString::Printf(TEXT("pelvisThighL=%.4f->%.4f"), A.PelvisThighLAngularErrorDeg, B.PelvisThighLAngularErrorDeg));
		}
		if (!FMath::IsNearlyEqual(A.PelvisThighRAngularErrorDeg, B.PelvisThighRAngularErrorDeg, CriticalLinkAngularToleranceDeg))
		{
			Diffs.Add(FString::Printf(TEXT("pelvisThighR=%.4f->%.4f"), A.PelvisThighRAngularErrorDeg, B.PelvisThighRAngularErrorDeg));
		}
		if (!FMath::IsNearlyEqual(A.PelvisSpine01AngularErrorDeg, B.PelvisSpine01AngularErrorDeg, CriticalLinkAngularToleranceDeg))
		{
			Diffs.Add(FString::Printf(TEXT("pelvisSpine=%.4f->%.4f"), A.PelvisSpine01AngularErrorDeg, B.PelvisSpine01AngularErrorDeg));
		}
		if (!FMath::IsNearlyEqual(A.ShellOffsetDeltaCm, B.ShellOffsetDeltaCm, Tolerance))
		{
			Diffs.Add(FString::Printf(TEXT("shellOffset=%.4f->%.4f"), A.ShellOffsetDeltaCm, B.ShellOffsetDeltaCm));
		}
		if (!FMath::IsNearlyEqual(A.ShellVelocityDeltaCmPerSecond, B.ShellVelocityDeltaCmPerSecond, Tolerance))
		{
			Diffs.Add(FString::Printf(TEXT("shellVelocity=%.4f->%.4f"), A.ShellVelocityDeltaCmPerSecond, B.ShellVelocityDeltaCmPerSecond));
		}
		if (!FMath::IsNearlyEqual(A.MaxTargetDeltaDeg, B.MaxTargetDeltaDeg, Tolerance))
		{
			Diffs.Add(FString::Printf(TEXT("maxTargetDelta=%.4f->%.4f"), A.MaxTargetDeltaDeg, B.MaxTargetDeltaDeg));
		}
		if (!FMath::IsNearlyEqual(A.MeanTargetDeltaDeg, B.MeanTargetDeltaDeg, Tolerance))
		{
			Diffs.Add(FString::Printf(TEXT("meanTargetDelta=%.4f->%.4f"), A.MeanTargetDeltaDeg, B.MeanTargetDeltaDeg));
		}
		if (A.PendingResetCount != B.PendingResetCount)
		{
			Diffs.Add(FString::Printf(TEXT("pendingResetCount=%d->%d"), A.PendingResetCount, B.PendingResetCount));
		}

		return Diffs.IsEmpty() ? TEXT("none") : FString::Join(Diffs, TEXT(", "));
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

	bool AreScoresNearForRepro(
		const FPhase1AutoCalibScore& A,
		const FPhase1AutoCalibScore& B,
		const float Epsilon)
	{
		return FMath::IsNearlyEqual(A.WorstDirectLinkAngularErrorDeg, B.WorstDirectLinkAngularErrorDeg, FMath::Max(Epsilon, ReproWorstDirectLinkToleranceDeg)) &&
			FMath::IsNearlyEqual(A.MeanTargetDeltaDeg, B.MeanTargetDeltaDeg, FMath::Max(Epsilon, ReproMeanTargetDeltaToleranceDeg)) &&
			FMath::IsNearlyEqual(A.MaxTargetDeltaDeg, B.MaxTargetDeltaDeg, FMath::Max(Epsilon, ReproMaxTargetDeltaToleranceDeg)) &&
			FMath::IsNearlyEqual(A.ThighAsymmetryDeg, B.ThighAsymmetryDeg, FMath::Max(Epsilon, ReproThighAsymmetryToleranceDeg)) &&
			FMath::IsNearlyEqual(A.PeakRootTiltDeg, B.PeakRootTiltDeg, FMath::Max(Epsilon, ReproPeakRootTiltToleranceDeg)) &&
			FMath::IsNearlyEqual(A.ShellOffsetDeltaCm, B.ShellOffsetDeltaCm, FMath::Max(Epsilon, ReproShellOffsetToleranceCm)) &&
			FMath::IsNearlyEqual(A.ShellVelocityDeltaCmPerSecond, B.ShellVelocityDeltaCmPerSecond, FMath::Max(Epsilon, ReproShellVelocityToleranceCmPerSecond)) &&
			FMath::IsNearlyEqual(A.PeakRootLinearSpeedCmPerSecond, B.PeakRootLinearSpeedCmPerSecond, FMath::Max(Epsilon, ReproPeakRootLinearSpeedToleranceCmPerSecond)) &&
			FMath::IsNearlyEqual(A.PeakRootAngularSpeedDegPerSecond, B.PeakRootAngularSpeedDegPerSecond, FMath::Max(Epsilon, ReproPeakRootAngularSpeedToleranceDegPerSecond));
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

	int32 ResolveStageTrialLimit(const FPhase1AutoCalibRequest& Request, const EPhase1AutoCalibBuildStage Stage)
	{
		if (Request.MaxTrials > 0)
		{
			return Request.MaxTrials;
		}

		if (Request.BudgetMode == EPhase1AutoCalibBudgetMode::Smoke)
		{
			switch (Stage)
			{
			case EPhase1AutoCalibBuildStage::StageA:
				return SmokeStageATrialCount;
			case EPhase1AutoCalibBuildStage::StageB:
				return SmokeStageBTrialCount;
			case EPhase1AutoCalibBuildStage::StageC:
				return SmokeStageCTrialCount;
			default:
				break;
			}
		}

		return INDEX_NONE;
	}

	bool IsBetterTrial(const FPhase1AutoCalibTrialResult& A, const FPhase1AutoCalibTrialResult& B)
	{
		return UPhysAnimComponent::IsBetterPhase1AutoCalibScore(A.Score, B.Score);
	}

	int32 GetFailureProgressRank(const FPhase1AutoCalibTrialResult& Trial)
	{
		if (Trial.Score.bContractPassed)
		{
			return 5;
		}

		if (Trial.Score.bReachedBalanceActiveStanding)
		{
			return 4;
		}

		if (Trial.TruthfulBlocker.StartsWith(TEXT("phase3_")))
		{
			return 3;
		}

		if (Trial.TruthfulBlocker.StartsWith(TEXT("phase2_")) || Trial.Score.bNoCouplingProofSatisfied)
		{
			return 2;
		}

		if (Trial.Score.bReachedRootOn)
		{
			return 1;
		}

		return 0;
	}

	bool IsMoreProgressedFailure(const FPhase1AutoCalibTrialResult& Candidate, const FPhase1AutoCalibTrialResult& CurrentBest)
	{
		const int32 CandidateProgressRank = GetFailureProgressRank(Candidate);
		const int32 CurrentProgressRank = GetFailureProgressRank(CurrentBest);
		if (CandidateProgressRank != CurrentProgressRank)
		{
			return CandidateProgressRank > CurrentProgressRank;
		}

		return IsBetterTrial(Candidate, CurrentBest);
	}

	FPhase1AutoCalibPresetSummary& FindOrAddPresetSummary(
		TArray<FPhase1AutoCalibPresetSummary>& Summaries,
		const EPhase1AutoCalibStrategyPreset Preset)
	{
		for (FPhase1AutoCalibPresetSummary& Summary : Summaries)
		{
			if (Summary.Preset == Preset)
			{
				return Summary;
			}
		}

		FPhase1AutoCalibPresetSummary& Summary = Summaries.AddDefaulted_GetRef();
		Summary.Preset = Preset;
		return Summary;
	}

	void AddBlockerCount(TArray<FPhase1AutoCalibBlockerCount>& BlockerCounts, const FString& TruthfulBlocker)
	{
		for (FPhase1AutoCalibBlockerCount& Entry : BlockerCounts)
		{
			if (Entry.TruthfulBlocker == TruthfulBlocker)
			{
				++Entry.Count;
				return;
			}
		}

		FPhase1AutoCalibBlockerCount& Entry = BlockerCounts.AddDefaulted_GetRef();
		Entry.TruthfulBlocker = TruthfulBlocker;
		Entry.Count = 1;
	}

	const FPhase1AutoCalibBlockerCount* FindDominantBlockerCount(const TArray<FPhase1AutoCalibBlockerCount>& BlockerCounts)
	{
		return BlockerCounts.IsEmpty() ? nullptr : &BlockerCounts[0];
	}

	const FPhase1AutoCalibPresetSummary* FindPresetSummary(
		const TArray<FPhase1AutoCalibPresetSummary>& Summaries,
		const EPhase1AutoCalibStrategyPreset Preset)
	{
		for (const FPhase1AutoCalibPresetSummary& Summary : Summaries)
		{
			if (Summary.Preset == Preset)
			{
				return &Summary;
			}
		}

		return nullptr;
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

bool UPhysAnimPhase1AutoCalibSubsystem::IsActiveTrialTimeoutReached(
	const bool bTrialStarted,
	const double TrialStartTimeSeconds,
	const double CurrentTimeSeconds,
	const double TimeoutSeconds)
{
	return bTrialStarted &&
		TrialStartTimeSeconds >= 0.0 &&
		CurrentTimeSeconds >= TrialStartTimeSeconds &&
		(CurrentTimeSeconds - TrialStartTimeSeconds) >= TimeoutSeconds;
}

bool UPhysAnimPhase1AutoCalibSubsystem::ShouldAccumulateActiveTrialMetrics(const bool bTrialStarted)
{
	return bTrialStarted;
}

bool UPhysAnimPhase1AutoCalibSubsystem::ShouldFinalizeActiveTrial(
	const EBalanceReadyTransitionPhase Phase,
	const double BalanceActiveStandingHoldSeconds,
	const bool bTransitionFailed,
	const bool bSafeDenied)
{
	return bTransitionFailed ||
		bSafeDenied ||
		(Phase == EBalanceReadyTransitionPhase::BRT_Succeeded &&
			IsStandingHoldBenchmarkSatisfied(BalanceActiveStandingHoldSeconds));
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
	
	if (Component->GetRuntimeState() == EPhysAnimRuntimeState::Uninitialized)
	{
		UE_LOG(LogPhysAnimBridge, Log, TEXT("[PhysAnimAutoCalib] Target component is uninitialized, calling StartBridge()..."));
		Component->StartBridge();
	}

	StopPhase1AutoCalib(TEXT("restart"));
	ActiveRequest = Request;
	TargetComponent = Component;
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
	BuildStageACandidates(ActiveRequest, StageACandidates);
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

	Component->SetPhase1AutoCalibOwnsStartRequests(true);
	bRunActive = true;
	UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimAutoCalib] Awaiting component readiness for baseline capture..."));

	return true;
}

void UPhysAnimPhase1AutoCalibSubsystem::StopPhase1AutoCalib(const FString& Reason)
{
	if (UPhysAnimComponent* const Component = TargetComponent.Get())
	{
		Component->SetPhase1AutoCalibOwnsStartRequests(false);
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
	bActiveTrialStarted = false;
	CurrentStage = Reason == TEXT("completed") ? EAutoCalibStage::Completed : EAutoCalibStage::Inactive;
	TargetComponent.Reset();
	PendingTrials.Reset();
	ActiveTrial = FPendingTrial();
	ActiveTrialStartTimeSeconds = -1.0;
	ActiveTrialFirstRootOnTimeSeconds = -1.0;
	ActiveTrialFirstNoCouplingProofTimeSeconds = -1.0;
	ActiveTrialFirstBalanceActiveStandingTimeSeconds = -1.0;
	ActiveTrialStandingHoldStartTimeSeconds = -1.0;
	ActiveTrialMaxBalanceActiveStandingHoldSeconds = 0.0;
	ActiveTrialPeakMetrics = FPhase1AutoCalibLiveMetrics();
}

void UPhysAnimPhase1AutoCalibSubsystem::BuildStageACandidates(const FPhase1AutoCalibRequest& Request, TArray<FPhase1AutoCalibParams>& OutCandidates)
{
	OutCandidates.Reset();
	const int32 StageLimit = ResolveStageTrialLimit(Request, EPhase1AutoCalibBuildStage::StageA);
	FRandomStream RandomStream(Request.Seed);
	TArray<FPhase1AutoCalibParams> PresetCandidatePools[UE_ARRAY_COUNT(GPhase1AutoCalibPresets)];

	for (int32 PresetIndex = 0; PresetIndex < UE_ARRAY_COUNT(GPhase1AutoCalibPresets); ++PresetIndex)
	{
		const EPhase1AutoCalibStrategyPreset Preset = GPhase1AutoCalibPresets[PresetIndex];
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
			PresetCandidatePools[PresetIndex].Add(ClampParams(Params));
		}
	}

	for (int32 SampleIndex = 0; SampleIndex < StageASamplesPerPreset; ++SampleIndex)
	{
		for (int32 PresetIndex = 0; PresetIndex < UE_ARRAY_COUNT(GPhase1AutoCalibPresets); ++PresetIndex)
		{
			if (StageLimit > 0 && OutCandidates.Num() >= StageLimit)
			{
				return;
			}

			OutCandidates.Add(PresetCandidatePools[PresetIndex][SampleIndex]);
		}
	}
}

void UPhysAnimPhase1AutoCalibSubsystem::BuildStageBRefinementCandidates(
	const TArray<FPhase1AutoCalibTrialResult>& StageAResults,
	const FPhase1AutoCalibRequest& Request,
	TArray<FPhase1AutoCalibParams>& OutCandidates)
{
	OutCandidates.Reset();
	const int32 StageLimit = ResolveStageTrialLimit(Request, EPhase1AutoCalibBuildStage::StageB);

	TArray<FPhase1AutoCalibTrialResult> Sorted = StageAResults;
	Algo::Sort(Sorted, [](const FPhase1AutoCalibTrialResult& A, const FPhase1AutoCalibTrialResult& B)
	{
		return IsBetterTrial(A, B);
	});

	const int32 CandidateCount = FMath::Min(StageBTopK, Sorted.Num());
	static const float SpineDeltas[StageBRounds] = { 0.10f, 0.05f, 0.025f };
	static const float ThighDeltas[StageBRounds] = { 0.05f, 0.025f, 0.0125f };
	static const float ScaleDeltas[StageBRounds] = { 0.25f, 0.125f, 0.0625f };
	static const float BiasDeltas[StageBRounds] = { 0.25f, 0.125f, 0.0625f };
	const auto AddCandidate = [&](FPhase1AutoCalibParams Params)
	{
		if (StageLimit > 0 && OutCandidates.Num() >= StageLimit)
		{
			return false;
		}

		OutCandidates.Add(ClampParams(Params));
		return true;
	};

	// Preserve the retained Stage A centers so bounded smoke budgets can
	// truthfully re-check any discovered pass before spending the budget on
	// local perturbations around the current best family.
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		if (!AddCandidate(Sorted[CandidateIndex].Params))
		{
			return;
		}
	}

	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		const FPhase1AutoCalibParams Base = Sorted[CandidateIndex].Params;
		for (int32 RoundIndex = 0; RoundIndex < StageBRounds; ++RoundIndex)
		{
			const float SpineDelta = SpineDeltas[RoundIndex];
			const float ThighDelta = ThighDeltas[RoundIndex];
			const float ScaleDelta = ScaleDeltas[RoundIndex];
			const float BiasDelta = BiasDeltas[RoundIndex];

			FPhase1AutoCalibParams Params = Base;
			Params.SpineInterpolationAlpha += SpineDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.SpineInterpolationAlpha -= SpineDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.WorstThighInterpolationAlpha += ThighDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.WorstThighInterpolationAlpha -= ThighDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.FocusedDeltaScale += ScaleDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.FocusedDeltaScale -= ScaleDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.UprightnessWeightScale += ScaleDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.UprightnessWeightScale -= ScaleDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.ClampStrengthScale += ScaleDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.ClampStrengthScale -= ScaleDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.PelvisPitchBiasDeg += BiasDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.PelvisPitchBiasDeg -= BiasDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.PelvisRollBiasDeg += BiasDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
			Params = Base;
			Params.PelvisRollBiasDeg -= BiasDelta;
			if (!AddCandidate(Params))
			{
				return;
			}
		}
	}
}

void UPhysAnimPhase1AutoCalibSubsystem::BuildStageCReproCandidates(
	const TArray<FPhase1AutoCalibTrialResult>& StageBResults,
	const FPhase1AutoCalibRequest& Request,
	TArray<FPhase1AutoCalibParams>& OutCandidates)
{
	OutCandidates.Reset();
	const int32 StageLimit = ResolveStageTrialLimit(Request, EPhase1AutoCalibBuildStage::StageC);

	TArray<FPhase1AutoCalibTrialResult> Sorted = StageBResults;
	Algo::Sort(Sorted, [](const FPhase1AutoCalibTrialResult& A, const FPhase1AutoCalibTrialResult& B)
	{
		return IsBetterTrial(A, B);
	});

	const int32 CandidateCount = FMath::Min(StageCTopK, Sorted.Num());
	for (int32 CandidateIndex = 0; CandidateIndex < CandidateCount; ++CandidateIndex)
	{
		for (int32 RepeatIndex = 0; RepeatIndex < StageCRepetitions; ++RepeatIndex)
		{
			if (StageLimit > 0 && OutCandidates.Num() >= StageLimit)
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
		if (Candidate.TerminalClass != Reference.TerminalClass ||
			Candidate.TruthfulBlocker != Reference.TruthfulBlocker)
		{
			return false;
		}

		if (Candidate.Score.bContractPassed != Reference.Score.bContractPassed ||
			Candidate.Score.bTimedOut != Reference.Score.bTimedOut ||
			Candidate.Score.bSafeDenied != Reference.Score.bSafeDenied ||
			Candidate.Score.bRestoreDeterministic != Reference.Score.bRestoreDeterministic ||
			Candidate.Score.bReachedRootOn != Reference.Score.bReachedRootOn ||
			Candidate.Score.bNoCouplingProofSatisfied != Reference.Score.bNoCouplingProofSatisfied ||
			Candidate.Score.bReachedBalanceActiveStanding != Reference.Score.bReachedBalanceActiveStanding)
		{
			return false;
		}

		if (!FMath::IsNearlyEqual(
				Candidate.Score.BalanceActiveStandingHoldSeconds,
				Reference.Score.BalanceActiveStandingHoldSeconds,
				FMath::Max(Epsilon, ReproBalanceActiveStandingHoldToleranceSeconds)))
		{
			return false;
		}

		if (!AreScoresNearForRepro(Candidate.Score, Reference.Score, Epsilon))
		{
			return false;
		}
	}

	return true;
}

bool UPhysAnimPhase1AutoCalibSubsystem::AreDeterminismFingerprintsNear(
	const FPhase1AutoCalibDeterminismFingerprint& A,
	const FPhase1AutoCalibDeterminismFingerprint& B,
	const float Tolerance)
{
	return AreFingerprintsNear(A, B, Tolerance);
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
		if (!IsValid(Candidate) || Candidate->GetWorld() != World || !AutoCalibMatchesFilter(*Candidate, FilterLower))
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
	if (!Component.RestorePhase1AutoCalibBaseline(Baseline, OutError))
	{
		return false;
	}
	Component.StopBalancePerturbationMode();
	Component.SetPhase1AutoCalibOwnsStartRequests(true);
	if (!Component.CapturePhase1AutoCalibDeterminismFingerprint(FingerprintA, OutError) ||
		!Component.RestorePhase1AutoCalibBaseline(Baseline, OutError))
	{
		return false;
	}
	Component.StopBalancePerturbationMode();
	Component.SetPhase1AutoCalibOwnsStartRequests(true);
	if (!Component.CapturePhase1AutoCalibDeterminismFingerprint(FingerprintB, OutError))
	{
		return false;
	}

	if (!AreFingerprintsNear(FingerprintA, FingerprintB, RestoreFingerprintTolerance))
	{
		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimAutoCalib] Determinism preflight mismatch: %s"),
			*DescribeFingerprintDiff(FingerprintA, FingerprintB, RestoreFingerprintTolerance));
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

	FString QueueReason;
	const FPhysAnimStabilizationSettings ReadinessSettings = Component->GetConfiguredStabilizationSettings();
	const bool bQueueReady = Component->EvaluateBalanceModeQueueGates(ReadinessSettings, QueueReason);
	FString PreEntryReason;
	const bool bPreEntryPass = Component->EvaluateBalanceBridgeActivePreEntryPrerequisites(ReadinessSettings, PreEntryReason);
	const bool bBaselineReady = bQueueReady && bPreEntryPass;

	if (bBaselineReady)
	{
		UE_LOG(
			LogPhysAnimBridge,
			Warning,
			TEXT("[PhysAnimAutoCalib] Queue-ready baseline reached after %.2fs. Capturing baseline. preEntry=ready"),
			World->GetTimeSeconds() - CurrentStageStartTimeSeconds);
		
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

		// Keep preflight and the first queued trial on the harness-owned start
		// path so auto-trigger cannot consume the baseline between them.
		Component->StopBalancePerturbationMode();
		Component->SetPhase1AutoCalibOwnsStartRequests(true);

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

	if (World->GetTimeSeconds() - LastReadinessLogTimeSeconds >= 1.0)
	{
		UE_LOG(LogPhysAnimBridge, Warning, TEXT("[PhysAnimAutoCalib] Awaiting queue-ready baseline... elapsed=%.1fs state=%s queue=%s preEntry=%s"),
			World->GetTimeSeconds() - CurrentStageStartTimeSeconds,
			UPhysAnimComponent::GetRuntimeStateName(Component->GetRuntimeState()),
			QueueReason.IsEmpty() ? TEXT("ready") : *QueueReason,
			PreEntryReason.IsEmpty() ? TEXT("ready") : *PreEntryReason);
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
	ActiveTrialStartTimeSeconds = -1.0;
	ActiveTrialFirstRootOnTimeSeconds = -1.0;
	ActiveTrialFirstNoCouplingProofTimeSeconds = -1.0;
	ActiveTrialFirstBalanceActiveStandingTimeSeconds = -1.0;
	ActiveTrialStandingHoldStartTimeSeconds = -1.0;
	ActiveTrialMaxBalanceActiveStandingHoldSeconds = 0.0;
	ActiveTrialPeakMetrics = FPhase1AutoCalibLiveMetrics();
	bActiveTrialStarted = false;
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

	const FPhysAnimStabilizationSettings& Settings = Component->GetConfiguredStabilizationSettings();
	const double TimeoutSeconds = static_cast<double>(
		Settings.BalancePhase1PrepareDuration +
		Settings.BalancePhase1LateValidateRequiredSeconds +
		PhysAnimAutoCalibBenchmark::RequiredBalanceActiveStandingHoldSeconds +
		3.5f);
	const double CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const double ElapsedSeconds = bActiveTrialStarted && ActiveTrialStartTimeSeconds >= 0.0
		? (CurrentTimeSeconds - ActiveTrialStartTimeSeconds)
		: 0.0;
	if (IsActiveTrialTimeoutReached(bActiveTrialStarted, ActiveTrialStartTimeSeconds, CurrentTimeSeconds, TimeoutSeconds))
	{
		FinalizeActiveTrial(true);
		return;
	}

	if (Component->GetBalanceReadyTransitionPhase() == EBalanceReadyTransitionPhase::BRT_Inactive && !bActiveTrialStarted)
	{
		FString QueueReason;
		if (!Component->EvaluateBalanceModeQueueGates(Settings, QueueReason))
		{
			return;
		}

		if (!Component->StartPhase1AutoCalibTrial(Error))
		{
			LastError = Error;
			StopPhase1AutoCalib(TEXT("trial_start_failed"));
			return;
		}

		bActiveTrialStarted = true;
		ActiveTrialStartTimeSeconds = CurrentTimeSeconds;
		ActiveTrialPeakMetrics = LiveMetrics;
	}
	else if (!bActiveTrialStarted)
	{
		// The component already entered Phase 1 before we could trigger it
		// (e.g. via the normal auto-triggered balance entry path).  Adopt the
		// already-running trial so the timeout guard and telemetry still work.
		bActiveTrialStarted = true;
		ActiveTrialStartTimeSeconds = CurrentTimeSeconds;
		ActiveTrialPeakMetrics = LiveMetrics;
	}
	else if (ShouldAccumulateActiveTrialMetrics(bActiveTrialStarted))
	{
		UpdatePeakMetrics(LiveMetrics);
	}

	const EBalanceReadyTransitionPhase Phase = Component->GetBalanceReadyTransitionPhase();
	const FPhysAnimBalanceReadyTransitionSnapshot Snapshot = Component->ExportBalanceReadyTransitionSnapshot();
	const bool bReachedRootOn = IsLaterThanPhase1(Phase);
	const bool bNoCouplingProofSatisfied =
		Snapshot.CertifiedLateValidationResult.bRootOnReadinessNoCouplingProofSatisfied ||
		Snapshot.CertifiedHandoff.bRootOnReadinessNoCouplingProofSatisfied;
	if (bReachedRootOn && ActiveTrialFirstRootOnTimeSeconds < 0.0)
	{
		ActiveTrialFirstRootOnTimeSeconds = ElapsedSeconds;
	}
	if (bNoCouplingProofSatisfied && ActiveTrialFirstNoCouplingProofTimeSeconds < 0.0)
	{
		ActiveTrialFirstNoCouplingProofTimeSeconds = ElapsedSeconds;
	}

	const bool bStandingActive = Component->GetRuntimeState() == EPhysAnimRuntimeState::BalanceActive_Standing;
	if (bStandingActive)
	{
		if (ActiveTrialFirstBalanceActiveStandingTimeSeconds < 0.0)
		{
			ActiveTrialFirstBalanceActiveStandingTimeSeconds = ElapsedSeconds;
		}
		if (ActiveTrialStandingHoldStartTimeSeconds < 0.0)
		{
			ActiveTrialStandingHoldStartTimeSeconds = CurrentTimeSeconds;
		}
		ActiveTrialMaxBalanceActiveStandingHoldSeconds = FMath::Max(
			ActiveTrialMaxBalanceActiveStandingHoldSeconds,
			CurrentTimeSeconds - ActiveTrialStandingHoldStartTimeSeconds);
	}
	else
	{
		ActiveTrialStandingHoldStartTimeSeconds = -1.0;
	}

	if (ShouldFinalizeActiveTrial(
		Phase,
		ActiveTrialMaxBalanceActiveStandingHoldSeconds,
		Component->HasBalanceReadyTransitionFailed(),
		Component->HasSafePhase2Denial()))
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
	bActiveTrialStarted = false;
	ActiveTrial = FPendingTrial();
	ActiveTrialStartTimeSeconds = -1.0;
	ActiveTrialFirstRootOnTimeSeconds = -1.0;
	ActiveTrialFirstNoCouplingProofTimeSeconds = -1.0;
	ActiveTrialFirstBalanceActiveStandingTimeSeconds = -1.0;
	ActiveTrialStandingHoldStartTimeSeconds = -1.0;
	ActiveTrialMaxBalanceActiveStandingHoldSeconds = 0.0;
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
		BuildStageBRefinementCandidates(StageAResults, ActiveRequest, StageBCandidates);
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
			BuildStageCReproCandidates(StageBResults, ActiveRequest, StageCCandidates);
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
	const FPhase1PelvisCouplingRotationForensics& Forensics = Component->LastPhase1PelvisCouplingRotationForensics;
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
	Result.Score.bReachedBalanceActiveStanding = ActiveTrialFirstBalanceActiveStandingTimeSeconds >= 0.0;
	Result.Score.BalanceActiveStandingHoldSeconds = static_cast<float>(ActiveTrialMaxBalanceActiveStandingHoldSeconds);
	Result.TrialTimeoutBudgetSeconds = static_cast<float>(
		Component->GetConfiguredStabilizationSettings().BalancePhase1PrepareDuration +
		Component->GetConfiguredStabilizationSettings().BalancePhase1LateValidateRequiredSeconds +
		PhysAnimAutoCalibBenchmark::RequiredBalanceActiveStandingHoldSeconds +
		3.5f);
	Result.TimeToRootOnSeconds = ActiveTrialFirstRootOnTimeSeconds >= 0.0 ? static_cast<float>(ActiveTrialFirstRootOnTimeSeconds) : -1.0f;
	Result.TimeToNoCouplingProofSeconds = ActiveTrialFirstNoCouplingProofTimeSeconds >= 0.0 ? static_cast<float>(ActiveTrialFirstNoCouplingProofTimeSeconds) : -1.0f;
	Result.TimeToBalanceActiveStandingSeconds = ActiveTrialFirstBalanceActiveStandingTimeSeconds >= 0.0 ? static_cast<float>(ActiveTrialFirstBalanceActiveStandingTimeSeconds) : -1.0f;
	Result.bTimedOutBeforeRootOn = bTimedOut && Result.TimeToRootOnSeconds < 0.0f;
	Result.bTimedOutBeforeNoCouplingProof = bTimedOut && Result.TimeToNoCouplingProofSeconds < 0.0f;

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
		Result.Score.bNoCouplingProofSatisfied &&
		Result.Score.bReachedBalanceActiveStanding &&
		IsStandingHoldBenchmarkSatisfied(ActiveTrialMaxBalanceActiveStandingHoldSeconds);

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
		Result.TerminalClass = TEXT("stable_balance_active_standing");
		Result.TruthfulBlocker = TEXT("ready");
	}
	else if (bTimedOut && Result.Score.bReachedBalanceActiveStanding)
	{
		Result.TruthfulBlocker = TEXT("phase3_balance_active_standing_not_sustained");
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

	Result.WinningSearchFamily = Forensics.WinningSearchFamily;
	Result.WinningSearchSource = Forensics.WinningSearchSource;
	Result.ExecutedSearchFamilies = Forensics.ExecutedSearchFamilies;
	Result.bCoupledTradeControlWon = Forensics.bCoupledTradeControlWon;

	UPhysAnimComponent::FinalizePhase1AutoCalibScore(Result.Score);
	return Result;
}

void UPhysAnimPhase1AutoCalibSubsystem::FinalizeReportData(FPhase1AutoCalibReport& InOutReport, TArray<FPhase1AutoCalibTrialResult>* StageCTrials)
{
	if (StageCTrials)
	{
		for (int32 Index = 0; Index + StageCRepetitions <= StageCTrials->Num(); Index += StageCRepetitions)
		{
			TArray<FPhase1AutoCalibTrialResult> Group;
			Group.Reserve(StageCRepetitions);
			for (int32 Offset = 0; Offset < StageCRepetitions; ++Offset)
			{
				Group.Add((*StageCTrials)[Index + Offset]);
			}

			const bool bReproducible = AreTrialResultsReproducible(Group, ScoreReproTolerance);
			for (int32 Offset = 0; Offset < StageCRepetitions; ++Offset)
			{
				(*StageCTrials)[Index + Offset].bReproducible = bReproducible;
			}
		}
	}

	for (FPhase1AutoCalibTrialResult& Trial : InOutReport.Trials)
	{
		Trial.PresetRank = INDEX_NONE;
		Trial.PresetNearPassRank = INDEX_NONE;
		if (!StageCTrials || Trial.StageName != TEXT("stage_c"))
		{
			continue;
		}

		for (const FPhase1AutoCalibTrialResult& StageCTrial : *StageCTrials)
		{
			if (StageCTrial.TrialId == Trial.TrialId)
			{
				Trial.bReproducible = StageCTrial.bReproducible;
				break;
			}
		}
	}

	Algo::Sort(InOutReport.Trials, [](const FPhase1AutoCalibTrialResult& A, const FPhase1AutoCalibTrialResult& B)
	{
		return IsBetterTrial(A, B);
	});

	for (int32 TrialIndex = 0; TrialIndex < InOutReport.Trials.Num(); ++TrialIndex)
	{
		FPhase1AutoCalibTrialResult& Trial = InOutReport.Trials[TrialIndex];
		int32 PresetRank = 0;
		int32 PresetNearPassRank = 0;
		for (const FPhase1AutoCalibTrialResult& Other : InOutReport.Trials)
		{
			if (Other.Params.SourcePreset != Trial.Params.SourcePreset)
			{
				continue;
			}

			++PresetRank;
			if (!Other.Score.bContractPassed)
			{
				++PresetNearPassRank;
			}
			if (Other.TrialId == Trial.TrialId)
			{
				break;
			}
		}
		Trial.PresetRank = PresetRank;
		Trial.PresetNearPassRank = Trial.Score.bContractPassed ? INDEX_NONE : PresetNearPassRank;
	}

	InOutReport.PresetSummaries.Reset();
	InOutReport.ParetoFrontier.Reset();
	InOutReport.OverallBlockerCounts.Reset();
	InOutReport.bHasBestCandidate = false;
	InOutReport.bHasBestNearPass = false;
	InOutReport.bHasFurthestProgressedFailure = false;
	InOutReport.bHasReproducibleTruthfulPass = false;
	InOutReport.FrontierClassification = EPhase1AutoCalibFrontierClassification::Unknown;
	InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::None;
	InOutReport.RecommendedExpansionName.Reset();
	InOutReport.DominantTruthfulBlocker.Reset();
	InOutReport.bAnyTimedOutBeforeRootOn = false;
	InOutReport.bAnyTimedOutBeforeNoCouplingProof = false;
	InOutReport.RequiredBalanceActiveStandingHoldSeconds = PhysAnimAutoCalibBenchmark::RequiredBalanceActiveStandingHoldSeconds;

	for (int32 TrialIndex = 0; TrialIndex < InOutReport.Trials.Num(); ++TrialIndex)
	{
		FPhase1AutoCalibTrialResult& Trial = InOutReport.Trials[TrialIndex];
		FPhase1AutoCalibPresetSummary& Summary = FindOrAddPresetSummary(InOutReport.PresetSummaries, Trial.Params.SourcePreset);
		++Summary.TrialCount;
		if (Trial.Score.bContractPassed)
		{
			++Summary.ContractPassedCount;
			if (Trial.bReproducible)
			{
				Summary.bHasReproducibleTruthfulPass = true;
			}
		}
		if (Trial.Score.bContractPassed && (!Summary.bHasBestCandidate || IsBetterTrial(Trial, Summary.BestCandidate)))
		{
			Summary.BestCandidate = Trial;
			Summary.bHasBestCandidate = true;
		}
		if (!Trial.Score.bContractPassed && (!Summary.bHasBestNearPass || IsBetterTrial(Trial, Summary.BestNearPass)))
		{
			Summary.BestNearPass = Trial;
			Summary.bHasBestNearPass = true;
		}
		AddBlockerCount(Summary.BlockerCounts, Trial.TruthfulBlocker);
		if (!InOutReport.bHasBestNearPass && !Trial.Score.bContractPassed)
		{
			InOutReport.BestNearPass = Trial;
			InOutReport.bHasBestNearPass = true;
		}
		if (!Trial.Score.bContractPassed &&
			(!InOutReport.bHasFurthestProgressedFailure || IsMoreProgressedFailure(Trial, InOutReport.FurthestProgressedFailure)))
		{
			InOutReport.FurthestProgressedFailure = Trial;
			InOutReport.bHasFurthestProgressedFailure = true;
		}
		InOutReport.bAnyTimedOutBeforeRootOn |= Trial.bTimedOutBeforeRootOn;
		InOutReport.bAnyTimedOutBeforeNoCouplingProof |= Trial.bTimedOutBeforeNoCouplingProof;
		AddBlockerCount(InOutReport.OverallBlockerCounts, Trial.TruthfulBlocker);
	}

	for (const FPhase1AutoCalibTrialResult& Trial : InOutReport.Trials)
	{
		if (Trial.Score.bContractPassed && Trial.bReproducible)
		{
			InOutReport.BestCandidate = Trial;
			InOutReport.bHasBestCandidate = true;
			InOutReport.bHasReproducibleTruthfulPass = true;
			break;
		}
	}

	if (!InOutReport.bHasBestCandidate)
	{
		for (const FPhase1AutoCalibTrialResult& Trial : InOutReport.Trials)
		{
			if (Trial.Score.bContractPassed)
			{
				InOutReport.BestCandidate = Trial;
				InOutReport.bHasBestCandidate = true;
				break;
			}
		}
	}

	for (int32 SummaryIndex = 0; SummaryIndex < InOutReport.PresetSummaries.Num(); ++SummaryIndex)
	{
		FPhase1AutoCalibPresetSummary& Summary = InOutReport.PresetSummaries[SummaryIndex];
		Algo::Sort(Summary.BlockerCounts, [](const FPhase1AutoCalibBlockerCount& A, const FPhase1AutoCalibBlockerCount& B)
		{
			if (A.Count != B.Count)
			{
				return A.Count > B.Count;
			}
			return A.TruthfulBlocker < B.TruthfulBlocker;
		});
		if (const FPhase1AutoCalibBlockerCount* const DominantBlocker = FindDominantBlockerCount(Summary.BlockerCounts))
		{
			Summary.DominantTruthfulBlocker = DominantBlocker->TruthfulBlocker;
		}
		else
		{
			Summary.DominantTruthfulBlocker.Reset();
		}
	}
	Algo::Sort(InOutReport.OverallBlockerCounts, [](const FPhase1AutoCalibBlockerCount& A, const FPhase1AutoCalibBlockerCount& B)
	{
		if (A.Count != B.Count)
		{
			return A.Count > B.Count;
		}
		return A.TruthfulBlocker < B.TruthfulBlocker;
	});
	if (const FPhase1AutoCalibBlockerCount* const DominantBlocker = FindDominantBlockerCount(InOutReport.OverallBlockerCounts))
	{
		InOutReport.DominantTruthfulBlocker = DominantBlocker->TruthfulBlocker;
	}
	Algo::Sort(InOutReport.PresetSummaries, [](const FPhase1AutoCalibPresetSummary& A, const FPhase1AutoCalibPresetSummary& B)
	{
		return static_cast<uint8>(A.Preset) < static_cast<uint8>(B.Preset);
	});

	const FPhase1AutoCalibPresetSummary* const CurrentDefaultSummary = FindPresetSummary(
		InOutReport.PresetSummaries,
		EPhase1AutoCalibStrategyPreset::CurrentDefault);
	const float BaselineWorstDirectLink =
		(CurrentDefaultSummary && CurrentDefaultSummary->bHasBestNearPass)
		? CurrentDefaultSummary->BestNearPass.Score.WorstDirectLinkAngularErrorDeg
		: TNumericLimits<float>::Max();
	const float BaselineThighAsymmetry =
		(CurrentDefaultSummary && CurrentDefaultSummary->bHasBestNearPass)
		? CurrentDefaultSummary->BestNearPass.Score.ThighAsymmetryDeg
		: TNumericLimits<float>::Max();

	for (FPhase1AutoCalibPresetSummary& Summary : InOutReport.PresetSummaries)
	{
		if (!Summary.bHasBestNearPass ||
			Summary.Preset == EPhase1AutoCalibStrategyPreset::CurrentDefault ||
			!CurrentDefaultSummary ||
			!CurrentDefaultSummary->bHasBestNearPass)
		{
			continue;
		}

		Summary.WorstDirectLinkImprovementVsCurrentDefaultDeg =
			BaselineWorstDirectLink - Summary.BestNearPass.Score.WorstDirectLinkAngularErrorDeg;
		Summary.ThighAsymmetryImprovementVsCurrentDefaultDeg =
			BaselineThighAsymmetry - Summary.BestNearPass.Score.ThighAsymmetryDeg;
		Summary.bImprovesWorstDirectLinkVsCurrentDefault =
			Summary.WorstDirectLinkImprovementVsCurrentDefaultDeg > ScoreReproTolerance;
		Summary.bImprovesThighAsymmetryVsCurrentDefault =
			Summary.ThighAsymmetryImprovementVsCurrentDefaultDeg > ScoreReproTolerance;
	}

	bool bHasThighBlockedPreset = false;
	bool bHasSpineBlockedPreset = false;
	bool bAnyPresetImprovesVsCurrentDefault = false;
	for (const FPhase1AutoCalibPresetSummary& Summary : InOutReport.PresetSummaries)
	{
		if (!Summary.DominantTruthfulBlocker.IsEmpty())
		{
			if (Summary.DominantTruthfulBlocker.Contains(TEXT("pelvis_thigh_margin_insufficient")))
			{
				bHasThighBlockedPreset = true;
			}
			if (Summary.DominantTruthfulBlocker.Contains(TEXT("pelvis_spine_margin_insufficient")))
			{
				bHasSpineBlockedPreset = true;
			}
		}

		if (Summary.bImprovesWorstDirectLinkVsCurrentDefault || Summary.bImprovesThighAsymmetryVsCurrentDefault)
		{
			bAnyPresetImprovesVsCurrentDefault = true;
		}
	}

	if (InOutReport.bHasReproducibleTruthfulPass)
	{
		InOutReport.FrontierClassification = EPhase1AutoCalibFrontierClassification::TruthfulPassFound;
		InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::PromoteBestCandidate;
	}
	else if (bHasThighBlockedPreset && bHasSpineBlockedPreset && bAnyPresetImprovesVsCurrentDefault)
	{
		InOutReport.FrontierClassification = EPhase1AutoCalibFrontierClassification::CoupledSpineThighFlip;
		InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::AddCoupledTradeControlExpansion;
		InOutReport.RecommendedExpansionName = TEXT("CoupledTradeControlFamily");
	}
	else if (bHasThighBlockedPreset && !bHasSpineBlockedPreset)
	{
		InOutReport.FrontierClassification = EPhase1AutoCalibFrontierClassification::StillThighBlocked;
		if (bAnyPresetImprovesVsCurrentDefault)
		{
			InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::AddCoupledTradeControlExpansion;
			InOutReport.RecommendedExpansionName = TEXT("CoupledTradeControlFamily");
		}
		else
		{
			InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::InvestigateCandidateGeneration;
		}
	}
	else if (!bHasThighBlockedPreset && bHasSpineBlockedPreset)
	{
		InOutReport.FrontierClassification = EPhase1AutoCalibFrontierClassification::StillSpineBlocked;
		if (bAnyPresetImprovesVsCurrentDefault)
		{
			InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::AddCoupledTradeControlExpansion;
			InOutReport.RecommendedExpansionName = TEXT("CoupledTradeControlFamily");
		}
		else
		{
			InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::InvestigateCandidateGeneration;
		}
	}
	else
	{
		InOutReport.FrontierClassification = EPhase1AutoCalibFrontierClassification::FlatNoMaterialImprovement;
		InOutReport.RecommendedAction = EPhase1AutoCalibRecommendedAction::InvestigateCandidateGeneration;
	}

	for (const FPhase1AutoCalibTrialResult& Candidate : InOutReport.Trials)
	{
		bool bDominated = false;
		for (const FPhase1AutoCalibTrialResult& Other : InOutReport.Trials)
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
			InOutReport.ParetoFrontier.Add(Candidate);
		}
	}
}

void UPhysAnimPhase1AutoCalibSubsystem::FinalizeReport()
{
	FinalizeReportData(LatestReport, &StageCResults);
	WriteArtifacts();
}

#if WITH_DEV_AUTOMATION_TESTS
bool UPhysAnimPhase1AutoCalibSubsystem::TestOnlyWriteArtifacts(FPhase1AutoCalibReport& InOutReport)
{
	UPhysAnimPhase1AutoCalibSubsystem* const Subsystem = NewObject<UPhysAnimPhase1AutoCalibSubsystem>();
	if (!Subsystem)
	{
		return false;
	}

	Subsystem->LatestReport = InOutReport;
	Subsystem->WriteArtifacts();
	InOutReport = Subsystem->LatestReport;
	return true;
}
#endif

void UPhysAnimPhase1AutoCalibSubsystem::WriteArtifacts()
{
	const FString OutputDirectory = LatestReport.OutputDirectory;
	if (OutputDirectory.IsEmpty())
	{
		return;
	}

	IFileManager::Get().MakeDirectory(*OutputDirectory, true);

	FString Csv = TEXT("trial_id,stage,repetition,preset_rank,preset_near_pass_rank,terminal_class,truthful_blocker,contract_passed,reproducible,trial_timeout_budget_seconds,time_to_root_on_seconds,time_to_no_coupling_proof_seconds,time_to_balance_active_standing_seconds,timed_out_before_root_on,timed_out_before_no_coupling_proof,winning_search_family,winning_search_source,executed_search_families,coupled_trade_control_won,source_preset,seed_family_preset,spine_alpha,worst_thigh_alpha,focused_delta_scale,uprightness_weight_scale,clamp_strength_scale,pelvis_pitch_bias_deg,pelvis_roll_bias_deg,worst_direct_link_angular_error_deg,mean_target_delta_deg,max_target_delta_deg,thigh_asymmetry_deg,peak_root_tilt_deg,shell_offset_delta_cm,shell_velocity_delta_cm_per_second,peak_root_linear_speed_cm_per_second,peak_root_angular_speed_deg_per_second,reached_balance_active_standing,balance_active_standing_hold_seconds\n");
	for (const FPhase1AutoCalibTrialResult& Trial : LatestReport.Trials)
	{
		const FString ExecutedFamilies = FString::Join(Trial.ExecutedSearchFamilies, TEXT("|"));
		Csv += FString::Printf(
			TEXT("%d,%s,%d,%d,%d,%s,%s,%s,%s,%.6f,%.6f,%.6f,%.6f,%s,%s,%s,%s,%s,%s,%s,%s,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s,%.6f\n"),
			Trial.TrialId,
			*Trial.StageName,
			Trial.RepetitionIndex,
			Trial.PresetRank,
			Trial.PresetNearPassRank,
			*Trial.TerminalClass,
			*Trial.TruthfulBlocker,
			Trial.Score.bContractPassed ? TEXT("true") : TEXT("false"),
			Trial.bReproducible ? TEXT("true") : TEXT("false"),
			Trial.TrialTimeoutBudgetSeconds,
			Trial.TimeToRootOnSeconds,
			Trial.TimeToNoCouplingProofSeconds,
			Trial.TimeToBalanceActiveStandingSeconds,
			Trial.bTimedOutBeforeRootOn ? TEXT("true") : TEXT("false"),
			Trial.bTimedOutBeforeNoCouplingProof ? TEXT("true") : TEXT("false"),
			*Trial.WinningSearchFamily,
			*Trial.WinningSearchSource,
			*ExecutedFamilies,
			Trial.bCoupledTradeControlWon ? TEXT("true") : TEXT("false"),
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
			Trial.Score.PeakRootAngularSpeedDegPerSecond,
			Trial.Score.bReachedBalanceActiveStanding ? TEXT("true") : TEXT("false"),
			Trial.Score.BalanceActiveStandingHoldSeconds);
	}
	FFileHelper::SaveStringToFile(Csv, *LatestReport.TrialsCsvPath);

	const auto BuildTrialJson = [](const FPhase1AutoCalibTrialResult& Trial) -> FString
	{
		FString ExecutedFamiliesJson = TEXT("[");
		for (int32 FamilyIndex = 0; FamilyIndex < Trial.ExecutedSearchFamilies.Num(); ++FamilyIndex)
		{
			if (FamilyIndex > 0)
			{
				ExecutedFamiliesJson += TEXT(",");
			}
			ExecutedFamiliesJson += FString::Printf(TEXT("\"%s\""), *JsonEscape(Trial.ExecutedSearchFamilies[FamilyIndex]));
		}
		ExecutedFamiliesJson += TEXT("]");
		return FString::Printf(
			TEXT("{\"trialId\":%d,\"stage\":\"%s\",\"repetition\":%d,\"presetRank\":%d,\"presetNearPassRank\":%d,\"terminalClass\":\"%s\",\"truthfulBlocker\":\"%s\",\"contractPassed\":%s,\"reproducible\":%s,\"trialTimeoutBudgetSeconds\":%.6f,\"timeToRootOnSeconds\":%.6f,\"timeToNoCouplingProofSeconds\":%.6f,\"timeToBalanceActiveStandingSeconds\":%.6f,\"timedOutBeforeRootOn\":%s,\"timedOutBeforeNoCouplingProof\":%s,\"winningSearchFamily\":\"%s\",\"winningSearchSource\":\"%s\",\"executedSearchFamilies\":%s,\"coupledTradeControlWon\":%s,\"sourcePreset\":\"%s\",\"seedFamilyPreset\":\"%s\",\"score\":{\"worstDirectLinkAngularErrorDeg\":%.6f,\"meanTargetDeltaDeg\":%.6f,\"maxTargetDeltaDeg\":%.6f,\"thighAsymmetryDeg\":%.6f,\"peakRootTiltDeg\":%.6f,\"shellOffsetDeltaCm\":%.6f,\"shellVelocityDeltaCmPerSecond\":%.6f,\"peakRootLinearSpeedCmPerSecond\":%.6f,\"peakRootAngularSpeedDegPerSecond\":%.6f,\"reachedBalanceActiveStanding\":%s,\"balanceActiveStandingHoldSeconds\":%.6f}}"),
			Trial.TrialId,
			*JsonEscape(Trial.StageName),
			Trial.RepetitionIndex,
			Trial.PresetRank,
			Trial.PresetNearPassRank,
			*JsonEscape(Trial.TerminalClass),
			*JsonEscape(Trial.TruthfulBlocker),
			Trial.Score.bContractPassed ? TEXT("true") : TEXT("false"),
			Trial.bReproducible ? TEXT("true") : TEXT("false"),
			Trial.TrialTimeoutBudgetSeconds,
			Trial.TimeToRootOnSeconds,
			Trial.TimeToNoCouplingProofSeconds,
			Trial.TimeToBalanceActiveStandingSeconds,
			Trial.bTimedOutBeforeRootOn ? TEXT("true") : TEXT("false"),
			Trial.bTimedOutBeforeNoCouplingProof ? TEXT("true") : TEXT("false"),
			*JsonEscape(Trial.WinningSearchFamily),
			*JsonEscape(Trial.WinningSearchSource),
			*ExecutedFamiliesJson,
			Trial.bCoupledTradeControlWon ? TEXT("true") : TEXT("false"),
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
			Trial.Score.PeakRootAngularSpeedDegPerSecond,
			Trial.Score.bReachedBalanceActiveStanding ? TEXT("true") : TEXT("false"),
			Trial.Score.BalanceActiveStandingHoldSeconds);
	};

	const auto BuildBlockerCountJson = [](const FPhase1AutoCalibBlockerCount& Entry) -> FString
	{
		return FString::Printf(
			TEXT("{\"truthfulBlocker\":\"%s\",\"count\":%d}"),
			*JsonEscape(Entry.TruthfulBlocker),
			Entry.Count);
	};

	const auto BuildPresetSummaryJson = [&BuildTrialJson, &BuildBlockerCountJson](const FPhase1AutoCalibPresetSummary& Summary) -> FString
	{
		FString BlockersJson = TEXT("[");
		for (int32 Index = 0; Index < Summary.BlockerCounts.Num(); ++Index)
		{
			if (Index > 0)
			{
				BlockersJson += TEXT(",");
			}
			BlockersJson += BuildBlockerCountJson(Summary.BlockerCounts[Index]);
		}
		BlockersJson += TEXT("]");

		return FString::Printf(
			TEXT("{\"preset\":\"%s\",\"trialCount\":%d,\"contractPassedCount\":%d,\"hasReproducibleTruthfulPass\":%s,\"dominantTruthfulBlocker\":\"%s\",\"bestCandidate\":%s,\"bestNearPass\":%s,\"worstDirectLinkImprovementVsCurrentDefaultDeg\":%.6f,\"thighAsymmetryImprovementVsCurrentDefaultDeg\":%.6f,\"improvesWorstDirectLinkVsCurrentDefault\":%s,\"improvesThighAsymmetryVsCurrentDefault\":%s,\"blockerCounts\":%s}"),
			StrategyPresetToString(Summary.Preset),
			Summary.TrialCount,
			Summary.ContractPassedCount,
			Summary.bHasReproducibleTruthfulPass ? TEXT("true") : TEXT("false"),
			*JsonEscape(Summary.DominantTruthfulBlocker),
			Summary.bHasBestCandidate ? *BuildTrialJson(Summary.BestCandidate) : TEXT("null"),
			Summary.bHasBestNearPass ? *BuildTrialJson(Summary.BestNearPass) : TEXT("null"),
			Summary.WorstDirectLinkImprovementVsCurrentDefaultDeg,
			Summary.ThighAsymmetryImprovementVsCurrentDefaultDeg,
			Summary.bImprovesWorstDirectLinkVsCurrentDefault ? TEXT("true") : TEXT("false"),
			Summary.bImprovesThighAsymmetryVsCurrentDefault ? TEXT("true") : TEXT("false"),
			*BlockersJson);
	};

	FString PresetSummariesJson = TEXT("[");
	for (int32 Index = 0; Index < LatestReport.PresetSummaries.Num(); ++Index)
	{
		if (Index > 0)
		{
			PresetSummariesJson += TEXT(",");
		}
		PresetSummariesJson += BuildPresetSummaryJson(LatestReport.PresetSummaries[Index]);
	}
	PresetSummariesJson += TEXT("]");

	FString OverallBlockersJson = TEXT("[");
	for (int32 Index = 0; Index < LatestReport.OverallBlockerCounts.Num(); ++Index)
	{
		if (Index > 0)
		{
			OverallBlockersJson += TEXT(",");
		}
		OverallBlockersJson += BuildBlockerCountJson(LatestReport.OverallBlockerCounts[Index]);
	}
	OverallBlockersJson += TEXT("]");

	FString SummaryJson = FString::Printf(
		TEXT("{\"outputDirectory\":\"%s\",\"trialCount\":%d,\"hasReproducibleTruthfulPass\":%s,\"requiredBalanceActiveStandingHoldSeconds\":%.6f,\"frontierClassification\":\"%s\",\"recommendedAction\":\"%s\",\"recommendedExpansionName\":\"%s\",\"dominantTruthfulBlocker\":\"%s\",\"anyTimedOutBeforeRootOn\":%s,\"anyTimedOutBeforeNoCouplingProof\":%s,\"bestCandidate\":%s,\"bestNearPass\":%s,\"furthestProgressedFailure\":%s,\"overallBlockerCounts\":%s,\"presetSummaries\":%s}"),
		*JsonEscape(LatestReport.OutputDirectory),
		LatestReport.Trials.Num(),
		LatestReport.bHasReproducibleTruthfulPass ? TEXT("true") : TEXT("false"),
		LatestReport.RequiredBalanceActiveStandingHoldSeconds,
		FrontierClassificationToString(LatestReport.FrontierClassification),
		RecommendedActionToString(LatestReport.RecommendedAction),
		*JsonEscape(LatestReport.RecommendedExpansionName),
		*JsonEscape(LatestReport.DominantTruthfulBlocker),
		LatestReport.bAnyTimedOutBeforeRootOn ? TEXT("true") : TEXT("false"),
		LatestReport.bAnyTimedOutBeforeNoCouplingProof ? TEXT("true") : TEXT("false"),
		LatestReport.bHasBestCandidate ? *BuildTrialJson(LatestReport.BestCandidate) : TEXT("null"),
		LatestReport.bHasBestNearPass ? *BuildTrialJson(LatestReport.BestNearPass) : TEXT("null"),
		LatestReport.bHasFurthestProgressedFailure ? *BuildTrialJson(LatestReport.FurthestProgressedFailure) : TEXT("null"),
		*OverallBlockersJson,
		*PresetSummariesJson);
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

#else

bool UPhysAnimPhase1AutoCalibSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const { return false; }
void UPhysAnimPhase1AutoCalibSubsystem::Tick(float DeltaTime) {}
TStatId UPhysAnimPhase1AutoCalibSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UPhysAnimPhase1AutoCalibSubsystem, STATGROUP_Tickables); }
void UPhysAnimPhase1AutoCalibSubsystem::Deinitialize() {}
bool UPhysAnimPhase1AutoCalibSubsystem::StartPhase1AutoCalib(const FPhase1AutoCalibRequest& Request, FString& OutError) { return false; }
void UPhysAnimPhase1AutoCalibSubsystem::StopPhase1AutoCalib(const FString& Reason) {}
void UPhysAnimPhase1AutoCalibSubsystem::BuildStageACandidates(const FPhase1AutoCalibRequest& Request, TArray<FPhase1AutoCalibParams>& OutCandidates) {}
void UPhysAnimPhase1AutoCalibSubsystem::BuildStageBRefinementCandidates(const TArray<FPhase1AutoCalibTrialResult>& StageAResults, const FPhase1AutoCalibRequest& Request, TArray<FPhase1AutoCalibParams>& OutCandidates) {}
void UPhysAnimPhase1AutoCalibSubsystem::BuildStageCReproCandidates(const TArray<FPhase1AutoCalibTrialResult>& StageBResults, const FPhase1AutoCalibRequest& Request, TArray<FPhase1AutoCalibParams>& OutCandidates) {}
bool UPhysAnimPhase1AutoCalibSubsystem::AreTrialResultsReproducible(const TArray<FPhase1AutoCalibTrialResult>& Trials, float Epsilon) { return false; }
void UPhysAnimPhase1AutoCalibSubsystem::FinalizeReportData(FPhase1AutoCalibReport& InOutReport, TArray<FPhase1AutoCalibTrialResult>* StageCTrials) {}

#endif
