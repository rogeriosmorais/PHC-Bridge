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

	bool ContainsReason(const TArray<EPhysAnimTerminalReason>& Reasons, EPhysAnimTerminalReason Expected)
	{
		return Reasons.Contains(Expected);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimFailureArbitrationTest,
		"PhysAnim.FailureArbitration",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimFailureArbitrationTest::RunTest(const FString& Parameters)
	{
		{
			// ARBIT-01: Simultaneous Plant + Support uses rank precedence.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationSupportFailure, 100));
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation, 100));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestTrue(TEXT("ARBIT-01 emits a terminal reason"), Result.bHasTerminalReason);
			TestEqual(
				TEXT("ARBIT-01 plant wins simultaneous support failure"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
			TestEqual(TEXT("ARBIT-01 timestamp preserved"), Result.TerminalSubstepTimestamp, static_cast<int64>(100));
			TestTrue(
				TEXT("ARBIT-01 support is co-terminal"),
				ContainsReason(Result.CoTerminalReasons, EPhysAnimTerminalReason::ActivationSupportFailure));
		}

		{
			// ARBIT-02: Simultaneous Support + Proxy uses rank precedence.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion, 200));
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationSupportFailure, 200));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestEqual(
				TEXT("ARBIT-02 support failure wins simultaneous proxy drift"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationSupportFailure));
			TestTrue(
				TEXT("ARBIT-02 proxy is co-terminal"),
				ContainsReason(Result.CoTerminalReasons, EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion));
		}

		{
			// ARBIT-03: Earlier lower-rank reason wins by temporal precedence.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation, 301));
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationAuthorityConflict, 300));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestEqual(
				TEXT("ARBIT-03 earlier authority conflict wins over later plant breach"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
			TestEqual(TEXT("ARBIT-03 earliest timestamp wins"), Result.TerminalSubstepTimestamp, static_cast<int64>(300));
			TestEqual(TEXT("ARBIT-03 later plant breach is not co-terminal"), Result.CoTerminalReasons.Num(), 0);
		}

		{
			// ARBIT-04: Multiple failures in one frame produce winner plus co-terminal reasons.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationAuthorityConflict, 400));
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationShellHelperViolation, 400));
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::ActivationMovementReclaim, 400));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestEqual(
				TEXT("ARBIT-04 movement reclaim wins same-frame authority failures"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationMovementReclaim));
			TestEqual(TEXT("ARBIT-04 records two co-terminal reasons"), Result.CoTerminalReasons.Num(), 2);
			TestTrue(
				TEXT("ARBIT-04 shell helper is co-terminal"),
				ContainsReason(Result.CoTerminalReasons, EPhysAnimTerminalReason::ActivationShellHelperViolation));
			TestTrue(
				TEXT("ARBIT-04 authority conflict is co-terminal"),
				ContainsReason(Result.CoTerminalReasons, EPhysAnimTerminalReason::ActivationAuthorityConflict));
		}

		{
			// ARBIT-05: No failures returns None.
			TArray<FPhysAnimFailureCandidate> Candidates;
			Candidates.Add(MakeCandidate(EPhysAnimTerminalReason::None, 500));

			const FPhysAnimFailureArbitrationResult Result = PhysAnimFailureArbitration::ArbitrateFailure(Candidates);

			TestFalse(TEXT("ARBIT-05 has no terminal reason"), Result.bHasTerminalReason);
			TestEqual(
				TEXT("ARBIT-05 terminal reason is None"),
				static_cast<uint8>(Result.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
			TestEqual(TEXT("ARBIT-05 has no co-terminal reasons"), Result.CoTerminalReasons.Num(), 0);
		}

		return true;
	}
}
