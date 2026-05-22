#include "PhysAnimFailureArbitration.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimFailureCandidate MakeBugFixCandidate(EPhysAnimTerminalReason Reason, int64 SubstepTimestamp)
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
			// BUGFIX-01: Simulation loss is preserved.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeBugFixCandidate(EPhysAnimTerminalReason::ActivationContinuousSimulationLost, 100));
			Candidates.Add(MakeBugFixCandidate(EPhysAnimTerminalReason::ActivationContinuousSimulationLost, 100));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestTrue(TEXT("BUGFIX-01 emits a terminal reason"), Result.bHasTerminalReason);
			TestEqual(
				TEXT("BUGFIX-01 simulation loss is preserved"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		{
			// BUGFIX-02: High-priority simulation loss is preserved.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeBugFixCandidate(EPhysAnimTerminalReason::ActivationContinuousSimulationLost, 200));
			Candidates.Add(MakeBugFixCandidate(EPhysAnimTerminalReason::ActivationContinuousSimulationLost, 200));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestTrue(TEXT("BUGFIX-02 emits a terminal reason"), Result.bHasTerminalReason);
			TestEqual(
				TEXT("BUGFIX-02 simulation loss is preserved"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationContinuousSimulationLost));
		}

		return true;
	}
}
