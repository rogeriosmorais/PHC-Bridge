#include "PhysAnimValidators.h"
#include "Misc/AutomationTest.h"

namespace
{
	FPhysAnimFailureCandidate MakeArtifactCandidate(EPhysAnimTerminalReason Reason, int64 SubstepTimestamp)
	{
		FPhysAnimFailureCandidate Candidate;
		Candidate.TerminalReason = Reason;
		Candidate.TerminalSubstepTimestamp = SubstepTimestamp;
		return Candidate;
	}

	bool ContainsArtifactReason(const TArray<EPhysAnimTerminalReason>& Reasons, EPhysAnimTerminalReason Expected)
	{
		return Reasons.Contains(Expected);
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(
		FPhysAnimValidatorsArtifactArbitrationTest,
		"PhysAnim.Validators.ArtifactArbitration",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

	bool FPhysAnimValidatorsArtifactArbitrationTest::RunTest(const FString& Parameters)
	{
		{
			FPhysAnimRunArtifactSnapshotInput Input;
			Input.Values.TerminalSubstepTimestamp = 999;
			Input.FailureCandidates.Add(MakeArtifactCandidate(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation, 20));
			Input.FailureCandidates.Add(MakeArtifactCandidate(EPhysAnimTerminalReason::ActivationAuthorityConflict, 10));

			const FPhysAnimRunArtifactSnapshot Snapshot = PhysAnimValidators::BuildRunArtifactSnapshot(Input);

			TestEqual(
				TEXT("Artifact arbitration uses temporal precedence over rank precedence"),
				static_cast<uint8>(Snapshot.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationAuthorityConflict));
			TestEqual(TEXT("Artifact arbitration preserves winning timestamp"), Snapshot.TerminalSubstepTimestamp, static_cast<int64>(10));
			TestEqual(TEXT("Later non-simultaneous failure is not co-terminal"), Snapshot.CoTerminalReasons.Num(), 0);
		}

		{
			FPhysAnimRunArtifactSnapshotInput Input;
			Input.FailureCandidates.Add(MakeArtifactCandidate(EPhysAnimTerminalReason::ActivationAuthorityConflict, 30));
			Input.FailureCandidates.Add(MakeArtifactCandidate(EPhysAnimTerminalReason::ActivationCapsuleContractViolation, 30));
			Input.FailureCandidates.Add(MakeArtifactCandidate(EPhysAnimTerminalReason::ActivationShellHelperViolation, 30));

			const FPhysAnimRunArtifactSnapshot Snapshot = PhysAnimValidators::BuildRunArtifactSnapshot(Input);

			TestEqual(
				TEXT("Artifact arbitration uses rank precedence for simultaneous failures"),
				static_cast<uint8>(Snapshot.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationCapsuleContractViolation));
			TestEqual(TEXT("Artifact arbitration preserves simultaneous timestamp"), Snapshot.TerminalSubstepTimestamp, static_cast<int64>(30));
			TestEqual(TEXT("Artifact arbitration records two co-terminal reasons"), Snapshot.CoTerminalReasons.Num(), 2);
			TestTrue(
				TEXT("Artifact arbitration records authority conflict as co-terminal"),
				ContainsArtifactReason(Snapshot.CoTerminalReasons, EPhysAnimTerminalReason::ActivationAuthorityConflict));
			TestTrue(
				TEXT("Artifact arbitration records shell helper as co-terminal"),
				ContainsArtifactReason(Snapshot.CoTerminalReasons, EPhysAnimTerminalReason::ActivationShellHelperViolation));
		}

		{
			FPhysAnimRunArtifactSnapshotInput Input;
			Input.Values.TerminalSubstepTimestamp = 42;

			const FPhysAnimRunArtifactSnapshot Snapshot = PhysAnimValidators::BuildRunArtifactSnapshot(Input);

			TestEqual(
				TEXT("Artifact arbitration returns None when no explicit candidates or validator failures exist"),
				static_cast<uint8>(Snapshot.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::None));
			TestEqual(TEXT("Artifact arbitration has no co-terminal reasons for clean input"), Snapshot.CoTerminalReasons.Num(), 0);
			TestEqual(TEXT("Clean artifact preserves input timestamp"), Snapshot.TerminalSubstepTimestamp, static_cast<int64>(42));
		}

		{
			FPhysAnimRunArtifactSnapshotInput Input;
			Input.Values.TerminalSubstepTimestamp = 77;
			Input.Plant.TerminalReason = EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation;
			Input.Authority.TerminalReason = EPhysAnimTerminalReason::ActivationAuthorityConflict;

			const FPhysAnimRunArtifactSnapshot Snapshot = PhysAnimValidators::BuildRunArtifactSnapshot(Input);

			TestEqual(
				TEXT("Artifact arbitration fallback candidates use canonical same-timestamp rank precedence"),
				static_cast<uint8>(Snapshot.TerminalReason),
				static_cast<uint8>(EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation));
			TestEqual(TEXT("Fallback arbitration preserves fallback timestamp"), Snapshot.TerminalSubstepTimestamp, static_cast<int64>(77));
			TestTrue(
				TEXT("Fallback arbitration records lower-rank reason as co-terminal"),
				ContainsArtifactReason(Snapshot.CoTerminalReasons, EPhysAnimTerminalReason::ActivationAuthorityConflict));
		}

		return true;
	}
}
