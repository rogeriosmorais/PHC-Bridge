#pragma once

#include "CoreMinimal.h"

enum class EPhysAnimEvidenceBaselineSegment : uint8
{
	PoseSearch,
	PhcPolicy,
	PhysicsControl,
	Chaos,
	RendererFacingMotion,
	Count
};

enum class EPhysAnimEvidenceBaselineSegmentState : uint8
{
	NotReached,
	ReachedButInactive,
	Active
};

enum class EPhysAnimEvidenceBaselineVerdict : uint8
{
	DiagnosticAllSignalsObserved,
	Diagnostic,
	Blocked,
	Contradictory,
	InsufficientEvidence
};

struct FPhysAnimEvidenceBaselineSegmentStates
{
	EPhysAnimEvidenceBaselineSegmentState PoseSearch = EPhysAnimEvidenceBaselineSegmentState::NotReached;
	EPhysAnimEvidenceBaselineSegmentState PhcPolicy = EPhysAnimEvidenceBaselineSegmentState::NotReached;
	EPhysAnimEvidenceBaselineSegmentState PhysicsControl = EPhysAnimEvidenceBaselineSegmentState::NotReached;
	EPhysAnimEvidenceBaselineSegmentState Chaos = EPhysAnimEvidenceBaselineSegmentState::NotReached;
	EPhysAnimEvidenceBaselineSegmentState RendererFacingMotion = EPhysAnimEvidenceBaselineSegmentState::NotReached;
};

struct FPhysAnimEvidenceBaselineTruthFlags
{
	bool bAssistanceTruthClean = true;
	bool bContinuityTruthClean = true;
	bool bSupportTruthClean = true;
	bool bSimulationTruthClean = true;
	bool bTerminalFailure = false;
	bool bArtifactLogContradiction = false;
	bool bMissingEvidence = false;
};

struct FPhysAnimEvidenceBaselineProofSignals
{
	TOptional<bool> TerminalProofJsonPassed;
	TOptional<bool> LogPass;
	TOptional<bool> ArtifactPass;
};

struct FPhysAnimEvidenceBaselineInput
{
	FPhysAnimEvidenceBaselineSegmentStates Segments;
	FPhysAnimEvidenceBaselineTruthFlags TruthFlags;
	FPhysAnimEvidenceBaselineProofSignals ProofSignals;
	bool bHoldThresholdSatisfied = false;
};

struct FPhysAnimEvidenceBaselineResult
{
	FPhysAnimEvidenceBaselineSegmentStates Segments;
	FPhysAnimEvidenceBaselineTruthFlags TruthFlags;
	FPhysAnimEvidenceBaselineProofSignals ProofSignals;
	bool bHoldThresholdSatisfied = false;
	EPhysAnimEvidenceBaselineVerdict Verdict = EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence;
};

namespace PhysAnimEvidenceClassifier
{
	EPhysAnimEvidenceBaselineSegmentState GetSegmentState(
		const FPhysAnimEvidenceBaselineSegmentStates& SegmentStates,
		EPhysAnimEvidenceBaselineSegment Segment);

	bool IsSegmentActive(
		const FPhysAnimEvidenceBaselineSegmentStates& SegmentStates,
		EPhysAnimEvidenceBaselineSegment Segment);

	int32 CountActiveSegments(const FPhysAnimEvidenceBaselineSegmentStates& SegmentStates);

	FPhysAnimEvidenceBaselineResult Classify(const FPhysAnimEvidenceBaselineInput& Input);
}
