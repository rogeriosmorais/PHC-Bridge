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
		FString& OutError)
	{
		OutError.Reset();

		if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Standing)
		{
			if (!bHasPhysicalContinuityEvidence)
			{
				OutError = TEXT("[PhysAnimPieBalanceSmoke] BalanceActive_Standing is not a benchmark success without physical continuity evidence.");
				return false;
			}
			return true;
		}

		if (UPhysAnimComponent::IsStandingActivationRuntimeState(RuntimeState))
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Standing activation did not complete within the smoke window. state=%s."),
				UPhysAnimComponent::GetRuntimeStateName(RuntimeState));
			return false;
		}

		if (IsTruthfulBalanceSmokeTerminalReason(TerminalReason))
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Recorded terminal failure is diagnostic evidence, not a passing smoke outcome. reason=%d."),
				static_cast<int32>(TerminalReason));
			return false;
		}

		if (RuntimeState == EPhysAnimRuntimeState::FailStopped)
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Explicit fail-stop observed. state=%s"),
				UPhysAnimComponent::GetRuntimeStateName(RuntimeState));
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
