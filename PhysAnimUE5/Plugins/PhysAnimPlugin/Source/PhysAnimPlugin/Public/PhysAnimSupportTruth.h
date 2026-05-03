#pragma once

#include "PhysAnimTruthTypes.h"

struct FPhysAnimSupportPoint2D
{
	FVector2D PositionCm = FVector2D::ZeroVector;
	FName BodyName = NAME_None;
	EPhysAnimSupportSide SupportSide = EPhysAnimSupportSide::Left;
};

struct FPhysAnimSupportPatch
{
	FName BodyName = NAME_None;
	EPhysAnimSupportSide SupportSide = EPhysAnimSupportSide::Left;
	TArray<FVector2D> HullPointsCm;
	double PatchAreaCm2 = 0.0;
	bool bValidInput = true;
};

struct FPhysAnimFrameHull
{
	TArray<FVector2D> HullPointsCm;
	double SupportHullAreaCm2 = 0.0;
	int32 ActiveSupportSideCount = 0;
};

struct FPhysAnimProxyAdjudicationInput
{
	FVector2D ProxyPositionCm = FVector2D::ZeroVector;
	TArray<FVector2D> HullPointsCm;
	int32 ActiveSupportSideCount = 0;
	TOptional<double> PreviousProxyOutsideHullDurationMs;
	double DeltaMs = 0.0;
	double ProxyDriftLimitMs = 0.0;
};

struct FPhysAnimProxyAdjudicationResult
{
	TOptional<bool> ProxyInsideHull;
	TOptional<double> ProxyOutsideHullDurationMs;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

struct FPhysAnimChurnEvent
{
	double TimestampSec = 0.0;
	EPhysAnimSupportSide SupportSide = EPhysAnimSupportSide::Left;
	bool bNewSupportState = false;
};

struct FPhysAnimChurnResult
{
	int32 SupportChurnCount = 0;
	double SupportChurnHz = 0.0;
};

struct FPhysAnimChurnCalculationInput
{
	double CurrentTimestampSec = 0.0;
	double WindowSeconds = 0.0;
	TArray<FPhysAnimChurnEvent> HistoricalEvents;
};

struct FPhysAnimSupportReportWindowInput
{
	TArray<EPhysAnimSupportMode> Modes;
	TArray<double> DurationsMs;
};

struct FPhysAnimSupportReportWindowResult
{
	EPhysAnimSupportMode SupportMode = EPhysAnimSupportMode::Airborne;
	double TotalWindowDurationMs = 0.0;
	bool bValidInput = true;
};

namespace PhysAnimSupportTruth
{
	FPhysAnimSupportPatch ExtractPatchHull(const TArray<FPhysAnimSupportPoint2D>& Points);
	FPhysAnimFrameHull BuildFrameHull(const TArray<FPhysAnimSupportPatch>& Patches);
	EPhysAnimSupportMode ClassifySupportMode(bool bLeftSupport, bool bRightSupport, double SupportGapTimerMs, double SupportGapMaxMs);
	FPhysAnimProxyAdjudicationResult AdjudicateProxy(const FPhysAnimProxyAdjudicationInput& Input);
	FPhysAnimChurnResult CalculateChurnHz(const FPhysAnimChurnCalculationInput& Input);
	FPhysAnimSupportReportWindowResult ReduceSupportModeForReportWindow(const FPhysAnimSupportReportWindowInput& Input);
}
