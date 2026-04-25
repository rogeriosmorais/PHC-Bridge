#pragma once

#include "PhysAnimTruthTypes.h"

struct FPhysAnimFailureCandidate
{
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	int64 TerminalSubstepTimestamp = 0;
};

struct FPhysAnimFailureArbitrationResult
{
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	TArray<EPhysAnimTerminalReason> CoTerminalReasons;
	int64 TerminalSubstepTimestamp = 0;
	bool bHasTerminalReason = false;
};

namespace PhysAnimFailureArbitration
{
	int32 GetTerminalReasonRank(EPhysAnimTerminalReason Reason);
	FPhysAnimFailureArbitrationResult ArbitrateFailure(const TArray<FPhysAnimFailureCandidate>& Candidates);
}
