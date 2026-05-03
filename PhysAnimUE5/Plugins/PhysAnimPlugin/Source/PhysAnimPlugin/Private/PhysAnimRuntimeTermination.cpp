#include "PhysAnimRuntimeTermination.h"

namespace PhysAnimRuntimeTermination
{
	FPhysAnimRuntimeTerminationCommand BuildTerminationCommand(const FPhysAnimRuntimeTerminationCommandInput& Input)
	{
		FPhysAnimRuntimeTerminationCommand Command;
		Command.Artifact = Input.SubstepResult.Artifact;
		Command.TerminalReason = Input.SubstepResult.TerminalReason;
		Command.TerminalSubstepTimestamp = Input.SubstepResult.TerminalSubstepTimestamp;

		Command.bTerminate =
			Input.bEnableTerminationCommand &&
			Input.SubstepResult.bShouldTerminate &&
			Input.SubstepResult.TerminalReason != EPhysAnimTerminalReason::None;

		Command.bCaptureTerminalArtifact =
			Command.bTerminate &&
			Input.SubstepResult.bTerminalFrameArtifactCaptured;

		Command.bDisablePolicyEvaluation = Command.bTerminate;
		Command.bFreezeBridgeOutput = Command.bTerminate;
		Command.bRequestPhysicsFailStop = Command.bTerminate;
		Command.bRequestMovementReclaim =
			Command.bTerminate &&
			Input.bAllowMovementReclaimOnTermination;

		Command.Artifact.bTerminalFrameArtifactCaptured = Command.bCaptureTerminalArtifact;

		return Command;
	}
}
