#include "PhysAnimEvidenceClassifier.h"
#include "Misc/AutomationTest.h"

namespace
{
	using namespace PhysAnimEvidenceClassifier;

	FPhysAnimEvidenceBaselineInput MakeAuthoritativeSuccessInput()
	{
		FPhysAnimEvidenceBaselineInput Input;
		Input.Segments.PoseSearch = EPhysAnimEvidenceBaselineSegmentState::Active;
		Input.Segments.PhcPolicy = EPhysAnimEvidenceBaselineSegmentState::Active;
		Input.Segments.PhysicsControl = EPhysAnimEvidenceBaselineSegmentState::Active;
		Input.Segments.Chaos = EPhysAnimEvidenceBaselineSegmentState::Active;
		Input.Segments.RendererFacingMotion = EPhysAnimEvidenceBaselineSegmentState::Active;
		Input.TruthFlags.bAssistanceTruthClean = true;
		Input.TruthFlags.bContinuityTruthClean = true;
		Input.TruthFlags.bSupportTruthClean = true;
		Input.TruthFlags.bSimulationTruthClean = true;
		Input.TruthFlags.bTerminalFailure = false;
		Input.TruthFlags.bArtifactLogContradiction = false;
		Input.TruthFlags.bMissingEvidence = false;
		Input.ProofSignals.TerminalProofJsonPassed = true;
		Input.ProofSignals.LogPass = true;
		Input.ProofSignals.ArtifactPass = true;
		Input.bHoldThresholdSatisfied = true;
		return Input;
	}

	void SetAllSegments(
		FPhysAnimEvidenceBaselineSegmentStates& Segments,
		const EPhysAnimEvidenceBaselineSegmentState State)
	{
		Segments.PoseSearch = State;
		Segments.PhcPolicy = State;
		Segments.PhysicsControl = State;
		Segments.Chaos = State;
		Segments.RendererFacingMotion = State;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceClassifierSegmentStateContractTest,
		"PhysAnim.EvidenceClassifier.SegmentStateContract",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceClassifierSegmentStateContractTest::RunTest(const FString& Parameters)
	{
		const EPhysAnimEvidenceBaselineSegmentState States[] =
		{
			EPhysAnimEvidenceBaselineSegmentState::NotReached,
			EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive,
			EPhysAnimEvidenceBaselineSegmentState::Active
		};

		for (const EPhysAnimEvidenceBaselineSegmentState State : States)
		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			SetAllSegments(Input.Segments, State);

			for (int32 Index = 0; Index < static_cast<int32>(EPhysAnimEvidenceBaselineSegment::Count); ++Index)
			{
				const EPhysAnimEvidenceBaselineSegment Segment = static_cast<EPhysAnimEvidenceBaselineSegment>(Index);
				const EPhysAnimEvidenceBaselineSegmentState ObservedState = GetSegmentState(Input.Segments, Segment);

				TestEqual(TEXT("Segment state is preserved for every architecture segment"), static_cast<uint8>(ObservedState), static_cast<uint8>(State));
				TestEqual(TEXT("Segment active predicate matches the stored state"), IsSegmentActive(Input.Segments, Segment), State == EPhysAnimEvidenceBaselineSegmentState::Active);
			}

			const int32 ExpectedActiveCount = State == EPhysAnimEvidenceBaselineSegmentState::Active ? 5 : 0;
			TestEqual(TEXT("Active segment count is deterministic across all five segments"), CountActiveSegments(Input.Segments), ExpectedActiveCount);
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceClassifierVerdictRulesTest,
		"PhysAnim.EvidenceClassifier.VerdictRules",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceClassifierVerdictRulesTest::RunTest(const FString& Parameters)
	{
		{
			const FPhysAnimEvidenceBaselineResult Result = Classify(MakeAuthoritativeSuccessInput());

			TestEqual(TEXT("All-active clean evidence reaches product-success candidate"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::DiagnosticAllSignalsObserved));
		}

		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			Input.Segments.RendererFacingMotion = EPhysAnimEvidenceBaselineSegmentState::ReachedButInactive;

			const FPhysAnimEvidenceBaselineResult Result = Classify(Input);

			TestEqual(TEXT("Partial segment success stays below product-success candidate"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::Diagnostic));
		}

		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			Input.TruthFlags.bSimulationTruthClean = false;

			const FPhysAnimEvidenceBaselineResult Result = Classify(Input);

			TestEqual(TEXT("Dirty simulation truth blocks the verdict"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::Blocked));
		}

		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			Input.TruthFlags.bMissingEvidence = true;
			Input.ProofSignals.TerminalProofJsonPassed.Reset();

			const FPhysAnimEvidenceBaselineResult Result = Classify(Input);

			TestEqual(TEXT("Missing evidence stays insufficient"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::InsufficientEvidence));
		}

		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			Input.ProofSignals.ArtifactPass = false;

			const FPhysAnimEvidenceBaselineResult Result = Classify(Input);

			TestEqual(TEXT("Log pass with artifact disagreement is contradictory"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::Contradictory));
		}

		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			Input.TruthFlags.bArtifactLogContradiction = true;

			const FPhysAnimEvidenceBaselineResult Result = Classify(Input);

			TestEqual(TEXT("Explicit artifact/log contradiction flag is contradictory"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::Contradictory));
		}

		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimEvidenceClassifierTerminalProofAuthorityTest,
		"PhysAnim.EvidenceClassifier.TerminalProofAuthority",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimEvidenceClassifierTerminalProofAuthorityTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			Input.ProofSignals.TerminalProofJsonPassed = false;
			Input.ProofSignals.LogPass = false;
			Input.ProofSignals.ArtifactPass = false;

			const FPhysAnimEvidenceBaselineResult Result = Classify(Input);

			TestEqual(TEXT("Terminal proof JSON false prevents product success"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::Blocked));
		}

		{
			FPhysAnimEvidenceBaselineInput Input = MakeAuthoritativeSuccessInput();
			Input.TruthFlags.bTerminalFailure = true;
			Input.ProofSignals.TerminalProofJsonPassed = false;
			Input.ProofSignals.LogPass = true;
			Input.ProofSignals.ArtifactPass = false;

			const FPhysAnimEvidenceBaselineResult Result = Classify(Input);

			TestEqual(TEXT("Terminal failure with pass/fail disagreement is contradictory"), static_cast<uint8>(Result.Verdict), static_cast<uint8>(EPhysAnimEvidenceBaselineVerdict::Contradictory));
		}

		return true;
	}
}
