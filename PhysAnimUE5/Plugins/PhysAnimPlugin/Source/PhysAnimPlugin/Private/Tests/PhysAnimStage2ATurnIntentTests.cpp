#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStage2ATurnIntentTest,
	"PhysAnim.Locomotion.Stage2ATurnIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStage2ATurnIntentTest::RunTest(const FString& Parameters)
{
	UPhysAnimComponent* TestComp = NewObject<UPhysAnimComponent>();
	
	// Helper to reset to "Allowed" state
	auto ResetToAllowed = [TestComp]() {
		TestComp->RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
		TestComp->BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked;
		TestComp->BridgeShellState = FBridgeShellState();
		TestComp->BridgeShellState.bInitialized = true;
		TestComp->LastPolicyControlUpdateTimeSeconds = TestComp->GetPhysAnimClockTime();
		TestComp->PolicyControlTicksExecuted = 1;
		TestComp->bStage2APhase3SimRootAttempted = false;
		TestComp->Stage2AConsecutivePolicyActiveFrames = 0;
		TestComp->bStage2AWalkIntentActive = false;
		TestComp->BridgeIntentState = FBridgeIntentState();
		TestComp->BridgeTrajectoryState = FBridgeTrajectoryState();
		TestComp->BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
		TestComp->Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
		TestComp->Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::NotEvaluated;
	};

	// 1. ActivationGating: Verify TryActivateStage2ATurnIntent respects frame threshold
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 2; // Below threshold (3)
		TestComp->TryActivateStage2ATurnIntent(0.05f, 45.0f);
		TestEqual(TEXT("TURN-01 ActiveIntent should remain empty at 2 frames"), TestComp->BridgeIntentState.ActiveIntent, FString());

		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3; // At threshold
		TestComp->TryActivateStage2ATurnIntent(0.05f, 45.0f);
		TestEqual(TEXT("TURN-02 ActiveIntent should be TurnLeft at 3 frames (positive yaw)"), TestComp->BridgeIntentState.ActiveIntent, FString(TEXT("TurnLeft")));
		TestEqual(TEXT("TURN-02B AuthorityState should be Locomoting"), (int32)TestComp->BridgeLocomotionAuthorityState, (int32)EBridgeLocomotionAuthorityState::Locomoting);
	}

	// 2. IntentSelection: Verify TurnLeft/TurnRight selection
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		
		TestComp->TryActivateStage2ATurnIntent(0.05f, 10.0f);
		TestEqual(TEXT("TURN-03 Positive delta -> TurnLeft"), TestComp->BridgeIntentState.ActiveIntent, FString(TEXT("TurnLeft")));

		TestComp->TryActivateStage2ATurnIntent(0.05f, -10.0f);
		TestEqual(TEXT("TURN-04 Negative delta -> TurnRight"), TestComp->BridgeIntentState.ActiveIntent, FString(TEXT("TurnRight")));
	}

	// 3. DesiredFacing: Verify DesiredFacingYawDegrees is updated
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		TestComp->TryActivateStage2ATurnIntent(0.05f, 90.0f);
		TestTrue(TEXT("TURN-05 HasDesiredFacing is true"), TestComp->BridgeIntentState.bHasDesiredFacing);
		TestEqual(TEXT("TURN-06 DesiredFacingYawDegrees updated"), TestComp->BridgeIntentState.DesiredFacingYawDegrees, 90.0f);
	}

	return true;
}
