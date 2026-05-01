#pragma once

#include "PhysAnimRuntimeTerminationState.h"

struct FPhysAnimRuntimeTerminationPipelineInput
{
	FPhysAnimRuntimeTerminationState PreviousState;
	FPhysAnimRuntimeSubstepInput SubstepInput;
	bool bEnableTerminationCommand = true;
	bool bAllowMovementReclaimOnTermination = true;
};

struct FPhysAnimRuntimeTerminationPipelineResult
{
	FPhysAnimRuntimeSubstepResult SubstepResult;
	FPhysAnimRuntimeTerminationCommand Command;
	FPhysAnimRuntimeTerminationStateApplyResult StateApplyResult;
};

struct FPhysAnimRuntimeProofFailureFailStopRoutingInput
{
	FPhysAnimRuntimeTerminationState PreviousState;
	FPhysAnimRunArtifactSnapshot Artifact;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	int64 TerminalSubstepTimestamp = 0;
	bool bEnableTerminationCommand = true;
	bool bAllowMovementReclaimOnTermination = true;
};

namespace PhysAnimRuntimeTerminationPipeline
{
	FPhysAnimRuntimeTerminationPipelineResult EvaluateTerminationPipeline(const FPhysAnimRuntimeTerminationPipelineInput& Input);
	FPhysAnimRuntimeTerminationPipelineResult EvaluateProofFailureFailStopRouting(const FPhysAnimRuntimeProofFailureFailStopRoutingInput& Input);
}
