#include "PhysAnimRuntimeTerminationState.h"

namespace PhysAnimRuntimeTerminationState
{
	FPhysAnimRuntimeTerminationStateApplyResult ApplyTerminationCommand(const FPhysAnimRuntimeTerminationStateApplyInput& Input)
	{
		FPhysAnimRuntimeTerminationStateApplyResult Result;
		Result.State = Input.PreviousState;
		Result.State.LatestArtifact = Input.Command.Artifact;

		if (!Input.Command.bTerminate)
		{
			return Result;
		}

		if (Input.PreviousState.bTerminated)
		{
			Result.bIgnoredBecauseAlreadyTerminated = true;
			Result.State = Input.PreviousState;
			return Result;
		}

		Result.State.bTerminated = true;
		Result.State.TerminalReason = Input.Command.TerminalReason;
		Result.State.TerminalSubstepTimestamp = Input.Command.TerminalSubstepTimestamp;
		Result.State.bTerminalFrameArtifactCaptured = Input.Command.bCaptureTerminalArtifact;
		Result.State.bPolicyEvaluationEnabled = !Input.Command.bDisablePolicyEvaluation;
		Result.State.bBridgeOutputFrozen = Input.Command.bFreezeBridgeOutput;
		Result.State.bPhysicsFailStopRequested = Input.Command.bRequestPhysicsFailStop;
		Result.State.bMovementReclaimRequested = Input.Command.bRequestMovementReclaim;
		Result.State.TerminalArtifact = Input.Command.Artifact;
		Result.State.LatestArtifact = Input.Command.Artifact;

		Result.bAppliedTermination = true;
		return Result;
	}
}
