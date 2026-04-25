#pragma once

#include "PhysAnimRuntimeTermination.h"

struct FPhysAnimRuntimeTerminationState
{
	bool bTerminated = false;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	int64 TerminalSubstepTimestamp = 0;
	bool bTerminalFrameArtifactCaptured = false;
	bool bPolicyEvaluationEnabled = true;
	bool bBridgeOutputFrozen = false;
	bool bPhysicsFailStopRequested = false;
	bool bMovementReclaimRequested = false;
	FPhysAnimRunArtifactSnapshot TerminalArtifact;
	FPhysAnimRunArtifactSnapshot LatestArtifact;
};

struct FPhysAnimRuntimeTerminationStateApplyInput
{
	FPhysAnimRuntimeTerminationState PreviousState;
	FPhysAnimRuntimeTerminationCommand Command;
};

struct FPhysAnimRuntimeTerminationStateApplyResult
{
	FPhysAnimRuntimeTerminationState State;
	bool bAppliedTermination = false;
	bool bIgnoredBecauseAlreadyTerminated = false;
};

namespace PhysAnimRuntimeTerminationState
{
	FPhysAnimRuntimeTerminationStateApplyResult ApplyTerminationCommand(const FPhysAnimRuntimeTerminationStateApplyInput& Input);
}
