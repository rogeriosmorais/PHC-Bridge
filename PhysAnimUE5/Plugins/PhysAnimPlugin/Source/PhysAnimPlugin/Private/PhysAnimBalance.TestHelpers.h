#pragma once

#if WITH_DEV_AUTOMATION_TESTS

#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR
#include "Editor.h"
#include "UObject/UObjectIterator.h"
#endif

namespace PhysAnimBalanceTestHelpers
{
	inline bool EvaluateBalanceModeSmokeOutcome(
		const EPhysAnimRuntimeState RuntimeState,
		const bool bInPublicBalanceEntryState,
		const EPhysAnimRuntimeState PublicBalanceEntryState,
		const bool bHasTransitionFailure,
		const bool bHasSafePhase2Denial,
		const FString& SafePhase2DenialReason,
		const FString& BalanceReadyTransitionFailureReason,
		FString& OutError)
	{
		OutError.Reset();

		if (RuntimeState == EPhysAnimRuntimeState::BalanceActive_Recovery ||
			RuntimeState == EPhysAnimRuntimeState::BalanceSafeDeny)
		{
			return true;
		}

		if (bInPublicBalanceEntryState)
		{
			OutError = FString::Printf(
				TEXT("[PhysAnimPieBalanceSmoke] Balance mode did not complete within the smoke window. state=%s."),
				UPhysAnimComponent::GetRuntimeStateName(PublicBalanceEntryState));
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
				Test->AddError(TEXT("[PhysAnimPieBalanceSmoke] PIE world was not available for outcome validation."));
				return true;
			}

			UPhysAnimComponent* FoundComponent = nullptr;
			for (TObjectIterator<UPhysAnimComponent> It; It; ++It)
			{
				if (It->GetWorld() == PlayWorld)
				{
					FoundComponent = *It;
					break;
				}
			}

			if (!FoundComponent)
			{
				Test->AddError(TEXT("[PhysAnimPieBalanceSmoke] No PhysAnim component was found in the PIE world."));
				return true;
			}

			const EPhysAnimRuntimeState RuntimeState = FoundComponent->GetRuntimeState();
			EPhysAnimRuntimeState PublicBalanceEntryState = RuntimeState;
			const bool bInPublicBalanceEntryState = FoundComponent->TryGetPublicBalanceEntryRuntimeState(PublicBalanceEntryState);
			FString OutcomeError;
			if (!EvaluateBalanceModeSmokeOutcome(
				RuntimeState,
				bInPublicBalanceEntryState,
				PublicBalanceEntryState,
				FoundComponent->HasBalanceReadyTransitionFailed(),
				FoundComponent->HasSafePhase2Denial(),
				FoundComponent->GetSafePhase2DenialReason(),
				FoundComponent->GetBalanceReadyTransitionFailureReason(),
				OutcomeError))
			{
				Test->AddError(OutcomeError);
			}
			return true;
		}

	private:
		FAutomationTestBase* Test = nullptr;
	};
#endif
}

#endif
