#pragma once

#include "PhysAnimRuntimeOrchestrator.h"

struct FPhysAnimRuntimeTerminationCommand
{
	bool bTerminate = false;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	int64 TerminalSubstepTimestamp = 0;
	bool bCaptureTerminalArtifact = false;
	bool bDisablePolicyEvaluation = false;
	bool bFreezeBridgeOutput = false;
	bool bRequestPhysicsFailStop = false;
	bool bRequestMovementReclaim = false;
	FPhysAnimRunArtifactSnapshot Artifact;
};

struct FPhysAnimRuntimeTerminationCommandInput
{
	FPhysAnimRuntimeSubstepResult SubstepResult;
	bool bEnableTerminationCommand = true;
	bool bAllowMovementReclaimOnTermination = true;
};

namespace PhysAnimRuntimeTermination
{
	FPhysAnimRuntimeTerminationCommand BuildTerminationCommand(const FPhysAnimRuntimeTerminationCommandInput& Input);
}
