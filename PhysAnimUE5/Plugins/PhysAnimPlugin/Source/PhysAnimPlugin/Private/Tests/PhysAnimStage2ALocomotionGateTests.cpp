#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStage2ALocomotionGateTest,
	"PhysAnim.Locomotion.Stage2AGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStage2ALocomotionGateTest::RunTest(const FString& Parameters)
{
	UPhysAnimComponent* TestComp = NewObject<UPhysAnimComponent>();
	
	// Helper to reset to "Allowed" state
	auto ResetToAllowed = [TestComp]() {
		TestComp->RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
		TestComp->BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly;
		TestComp->BridgeShellState.bInitialized = true;
		TestComp->bPolicyTargetsAppliedLastFrame = true;
		TestComp->PolicyControlTicksExecuted = 1;
		TestComp->LastPolicyControlUpdateTimeSeconds = TestComp->GetPhysAnimClockTime();
		TestComp->bStage2APhase3SimRootAttempted = false;
	};

	// Case: Allowed
	{
		ResetToAllowed();
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-01 Allowed when all conditions met"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Allowed);
	}

	// Case: Denied_NotBalanceActiveStanding
	{
		ResetToAllowed();
		TestComp->RuntimeState = EPhysAnimRuntimeState::BridgeActive;
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-02 Denied when not BalanceActive_Standing"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Denied_NotBalanceActiveStanding);
	}

	// Case: Denied_PolicyOutputInactive
	{
		ResetToAllowed();
		TestComp->bPolicyTargetsAppliedLastFrame = false;
		TestComp->PolicyControlTicksExecuted = 0;
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-03 Denied when policy output inactive"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Denied_PolicyOutputInactive);
	}

	// Case: Denied_SimRootAttempted
	{
		ResetToAllowed();
		TestComp->bStage2APhase3SimRootAttempted = true;
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-04 Denied when SimRoot attempted"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Denied_SimRootAttempted);
	}

	// Case: production standing state owns shell authority without activating the explicit transition lock.
	{
		ResetToAllowed();
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-05 Production standing shell authority is accepted"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Allowed);
	}

	// Case: Denied_ShellRootUnlocked (bInitialized=false)
	{
		ResetToAllowed();
		TestComp->BridgeShellState.bInitialized = false;
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-06 Denied when shell state not initialized"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Denied_ShellRootUnlocked);
	}

	// Case: TryOpen success
	{
		ResetToAllowed();
		bool bOpened = TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("TestReason"));
		TestTrue(TEXT("TryOpen success"), bOpened);
		TestEqual(TEXT("State should be LocomotionActiveShell"), (uint8)TestComp->RuntimeState, (uint8)EPhysAnimRuntimeState::LocomotionActiveShell);
	}

	// Case: TryOpen denial (trigger denial while in Standing -> transition to Denied)
	{
		ResetToAllowed();
		TestComp->bPolicyTargetsAppliedLastFrame = false; 
		TestComp->PolicyControlTicksExecuted = 0;
		bool bOpened = TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("TestReason"));
		TestFalse(TEXT("TryOpen denied"), bOpened);
		TestEqual(TEXT("State should be LocomotionActiveShellDenied"), (uint8)TestComp->RuntimeState, (uint8)EPhysAnimRuntimeState::LocomotionActiveShellDenied);
	}

	// Case: Re-entry from LocomotionActiveShell is allowed only while explicitly requested.
	{
		ResetToAllowed();
		TestComp->RuntimeState = EPhysAnimRuntimeState::LocomotionActiveShell;
		TestComp->Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::LocomotionRequested;
		TestComp->BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Locomoting;
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-10 LocomotionActiveShell re-entry allowed when request is active"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Allowed);
	}

	// Case: Re-entry from LocomotionActiveShell is denied when request state is stale/cleared.
	{
		ResetToAllowed();
		TestComp->RuntimeState = EPhysAnimRuntimeState::LocomotionActiveShell;
		TestComp->Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
		EStage2ALocomotionTerminalState Result = TestComp->EvaluateStage2ALocomotionRequestGate();
		TestEqual(TEXT("GATE-11 LocomotionActiveShell denied without active request"), (uint8)Result, (uint8)EStage2ALocomotionTerminalState::Denied_NotBalanceActiveStanding);
	}

	// Case: TryOpen denial (not in Standing -> should NOT change state)
	{
		ResetToAllowed();
		TestComp->RuntimeState = EPhysAnimRuntimeState::BridgeActive;
		bool bOpened = TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("TestReason"));
		TestFalse(TEXT("TryOpen denied"), bOpened);
		TestEqual(TEXT("State should remain BridgeActive"), (uint8)TestComp->RuntimeState, (uint8)EPhysAnimRuntimeState::BridgeActive);
	}

	return true;
}
