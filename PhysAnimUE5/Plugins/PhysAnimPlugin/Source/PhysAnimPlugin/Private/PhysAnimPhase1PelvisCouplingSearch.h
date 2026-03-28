#pragma once

#include "PhysAnimComponent.h"

#if !UE_BUILD_SHIPPING

enum class EPhase1PelvisCouplingSearchFamily : uint8
{
	Unknown,
	DirectSeed,
	SpineBiasedDirectBlend,
	PairBlend,
	SpineConstraintInterpolation,
	WorstThighInterpolation,
	FocusedDelta,
	TiltSpineRescue,
	ForensicSpineRescue,
	CoupledTradeControl,
	PairBlendFrontierFollowThrough
};

struct FPhase1PelvisCouplingSearchConfig
{
	EPhase1AutoCalibStrategyPreset SourcePreset = EPhase1AutoCalibStrategyPreset::CurrentDefault;
	EPhase1AutoCalibStrategyPreset SeedFamilyPreset = EPhase1AutoCalibStrategyPreset::CurrentDefault;
	float SpineInterpolationAlpha = 0.10f;
	float WorstThighInterpolationAlpha = 0.05f;
	float FocusedDeltaScale = 1.0f;
	float UprightnessWeightScale = 1.0f;
	float ClampStrengthScale = 1.0f;
	float PelvisPitchBiasDeg = 0.0f;
	float PelvisRollBiasDeg = 0.0f;
	bool bEnableSpineRescueSearch = true;
	bool bEnableSpineBiasedDirectBlendSeeds = true;
	bool bEnablePairBlendSeeds = true;
	bool bEnableConstraintInterpolationSweep = true;
	bool bEnableWorstThighInterpolationSweep = true;
	bool bEnableFocusedDeltaRefinement = true;
	bool bEnableMarginSweepRefinement = true;
	bool bEnableForensicSearch = true;
	bool bEnableFinalMergePolicy = true;
	bool bEnableCoupledTradeControlPass = false;
	float CoupledTradeSpineGainWeight = 1.0f;
	float CoupledTradeThighGainWeight = 1.0f;
	float CoupledTradeMaxPairedRegressionDeg = 0.35f;
	bool bEnablePairBlendFrontierFollowThroughPass = false;
	bool bEnablePairBlendFrontierInterpolationPass = false;
	float PairBlendFrontierWeightPerturbationRadius = 0.10f;
	float PairBlendFrontierPitchDeltaRadiusDeg = 0.25f;
	float PairBlendFrontierRollDeltaRadiusDeg = 0.25f;
	float PairBlendFrontierBlockerPriorityGainWeight = 1.50f;
	float PairBlendFrontierSecondaryGainWeight = 1.00f;
	float PairBlendFrontierMaxPairedRegressionDeg = 0.25f;
};

struct FPhase1PelvisCouplingSearchResult
{
	FString WinningSearchFamily;
	FString WinningSearchSource;
	TArray<FString> ExecutedSearchFamilies;
	bool bCoupledTradeControlWon = false;
};

const TCHAR* Phase1AutoCalibStrategyPresetToString(EPhase1AutoCalibStrategyPreset Preset);
const TCHAR* Phase1PelvisCouplingSearchFamilyToString(EPhase1PelvisCouplingSearchFamily Family);
FPhase1PelvisCouplingSearchConfig BuildPhase1PelvisCouplingSearchConfig(const TOptional<FPhase1AutoCalibParams>& ActiveParams);
EPhase1PelvisCouplingSearchFamily ClassifyPhase1PelvisCouplingSearchFamily(const FString& Source);
void BuildPhase1PelvisCouplingExecutedFamilies(
	const FPhase1PelvisCouplingSearchConfig& Config,
	const FPhase1PelvisCouplingRotationForensics& Forensics,
	const FString& WinningSource,
	TArray<FString>& OutFamilies);
bool ShouldAcceptPhase1CoupledTradeControlCandidate(
	float CurrentLeftThighAngularErrorDeg,
	float CurrentRightThighAngularErrorDeg,
	float CurrentSpineAngularErrorDeg,
	float CandidateLeftThighAngularErrorDeg,
	float CandidateRightThighAngularErrorDeg,
	float CandidateSpineAngularErrorDeg,
	float SpineGainWeight,
	float ThighGainWeight,
	float MaxPairedRegressionDeg);
bool ShouldAcceptPhase1PairBlendFrontierCandidate(
	float CurrentLeftThighAngularErrorDeg,
	float CurrentRightThighAngularErrorDeg,
	float CurrentSpineAngularErrorDeg,
	float CandidateLeftThighAngularErrorDeg,
	float CandidateRightThighAngularErrorDeg,
	float CandidateSpineAngularErrorDeg,
	bool bPrioritizeSpineBlocker,
	float BlockerPriorityGainWeight,
	float SecondaryGainWeight,
	float MaxPairedRegressionDeg);

#endif
