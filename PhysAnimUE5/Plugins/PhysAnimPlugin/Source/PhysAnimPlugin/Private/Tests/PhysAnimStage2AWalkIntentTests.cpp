#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStage2AWalkIntentTest,
	"PhysAnim.Locomotion.Stage2AWalkIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPhysAnimStage2AWalkIntentTest::RunTest(const FString& Parameters)
{
	UPhysAnimComponent* TestComp = NewObject<UPhysAnimComponent>();
	
	// Helper to reset to "Allowed" state
	auto ResetToAllowed = [TestComp]() {
		TestComp->RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
		TestComp->BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked;
		TestComp->BridgeShellState.bInitialized = true;
		TestComp->LastPolicyControlUpdateTimeSeconds = FPlatformTime::Seconds();
		TestComp->PolicyControlTicksExecuted = 1;
		TestComp->bStage2APhase3SimRootAttempted = false;
		TestComp->Stage2AConsecutivePolicyActiveFrames = 0;
		TestComp->bStage2AWalkIntentActive = false;
		TestComp->BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
	};

	// 1. DeltaClamping: Verify BuildStage2AWalkDeltaCm clamps correctly
	{
		ResetToAllowed();
		// Speed = 60cm/s. 0.05s * 60 = 3.0cm (Below 6.0cm limit)
		FVector Delta1 = TestComp->BuildStage2AWalkDeltaCm(0.05f);
		TestEqual(TEXT("WALK-01 50ms delta should be 3cm forward"), (float)Delta1.X, 3.0f);

		// 1.0s * 60 = 60.0cm (Above 6.0cm limit -> Clamped to 6.0cm)
		FVector Delta2 = TestComp->BuildStage2AWalkDeltaCm(1.0f);
		TestEqual(TEXT("WALK-02 1s delta should be clamped to 6cm forward"), (float)Delta2.X, 6.0f);
	}

	// 2. ActivationGating: Verify TryActivateStage2AWalkIntent respects frame threshold
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 2; // Below threshold (3)
		TestComp->TryActivateStage2AWalkIntent(0.05f);
		TestFalse(TEXT("WALK-03 Should not activate at 2 frames"), TestComp->bStage2AWalkIntentActive);

		TestComp->Stage2AConsecutivePolicyActiveFrames = 3; // At threshold
		TestComp->TryActivateStage2AWalkIntent(0.05f);
		TestTrue(TEXT("WALK-04 Should activate at 3 frames"), TestComp->bStage2AWalkIntentActive);
	}

	// 3. StatePersistence: Verify bStage2AWalkIntentActive stays true
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		TestComp->TryActivateStage2AWalkIntent(0.05f);
		TestTrue(TEXT("WALK-05 bStage2AWalkIntentActive is true"), TestComp->bStage2AWalkIntentActive);
	}

	// 4. TrajectorySync: Verify BridgeTrajectoryState.MotionSource is set correctly
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		TestComp->TryActivateStage2AWalkIntent(0.05f);
		TestEqual(TEXT("WALK-06 MotionSource is Stage2A_KinematicShell"), TestComp->BridgeTrajectoryState.MotionSource, FString(TEXT("Stage2A_KinematicShell")));
	}

	// 5. AuthorityState: Verify BridgeLocomotionAuthorityState is ActiveLocomotion
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		TestComp->TryActivateStage2AWalkIntent(0.05f);
		TestEqual(TEXT("WALK-07 AuthorityState is ActiveLocomotion"), (int32)TestComp->BridgeLocomotionAuthorityState, (int32)EBridgeLocomotionAuthorityState::ActiveLocomotion);
	}

	// 6. MovementDelta: Verify non-zero delta command is recorded
	{
		ResetToAllowed();
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		const float DeltaTime = 0.05f;
		TestComp->TryActivateStage2AWalkIntent(DeltaTime);
		TestTrue(TEXT("WALK-08 LastWalkDeltaCm (command) is non-zero"), !TestComp->Stage2ALastWalkDeltaCm.IsNearlyZero());
		TestEqual(TEXT("WALK-09 LastWalkDeltaCm matches expected speed * time"), (double)TestComp->Stage2ALastWalkDeltaCm.X, (double)(60.0f * DeltaTime), 0.01);
		TestTrue(TEXT("WALK-10 Move is blocked (headless)"), TestComp->BridgeShellState.bLastMoveBlocked);
		TestTrue(TEXT("WALK-11 AcceptedWorldDeltaCm is zero (headless)"), TestComp->BridgeShellState.AcceptedWorldDeltaCm.IsNearlyZero());
	}

	return true;
}
