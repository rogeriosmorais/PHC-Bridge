#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimStage2AWalkIntentTest,
	"PhysAnim.Locomotion.Stage2AWalkIntent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FStage2AWalkIntentFixture
	{
		AActor* Actor = nullptr;
		USceneComponent* Root = nullptr;
		UPhysAnimComponent* Component = nullptr;

		static FStage2AWalkIntentFixture Create(UWorld* World)
		{
			FStage2AWalkIntentFixture Fixture;
			Fixture.Actor = World ? World->SpawnActor<AActor>() : nullptr;
			if (!Fixture.Actor)
			{
				return Fixture;
			}

			Fixture.Root = NewObject<USceneComponent>(Fixture.Actor);
			Fixture.Actor->SetRootComponent(Fixture.Root);
			Fixture.Root->RegisterComponent();

			Fixture.Component = NewObject<UPhysAnimComponent>(Fixture.Actor);
			Fixture.Component->RegisterComponent();
			return Fixture;
		}

		void Destroy()
		{
			if (Actor)
			{
				Actor->Destroy();
			}
		}
	};
}

bool FPhysAnimStage2AWalkIntentTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddError(TEXT("WALK-SETUP Missing automation world"));
		return false;
	}

	FStage2AWalkIntentFixture Fixture = FStage2AWalkIntentFixture::Create(World);
	if (!Fixture.Actor || !Fixture.Component)
	{
		AddError(TEXT("WALK-SETUP Failed to create actor/component fixture"));
		return false;
	}

	UPhysAnimComponent* TestComp = Fixture.Component;

	// Helper to reset to "Allowed" state - must be lambda in RunTest to access private members via friendship
	auto ResetToAllowed = [TestComp]()
	{
		TestComp->RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
		TestComp->BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::TransitionOwnedShellLocked;
		TestComp->BridgeShellState = FBridgeShellState();
		TestComp->BridgeShellState.bInitialized = true;
		TestComp->LastPolicyControlUpdateTimeSeconds = TestComp->GetPhysAnimClockTime();
		TestComp->PolicyControlTicksExecuted = 1;
		TestComp->bStage2APhase3SimRootAttempted = false;
		TestComp->Stage2AConsecutivePolicyActiveFrames = 0;
		TestComp->bStage2AWalkIntentActive = false;
		TestComp->Stage2ALastWalkDeltaCm = FVector::ZeroVector;
		TestComp->BridgeIntentState = FBridgeIntentState();
		TestComp->BridgeTrajectoryState = FBridgeTrajectoryState();
		TestComp->BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
		TestComp->Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
		TestComp->Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::NotEvaluated;
	};

	// 1. DeltaClamping: actor forward +X, 50ms = 3cm; 1s clamps to 6cm.
	{
		ResetToAllowed();
		Fixture.Actor->SetActorRotation(FRotator::ZeroRotator);
		const FVector Delta1 = TestComp->BuildStage2AWalkDeltaCm(0.05f);
		TestEqual(TEXT("WALK-01 50ms delta should be 3cm along actor forward X"), (double)Delta1.X, 3.0, 0.01);
		TestEqual(TEXT("WALK-01B 50ms delta should not drift Y"), (double)Delta1.Y, 0.0, 0.01);

		const FVector Delta2 = TestComp->BuildStage2AWalkDeltaCm(1.0f);
		TestEqual(TEXT("WALK-02 1s delta should be clamped to 6cm forward"), (double)Delta2.X, 6.0, 0.01);
	}

	// 2. Actor forward rotation: yaw 90 must move along +Y, not hard-coded +X.
	{
		ResetToAllowed();
		Fixture.Actor->SetActorLocation(FVector::ZeroVector, false);
		Fixture.Actor->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
		const FVector RotatedDelta = TestComp->BuildStage2AWalkDeltaCm(0.05f);
		TestEqual(TEXT("WALK-03 Rotated actor should not move on X"), (double)RotatedDelta.X, 0.0, 0.01);
		TestEqual(TEXT("WALK-04 Rotated actor should move 3cm on Y"), (double)RotatedDelta.Y, 3.0, 0.01);
	}

	// 3. ActivationGating: Verify TryActivateStage2AWalkIntent respects frame threshold.
	{
		ResetToAllowed();
		Fixture.Actor->SetActorLocation(FVector::ZeroVector, false);
		Fixture.Actor->SetActorRotation(FRotator::ZeroRotator);
		TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("WalkIntentTest"));
		TestComp->Stage2AConsecutivePolicyActiveFrames = 2;
		TestFalse(TEXT("WALK-05 Should not activate at 2 frames"), TestComp->TryActivateStage2AWalkIntent(0.05f));
		TestFalse(TEXT("WALK-06 bStage2AWalkIntentActive remains false at 2 frames"), TestComp->bStage2AWalkIntentActive);
		TestEqual(TEXT("WALK-07 Denial terminal state is policy inactive/min frames"), (int32)TestComp->Stage2ALastLocomotionTerminalState, (int32)EStage2ALocomotionTerminalState::Denied_PolicyOutputInactive);

		ResetToAllowed();
		Fixture.Actor->SetActorLocation(FVector::ZeroVector, false);
		Fixture.Actor->SetActorRotation(FRotator::ZeroRotator);
		TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("WalkIntentTest"));
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		TestTrue(TEXT("WALK-08 Should activate at 3 frames"), TestComp->TryActivateStage2AWalkIntent(0.05f));
		TestTrue(TEXT("WALK-09 bStage2AWalkIntentActive is true"), TestComp->bStage2AWalkIntentActive);
	}

	// 4. Intent and trajectory state must be fully populated.
	{
		ResetToAllowed();
		Fixture.Actor->SetActorLocation(FVector::ZeroVector, false);
		Fixture.Actor->SetActorRotation(FRotator::ZeroRotator);
		TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("WalkIntentStateTest"));
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		TestTrue(TEXT("WALK-10 Activation succeeds for state sync"), TestComp->TryActivateStage2AWalkIntent(0.05f));

		TestEqual(TEXT("WALK-11 ActiveIntent is WalkForward"), TestComp->BridgeIntentState.ActiveIntent, FString(TEXT("WalkForward")));
		TestEqual(TEXT("WALK-12 MotionSource is Stage2A_KinematicShell"), TestComp->BridgeTrajectoryState.MotionSource, FString(TEXT("Stage2A_KinematicShell")));
		TestEqual(TEXT("WALK-13 Intent magnitude is 1"), (double)TestComp->BridgeIntentState.IntentMagnitude, 1.0, 0.01);
		TestEqual(TEXT("WALK-14 Desired speed is 60cm/s"), (double)TestComp->BridgeIntentState.DesiredSpeedCmPerSecond, 60.0, 0.01);
		TestFalse(TEXT("WALK-15 Walk does not request desired facing"), TestComp->BridgeIntentState.bHasDesiredFacing);
		TestTrue(TEXT("WALK-16 Trajectory initialized"), TestComp->BridgeTrajectoryState.bInitialized);
		TestEqual(TEXT("WALK-17 Trajectory dt is 0.05"), (double)TestComp->BridgeTrajectoryState.LastDeltaTimeSeconds, 0.05, 0.001);
		TestEqual(TEXT("WALK-18 Desired velocity X is 60cm/s"), (double)TestComp->BridgeTrajectoryState.DesiredVelocityCmPerSecond.X, 60.0, 0.01);
	}

	// 5. Movement acceptance state and telemetry.
	{
		ResetToAllowed();
		Fixture.Actor->SetActorLocation(FVector::ZeroVector, false);
		Fixture.Actor->SetActorRotation(FRotator::ZeroRotator);
		TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("WalkIntentMovementTest"));
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		const float DeltaTime = 0.05f;
		TestTrue(TEXT("WALK-19 Activation succeeds for movement"), TestComp->TryActivateStage2AWalkIntent(DeltaTime));
		TestEqual(TEXT("WALK-20 LastWalkDeltaCm command is 3cm"), (double)TestComp->Stage2ALastWalkDeltaCm.X, (double)(60.0f * DeltaTime), 0.01);
		TestEqual(TEXT("WALK-21 AcceptedWorldDeltaCm is 3cm"), (double)TestComp->BridgeShellState.AcceptedWorldDeltaCm.X, (double)(60.0f * DeltaTime), 0.01);
		TestFalse(TEXT("WALK-22 Move is not blocked"), TestComp->BridgeShellState.bLastMoveBlocked);
		TestEqual(TEXT("WALK-23 AuthorityState is Locomoting"), (int32)TestComp->BridgeLocomotionAuthorityState, (int32)EBridgeLocomotionAuthorityState::Locomoting);
		TestEqual(TEXT("WALK-24 RuntimeState is LocomotionActiveShell"), (int32)TestComp->RuntimeState, (int32)EPhysAnimRuntimeState::LocomotionActiveShell);
		TestTrue(TEXT("WALK-25 Telemetry reports WalkForward"), TestComp->LastStage2ALocomotionTelemetryLine.Contains(TEXT("locomotion_intent=WalkForward")));
		TestTrue(TEXT("WALK-26 Telemetry reports terminal_state=Allowed"), TestComp->LastStage2ALocomotionTelemetryLine.Contains(TEXT("terminal_state=Allowed")));
	}

	// 6. Denial cleanup: Intent and trajectory must be cleared on denial.
	{
		ResetToAllowed();
		Fixture.Actor->SetActorLocation(FVector::ZeroVector, false);
		
		// Manually populate state before a failed attempt
		TestComp->BridgeIntentState.ActiveIntent = TEXT("StaleIntent");
		TestComp->BridgeTrajectoryState.bInitialized = true;
		
		// Attempt activation without opening the gate - this should fail
		TestFalse(TEXT("WALK-27 Activation fails when gate is closed"), TestComp->TryActivateStage2AWalkIntent(0.05f));
		
		// Verify state is cleared
		TestTrue(TEXT("WALK-28 Intent name is cleared on denial"), TestComp->BridgeIntentState.ActiveIntent.IsEmpty());
		TestFalse(TEXT("WALK-29 Trajectory bInitialized is cleared on denial"), TestComp->BridgeTrajectoryState.bInitialized);
		TestEqual(TEXT("WALK-30 RequestState is LocomotionRequestDenied"), (int32)TestComp->Stage2ALocomotionRequestState, (int32)EBridgeLocomotionRequestState::LocomotionRequestDenied);
	}

	Fixture.Destroy();
	return true;
}
