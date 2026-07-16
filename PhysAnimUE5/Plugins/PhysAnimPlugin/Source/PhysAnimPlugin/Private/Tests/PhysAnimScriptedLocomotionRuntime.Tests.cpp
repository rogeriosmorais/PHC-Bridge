#include "PhysAnimComponent.h"
#include "Misc/AutomationTest.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FPhysAnimScriptedLocomotionRuntimeTest,
	"PhysAnim.Locomotion.ScriptedRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FScriptedLocomotionFixture
	{
		AActor* Actor = nullptr;
		USceneComponent* Root = nullptr;
		UPhysAnimComponent* Component = nullptr;

		static FScriptedLocomotionFixture Create(UWorld* World)
		{
			FScriptedLocomotionFixture Fixture;
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

bool FPhysAnimScriptedLocomotionRuntimeTest::RunTest(const FString& Parameters)
{
	UWorld* World = GWorld;
	if (!World)
	{
		AddError(TEXT("SCRIPTED-SETUP Missing automation world"));
		return false;
	}

	FScriptedLocomotionFixture Fixture = FScriptedLocomotionFixture::Create(World);
	if (!Fixture.Actor || !Fixture.Component)
	{
		AddError(TEXT("SCRIPTED-SETUP Failed to create actor/component fixture"));
		return false;
	}

	UPhysAnimComponent* TestComp = Fixture.Component;
	auto ResetToAllowed = [TestComp, &Fixture]()
	{
		Fixture.Actor->SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator, false);
		TestComp->RuntimeState = EPhysAnimRuntimeState::BalanceActive_Standing;
		TestComp->BalanceTransitionShellAuthorityMode = EBalanceTransitionShellAuthorityMode::GameplayShellObservedOnly;
		TestComp->BridgeShellState = FBridgeShellState();
		TestComp->BridgeShellState.bInitialized = true;
		TestComp->BridgeShellState.LastAcceptedActorLocation = FVector::ZeroVector;
		TestComp->LastPolicyControlUpdateTimeSeconds = TestComp->GetPhysAnimClockTime();
		TestComp->PolicyControlTicksExecuted = 1;
		TestComp->bStage2APhase3SimRootAttempted = false;
		TestComp->Stage2AConsecutivePolicyActiveFrames = 3;
		TestComp->bStage2AWalkIntentActive = false;
		TestComp->Stage2ALastWalkDeltaCm = FVector::ZeroVector;
		TestComp->Stage2ALastTurnYawDeltaDegrees = 0.0f;
		TestComp->BridgeIntentState = FBridgeIntentState();
		TestComp->BridgeTrajectoryState = FBridgeTrajectoryState();
		TestComp->BridgeLocomotionAuthorityState = EBridgeLocomotionAuthorityState::Idle;
		TestComp->Stage2ALocomotionRequestState = EBridgeLocomotionRequestState::BalanceActiveStanding;
		TestComp->Stage2ALastLocomotionTerminalState = EStage2ALocomotionTerminalState::NotEvaluated;
	};

	// Red contract 1: scalable forward movement and yaw are composed in one real runtime step.
	{
		ResetToAllowed();
		TestTrue(TEXT("SCRIPTED-01 Gate opens from standing"), TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("ScriptedRuntimeTest")));
		TestTrue(
			TEXT("SCRIPTED-02 Half-speed scripted locomotion with yaw succeeds"),
			TestComp->TryActivateStage2AScriptedLocomotionIntent(0.05f, 0.5f, 5.0f, true));
		TestEqual(TEXT("SCRIPTED-03 Runtime enters locomotion"), (int32)TestComp->RuntimeState, (int32)EPhysAnimRuntimeState::LocomotionActiveShell);
		TestEqual(TEXT("SCRIPTED-04 Intent magnitude is clamped/scaled"), (double)TestComp->BridgeIntentState.IntentMagnitude, 0.5, 1.0e-6);
		TestEqual(TEXT("SCRIPTED-05 Desired speed is 30 cm/s"), (double)TestComp->BridgeIntentState.DesiredSpeedCmPerSecond, 30.0, 1.0e-4);
		TestTrue(TEXT("SCRIPTED-06 Desired facing is published"), TestComp->BridgeIntentState.bHasDesiredFacing);
		TestEqual(TEXT("SCRIPTED-07 Actor yaw is composed"), (double)Fixture.Actor->GetActorRotation().Yaw, 5.0, 1.0e-3);
		TestEqual(TEXT("SCRIPTED-08 Accepted displacement magnitude is 1.5 cm"), (double)TestComp->BridgeShellState.AcceptedWorldDeltaCm.Size2D(), 1.5, 1.0e-3);
		TestTrue(TEXT("SCRIPTED-09 Telemetry identifies scripted locomotion"), TestComp->LastStage2ALocomotionTelemetryLine.Contains(TEXT("locomotion_intent=ScriptedLocomotion")));
		TestTrue(
			TEXT("SCRIPTED-09B A second locomotion step continues without reopening the standing gate"),
			TestComp->TryActivateStage2AScriptedLocomotionIntent(0.05f, 0.5f, 0.0f, true));
		TestEqual(TEXT("SCRIPTED-09C Two half-speed steps travel 3 cm"), (double)Fixture.Actor->GetActorLocation().Size2D(), 3.0, 1.0e-3);
	}

	// Red contract 2: destructive control applies the same shell transform but hides trajectory conditioning.
	{
		ResetToAllowed();
		TestTrue(TEXT("SCRIPTED-10 Gate opens for conditioning-drop arm"), TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("ScriptedRuntimeNoConditioningTest")));
		TestTrue(
			TEXT("SCRIPTED-11 Shell motion succeeds with conditioning suppressed"),
			TestComp->TryActivateStage2AScriptedLocomotionIntent(0.05f, 0.5f, 5.0f, false));
		TestEqual(TEXT("SCRIPTED-12 Shell displacement is preserved"), (double)TestComp->BridgeShellState.AcceptedWorldDeltaCm.Size2D(), 1.5, 1.0e-3);
		TestTrue(TEXT("SCRIPTED-13 Intent is hidden from downstream conditioning"), TestComp->BridgeIntentState.ActiveIntent.IsEmpty());
		TestEqual(TEXT("SCRIPTED-14 Hidden intent magnitude is zero"), (double)TestComp->BridgeIntentState.IntentMagnitude, 0.0, 0.0);
		TestFalse(TEXT("SCRIPTED-15 Hidden trajectory is uninitialized"), TestComp->BridgeTrajectoryState.bInitialized);
		TestEqual(TEXT("SCRIPTED-16 Runtime remains locomotion active"), (int32)TestComp->RuntimeState, (int32)EPhysAnimRuntimeState::LocomotionActiveShell);
	}

	// Red contract 3: stop returns to standing without resetting bridge/actor identity.
	{
		ResetToAllowed();
		TestTrue(TEXT("SCRIPTED-17 Gate opens before stop test"), TestComp->TryOpenStage2ALocomotionRequestGate(TEXT("ScriptedRuntimeStopTest")));
		TestTrue(TEXT("SCRIPTED-18 Locomotion starts before stop"), TestComp->TryActivateStage2AScriptedLocomotionIntent(0.05f, 1.0f, 0.0f, true));
		const FVector LocationBeforeStop = Fixture.Actor->GetActorLocation();
		TestTrue(TEXT("SCRIPTED-19 Stop transition succeeds"), TestComp->StopStage2AScriptedLocomotionAndReturnToStanding());
		TestEqual(TEXT("SCRIPTED-20 Runtime returns to standing"), (int32)TestComp->RuntimeState, (int32)EPhysAnimRuntimeState::BalanceActive_Standing);
		TestEqual(TEXT("SCRIPTED-21 Actor is not teleported during stop"), (double)FVector::Distance(LocationBeforeStop, Fixture.Actor->GetActorLocation()), 0.0, 0.0);
		TestTrue(TEXT("SCRIPTED-22 Intent is cleared on stop"), TestComp->BridgeIntentState.ActiveIntent.IsEmpty());
		TestFalse(TEXT("SCRIPTED-23 Trajectory is cleared on stop"), TestComp->BridgeTrajectoryState.bInitialized);
		TestEqual(TEXT("SCRIPTED-24 Accepted velocity is zero on stop"), (double)TestComp->BridgeShellState.AcceptedPlanarVelocityCmPerSecond.Size2D(), 0.0, 0.0);
		TestEqual(TEXT("SCRIPTED-25 Locomotion authority returns idle"), (int32)TestComp->BridgeLocomotionAuthorityState, (int32)EBridgeLocomotionAuthorityState::Idle);
		TestEqual(TEXT("SCRIPTED-26 Request state returns standing"), (int32)TestComp->Stage2ALocomotionRequestState, (int32)EBridgeLocomotionRequestState::BalanceActiveStanding);
		TestTrue(TEXT("SCRIPTED-27 Stop telemetry is emitted"), TestComp->LastStage2ALocomotionTelemetryLine.Contains(TEXT("locomotion_intent=StopAndSettle")));
	}

	// Red contract 4: the new seam cannot bypass the existing policy/gate requirements.
	{
		ResetToAllowed();
		TestComp->RuntimeState = EPhysAnimRuntimeState::BridgeActive;
		TestFalse(
			TEXT("SCRIPTED-28 Invalid runtime state denies scripted locomotion"),
			TestComp->TryActivateStage2AScriptedLocomotionIntent(0.05f, 1.0f, 0.0f, true));
		TestEqual(TEXT("SCRIPTED-29 Denial preserves actor position"), (double)Fixture.Actor->GetActorLocation().Size(), 0.0, 0.0);
	}

	Fixture.Destroy();
	return true;
}
