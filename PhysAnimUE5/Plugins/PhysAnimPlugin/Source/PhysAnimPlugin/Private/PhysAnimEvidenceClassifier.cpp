#include "PhysAnimEvidenceClassifier.h"

namespace
{
	bool HasProofSignalDisagreement(const FPhysAnimEvidenceBaselineProofSignals& ProofSignals)
	{
		const TOptional<bool> Signals[] =
		{
			ProofSignals.TerminalProofJsonPassed,
			ProofSignals.LogPass,
			ProofSignals.ArtifactPass
		};

		TOptional<bool> FirstObservedValue;
		for (const TOptional<bool>& Signal : Signals)
		{
			if (!Signal.IsSet())
			{
				continue;
			}

			if (!FirstObservedValue.IsSet())
			{
				FirstObservedValue = Signal;
				continue;
			}

			if (FirstObservedValue.GetValue() != Signal.GetValue())
			{
				return true;
			}
		}

		return false;
	}

	bool HasAllSegmentsActive(const FPhysAnimEvidenceBaselineSegmentStates& SegmentStates)
	{
		return PhysAnimEvidenceClassifier::CountActiveSegments(SegmentStates) ==
			static_cast<int32>(EPhysAnimEvidenceBaselineSegment::Count);
	}

	bool HasCleanTruthSignals(const FPhysAnimEvidenceBaselineTruthFlags& TruthFlags)
	{
		return TruthFlags.bAssistanceTruthClean &&
			TruthFlags.bContinuityTruthClean &&
			TruthFlags.bSupportTruthClean &&
			TruthFlags.bSimulationTruthClean;
	}
}

namespace PhysAnimEvidenceClassifier
{
	EPhysAnimEvidenceBaselineSegmentState GetSegmentState(
		const FPhysAnimEvidenceBaselineSegmentStates& SegmentStates,
		const EPhysAnimEvidenceBaselineSegment Segment)
	{
		switch (Segment)
		{
		case EPhysAnimEvidenceBaselineSegment::PoseSearch:
			return SegmentStates.PoseSearch;
		case EPhysAnimEvidenceBaselineSegment::PhcPolicy:
			return SegmentStates.PhcPolicy;
		case EPhysAnimEvidenceBaselineSegment::PhysicsControl:
			return SegmentStates.PhysicsControl;
		case EPhysAnimEvidenceBaselineSegment::Chaos:
			return SegmentStates.Chaos;
		case EPhysAnimEvidenceBaselineSegment::RendererFacingMotion:
			return SegmentStates.RendererFacingMotion;
		case EPhysAnimEvidenceBaselineSegment::Count:
		default:
			return EPhysAnimEvidenceBaselineSegmentState::NotReached;
		}
	}

	bool IsSegmentActive(
		const FPhysAnimEvidenceBaselineSegmentStates& SegmentStates,
		const EPhysAnimEvidenceBaselineSegment Segment)
	{
		return GetSegmentState(SegmentStates, Segment) == EPhysAnimEvidenceBaselineSegmentState::Active;
	}

	int32 CountActiveSegments(const FPhysAnimEvidenceBaselineSegmentStates& SegmentStates)
	{
		int32 ActiveCount = 0;
		for (int32 Index = 0; Index < static_cast<int32>(EPhysAnimEvidenceBaselineSegment::Count); ++Index)
		{
			const EPhysAnimEvidenceBaselineSegment Segment = static_cast<EPhysAnimEvidenceBaselineSegment>(Index);
			ActiveCount += IsSegmentActive(SegmentStates, Segment) ? 1 : 0;
		}

		return ActiveCount;
	}

	FPhysAnimEvidenceBaselineResult Classify(const FPhysAnimEvidenceBaselineInput& Input)
	{
		FPhysAnimEvidenceBaselineResult Result;
		Result.Segments = Input.Segments;
		Result.TruthFlags = Input.TruthFlags;
		Result.ProofSignals = Input.ProofSignals;
		Result.bHoldThresholdSatisfied = Input.bHoldThresholdSatisfied;

		const bool bHasProofDisagreement =
			Input.TruthFlags.bArtifactLogContradiction ||
			HasProofSignalDisagreement(Input.ProofSignals);

		if (bHasProofDisagreement)
		{
			Result.Verdict = EPhysAnimEvidenceBaselineVerdict::Contradictory;
			return Result;
		}

		const bool bTerminalProofKnown = Input.ProofSignals.TerminalProofJsonPassed.IsSet();
		const bool bTerminalProofPassed = bTerminalProofKnown && Input.ProofSignals.TerminalProofJsonPassed.GetValue();
		const bool bHasMissingEvidence = Input.TruthFlags.bMissingEvidence || !bTerminalProofKnown;

		if (bHasMissingEvidence)
		{
			Result.Verdict = EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence;
			return Result;
		}

		if (Input.TruthFlags.bTerminalFailure || !bTerminalProofPassed || !HasCleanTruthSignals(Input.TruthFlags))
		{
			Result.Verdict = EPhysAnimEvidenceBaselineVerdict::Blocked;
			return Result;
		}

		if (HasAllSegmentsActive(Input.Segments) && Input.bHoldThresholdSatisfied)
		{
			Result.Verdict = EPhysAnimEvidenceBaselineVerdict::DiagnosticAllSignalsObserved;
			return Result;
		}

		Result.Verdict = EPhysAnimEvidenceBaselineVerdict::Diagnostic;
		return Result;
	}
}
