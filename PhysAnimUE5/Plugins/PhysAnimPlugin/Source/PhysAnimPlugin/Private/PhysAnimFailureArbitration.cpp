#include "PhysAnimFailureArbitration.h"

namespace PhysAnimFailureArbitration
{
	int32 GetTerminalReasonRank(EPhysAnimTerminalReason Reason)
	{
		switch (Reason)
		{
		case EPhysAnimTerminalReason::ActivationPhysicsNotStarted:
			return 1;
		case EPhysAnimTerminalReason::ActivationPhysicsAssetContractViolation:
			return 2;
		case EPhysAnimTerminalReason::ActivationCapsuleContractViolation:
			return 3;
		case EPhysAnimTerminalReason::ActivationTopologyChange:
			return 4;
		case EPhysAnimTerminalReason::ActivationKineticGateActive:
			return 5;
		case EPhysAnimTerminalReason::ActivationContinuousSimulationLost:
			return 6;
		case EPhysAnimTerminalReason::ActivationSupportFailure:
			return 7;
		case EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion:
			return 8;
		case EPhysAnimTerminalReason::ActivationTargetDiscontinuity:
			return 9;
		case EPhysAnimTerminalReason::ActivationUnstableGainOrDamping:
			return 10;
		case EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach:
			return 11;
		case EPhysAnimTerminalReason::ActivationPoseReferenceMismatch:
			return 12;
		case EPhysAnimTerminalReason::ActivationMovementReclaim:
			return 13;
		case EPhysAnimTerminalReason::ActivationShellHelperViolation:
			return 14;
		case EPhysAnimTerminalReason::ActivationAuthorityConflict:
			return 15;
		case EPhysAnimTerminalReason::ActivationStandingValidationTimeout:
			return 16;
		case EPhysAnimTerminalReason::None:
		default:
			return MAX_int32;
		}
	}

	FPhysAnimFailureArbitrationResult ArbitrateFailure(const TArray<FPhysAnimFailureCandidate>& Candidates)
	{
		FPhysAnimFailureArbitrationResult Result;

		int32 WinningIndex = INDEX_NONE;

		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FPhysAnimFailureCandidate& Candidate = Candidates[Index];
			if (Candidate.TerminalReason == EPhysAnimTerminalReason::None)
			{
				continue;
			}

			if (WinningIndex == INDEX_NONE)
			{
				WinningIndex = Index;
				continue;
			}

			const FPhysAnimFailureCandidate& CurrentWinner = Candidates[WinningIndex];

			if (Candidate.TerminalSubstepTimestamp < CurrentWinner.TerminalSubstepTimestamp)
			{
				WinningIndex = Index;
				continue;
			}

			if (Candidate.TerminalSubstepTimestamp == CurrentWinner.TerminalSubstepTimestamp &&
				GetTerminalReasonRank(Candidate.TerminalReason) < GetTerminalReasonRank(CurrentWinner.TerminalReason))
			{
				WinningIndex = Index;
			}
		}

		if (WinningIndex == INDEX_NONE)
		{
			return Result;
		}

		const FPhysAnimFailureCandidate& Winner = Candidates[WinningIndex];

		Result.TerminalReason = Winner.TerminalReason;
		Result.TerminalSubstepTimestamp = Winner.TerminalSubstepTimestamp;
		Result.bHasTerminalReason = true;

		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			if (Index == WinningIndex)
			{
				continue;
			}

			const FPhysAnimFailureCandidate& Candidate = Candidates[Index];
			if (Candidate.TerminalReason == EPhysAnimTerminalReason::None)
			{
				continue;
			}

			if (Candidate.TerminalSubstepTimestamp == Winner.TerminalSubstepTimestamp)
			{
				Result.CoTerminalReasons.Add(Candidate.TerminalReason);
			}
		}

		return Result;
	}
}
