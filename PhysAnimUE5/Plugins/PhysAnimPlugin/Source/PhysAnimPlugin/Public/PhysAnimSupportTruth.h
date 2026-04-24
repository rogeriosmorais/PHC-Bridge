#pragma once

#include "CoreMinimal.h"
#include "PhysAnimTruthTypes.h"

/** Pure data types for support truth extraction. */

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
	double ProxyDriftLimitMs = 100.0;
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
	double WindowSeconds = 1.0;
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
	/** Extract a convex hull from raw support points. */
	PHYSANIMPLUGIN_API FPhysAnimSupportPatch ExtractPatchHull(const TArray<FPhysAnimSupportPoint2D>& Points);

	/** Build the frame-level support hull from per-body patches. Sets ActiveSupportSideCount. */
	PHYSANIMPLUGIN_API FPhysAnimFrameHull BuildFrameHull(const TArray<FPhysAnimSupportPatch>& Patches);

	/** Classify the support mode based on side-support states and gap timing. */
	PHYSANIMPLUGIN_API EPhysAnimSupportMode ClassifySupportMode(bool bLeftSupport, bool bRightSupport, double SupportGapTimerMs, double SupportGapMaxMs);

	/** Adjudicate proxy drift against the frame hull. Returns EPhysAnimTerminalReason::None if valid. */
	PHYSANIMPLUGIN_API FPhysAnimProxyAdjudicationResult AdjudicateProxy(const FPhysAnimProxyAdjudicationInput& Input);

	/** Calculate transition frequency (Churn Hz) over a rolling window. */
	PHYSANIMPLUGIN_API FPhysAnimChurnResult CalculateChurnHz(const FPhysAnimChurnCalculationInput& Input);

	/** Reduce 30Hz modes into the dominant mode for the artifact report window. */
	PHYSANIMPLUGIN_API FPhysAnimSupportReportWindowResult ReduceSupportModeForReportWindow(const FPhysAnimSupportReportWindowInput& Input);
}
