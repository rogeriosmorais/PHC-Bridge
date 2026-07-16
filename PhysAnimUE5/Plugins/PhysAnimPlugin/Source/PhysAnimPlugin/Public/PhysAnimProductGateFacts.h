#pragma once

#include "CoreMinimal.h"

struct FPhysAnimStandingWindowAccumulator
{
	double ContinuousSeconds = 0.0;
	int32 ExitCount = 0;
	int32 SampleCount = 0;
	double MaxDeltaSeconds = 0.0;
	bool bWasStanding = false;

	void Advance(const bool bIsBalanceActiveStanding, const double DeltaTimeSeconds)
	{
		if (!bIsBalanceActiveStanding)
		{
			if (bWasStanding)
			{
				++ExitCount;
			}
			ContinuousSeconds = 0.0;
			SampleCount = 0;
			MaxDeltaSeconds = 0.0;
			bWasStanding = false;
			return;
		}

		const double PositiveDeltaSeconds =
			FMath::IsFinite(DeltaTimeSeconds) ? FMath::Max(0.0, DeltaTimeSeconds) : 0.0;
		ContinuousSeconds += PositiveDeltaSeconds;
		if (PositiveDeltaSeconds > 0.0)
		{
			++SampleCount;
			MaxDeltaSeconds = FMath::Max(MaxDeltaSeconds, PositiveDeltaSeconds);
		}
		bWasStanding = true;
	}

	bool HasValidCadence(
		const int32 MinimumSampleCount,
		const double MaximumAllowedDeltaSeconds) const
	{
		return MinimumSampleCount >= 2 &&
			SampleCount >= MinimumSampleCount &&
			FMath::IsFinite(MaxDeltaSeconds) &&
			FMath::IsFinite(MaximumAllowedDeltaSeconds) &&
			MaxDeltaSeconds > 0.0 &&
			MaximumAllowedDeltaSeconds > 0.0 &&
			MaxDeltaSeconds <= MaximumAllowedDeltaSeconds;
	}
};

struct FPhysAnimBodyContinuityAccumulator
{
	int32 SampleCount = 0;
	int32 MinSimulatingBodyCount = 0;
	int32 CriticalBodyValidAllFramesMask = 0;
	int32 CriticalBodySimulatingAllFramesMask = 0;
	int32 SupportBodyValidAllFramesMask = 0;
	int32 SupportBodySimulatingAllFramesMask = 0;

	void RecordSample(
		const int32 SimulatingBodyCount,
		const int32 CriticalBodyValidMask,
		const int32 CriticalBodySimulatingMask,
		const int32 SupportBodyValidMask,
		const int32 SupportBodySimulatingMask)
	{
		if (SampleCount == 0)
		{
			MinSimulatingBodyCount = SimulatingBodyCount;
			CriticalBodyValidAllFramesMask = CriticalBodyValidMask;
			CriticalBodySimulatingAllFramesMask = CriticalBodySimulatingMask;
			SupportBodyValidAllFramesMask = SupportBodyValidMask;
			SupportBodySimulatingAllFramesMask = SupportBodySimulatingMask;
		}
		else
		{
			MinSimulatingBodyCount = FMath::Min(MinSimulatingBodyCount, SimulatingBodyCount);
			CriticalBodyValidAllFramesMask &= CriticalBodyValidMask;
			CriticalBodySimulatingAllFramesMask &= CriticalBodySimulatingMask;
			SupportBodyValidAllFramesMask &= SupportBodyValidMask;
			SupportBodySimulatingAllFramesMask &= SupportBodySimulatingMask;
		}

		++SampleCount;
	}
};
