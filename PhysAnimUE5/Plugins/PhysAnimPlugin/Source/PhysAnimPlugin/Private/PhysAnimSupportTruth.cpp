#include "PhysAnimSupportTruth.h"

namespace PhysAnimSupportTruth
{
	FPhysAnimSupportPatch ExtractPatchHull(const TArray<FPhysAnimSupportPoint2D>& Points)
	{
		FPhysAnimSupportPatch Result;
		if (Points.Num() == 0)
		{
			return Result;
		}

		Result.BodyName = Points[0].BodyName;
		Result.SupportSide = Points[0].SupportSide;

		for (const FPhysAnimSupportPoint2D& Point : Points)
		{
			if (Point.BodyName != Result.BodyName || Point.SupportSide != Result.SupportSide)
			{
				Result.bValidInput = false;
				Result.HullPointsCm.Reset();
				Result.PatchAreaCm2 = 0.0;
				return Result;
			}
		}

		// Simple stub for hull extraction (not the focus of current task)
		Result.HullPointsCm.SetNum(Points.Num());
		for (int32 i = 0; i < Points.Num(); ++i)
		{
			Result.HullPointsCm[i] = Points[i].PositionCm;
		}
		
		return Result;
	}

	FPhysAnimFrameHull BuildFrameHull(const TArray<FPhysAnimSupportPatch>& Patches)
	{
		FPhysAnimFrameHull Result;
		TSet<EPhysAnimSupportSide> ActiveSides;

		for (const FPhysAnimSupportPatch& Patch : Patches)
		{
			if (Patch.HullPointsCm.Num() > 0)
			{
				ActiveSides.Add(Patch.SupportSide);
				Result.HullPointsCm.Append(Patch.HullPointsCm);
			}
		}

		Result.ActiveSupportSideCount = ActiveSides.Num();
		return Result;
	}

	EPhysAnimSupportMode ClassifySupportMode(bool bLeftSupport, bool bRightSupport, double SupportGapTimerMs, double SupportGapMaxMs)
	{
		if (bLeftSupport && bRightSupport)
		{
			return EPhysAnimSupportMode::TwoFootStable;
		}
		if (bLeftSupport || bRightSupport)
		{
			return EPhysAnimSupportMode::SingleFootSurvival;
		}
		if (SupportGapTimerMs <= SupportGapMaxMs)
		{
			return EPhysAnimSupportMode::TransientRecovery;
		}
		return EPhysAnimSupportMode::Airborne;
	}

	FPhysAnimProxyAdjudicationResult AdjudicateProxy(const FPhysAnimProxyAdjudicationInput& Input)
	{
		FPhysAnimProxyAdjudicationResult Result;
		if (Input.ActiveSupportSideCount == 0)
		{
			return Result;
		}

		// Simplified logic
		bool bInside = Input.HullPointsCm.Num() >= 3; 
		Result.ProxyInsideHull = bInside;

		if (!bInside)
		{
			double PrevDuration = Input.PreviousProxyOutsideHullDurationMs.Get(0.0);
			Result.ProxyOutsideHullDurationMs = PrevDuration + Input.DeltaMs;
			if (Result.ProxyOutsideHullDurationMs.GetValue() > Input.ProxyDriftLimitMs)
			{
				Result.TerminalReason = EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion;
			}
		}
		else
		{
			Result.ProxyOutsideHullDurationMs = 0.0;
		}

		return Result;
	}

	FPhysAnimChurnResult CalculateChurnHz(const FPhysAnimChurnCalculationInput& Input)
	{
		FPhysAnimChurnResult Result;
		double WindowStart = Input.CurrentTimestampSec - Input.WindowSeconds;

		for (const FPhysAnimChurnEvent& Event : Input.HistoricalEvents)
		{
			if (Event.TimestampSec > WindowStart && Event.TimestampSec <= Input.CurrentTimestampSec)
			{
				Result.SupportChurnCount++;
			}
		}

		Result.SupportChurnHz = (Input.WindowSeconds > 0.0) ? (Result.SupportChurnCount / Input.WindowSeconds) : 0.0;
		return Result;
	}

	FPhysAnimSupportReportWindowResult ReduceSupportModeForReportWindow(const FPhysAnimSupportReportWindowInput& Input)
	{
		FPhysAnimSupportReportWindowResult Result;

		// LOGIC-14B: Array parity check
		if (Input.Modes.Num() != Input.DurationsMs.Num())
		{
			Result.SupportMode = EPhysAnimSupportMode::Airborne;
			Result.TotalWindowDurationMs = 0.0;
			Result.bValidInput = false;
			return Result;
		}

		// LOGIC-14A: Empty check
		if (Input.Modes.Num() == 0)
		{
			Result.SupportMode = EPhysAnimSupportMode::Airborne;
			Result.TotalWindowDurationMs = 0.0;
			Result.bValidInput = true;
			return Result;
		}

		TMap<EPhysAnimSupportMode, double> ModeDurations;
		double TotalDuration = 0.0;

		for (int32 i = 0; i < Input.Modes.Num(); ++i)
		{
			// LOGIC-14C: Negative duration clamping
			double Duration = FMath::Max(0.0, Input.DurationsMs[i]);
			ModeDurations.FindOrAdd(Input.Modes[i]) += Duration;
			TotalDuration += Duration;
		}

		Result.TotalWindowDurationMs = TotalDuration;

		// LOGIC-14A: Zero total duration check
		if (TotalDuration <= 0.0)
		{
			Result.SupportMode = EPhysAnimSupportMode::Airborne;
			return Result;
		}

		// Accumulation & Tie-break
		// severity tie-break: Airborne > TransientRecovery > SingleFootSurvival > TwoFootStable
		auto GetSeverity = [](EPhysAnimSupportMode Mode) -> int32
		{
			switch (Mode)
			{
				case EPhysAnimSupportMode::Airborne: return 4;
				case EPhysAnimSupportMode::TransientRecovery: return 3;
				case EPhysAnimSupportMode::SingleFootSurvival: return 2;
				case EPhysAnimSupportMode::TwoFootStable: return 1;
				default: return 0;
			}
		};

		EPhysAnimSupportMode DominantMode = EPhysAnimSupportMode::Airborne;
		double MaxDuration = -1.0;

		for (auto& Kvp : ModeDurations)
		{
			if (Kvp.Value > MaxDuration)
			{
				MaxDuration = Kvp.Value;
				DominantMode = Kvp.Key;
			}
			else if (FMath::IsNearlyEqual(Kvp.Value, MaxDuration))
			{
				if (GetSeverity(Kvp.Key) > GetSeverity(DominantMode))
				{
					DominantMode = Kvp.Key;
				}
			}
		}

		Result.SupportMode = DominantMode;
		return Result;
	}
}
