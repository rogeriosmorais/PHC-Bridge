#include "PhysAnimValidators.h"

namespace PhysAnimValidators
{
	FPhysAnimContinuityValidationResult ValidateContinuity(const FPhysAnimContinuitySnapshot& Snapshot)
	{
		constexpr double PelvisSleepLimitMs = 100.0;

		FPhysAnimContinuityValidationResult Result;
		Result.TopologyChangeCount = Snapshot.TopologyChangeCount;
		Result.bContinuityBookkeepingMismatch = Snapshot.bContinuityBookkeepingMismatch;
		Result.PelvisSleepDurationMs = Snapshot.PelvisSleepDurationMs;

		if (Snapshot.TopologyChangeCount > 0 || !Snapshot.bAllCriticalBodiesValid)
		{
			Result.bPhysicalContinuityValidatorPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationTopologyChange;
			return Result;
		}

		if (!Snapshot.bAllCriticalBodiesSimulating || Snapshot.PelvisSleepDurationMs > PelvisSleepLimitMs)
		{
			Result.bPhysicalContinuityValidatorPassed = false;
			Result.TerminalReason = EPhysAnimTerminalReason::ActivationContinuousSimulationLost;
			return Result;
		}

		return Result;
	}
}
