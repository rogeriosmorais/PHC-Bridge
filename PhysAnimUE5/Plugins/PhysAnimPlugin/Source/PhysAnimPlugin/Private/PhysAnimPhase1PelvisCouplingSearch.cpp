#include "PhysAnimPhase1PelvisCouplingSearch.h"

#if !UE_BUILD_SHIPPING

namespace
{
	void AddUniqueFamily(TArray<FString>& Families, const TCHAR* Family)
	{
		if (!Family || FCString::Strlen(Family) == 0)
		{
			return;
		}

		const FString FamilyString(Family);
		if (!Families.Contains(FamilyString))
		{
			Families.Add(FamilyString);
		}
	}

	float ResolveWorstThighError(const float LeftThighAngularErrorDeg, const float RightThighAngularErrorDeg)
	{
		return FMath::Max(LeftThighAngularErrorDeg, RightThighAngularErrorDeg);
	}
}

const TCHAR* Phase1AutoCalibStrategyPresetToString(const EPhase1AutoCalibStrategyPreset Preset)
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
	case EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily:
		return TEXT("CoupledTradeControlFamily");
	case EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough:
		return TEXT("PairBlendFrontierFollowThrough");
	case EPhase1AutoCalibStrategyPreset::CurrentDefault:
	default:
		return TEXT("CurrentDefault");
	}
}

const TCHAR* Phase1PelvisCouplingSearchFamilyToString(const EPhase1PelvisCouplingSearchFamily Family)
{
	switch (Family)
	{
	case EPhase1PelvisCouplingSearchFamily::DirectSeed:
		return TEXT("direct_seed");
	case EPhase1PelvisCouplingSearchFamily::SpineBiasedDirectBlend:
		return TEXT("spine_biased_direct_blend");
	case EPhase1PelvisCouplingSearchFamily::PairBlend:
		return TEXT("pair_blend");
	case EPhase1PelvisCouplingSearchFamily::SpineConstraintInterpolation:
		return TEXT("spine_constraint_interpolation");
	case EPhase1PelvisCouplingSearchFamily::WorstThighInterpolation:
		return TEXT("worst_thigh_interpolation");
	case EPhase1PelvisCouplingSearchFamily::FocusedDelta:
		return TEXT("focused_delta");
	case EPhase1PelvisCouplingSearchFamily::TiltSpineRescue:
		return TEXT("tilt_spine_rescue");
	case EPhase1PelvisCouplingSearchFamily::ForensicSpineRescue:
		return TEXT("forensic_spine_rescue");
	case EPhase1PelvisCouplingSearchFamily::CoupledTradeControl:
		return TEXT("coupled_trade_control");
	case EPhase1PelvisCouplingSearchFamily::PairBlendFrontierFollowThrough:
		return TEXT("pair_blend_frontier_follow_through");
	case EPhase1PelvisCouplingSearchFamily::Unknown:
	default:
		return TEXT("unknown");
	}
}

FPhase1PelvisCouplingSearchConfig BuildPhase1PelvisCouplingSearchConfig(const TOptional<FPhase1AutoCalibParams>& ActiveParams)
{
	FPhase1PelvisCouplingSearchConfig Config;
	if (ActiveParams.IsSet())
	{
		const FPhase1AutoCalibParams& Params = ActiveParams.GetValue();
		Config.SourcePreset = Params.SourcePreset;
		Config.SeedFamilyPreset = Params.SeedFamilyPreset;
		Config.SpineInterpolationAlpha = FMath::Clamp(Params.SpineInterpolationAlpha, 0.0f, 1.0f);
		Config.WorstThighInterpolationAlpha = FMath::Clamp(Params.WorstThighInterpolationAlpha, 0.0f, 1.0f);
		Config.FocusedDeltaScale = FMath::Clamp(Params.FocusedDeltaScale, 0.25f, 4.0f);
		Config.UprightnessWeightScale = Params.UprightnessWeightScale;
		Config.ClampStrengthScale = FMath::Clamp(Params.ClampStrengthScale, 0.25f, 4.0f);
		Config.PelvisPitchBiasDeg = Params.PelvisPitchBiasDeg;
		Config.PelvisRollBiasDeg = Params.PelvisRollBiasDeg;
	}

	switch (Config.SourcePreset)
	{
	case EPhase1AutoCalibStrategyPreset::RescueOnly:
		Config.bEnableSpineBiasedDirectBlendSeeds = false;
		Config.bEnablePairBlendSeeds = false;
		Config.bEnableConstraintInterpolationSweep = false;
		Config.bEnableWorstThighInterpolationSweep = false;
		break;
	case EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily:
		Config.bEnableCoupledTradeControlPass = true;
		Config.CoupledTradeSpineGainWeight = 1.25f;
		Config.CoupledTradeThighGainWeight = 1.00f;
		Config.CoupledTradeMaxPairedRegressionDeg = 0.25f;
		break;
	case EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough:
		Config.bEnableSpineBiasedDirectBlendSeeds = false;
		Config.bEnableConstraintInterpolationSweep = false;
		Config.bEnableWorstThighInterpolationSweep = false;
		Config.bEnableForensicSearch = false;
		Config.bEnableCoupledTradeControlPass = false;
		Config.bEnablePairBlendFrontierFollowThroughPass = true;
		Config.bEnablePairBlendFrontierInterpolationPass = true;
		Config.PairBlendFrontierWeightPerturbationRadius = 0.10f;
		Config.PairBlendFrontierPitchDeltaRadiusDeg = 0.25f;
		Config.PairBlendFrontierRollDeltaRadiusDeg = 0.25f;
		Config.PairBlendFrontierBlockerPriorityGainWeight = 1.50f;
		Config.PairBlendFrontierSecondaryGainWeight = 1.00f;
		Config.PairBlendFrontierMaxPairedRegressionDeg = 0.25f;
		break;
	case EPhase1AutoCalibStrategyPreset::CurrentDefault:
	case EPhase1AutoCalibStrategyPreset::SpineBiased:
	case EPhase1AutoCalibStrategyPreset::WorstThighBiased:
	case EPhase1AutoCalibStrategyPreset::BalancedCoupled:
	case EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh:
	default:
		break;
	}

	if (Config.SeedFamilyPreset == EPhase1AutoCalibStrategyPreset::CoupledTradeControlFamily)
	{
		Config.SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::SpineThenWorstThigh;
	}
	if (Config.SeedFamilyPreset == EPhase1AutoCalibStrategyPreset::PairBlendFrontierFollowThrough)
	{
		Config.SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::RescueOnly;
	}

	return Config;
}

EPhase1PelvisCouplingSearchFamily ClassifyPhase1PelvisCouplingSearchFamily(const FString& Source)
{
	if (Source.Contains(TEXT("pair_frontier")))
	{
		return EPhase1PelvisCouplingSearchFamily::PairBlendFrontierFollowThrough;
	}
	if (Source.Contains(TEXT("coupled_trade")))
	{
		return EPhase1PelvisCouplingSearchFamily::CoupledTradeControl;
	}
	if (Source.Contains(TEXT("tilt_spine_rescue")) || Source.Contains(TEXT("applied_spine_rescue")))
	{
		return EPhase1PelvisCouplingSearchFamily::TiltSpineRescue;
	}
	if (Source.Contains(TEXT("forensic_spine_rescue")))
	{
		return EPhase1PelvisCouplingSearchFamily::ForensicSpineRescue;
	}
	if (Source.Contains(TEXT("spine_interp")))
	{
		return EPhase1PelvisCouplingSearchFamily::SpineConstraintInterpolation;
	}
	if (Source.Contains(TEXT("worst_thigh")))
	{
		return EPhase1PelvisCouplingSearchFamily::WorstThighInterpolation;
	}
	if (Source.Contains(TEXT("focus_delta")) || Source.Contains(TEXT("focused_delta")))
	{
		return EPhase1PelvisCouplingSearchFamily::FocusedDelta;
	}
	if (Source.Contains(TEXT("blend_spine_bias")))
	{
		return EPhase1PelvisCouplingSearchFamily::SpineBiasedDirectBlend;
	}
	if (Source.Contains(TEXT("blend_")))
	{
		return EPhase1PelvisCouplingSearchFamily::PairBlend;
	}
	if (Source.Contains(TEXT("animated")) || Source.Contains(TEXT("constraint_")) || Source.Contains(TEXT("autocalib_")) || Source.Contains(TEXT("live_pelvis_rotation")))
	{
		return EPhase1PelvisCouplingSearchFamily::DirectSeed;
	}
	return EPhase1PelvisCouplingSearchFamily::Unknown;
}

void BuildPhase1PelvisCouplingExecutedFamilies(
	const FPhase1PelvisCouplingSearchConfig& Config,
	const FPhase1PelvisCouplingRotationForensics& Forensics,
	const FString& WinningSource,
	TArray<FString>& OutFamilies)
{
	OutFamilies.Reset();
	AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::DirectSeed));
	if (Config.bEnableSpineBiasedDirectBlendSeeds)
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::SpineBiasedDirectBlend));
	}
	if (Config.bEnablePairBlendSeeds)
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::PairBlend));
	}
	if (Config.bEnableConstraintInterpolationSweep)
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::SpineConstraintInterpolation));
	}
	if (Config.bEnableWorstThighInterpolationSweep)
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::WorstThighInterpolation));
	}
	if (Config.bEnableFocusedDeltaRefinement)
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::FocusedDelta));
	}
	if (Forensics.bTriggeredTiltSpineRescuePath)
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::TiltSpineRescue));
	}
	if (Forensics.bTriggeredForensicSpineRescuePath)
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::ForensicSpineRescue));
	}
	if (Config.bEnableCoupledTradeControlPass || WinningSource.Contains(TEXT("coupled_trade")))
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::CoupledTradeControl));
	}
	if (Config.bEnablePairBlendFrontierFollowThroughPass || WinningSource.Contains(TEXT("pair_frontier")))
	{
		AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily::PairBlendFrontierFollowThrough));
	}

	const EPhase1PelvisCouplingSearchFamily WinningFamily = ClassifyPhase1PelvisCouplingSearchFamily(WinningSource);
	AddUniqueFamily(OutFamilies, Phase1PelvisCouplingSearchFamilyToString(WinningFamily));
}

bool ShouldAcceptPhase1CoupledTradeControlCandidate(
	const float CurrentLeftThighAngularErrorDeg,
	const float CurrentRightThighAngularErrorDeg,
	const float CurrentSpineAngularErrorDeg,
	const float CandidateLeftThighAngularErrorDeg,
	const float CandidateRightThighAngularErrorDeg,
	const float CandidateSpineAngularErrorDeg,
	const float SpineGainWeight,
	const float ThighGainWeight,
	const float MaxPairedRegressionDeg)
{
	const float CurrentWorstThigh = ResolveWorstThighError(CurrentLeftThighAngularErrorDeg, CurrentRightThighAngularErrorDeg);
	const float CandidateWorstThigh = ResolveWorstThighError(CandidateLeftThighAngularErrorDeg, CandidateRightThighAngularErrorDeg);
	const float SpineImprovementDeg = CurrentSpineAngularErrorDeg - CandidateSpineAngularErrorDeg;
	const float ThighImprovementDeg = CurrentWorstThigh - CandidateWorstThigh;
	const float SpineRegressionDeg = FMath::Max(0.0f, CandidateSpineAngularErrorDeg - CurrentSpineAngularErrorDeg);
	const float ThighRegressionDeg = FMath::Max(0.0f, CandidateWorstThigh - CurrentWorstThigh);

	if (SpineRegressionDeg > MaxPairedRegressionDeg || ThighRegressionDeg > MaxPairedRegressionDeg)
	{
		return false;
	}

	const float WeightedGain =
		FMath::Max(0.0f, SpineImprovementDeg) * SpineGainWeight +
		FMath::Max(0.0f, ThighImprovementDeg) * ThighGainWeight;
	const float WeightedRegression =
		SpineRegressionDeg * SpineGainWeight +
		ThighRegressionDeg * ThighGainWeight;
	return WeightedGain > WeightedRegression + KINDA_SMALL_NUMBER;
}

bool ShouldAcceptPhase1PairBlendFrontierCandidate(
	const float CurrentLeftThighAngularErrorDeg,
	const float CurrentRightThighAngularErrorDeg,
	const float CurrentSpineAngularErrorDeg,
	const float CandidateLeftThighAngularErrorDeg,
	const float CandidateRightThighAngularErrorDeg,
	const float CandidateSpineAngularErrorDeg,
	const bool bPrioritizeSpineBlocker,
	const float BlockerPriorityGainWeight,
	const float SecondaryGainWeight,
	const float MaxPairedRegressionDeg)
{
	const float CurrentWorstThigh = ResolveWorstThighError(CurrentLeftThighAngularErrorDeg, CurrentRightThighAngularErrorDeg);
	const float CandidateWorstThigh = ResolveWorstThighError(CandidateLeftThighAngularErrorDeg, CandidateRightThighAngularErrorDeg);
	const float SpineImprovementDeg = CurrentSpineAngularErrorDeg - CandidateSpineAngularErrorDeg;
	const float ThighImprovementDeg = CurrentWorstThigh - CandidateWorstThigh;
	const float SpineRegressionDeg = FMath::Max(0.0f, CandidateSpineAngularErrorDeg - CurrentSpineAngularErrorDeg);
	const float ThighRegressionDeg = FMath::Max(0.0f, CandidateWorstThigh - CurrentWorstThigh);

	if (SpineRegressionDeg > MaxPairedRegressionDeg || ThighRegressionDeg > MaxPairedRegressionDeg)
	{
		return false;
	}

	const float PrimaryImprovementDeg = bPrioritizeSpineBlocker ? SpineImprovementDeg : ThighImprovementDeg;
	const float PrimaryRegressionDeg = bPrioritizeSpineBlocker ? SpineRegressionDeg : ThighRegressionDeg;
	const float SecondaryImprovementDeg = bPrioritizeSpineBlocker ? ThighImprovementDeg : SpineImprovementDeg;
	const float SecondaryRegressionDeg = bPrioritizeSpineBlocker ? ThighRegressionDeg : SpineRegressionDeg;

	if (PrimaryImprovementDeg <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float WeightedGain =
		(FMath::Max(0.0f, PrimaryImprovementDeg) * BlockerPriorityGainWeight) +
		(FMath::Max(0.0f, SecondaryImprovementDeg) * SecondaryGainWeight);
	const float WeightedRegression =
		(PrimaryRegressionDeg * BlockerPriorityGainWeight) +
		(SecondaryRegressionDeg * SecondaryGainWeight);
	return WeightedGain > WeightedRegression + KINDA_SMALL_NUMBER;
}

#endif
