#pragma once

#include "CoreMinimal.h"
#include "PhysAnimEvidenceClassifier.h"
#include "PhysAnimTruthTypes.h"

struct FPhysAnimEvidenceSummaryCommandMetadata
{
	FString CommandName;
	FString CommandLine;
	FString WorkingDirectory;
};

struct FPhysAnimEvidenceSummarySegmentMetrics
{
	int32 SampleCount = 0;
	double Confidence = 0.0;
	double Score = 0.0;
	FString SelectedSourceIdentity;
	double SelectedSourceTime = 0.0;
};

struct FPhysAnimEvidenceSummarySegment
{
	FString SegmentName;
	EPhysAnimEvidenceBaselineSegmentState State = EPhysAnimEvidenceBaselineSegmentState::NotReached;
	FPhysAnimEvidenceSummarySegmentMetrics Metrics;
	TArray<FString> MissingRequiredFields;
	TArray<FString> DiagnosticNotes;
	TArray<FString> SourceProvenance;
};

struct FPhysAnimEvidenceSummary
{
	int32 SchemaVersion = 1;
	FString AttemptUuid;
	FString TestName;
	FString MapName;
	double Timestamp = 0.0;
	TOptional<FPhysAnimEvidenceSummaryCommandMetadata> CommandMetadata;
	TArray<FPhysAnimEvidenceSummarySegment> Segments;
	FPhysAnimEvidenceBaselineTruthFlags QualityFlags;
	EPhysAnimTerminalReason TerminalReason = EPhysAnimTerminalReason::None;
	EPhysAnimEvidenceBaselineVerdict StrictVerdict = EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence;
	FString TerminalArtifactPath;
};

struct FPhysAnimEvidenceSummaryWriteInput
{
	FPhysAnimEvidenceSummary Summary;
	FPhysAnimEvidenceBaselineResult ClassifierResult;
	FString OutputPathOverride;
};

struct FPhysAnimEvidenceSummaryWriteResult
{
	bool bJsonWritten = false;
	bool bEvidenceCaptureFailure = false;
	FString JsonPath;
};

namespace PhysAnimEvidenceSummary
{
	FString BuildEvidenceSummaryJsonPath(const FString& AttemptUuid);

	FString SerializeToJsonString(const FPhysAnimEvidenceSummary& Summary);
	bool DeserializeFromJsonString(const FString& JsonString, FPhysAnimEvidenceSummary& OutSummary);
	FPhysAnimEvidenceSummaryWriteResult WriteEvidenceSummaryJson(const FPhysAnimEvidenceSummaryWriteInput& Input);
}
