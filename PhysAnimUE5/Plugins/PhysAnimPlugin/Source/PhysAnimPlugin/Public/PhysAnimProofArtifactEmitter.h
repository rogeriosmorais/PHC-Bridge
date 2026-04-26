#pragma once

#include "CoreMinimal.h"
#include "PhysAnimRuntimeTerminationPipeline.h"

struct FPhysAnimProofArtifactEmitInput
{
	FString AttemptUuid;
	double StandingSeconds = 0.0;
	int32 RuntimeHitCount = 0;
	int32 MappedSupportHitCount = 0;
	FPhysAnimRuntimeTerminationPipelineResult PipelineResult;
};

struct FPhysAnimProofArtifactEmitResult
{
	bool bJsonWritten = false;
	FString JsonPath;
};

namespace PhysAnimProofArtifactEmitter
{
	FString ToTerminalReasonString(EPhysAnimTerminalReason Reason);
	FString ToSupportModeString(EPhysAnimSupportMode Mode);

	void LogAttemptStart(const FString& AttemptUuid);

	void LogRuntimeEvidence(
		const FString& AttemptUuid,
		int32 RuntimeHitCount,
		int32 MappedSupportHitCount,
		const FPhysAnimRunArtifactSnapshot& Artifact);

	void LogStandingProgress(
		const FString& AttemptUuid,
		double StandingSeconds,
		EPhysAnimTerminalReason TerminalReason,
		int32 RuntimeHitCount,
		int32 MappedSupportHitCount,
		const FPhysAnimRunArtifactSnapshot& Artifact);

	FPhysAnimProofArtifactEmitResult EmitTerminalArtifactAndWriteJson(const FPhysAnimProofArtifactEmitInput& Input);

	void LogAttemptResult(
		const FString& AttemptUuid,
		bool bPassed,
		double StandingSeconds,
		EPhysAnimTerminalReason TerminalReason);

	FString BuildTerminalArtifactJsonPath(const FString& AttemptUuid);
	bool WriteTerminalArtifactJson(const FString& OutputPath, const FPhysAnimProofArtifactEmitInput& Input);
}
