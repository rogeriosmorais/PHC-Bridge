#include "PhysAnimRuntimeTerminationPipeline.h"

namespace PhysAnimRuntimeTerminationPipeline
{
	FPhysAnimRuntimeTerminationPipelineResult EvaluateTerminationPipeline(const FPhysAnimRuntimeTerminationPipelineInput& Input)
	{
		FPhysAnimRuntimeTerminationPipelineResult Result;

		// 1. Evaluate Substep (Aggregation)
		Result.SubstepResult = PhysAnimRuntimeOrchestrator::EvaluateRuntimeSubstep(Input.SubstepInput);

		// 2. Build Command (Conversion)
		FPhysAnimRuntimeTerminationCommandInput CommandInput;
		CommandInput.SubstepResult = Result.SubstepResult;
		CommandInput.bEnableTerminationCommand = Input.bEnableTerminationCommand;
		CommandInput.bAllowMovementReclaimOnTermination = Input.bAllowMovementReclaimOnTermination;

		Result.Command = PhysAnimRuntimeTermination::BuildTerminationCommand(CommandInput);

		// 3. Apply to State (Transition)
		FPhysAnimRuntimeTerminationStateApplyInput StateInput;
		StateInput.PreviousState = Input.PreviousState;
		StateInput.Command = Result.Command;

		Result.StateApplyResult = PhysAnimRuntimeTerminationState::ApplyTerminationCommand(StateInput);

		return Result;
	}

	FPhysAnimRuntimeTerminationPipelineResult EvaluateProofFailureFailStopRouting(const FPhysAnimRuntimeProofFailureFailStopRoutingInput& Input)
	{
		FPhysAnimRuntimeTerminationPipelineResult Result;
		Result.SubstepResult.Artifact = Input.Artifact;
		Result.SubstepResult.Artifact.TerminalReason = Input.TerminalReason;
		Result.SubstepResult.Artifact.TerminalSubstepTimestamp = Input.TerminalSubstepTimestamp;
		Result.SubstepResult.Artifact.bTerminalFrameArtifactCaptured = true;
		Result.SubstepResult.bShouldTerminate = Input.TerminalReason != EPhysAnimTerminalReason::None;
		Result.SubstepResult.TerminalReason = Input.TerminalReason;
		Result.SubstepResult.TerminalSubstepTimestamp = Input.TerminalSubstepTimestamp;
		Result.SubstepResult.bTerminalFrameArtifactCaptured = true;

		FPhysAnimRuntimeTerminationCommandInput CommandInput;
		CommandInput.SubstepResult = Result.SubstepResult;
		CommandInput.bEnableTerminationCommand = Input.bEnableTerminationCommand;
		CommandInput.bAllowMovementReclaimOnTermination = Input.bAllowMovementReclaimOnTermination;

		Result.Command = PhysAnimRuntimeTermination::BuildTerminationCommand(CommandInput);

		FPhysAnimRuntimeTerminationStateApplyInput StateInput;
		StateInput.PreviousState = Input.PreviousState;
		StateInput.Command = Result.Command;
		Result.StateApplyResult = PhysAnimRuntimeTerminationState::ApplyTerminationCommand(StateInput);

		return Result;
	}
}
