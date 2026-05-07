#include "PhysAnimFailureArbitration.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimFailureCandidate MakeCandidate(EPhysAnimTerminalReason Reason, int64 SubstepTimestamp)
	{
		FPhysAnimFailureCandidate Candidate;
		Candidate.TerminalReason = Reason;
		Candidate.TerminalSubstepTimestamp = SubstepTimestamp;
		return Candidate;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFailureArbitrationBugFixTest,
		"PhysAnim.FailureArbitration.BugFix",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFailureArbitrationBugFixTest::RunTest(const FString& Parameters)
	{
		{
			// BUGFIX-01: Kinetic gate should win over generic simulation loss.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationKineticGateActive, 100));
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationContinuousSimulationLost, 100));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestTrue(TEXT("BUGFIX-01 emits a terminal reason"), Result.bHasTerminalReason);
			TestEqual(
				TEXT("BUGFIX-01 kinetic gate wins over generic simulation loss"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationKineticGateActive));
		}

		{
			// BUGFIX-02: Physics not started should be a high-priority failure.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationPhysicsNotStarted, 200));
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationContinuousSimulationLost, 200));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestTrue(TEXT("BUGFIX-02 emits a terminal reason"), Result.bHasTerminalReason);
			TestEqual(
				TEXT("BUGFIX-02 physics not started wins over generic simulation loss"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsNotStarted));
		}

		return true;
	}
}
