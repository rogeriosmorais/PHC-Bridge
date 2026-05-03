#include "PhysAnimRuntimeOrchestrator.h"

namespace
{
	void AddCandidateIfTerminal(
		TArray<FPhysAnimFailureCandidate>& Candidates,
		EPhysAnimTerminalReason Reason,
		int64 TerminalSubstepTimestamp)
	{
		if (Reason == EPhysAnimTerminalReason::None)
		{
			return;
		}

		FPhysAnimFailureCandidate Candidate;
		Candidate.TerminalReason = Reason;
		Candidate.TerminalSubstepTimestamp = TerminalSubstepTimestamp;
		Candidates.Add(Candidate);
	}
}

namespace PhysAnimRuntimeOrchestrator
{
	FPhysAnimRuntimeSubstepResult EvaluateRuntimeSubstep(const FPhysAnimRuntimeSubstepInput& Input)
	{
		const int64 CurrentSubstepTimestamp = Input.Values.TerminalSubstepTimestamp;

		FPhysAnimRunArtifactSnapshotInput ArtifactInput;
		ArtifactInput.Plant = Input.Plant;
		ArtifactInput.Capsule = Input.Capsule;
		ArtifactInput.Continuity = Input.Continuity;
		ArtifactInput.Support = Input.SupportObservation.Validation;
		ArtifactInput.Authority = Input.Authority;
		ArtifactInput.MovementReclaim = Input.MovementReclaim;
		ArtifactInput.ShellHelper = Input.ShellHelper;
		ArtifactInput.ControllerStability = Input.ControllerStability;
		ArtifactInput.Values = Input.Values;
		ArtifactInput.FailureCandidates = Input.AdditionalFailureCandidates;

		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.Plant.TerminalReason, CurrentSubstepTimestamp);
		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.Capsule.TerminalReason, CurrentSubstepTimestamp);
		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.Continuity.TerminalReason, CurrentSubstepTimestamp);
		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.SupportObservation.Validation.TerminalReason, CurrentSubstepTimestamp);
		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.Authority.TerminalReason, CurrentSubstepTimestamp);
		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.MovementReclaim.TerminalReason, CurrentSubstepTimestamp);
		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.ShellHelper.TerminalReason, CurrentSubstepTimestamp);
		AddCandidateIfTerminal(ArtifactInput.FailureCandidates, Input.ControllerStability.TerminalReason, CurrentSubstepTimestamp);

		FPhysAnimRuntimeSubstepResult Result;
		Result.Artifact = PhysAnimValidators::BuildRunArtifactSnapshot(ArtifactInput);
		Result.TerminalReason = Result.Artifact.TerminalReason;
		Result.TerminalSubstepTimestamp = Result.Artifact.TerminalSubstepTimestamp;
		Result.bShouldTerminate = Input.bEnableTermination && Result.TerminalReason != EPhysAnimTerminalReason::None;
		Result.bTerminalFrameArtifactCaptured = Result.bShouldTerminate;

		Result.Artifact.bTerminalFrameArtifactCaptured = Result.bTerminalFrameArtifactCaptured;

		return Result;
	}
}
