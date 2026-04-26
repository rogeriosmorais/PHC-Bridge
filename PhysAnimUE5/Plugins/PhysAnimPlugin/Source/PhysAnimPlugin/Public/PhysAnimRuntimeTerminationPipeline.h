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

namespace PhysAnimRuntimeTerminationPipeline
{
	FPhysAnimRuntimeTerminationPipelineResult EvaluateTerminationPipeline(const FPhysAnimRuntimeTerminationPipelineInput& Input);
}
