#pragma once

#include "PhysAnimRuntimeAdapter.h"

struct FPhysAnimRuntimeSubstepInput
{
	FPhysAnimPlantContractValidationResult Plant;
	FPhysAnimCapsuleContractValidationResult Capsule;
	FPhysAnimContinuityValidationResult Continuity;
	FPhysAnimSupportObservationResult SupportObservation;
	FPhysAnimAuthorityValidationResult Authority;
	FPhysAnimMovementReclaimValidationResult MovementReclaim;
	FPhysAnimShellHelperValidationResult ShellHelper;
	FPhysAnimControllerStabilityValidationResult ControllerStability;
	FPhysAnimRunArtifactSnapshot Values;
	TArray<FPhysAnimFailureCandidate> AdditionalFailureCandidates;
	bool bEnableTermination = true;
};

struct FPhysAnimRuntimeSubstepResult
{
	FPhysAnimRunArtifactSnapshot Artifact;
	bool bShouldTerminate = false;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	int64 TerminalSubstepTimestamp = 0;
	bool bTerminalFrameArtifactCaptured = false;
};

namespace PhysAnimRuntimeOrchestrator
{
	FPhysAnimRuntimeSubstepResult EvaluateRuntimeSubstep(const FPhysAnimRuntimeSubstepInput& Input);
}
