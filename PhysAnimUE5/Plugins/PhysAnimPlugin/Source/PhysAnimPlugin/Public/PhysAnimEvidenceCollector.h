#pragma once

#include "CoreMinimal.h"
#include "PhysAnimEvidenceClassifier.h"
#include "PhysAnimEvidenceSummary.h"
#include "PhysAnimValidators.h"

struct FPhysAnimEvidenceCollectorLogSignal
{
	bool bFound = false;
	bool bPass = false;
	FString LogPath;
	FString LogContents;
};

struct FPhysAnimEvidenceCollectorTerminalArtifactResult
{
	bool bFound = false;
	bool bExplicitPassProvided = false;
	bool bExplicitPass = false;
	FString JsonPath;
	FPhysAnimRunArtifactSnapshot TerminalArtifact;
};

struct FPhysAnimEvidenceCollectorEvidenceSummaryResult
{
	bool bFound = false;
	FString JsonPath;
	FPhysAnimEvidenceSummary Summary;
};

struct FPhysAnimEvidenceCollectorInput
{
	FString TerminalArtifactDirectory;
	FString EvidenceSummaryDirectory;
	FString LogDirectory;
};

struct FPhysAnimEvidenceCollectorResult
{
	FPhysAnimEvidenceCollectorTerminalArtifactResult TerminalArtifact;
	FPhysAnimEvidenceCollectorEvidenceSummaryResult EvidenceSummary;
	FPhysAnimEvidenceCollectorLogSignal LogSignal;
	FPhysAnimEvidenceBaselineProofSignals ProofSignals;
	FPhysAnimEvidenceBaselineInput ClassificationInput;
	FPhysAnimEvidenceBaselineResult ClassificationResult;
	FString Report;
};

namespace PhysAnimEvidenceCollector
{
	FPhysAnimEvidenceCollectorResult Collect(const FPhysAnimEvidenceCollectorInput& Input);
}
