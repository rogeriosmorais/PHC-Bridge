#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "PhysAnimRuntimeTerminationState.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR
#include "Editor.h"
#include "UObject/UObjectIterator.h"
#endif

namespace PhysAnimBalanceTestHelpers
{
	inline bool IsTruthfulBalanceSmokeSafeDenyReason(const FString& SafePhase2DenialReason)
	{
		return !SafePhase2DenialReason.IsEmpty() &&
			SafePhase2DenialReason != BalanceReadinessReasons::Phase2FailStopPrecursor;
	}

	inline bool IsTruthfulBalanceSmokeTerminalReason(const EPhysAnimTerminalReason Reason)
	{
		switch (Reason)
		{
		case EPhysAnimTerminalReason::ActivationContinuousSimulationLost:
		case EPhysAnimTerminalReason::ActivationSupportFailure:
		case EPhysAnimTerminalReason::ActivationProxyOutsideSupportRegion:
		case EPhysAnimTerminalReason::ActivationInstabilityThresholdBreach:
		case EPhysAnimTerminalReason::ActivationStandingValidationTimeout:
			return true;
		default:
			break;
		}
		return false;
	}

	inline bool EvaluateBalanceModeSmokeOutcome(
		const EPhysAnimRuntimeState RuntimeState,
		const bool bHasPhysicalContinuityEvidence,
		const EPhysAnimTerminalReason TerminalReason,
		const bool bInPublicBalanceEntryState,
		const EPhysAnimRuntimeState PublicBalanceEntryState,
		const bool bHasTransitionFailure,
		const bool bHasSafePhase2Denial,
		const FString& SafePhase2DenialReason,
		const FString& BalanceReadyTransitionFailureReason,
		const bool bLegacyStage1Context,
		FString& OutError)
	{
		OutError.Reset();
		static_cast<void>(bLegacyStage1Context);

		if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
		{
			if (!bHasPhysicalContinuityEvidence)
			{
				OutError = TEXT("[PhysAnimPieBalanceSmoke] BalanceActive_Standing is not a benchmark success without physical continuity evidence.");
				return false;
			}
			return true;
		}

		if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery)
		{
			OutError = TEXT("[PhysAnimPieBalanceSmoke] Balance entry did not settle into BalanceActive_Standing within the benchmark window.");
			return false;
		}

		if (RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny)
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] BalanceSafeDeny is not a benchmark success. reason=%s truthful=%s"),
				*SafePhase2DenialReason,
				IsTruthfulBalanceSmokeSafeDenyReason(SafePhase2DenialReason) ? TEXT("true") : TEXT("false"));
			return false;
		}

		if (bInPublicBalanceEntryState)
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Balance mode did not complete within the smoke window. state=%s."),
				UPhysAnimComponent::GetRuntimeStateName(PublicBalanceEntryState));
			return false;
		}

		if (IsTruthfulBalanceSmokeTerminalReason(TerminalReason))
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Recorded terminal failure is diagnostic evidence, not a passing smoke outcome. reason=%d."),
				static_cast<int32>(TerminalReason));
			return false;
		}

		if (RuntimeState == EPhysAnimRuntimeState::FailStopped || bHasTransitionFailure)
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Unsafe failure path observed. state=%s denial=%s fail=%s"),
				UPhysAnimComponent::GetRuntimeStateName(RuntimeState),
				*SafePhase2DenialReason,
				*BalanceReadyTransitionFailureReason);
			return false;
		}

		if (bHasSafePhase2Denial)
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Balance mode denied entry without publishing BalanceSafeDeny. state=%s reason=%s."),
				UPhysAnimComponent::GetRuntimeStateName(RuntimeState),
				*SafePhase2DenialReason);
			return false;
		}

		OutError = FString::Printf(
			TEXT("[PhysAnimPieBalanceSmoke] Expected active balance mode, but found state=%s."),
			UPhysAnimComponent::GetRuntimeStateName(RuntimeState));
		return false;
	}

#if WITH_EDITOR
	class FValidateBalanceModeSmokeOutcomeCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FValidateBalanceModeSmokeOutcomeCommand(FAutomationTestBase* InTest)
			: Test(InTest)
		{
		}

		virtual bool Update() override
		{
			UWorld* const PlayWorld = GEditor ? GEditor->PlayWorld : nullptr;
			if (!PlayWorld)
			{
				return true;
			}

			UPhysAnimComponent* Component = nullptr;
			for (TObjectIterator<UPhysAnimComponent> It; It; ++It)
			{
				if (It->GetWorld() == PlayWorld)
				{
					Component = *It;
					break;
				}
			}

			if (Component)
			{
				const EPhysAnimRuntimeState RuntimeState = Component->GetRuntimeState();
				const FPhysAnimRuntimeTerminationState& TerminationState = Component->GetLiveRuntimeEvidenceTerminationState();
				FString Error;
				const bool bSuccess = EvaluateBalanceModeSmokeOutcome(
					RuntimeState,
					UPhysAnimComponent::TestOnlyHasStartupProofPhysicalContinuityEvidence(TerminationState.LatestArtifact),
					TerminationState.TerminalReason,
					UPhysAnimComponent::TestOnlyIsBalanceEntryState(RuntimeState),
					RuntimeState,
					Component->HasRecordedBalanceTransitionFailure(),
					Component->HasSafePhase2Denial(),
					Component->GetSafePhase2DenialReason(),
					Component->GetBalanceReadyTransitionFailureReason(),
					Component->IsStage1(),
					Error);

				if (!bSuccess)
				{
					Test->AddError(Error);
				}
			}
			else
			{
				Test->AddError(TEXT("[PhysAnimPieBalanceSmoke] Could not find UPhysAnimComponent to validate outcome."));
			}

			return true;
		}

	private:
		FAutomationTestBase* Test;
	};
#endif
}

#endif
