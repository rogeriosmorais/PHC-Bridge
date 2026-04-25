#pragma once

#include "PhysAnimTruthTypes.h"

struct FPhysAnimContinuitySnapshot
{
	int32 TopologyChangeCount = 0;
	bool bAllCriticalBodiesValid = true;
	bool bAllCriticalBodiesSimulating = true;
	double PelvisSleepDurationMs = 0.0;
	bool bContinuityBookkeepingMismatch = false;
};

struct FPhysAnimContinuityValidationResult
{
	int32 TopologyChangeCount = 0;
	bool bContinuityBookkeepingMismatch = false;
	double PelvisSleepDurationMs = 0.0;
	bool bPhysicalContinuityValidatorPassed = true;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
};

namespace PhysAnimValidators
{
}
